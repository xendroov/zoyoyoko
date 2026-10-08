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

    // HP kontrolu
    if (config_.potion.hpEnabled && player.maxHp > 0) {
        float hpPercent = (static_cast<float>(player.curHp) / player.maxHp) * 100.0f;
        if (hpPercent < config_.potion.hpThreshold) {
            state_ = BotState::Healing;
            if (config_.potion.hpSkillId > 0)
                game.UseSkill(config_.potion.hpSkillId);
            gui::Bridge::Get().SendLog("[POT] HP potion kullanildi");
        }
    }

    // MP kontrolu
    if (config_.potion.mpEnabled && player.maxMp > 0) {
        float mpPercent = (static_cast<float>(player.curMp) / player.maxMp) * 100.0f;
        if (mpPercent < config_.potion.mpThreshold) {
            if (config_.potion.mpSkillId > 0)
                game.UseSkill(config_.potion.mpSkillId);
            gui::Bridge::Get().SendLog("[POT] MP potion kullanildi");
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
    for (uint32_t skillId : config_.buff.skills) {
        game.UseSkill(skillId);
        Sleep(HumanizedDelay(500, 120));
    }

    gui::Bridge::Get().SendLog("[BUFF] Buff'lar yenilendi");
}

void Engine::TickAttack() {
    auto& game = core::Game::Get();

    // Hedef yoksa veya gecersizse yeni hedef bul
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

    // Skill rotation
    if (!config_.attack.skillRotation.empty()) {
        uint32_t skillId = config_.attack.skillRotation[skillRotationIdx_];
        game.UseSkill(skillId);
        skillRotationIdx_ = (skillRotationIdx_ + 1) % config_.attack.skillRotation.size();
    } else {
        // Normal attack: WIZ_ATTACK paketi
        core::Packet pkt(core::Opcode::WIZ_ATTACK);
        pkt.WriteWord(static_cast<uint16_t>(currentTargetId_));
        pkt.WriteByte(1); // attack type
        game.SendPacket(pkt.Data(), pkt.Size());
    }
}

void Engine::TickLoot() {
    // TODO: Yerdeki itemleri toplama
    // WIZ_ITEM_GET paketi ile
}

void Engine::TickDeath() {
    state_ = BotState::Dead;
    gui::Bridge::Get().SendLog("[BOT] Karakter oldu");
}

bool Engine::FindAndSelectTarget() {
    auto& game = core::Game::Get();
    auto npcs = game.GetNearbyNpcs(config_.target.radius);

    std::lock_guard<std::mutex> lock(configMutex_);

    for (auto& npc : npcs) {
        if (npc.isDead || !npc.isEnemy) continue;

        // Secili mob listesi varsa kontrol et
        if (!config_.target.selectedMobNames.empty()) {
            bool found = false;
            for (auto& name : config_.target.selectedMobNames) {
                if (std::string(npc.name).find(name) != std::string::npos) {
                    found = true;
                    break;
                }
            }
            if (!found) continue;
        }

        currentTargetId_ = npc.id;
        game.SelectTarget(npc.id);

        std::ostringstream ss;
        ss << "[TARGET] Hedef secildi: " << npc.name << " (ID: " << npc.id << ")";
        gui::Bridge::Get().SendLog(ss.str());
        return true;
    }
    return false;
}

bool Engine::IsTargetValid() {
    if (currentTargetId_ == 0) return false;
    // TODO: Hedefin hala yasayip yasamadigini kontrol et
    return true;
}

// --- JSON Config ---
// Basit JSON serialization (harici kutuphane bagimliligini azaltmak icin)

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
    ss << "    \"mpEnabled\": " << (config_.potion.mpEnabled ? "true" : "false") << ",\n";
    ss << "    \"mpThreshold\": " << config_.potion.mpThreshold << "\n";
    ss << "  },\n";

    ss << "  \"loot\": {\n";
    ss << "    \"autoLoot\": " << (config_.loot.autoLoot ? "true" : "false") << ",\n";
    ss << "    \"lootCoins\": " << (config_.loot.lootCoins ? "true" : "false") << ",\n";
    ss << "    \"lootItems\": " << (config_.loot.lootItems ? "true" : "false") << "\n";
    ss << "  },\n";

    ss << "  \"target\": {\n";
    ss << "    \"radius\": " << config_.target.radius << "\n";
    ss << "  }\n";

    ss << "}";
    return ss.str();
}

bool Engine::ConfigFromJson(const std::string& json) {
    // TODO: JSON parsing (nlohmann/json veya minimal parser)
    return true;
}

} // namespace bot
