#pragma once
#include <cstdint>
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <random>

namespace bot {

struct BotConfig {
    bool enabled = false;

    struct {
        bool   enabled = false;
        bool   targetLock = true;
        float  range = 5000.0f;
        float  attackSpeed = 1.0f;
        std::vector<uint32_t> selectedMobs;
        std::vector<uint32_t> skillRotation;
    } attack;

    struct {
        bool enabled = false;
        std::vector<uint32_t> skills;
        float checkInterval = 30.0f;
    } buff;

    struct {
        bool  hpEnabled = false;
        float hpThreshold = 60.0f;
        uint16_t hpItemCell = 0;

        bool  spEnabled = false;
        float spThreshold = 30.0f;
        uint16_t spItemCell = 0;
    } potion;

    struct {
        bool autoLoot = false;
        float lootRadius = 500.0f;
    } loot;

    struct {
        float   radius = 5000.0f;
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
    DWORD HumanizedDelay(DWORD baseMs, DWORD sigmaMs);

    std::atomic<bool>     running_{false};
    std::atomic<BotState> state_{BotState::Idle};
    std::thread           thread_;
    BotConfig             config_;
    std::mutex            configMutex_;

    uint32_t currentTargetVid_ = 0;
    float    lastAttackTime_ = 0.0f;
    float    lastBuffTime_ = 0.0f;
    size_t   skillRotationIdx_ = 0;

    std::mt19937 rng_{std::random_device{}()};
    std::normal_distribution<double> dist_{0.0, 1.0};
};

} // namespace bot
