#include "core/game.h"
#include "core/memory.h"
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

    // KnightOnLine.exe uzerinde pattern scan
    const char* moduleName = nullptr; // ana modul

    // CPlayer base pointer
    uintptr_t addr = Memory::PatternScan(moduleName, Patterns::CHAR_BASE);
    if (addr) {
        // Pattern'den 2 byte sonrasi pointer adresi (8B 0D [XX XX XX XX])
        pointers_.ptrChar = Memory::Read<uintptr_t>(addr + 2);
    }

    // Send fonksiyonu
    addr = Memory::PatternScan(moduleName, Patterns::SEND_FNC);
    if (addr) {
        pointers_.fncSend = addr;
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

    // KO CPlayer offsetleri (replika sunucuya gore ayarlanmali)
    // Bunlar ornek offset'ler, gercek degerler sunucu versiyonuna bagli
    info.id       = Memory::Read<uint32_t>(pBase + 0x00);
    Memory::ReadBytes(pBase + 0x04, info.name, 32);
    info.nation   = Memory::Read<uint8_t>(pBase + 0x28);
    info.classType= Memory::Read<uint8_t>(pBase + 0x2C);
    info.level    = Memory::Read<uint8_t>(pBase + 0x30);
    info.curHp    = Memory::Read<int16_t>(pBase + 0x34);
    info.maxHp    = Memory::Read<int16_t>(pBase + 0x36);
    info.curMp    = Memory::Read<int16_t>(pBase + 0x38);
    info.maxMp    = Memory::Read<int16_t>(pBase + 0x3A);
    info.posX     = Memory::Read<float>(pBase + 0x40);
    info.posY     = Memory::Read<float>(pBase + 0x44);
    info.posZ     = Memory::Read<float>(pBase + 0x48);
    info.gold     = Memory::Read<uint32_t>(pBase + 0x50);
    info.zone     = Memory::Read<uint8_t>(pBase + 0x54);
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

    // KO send fonksiyonu genelde __thiscall convention kullanir
    // typedef bool (__thiscall* tSendPacket)(void* pThis, const uint8_t* data, size_t size);
    // Gercek cagri sunucu versiyonuna gore ayarlanmali

    // Simdilik hook uzerinden yonlendiriyoruz
    using SendFn = void(__cdecl*)(const uint8_t*, int);
    auto fn = reinterpret_cast<SendFn>(pointers_.fncSend);
    fn(data, static_cast<int>(size));
    return true;
}

} // namespace core
