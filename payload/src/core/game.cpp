#include "core/game.h"
#include "core/memory.h"
#include "core/packets.h"
#include <cmath>

namespace core {

Game& Game::Get() {
    static Game instance;
    return instance;
}

bool Game::Initialize() {
    return ResolvePointers();
}

bool Game::ResolvePointers() {
    pointers_ = {};

    const char* moduleName = nullptr;

    // CPlayer base pointer - bilinen adres (module base + offset)
    uintptr_t moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
    pointers_.ptrChar = moduleBase + Addresses::OFF_BASE_CHAR;

    // Fallback: pattern scan
    if (Memory::Read<uintptr_t>(pointers_.ptrChar) == 0) {
        uintptr_t addr = Memory::PatternScan(moduleName, Patterns::CHAR_BASE);
        if (addr) {
            pointers_.ptrChar = Memory::Read<uintptr_t>(addr + 2);
        }
    }

    // Send fonksiyonu - once bilinen adres dene, sonra pattern scan
    if (Memory::Read<uint8_t>(Addresses::SND_FNC) == 0x55) {
        pointers_.fncSend = Addresses::SND_FNC;
    } else {
        addr = Memory::PatternScan(moduleName, Patterns::SEND_FNC);
        if (addr) pointers_.fncSend = addr;
    }

    // Recv hook noktasi
    addr = Memory::PatternScan(moduleName, Patterns::RECV_HOOK);
    if (addr) {
        pointers_.fncRecv = addr;
    }

    // Target select fonksiyonu
    addr = Memory::PatternScan(moduleName, Patterns::TARGET_SELECT);
    if (addr) {
        pointers_.fncTargetSelect = addr;
    }

    // NPC is enemy fonksiyonu
    addr = Memory::PatternScan(moduleName, Patterns::NPC_IS_ENEMY);
    if (addr) {
        pointers_.fncNpcIsEnemy = addr;
    }

    // Minimum gerekli pointer'lar bulundu mu?
    pointers_.resolved = (pointers_.ptrChar != 0 && pointers_.fncSend != 0);
    return pointers_.resolved;
}

PlayerInfo Game::GetPlayer() {
    PlayerInfo info{};
    if (!pointers_.ptrChar) return info;

    uintptr_t pBase = Memory::Read<uintptr_t>(pointers_.ptrChar);
    if (!pBase) return info;

    // 2626 sunucu CPlayer offsetleri
    Memory::ReadBytes(pBase + CPlayerOff::NAME, info.name, 16);
    info.level    = Memory::Read<uint8_t>(pBase + CPlayerOff::LEVEL);
    info.curHp    = Memory::Read<int16_t>(pBase + CPlayerOff::HP);
    info.maxHp    = Memory::Read<int16_t>(pBase + CPlayerOff::HP_MAX);
    info.curMp    = Memory::Read<int16_t>(pBase + CPlayerOff::MP);
    info.maxMp    = Memory::Read<int16_t>(pBase + CPlayerOff::MP_MAX);
    info.posX     = Memory::Read<float>(pBase + CPlayerOff::POS_X);
    info.posY     = Memory::Read<float>(pBase + CPlayerOff::POS_Y);
    info.posZ     = Memory::Read<float>(pBase + CPlayerOff::POS_Z);
    info.isDead   = (info.curHp <= 0);

    return info;
}

bool Game::IsPlayerDead() {
    return GetPlayer().isDead;
}

bool Game::IsPlayerInGame() {
    if (!pointers_.ptrChar) return false;
    uintptr_t pBase = Memory::Read<uintptr_t>(pointers_.ptrChar);
    return pBase != 0;
}

std::vector<NpcInfo> Game::GetNearbyNpcs(float radius) {
    std::vector<NpcInfo> npcs;
    // NPC listesi okuma - sunucu versiyonuna gore implementasyon degisir
    // Genelde CPlayerNpc array'i uzerinden iterate edilir
    // TODO: Sunucu versiyonuna gore implement et
    return npcs;
}

NpcInfo Game::GetTargetNpc() {
    NpcInfo info{};
    // TODO: Secili target bilgisini oku
    return info;
}

bool Game::SelectTarget(uint32_t npcId) {
    if (!pointers_.fncTargetSelect) return false;

    // Target select fonksiyonunu cagir
    // typedef void (__thiscall* tSelectTarget)(void* pThis, uint32_t id);
    // auto fn = reinterpret_cast<tSelectTarget>(pointers_.fncTargetSelect);
    // fn(playerBase, npcId);

    // Alternatif: WIZ_SELECT_TARGET paketi gonder
    Packet pkt(Opcode::WIZ_SELECT_TARGET);
    pkt.WriteWord(static_cast<uint16_t>(npcId));
    return SendPacket(pkt.Data(), pkt.Size());
}

bool Game::UseSkill(uint32_t skillId) {
    if (!pointers_.fncSend) return false;

    Packet pkt(Opcode::WIZ_MAGIC_PROCESS);
    pkt.WriteDword(skillId);
    // Skill paketi detaylari sunucu versiyonuna gore degisir
    return SendPacket(pkt.Data(), pkt.Size());
}

std::vector<SkillInfo> Game::GetSkillList() {
    std::vector<SkillInfo> skills;
    // TODO: Karakter skill listesini bellekten oku
    return skills;
}

bool Game::UseItem(uint8_t slot) {
    Packet pkt(Opcode::WIZ_ITEM_USE);
    pkt.WriteByte(slot);
    return SendPacket(pkt.Data(), pkt.Size());
}

std::vector<ItemInfo> Game::GetInventory() {
    std::vector<ItemInfo> items;
    // TODO: Envanter okuma
    return items;
}

bool Game::SendPacket(const uint8_t* data, size_t size) {
    if (!pointers_.fncSend || !data || size == 0) return false;

    // CAPISocket::Send __thiscall convention
    // ECX = CAPISocket* this pointer (ptrPkt'den veya ptrChar yakinindaki socket pointer)
    // Parametre: const uint8_t* data, int size
    // Hook sifrelemeden ONCE yakaliyor - plaintext gonderiyoruz
    using tSend = void(__thiscall*)(void*, const uint8_t*, int);
    auto fn = reinterpret_cast<tSend>(pointers_.fncSend);

    // CAPISocket pointer'i - genelde CPlayer yakininda
    void* pSocket = nullptr;
    if (pointers_.ptrChar) {
        uintptr_t pBase = Memory::Read<uintptr_t>(pointers_.ptrChar);
        if (pBase) {
            // CAPISocket genelde global veya CPlayer icerisinde
            // Offset sunucuya gore ayarlanmali
            pSocket = reinterpret_cast<void*>(pBase);
        }
    }

    if (!pSocket) return false;
    fn(pSocket, data, static_cast<int>(size));
    return true;
}

} // namespace core
