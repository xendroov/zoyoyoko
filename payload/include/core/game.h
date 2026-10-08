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
    char     name[16];
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

// Pattern'ler - analizden elde edilen gercek degerler
namespace Patterns {
    // CAPISocket::Send - 0x00704070 (unpacked EXE)
    // push ebp / mov ebp,esp / push -1 / push SEH_handler
    constexpr const char* SEND_FNC      = "55 8B EC 6A FF 68 ?? ?? ?? 00 64 A1 00 00 00 00";

    // CPlayer base pointer (sunucuya gore degisebilir)
    constexpr const char* CHAR_BASE     = "8B 0D ?? ?? ?? ?? 85 C9 74 ?? 8B 01 FF 50";

    // Recv hook noktasi
    constexpr const char* RECV_HOOK     = "55 8B EC 83 EC ?? 56 8B F1 8D 4D";

    // Target select fonksiyonu
    constexpr const char* TARGET_SELECT = "55 8B EC 8B 45 ?? 56 8B F1 89 86";

    // NPC dusman kontrolu
    constexpr const char* NPC_IS_ENEMY  = "55 8B EC 56 8B F1 8B 46 ?? 83 F8";
}

// Bilinen sabit adresler (2626 sunucu, Themida unpack sonrasi)
namespace Addresses {
    constexpr uintptr_t SND_FNC         = 0x00704070; // CAPISocket::Send
    constexpr uintptr_t ENCRYPT_FLAG    = 0x11159B4;  // 0=plaintext, !=0 encrypted
    constexpr uintptr_t PACKET_SEQ      = 0x1111FCC;  // sequence counter (1-250)
    constexpr uintptr_t PACK_LOG_FLAG   = 0x1111FC5;  // debug log flag
    constexpr uintptr_t DYNAMIC_KEY     = 0x11166D0;  // AES key (sunucudan)
    constexpr uintptr_t DYNAMIC_KEY_LEN = 0x11166C8;  // key length
    constexpr uintptr_t OFF_BASE_CHAR   = 0x00D15574; // CPlayer pointer (module base + offset)
}

// CPlayer struct offsetleri (2626 sunucu)
namespace CPlayerOff {
    constexpr uint32_t NAME     = 0x6A4;  // char[16]
    constexpr uint32_t LEVEL    = 0x6D0;
    constexpr uint32_t HP_MAX   = 0x6D4;
    constexpr uint32_t HP       = 0x6D8;
    constexpr uint32_t POS_Z    = 0x194;
    constexpr uint32_t POS_X    = 0x3CC;
    constexpr uint32_t POS_Y    = 0x3D4;
    constexpr uint32_t MP_MAX   = 0xBEC;
    constexpr uint32_t MP       = 0xBF0;
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
