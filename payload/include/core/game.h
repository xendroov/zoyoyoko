#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace core {

// KO oyun sabitleri
namespace Nation {
    constexpr uint8_t KARUS   = 1;
    constexpr uint8_t ELMORAD = 2;
}

namespace ClassType {
    constexpr uint8_t WARRIOR = 1;
    constexpr uint8_t ROGUE   = 2;
    constexpr uint8_t MAGE    = 3;
    constexpr uint8_t PRIEST  = 4;
}

// Oyuncu bilgileri
struct PlayerInfo {
    uint32_t id;
    char     name[32];
    uint8_t  nation;
    uint8_t  classType;
    uint8_t  level;
    int16_t  curHp;
    int16_t  maxHp;
    int16_t  curMp;
    int16_t  maxMp;
    float    posX;
    float    posY;
    float    posZ;
    uint32_t gold;
    uint8_t  zone;
    bool     isDead;
};

// NPC/Mob bilgileri
struct NpcInfo {
    uint32_t id;
    uint32_t protoId;
    char     name[32];
    int16_t  curHp;
    int16_t  maxHp;
    float    posX;
    float    posY;
    float    posZ;
    float    distance;
    bool     isEnemy;
    bool     isDead;
};

// Skill bilgileri
struct SkillInfo {
    uint32_t id;
    char     name[64];
    uint8_t  type;       // 1=attack, 2=buff, 3=heal
    uint16_t mpCost;
    float    cooldown;
    float    lastUsed;
};

// Envanter item
struct ItemInfo {
    uint32_t id;
    uint32_t protoId;
    uint16_t count;
    uint8_t  slot;
    uint8_t  durability;
};

// KO memory pointer'lari - pattern scan ile bulunacak
struct GamePointers {
    uintptr_t ptrChar;          // CPlayer base
    uintptr_t ptrPkt;           // Packet buffer
    uintptr_t fncSend;          // Send packet fonksiyonu
    uintptr_t fncRecv;          // Recv packet hook noktasi
    uintptr_t fncTargetSelect;  // Target secme fonksiyonu
    uintptr_t fncNpcIsEnemy;    // NPC dusman mi kontrolu
    uintptr_t fncMapNextItem;   // Map item iterasyonu
    uintptr_t ptrGameProcMain;  // Ana oyun proc
    bool      resolved;
};

// Pattern'ler - sunucu versiyonuna gore guncellenmeli
namespace Patterns {
    // Ornek pattern'ler (replika sunucuya gore degisir)
    constexpr const char* CHAR_BASE     = "8B 0D ?? ?? ?? ?? 85 C9 74 ?? 8B 01 FF 50";
    constexpr const char* SEND_FNC      = "55 8B EC 83 EC ?? 53 56 57 8B F1 8B 4E";
    constexpr const char* RECV_HOOK     = "55 8B EC 83 EC ?? 56 8B F1 8D 4D";
    constexpr const char* TARGET_SELECT = "55 8B EC 8B 45 ?? 56 8B F1 89 86";
    constexpr const char* NPC_IS_ENEMY  = "55 8B EC 56 8B F1 8B 46 ?? 83 F8";
}

class Game {
public:
    static Game& Get();

    bool Initialize();
    bool IsInitialized() const { return pointers_.resolved; }

    // Pointer'lari bul
    bool ResolvePointers();

    // Oyuncu bilgileri
    PlayerInfo GetPlayer();
    bool IsPlayerDead();
    bool IsPlayerInGame();

    // NPC/Mob islemleri
    std::vector<NpcInfo> GetNearbyNpcs(float radius = 50.0f);
    NpcInfo GetTargetNpc();
    bool SelectTarget(uint32_t npcId);

    // Skill kullanimi
    bool UseSkill(uint32_t skillId);
    std::vector<SkillInfo> GetSkillList();

    // Item kullanimi
    bool UseItem(uint8_t slot);
    std::vector<ItemInfo> GetInventory();

    // Packet gonderme
    bool SendPacket(const uint8_t* data, size_t size);

    const GamePointers& GetPointers() const { return pointers_; }

private:
    Game() = default;

    GamePointers pointers_{};
};

} // namespace core
