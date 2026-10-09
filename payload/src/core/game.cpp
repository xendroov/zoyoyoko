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

    // CPythonPlayer instance
    if (Addresses::PLAYER_INST != 0) {
        pointers_.ptrPlayer = Addresses::PLAYER_INST;
    } else {
        uintptr_t addr = Memory::PatternScan(moduleName, Patterns::PLAYER_INST);
        if (addr) {
            pointers_.ptrPlayer = Memory::Read<uintptr_t>(addr + 1);
        }
    }

    // CPythonNetworkStream instance
    if (Addresses::NETWORK_INST != 0) {
        pointers_.ptrNetwork = Addresses::NETWORK_INST;
    }

    // CPythonCharacterManager instance
    if (Addresses::CHAR_MGR_INST != 0) {
        pointers_.ptrCharMgr = Addresses::CHAR_MGR_INST;
    } else {
        uintptr_t addr = Memory::PatternScan(moduleName, Patterns::CHAR_MGR);
        if (addr) {
            pointers_.ptrCharMgr = Memory::Read<uintptr_t>(addr + 1);
        }
    }

    // Send fonksiyonu
    uintptr_t addr = Memory::PatternScan(moduleName, Patterns::SEND_FNC);
    if (addr) {
        pointers_.fncSend = addr;
    }

    pointers_.resolved = (pointers_.ptrPlayer != 0);
    return pointers_.resolved;
}

PlayerInfo Game::GetPlayer() {
    PlayerInfo info{};
    if (!pointers_.ptrPlayer) return info;

    uintptr_t pBase = Memory::Read<uintptr_t>(pointers_.ptrPlayer);
    if (!pBase) return info;

    // CE ile dogrulanan offsetler (PlayerOff namespace'inden)
    if (PlayerOff::NAME != 0)
        Memory::ReadBytes(pBase + PlayerOff::NAME, info.name, 24);
    if (PlayerOff::LEVEL != 0)
        info.level = Memory::Read<uint8_t>(pBase + PlayerOff::LEVEL);
    if (PlayerOff::HP != 0)
        info.curHp = Memory::Read<int32_t>(pBase + PlayerOff::HP);
    if (PlayerOff::HP_MAX != 0)
        info.maxHp = Memory::Read<int32_t>(pBase + PlayerOff::HP_MAX);
    if (PlayerOff::SP != 0)
        info.curSp = Memory::Read<int32_t>(pBase + PlayerOff::SP);
    if (PlayerOff::SP_MAX != 0)
        info.maxSp = Memory::Read<int32_t>(pBase + PlayerOff::SP_MAX);
    if (PlayerOff::EXP != 0)
        info.exp = Memory::Read<uint32_t>(pBase + PlayerOff::EXP);
    if (PlayerOff::GOLD != 0)
        info.gold = Memory::Read<uint32_t>(pBase + PlayerOff::GOLD);
    if (PlayerOff::POS_X != 0)
        info.posX = Memory::Read<float>(pBase + PlayerOff::POS_X);
    if (PlayerOff::POS_Y != 0)
        info.posY = Memory::Read<float>(pBase + PlayerOff::POS_Y);
    if (PlayerOff::VID != 0)
        info.vid = Memory::Read<uint32_t>(pBase + PlayerOff::VID);

    info.isDead = (info.curHp <= 0);
    return info;
}

bool Game::IsPlayerDead() {
    return GetPlayer().isDead;
}

bool Game::IsPlayerInGame() {
    if (!pointers_.ptrPlayer) return false;
    uintptr_t pBase = Memory::Read<uintptr_t>(pointers_.ptrPlayer);
    return pBase != 0;
}

std::vector<MobInfo> Game::GetNearbyMobs(float radius) {
    std::vector<MobInfo> mobs;
    // CPythonCharacterManager uzerinden entity iterasyonu
    // CE ile CharMgr offsetleri bulundugunda implement edilecek
    return mobs;
}

MobInfo Game::GetTargetMob() {
    MobInfo info{};
    return info;
}

bool Game::SelectTarget(uint32_t vid) {
    Packet pkt(Opcode::CG_TARGET);
    pkt.WriteDword(vid);
    return SendPacket(pkt.Data(), pkt.Size());
}

bool Game::Attack() {
    Packet pkt(Opcode::CG_ATTACK);
    pkt.WriteByte(0); // attack type: normal
    return SendPacket(pkt.Data(), pkt.Size());
}

bool Game::UseSkill(uint32_t skillSlot) {
    Packet pkt(Opcode::CG_USE_SKILL);
    pkt.WriteDword(skillSlot);
    return SendPacket(pkt.Data(), pkt.Size());
}

bool Game::UseItem(uint16_t cell) {
    Packet pkt(Opcode::CG_ITEM_USE);
    pkt.WriteWord(cell);
    return SendPacket(pkt.Data(), pkt.Size());
}

bool Game::PickupItem() {
    Packet pkt(Opcode::CG_ITEM_PICKUP);
    return SendPacket(pkt.Data(), pkt.Size());
}

std::vector<GroundItemInfo> Game::GetGroundItems(float radius) {
    std::vector<GroundItemInfo> items;
    return items;
}

bool Game::SendPacket(const uint8_t* data, size_t size) {
    if (!pointers_.fncSend || !data || size == 0) return false;

    // CPythonNetworkStream::Send - genelde __thiscall
    using tSend = bool(__thiscall*)(void*, const uint8_t*, int);
    auto fn = reinterpret_cast<tSend>(pointers_.fncSend);

    void* pNetwork = nullptr;
    if (pointers_.ptrNetwork) {
        pNetwork = reinterpret_cast<void*>(Memory::Read<uintptr_t>(pointers_.ptrNetwork));
    }

    if (!pNetwork) return false;
    return fn(pNetwork, data, static_cast<int>(size));
}

} // namespace core
