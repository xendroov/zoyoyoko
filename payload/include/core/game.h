#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace core {

namespace Race {
    constexpr uint8_t WARRIOR_M  = 0;
    constexpr uint8_t ASSASSIN_F = 1;
    constexpr uint8_t SURA_M     = 2;
    constexpr uint8_t SHAMAN_F   = 3;
    constexpr uint8_t WARRIOR_F  = 4;
    constexpr uint8_t ASSASSIN_M = 5;
    constexpr uint8_t SURA_F     = 6;
    constexpr uint8_t SHAMAN_M   = 7;
}

namespace EntityType {
    constexpr uint8_t PC       = 0;
    constexpr uint8_t NPC      = 1;
    constexpr uint8_t MONSTER  = 2;
    constexpr uint8_t STONE    = 3;
    constexpr uint8_t WARP     = 4;
    constexpr uint8_t DOOR     = 5;
    constexpr uint8_t BUILDING = 6;
}

struct PlayerInfo {
    uint32_t vid;
    char     name[24];
    uint8_t  race;
    uint8_t  level;
    int32_t  curHp;
    int32_t  maxHp;
    int32_t  curSp;
    int32_t  maxSp;
    uint32_t exp;
    uint32_t gold;
    float    posX;
    float    posY;
    float    posZ;
    float    rotation;
    bool     isDead;
};

struct MobInfo {
    uint32_t vid;
    uint32_t vnum;
    char     name[24];
    int32_t  curHp;
    int32_t  maxHp;
    float    posX;
    float    posY;
    float    posZ;
    float    distance;
    bool     isDead;
    uint8_t  type;
};

struct GroundItemInfo {
    uint32_t vid;
    uint32_t vnum;
    float    posX;
    float    posY;
    float    posZ;
    float    distance;
};

struct ItemInfo {
    uint32_t vnum;
    uint16_t count;
    uint8_t  window;
    uint16_t cell;
};

struct SkillInfo {
    uint32_t index;
    char     name[32];
    uint8_t  level;
    float    cooldown;
    float    lastUsed;
};

struct GamePointers {
    uintptr_t ptrPlayer;
    uintptr_t ptrNetwork;
    uintptr_t ptrCharMgr;
    uintptr_t ptrItemMgr;
    uintptr_t fncSend;
    bool      resolved;
};

// Pattern'ler (official client versiyonuna gore degisir)
namespace Patterns {
    constexpr const char* SEND_FNC    = "55 8B EC 83 EC ?? 56 8B F1 8B 46";
    constexpr const char* PLAYER_INST = "B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 74 ?? 8B 48";
    constexpr const char* CHAR_MGR    = "B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B ?? 85 ?? 74 ?? 8B 40";
}

// CE ile bulunacak adresler (official client)
// 0 = henuz bulunamadi, CE ile doldurulacak
namespace Addresses {
    constexpr uintptr_t PLAYER_INST   = 0x00;
    constexpr uintptr_t NETWORK_INST  = 0x00;
    constexpr uintptr_t CHAR_MGR_INST = 0x00;
    constexpr uintptr_t ITEM_MGR_INST = 0x00;
}

// CPythonPlayer offsetleri (CE ile dogrulanacak)
// Kaynak koddan bilinen yapilar, degerler versiyona gore degisir
namespace PlayerOff {
    constexpr uint32_t VID      = 0x00;
    constexpr uint32_t NAME     = 0x00;
    constexpr uint32_t RACE     = 0x00;
    constexpr uint32_t LEVEL    = 0x00;
    constexpr uint32_t HP       = 0x00;
    constexpr uint32_t HP_MAX   = 0x00;
    constexpr uint32_t SP       = 0x00;
    constexpr uint32_t SP_MAX   = 0x00;
    constexpr uint32_t EXP      = 0x00;
    constexpr uint32_t GOLD     = 0x00;
    constexpr uint32_t POS_X    = 0x00;
    constexpr uint32_t POS_Y    = 0x00;
}

class Game {
public:
    static Game& Get();

    bool Initialize();
    bool IsInitialized() const { return pointers_.resolved; }
    bool ResolvePointers();

    PlayerInfo GetPlayer();
    bool IsPlayerDead();
    bool IsPlayerInGame();

    std::vector<MobInfo> GetNearbyMobs(float radius = 5000.0f);
    MobInfo GetTargetMob();
    bool SelectTarget(uint32_t vid);

    bool Attack();
    bool UseSkill(uint32_t skillSlot);

    bool UseItem(uint16_t cell);
    bool PickupItem();
    std::vector<GroundItemInfo> GetGroundItems(float radius = 500.0f);

    bool SendPacket(const uint8_t* data, size_t size);

    const GamePointers& GetPointers() const { return pointers_; }

private:
    Game() = default;
    GamePointers pointers_{};
};

} // namespace core
