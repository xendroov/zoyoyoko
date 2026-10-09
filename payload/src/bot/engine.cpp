#include "bot/engine.h"
#include "core/game.h"
#include "core/packets.h"
#include "gui/bridge.h"
#include <Windows.h>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace bot {

Engine& Engine::Get() {
    static Engine instance;
    return instance;
}

void Engine::Start() {
    if (running_.load()) return;
    running_ = true;
    state_ = BotState::Idle;
    thread_ = std::thread(&Engine::BotLoop, this);
}

void Engine::Stop() {
    running_ = false;
    if (thread_.joinable())
        thread_.join();
    state_ = BotState::Idle;
}

const char* Engine::GetStateString() const {
    switch (state_.load()) {
        case BotState::Idle:      return "idle";
        case BotState::Searching: return "searching";
        case BotState::Attacking: return "attacking";
        case BotState::Buffing:   return "buffing";
        case BotState::Healing:   return "healing";
        case BotState::Looting:   return "looting";
        case BotState::Dead:      return "dead";
    }
    return "unknown";
}

DWORD Engine::HumanizedDelay(DWORD baseMs, DWORD sigmaMs) {
    double val = baseMs + dist_(rng_) * sigmaMs;
    if (val < baseMs * 0.3) val = baseMs * 0.3;
    if (val > baseMs * 2.5) val = baseMs * 2.5;
    return static_cast<DWORD>(val);
}

void Engine::BotLoop() {
    while (running_.load()) {
        auto& game = core::Game::Get();
        if (!game.IsPlayerInGame()) {
            Sleep(HumanizedDelay(1000, 200));
            continue;
        }

        if (game.IsPlayerDead()) {
            TickDeath();
            Sleep(HumanizedDelay(500, 100));
            continue;
        }

        TickPotion();
        TickBuff();

        if (config_.attack.enabled)
            TickAttack();

        if (config_.loot.autoLoot)
            TickLoot();

        Sleep(HumanizedDelay(100, 30));
    }
}

void Engine::TickPotion() {
    auto& game = core::Game::Get();
    auto player = game.GetPlayer();

    std::lock_guard<std::mutex> lock(configMutex_);

    if (config_.potion.hpEnabled && player.maxHp > 0) {
        float hpPercent = (static_cast<float>(player.curHp) / player.maxHp) * 100.0f;
        if (hpPercent < config_.potion.hpThreshold) {
            state_ = BotState::Healing;
            if (config_.potion.hpItemCell > 0)
                game.UseItem(config_.potion.hpItemCell);
            gui::Bridge::Get().SendLog("[POT] HP potion kullanildi");
        }
    }

    if (config_.potion.spEnabled && player.maxSp > 0) {
        float spPercent = (static_cast<float>(player.curSp) / player.maxSp) * 100.0f;
        if (spPercent < config_.potion.spThreshold) {
            if (config_.potion.spItemCell > 0)
                game.UseItem(config_.potion.spItemCell);
            gui::Bridge::Get().SendLog("[POT] SP potion kullanildi");
        }
    }
}

void Engine::TickBuff() {
    if (!config_.buff.enabled) return;

    auto now = static_cast<float>(GetTickCount()) / 1000.0f;
    if (now - lastBuffTime_ < config_.buff.checkInterval) return;
    lastBuffTime_ = now;

    state_ = BotState::Buffing;
    auto& game = core::Game::Get();

    std::lock_guard<std::mutex> lock(configMutex_);
    for (uint32_t skillSlot : config_.buff.skills) {
        game.UseSkill(skillSlot);
        Sleep(HumanizedDelay(500, 120));
    }

    gui::Bridge::Get().SendLog("[BUFF] Buff'lar yenilendi");
}

void Engine::TickAttack() {
    auto& game = core::Game::Get();

    if (!IsTargetValid()) {
        state_ = BotState::Searching;
        if (!FindAndSelectTarget()) {
            Sleep(HumanizedDelay(500, 150));
            return;
        }
    }

    state_ = BotState::Attacking;

    auto now = static_cast<float>(GetTickCount()) / 1000.0f;
    float attackInterval = 1.0f / config_.attack.attackSpeed;
    if (now - lastAttackTime_ < attackInterval) return;
    lastAttackTime_ = now;

    std::lock_guard<std::mutex> lock(configMutex_);

    if (!config_.attack.skillRotation.empty()) {
        uint32_t skillSlot = config_.attack.skillRotation[skillRotationIdx_];
        game.UseSkill(skillSlot);
        skillRotationIdx_ = (skillRotationIdx_ + 1) % config_.attack.skillRotation.size();
    } else {
        game.Attack();
    }
}

void Engine::TickLoot() {
    auto& game = core::Game::Get();
    auto items = game.GetGroundItems(config_.loot.lootRadius);

    if (!items.empty()) {
        state_ = BotState::Looting;
        game.PickupItem();
        Sleep(HumanizedDelay(300, 80));
    }
}

void Engine::TickDeath() {
    state_ = BotState::Dead;
    gui::Bridge::Get().SendLog("[BOT] Karakter oldu");
}

bool Engine::FindAndSelectTarget() {
    auto& game = core::Game::Get();
    auto mobs = game.GetNearbyMobs(config_.target.radius);

    std::lock_guard<std::mutex> lock(configMutex_);

    for (auto& mob : mobs) {
        if (mob.isDead) continue;
        if (mob.type != 2) continue; // sadece monster

        if (!config_.target.selectedMobNames.empty()) {
            bool found = false;
            for (auto& name : config_.target.selectedMobNames) {
                if (std::string(mob.name).find(name) != std::string::npos) {
                    found = true;
                    break;
                }
            }
            if (!found) continue;
        }

        currentTargetVid_ = mob.vid;
        game.SelectTarget(mob.vid);

        std::ostringstream ss;
        ss << "[TARGET] Hedef secildi: " << mob.name << " (VID: " << mob.vid << ")";
        gui::Bridge::Get().SendLog(ss.str());
        return true;
    }
    return false;
}

bool Engine::IsTargetValid() {
    if (currentTargetVid_ == 0) return false;
    return true;
}

std::string Engine::ConfigToJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"enabled\": " << (config_.enabled ? "true" : "false") << ",\n";

    ss << "  \"attack\": {\n";
    ss << "    \"enabled\": " << (config_.attack.enabled ? "true" : "false") << ",\n";
    ss << "    \"targetLock\": " << (config_.attack.targetLock ? "true" : "false") << ",\n";
    ss << "    \"range\": " << config_.attack.range << ",\n";
    ss << "    \"attackSpeed\": " << config_.attack.attackSpeed << "\n";
    ss << "  },\n";

    ss << "  \"buff\": {\n";
    ss << "    \"enabled\": " << (config_.buff.enabled ? "true" : "false") << ",\n";
    ss << "    \"checkInterval\": " << config_.buff.checkInterval << "\n";
    ss << "  },\n";

    ss << "  \"potion\": {\n";
    ss << "    \"hpEnabled\": " << (config_.potion.hpEnabled ? "true" : "false") << ",\n";
    ss << "    \"hpThreshold\": " << config_.potion.hpThreshold << ",\n";
    ss << "    \"spEnabled\": " << (config_.potion.spEnabled ? "true" : "false") << ",\n";
    ss << "    \"spThreshold\": " << config_.potion.spThreshold << "\n";
    ss << "  },\n";

    ss << "  \"loot\": {\n";
    ss << "    \"autoLoot\": " << (config_.loot.autoLoot ? "true" : "false") << ",\n";
    ss << "    \"lootRadius\": " << config_.loot.lootRadius << "\n";
    ss << "  },\n";

    ss << "  \"target\": {\n";
    ss << "    \"radius\": " << config_.target.radius << "\n";
    ss << "  }\n";

    ss << "}";
    return ss.str();
}

bool Engine::ConfigFromJson(const std::string& json) {
    return true;
}

} // namespace bot
