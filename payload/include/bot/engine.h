#pragma once
#include <cstdint>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <mutex>

namespace bot {

struct BotConfig {
    // Genel
    bool enabled = false;

    // Attack
    struct {
        bool   enabled = false;
        bool   targetLock = true;
        float  range = 50.0f;
        float  attackSpeed = 1.0f;
        std::vector<uint32_t> selectedMobs;
        std::vector<uint32_t> skillRotation;
    } attack;

    // Buff
    struct {
        bool enabled = false;
        std::vector<uint32_t> skills;
        float checkInterval = 30.0f;
    } buff;

    // Potion
    struct {
        bool  hpEnabled = false;
        float hpThreshold = 60.0f;
        uint32_t hpSkillId = 0;  // 0 = otomatik sec

        bool  mpEnabled = false;
        float mpThreshold = 30.0f;
        uint32_t mpSkillId = 0;

        bool  minorEnabled = false;
        float minorThreshold = 80.0f;
        uint32_t minorSkillId = 0;
    } potion;

    // Loot
    struct {
        bool autoLoot = false;
        bool lootCoins = true;
        bool lootItems = true;
    } loot;

    // Target
    struct {
        float   radius = 50.0f;
        std::vector<std::string> selectedMobNames;
    } target;
};

enum class BotState {
    Idle,
    Searching,
    Attacking,
    Buffing,
    Healing,
    Looting,
    Dead
};

class Engine {
public:
    static Engine& Get();

    void Start();
    void Stop();
    bool IsRunning() const { return running_.load(); }

    BotState GetState() const { return state_.load(); }
    const char* GetStateString() const;

    BotConfig& Config() { return config_; }
    const BotConfig& Config() const { return config_; }

    // JSON config
    std::string ConfigToJson() const;
    bool ConfigFromJson(const std::string& json);

private:
    Engine() = default;
    void BotLoop();

    void TickAttack();
    void TickBuff();
    void TickPotion();
    void TickLoot();
    void TickDeath();

    bool FindAndSelectTarget();
    bool IsTargetValid();

    std::atomic<bool>     running_{false};
    std::atomic<BotState> state_{BotState::Idle};
    std::thread           thread_;
    BotConfig             config_;
    std::mutex            configMutex_;

    uint32_t currentTargetId_ = 0;
    float    lastAttackTime_ = 0.0f;
    float    lastBuffTime_ = 0.0f;
    size_t   skillRotationIdx_ = 0;
};

} // namespace bot
