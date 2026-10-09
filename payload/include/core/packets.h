#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <mutex>

namespace core {

// Metin2 packet opcode'lari (Client -> Server)
namespace Opcode {
    constexpr uint8_t CG_PONG           = 1;
    constexpr uint8_t CG_ATTACK         = 2;
    constexpr uint8_t CG_CHAT           = 3;
    constexpr uint8_t CG_WHISPER        = 4;
    constexpr uint8_t CG_ITEM_USE       = 10;
    constexpr uint8_t CG_ITEM_MOVE      = 11;
    constexpr uint8_t CG_ITEM_PICKUP    = 15;
    constexpr uint8_t CG_QUICK_SLOT_ADD = 16;
    constexpr uint8_t CG_QUICK_SLOT_DEL = 17;
    constexpr uint8_t CG_MOVE           = 18;
    constexpr uint8_t CG_SYNC_POSITION  = 19;
    constexpr uint8_t CG_ITEM_DROP      = 20;
    constexpr uint8_t CG_ON_CLICK       = 26;
    constexpr uint8_t CG_EXCHANGE       = 27;
    constexpr uint8_t CG_SHOP           = 50;
    constexpr uint8_t CG_USE_SKILL      = 61;
    constexpr uint8_t CG_TARGET         = 65;
    constexpr uint8_t CG_WARP           = 70;
    constexpr uint8_t CG_FISHING        = 89;
}

class Packet {
public:
    Packet() = default;
    explicit Packet(uint8_t opcode);
    Packet(const uint8_t* data, size_t size);

    Packet& WriteByte(uint8_t v);
    Packet& WriteWord(uint16_t v);
    Packet& WriteDword(uint32_t v);
    Packet& WriteFloat(float v);
    Packet& WriteString(const std::string& s);
    Packet& WriteBytes(const void* data, size_t size);

    uint8_t  ReadByte();
    uint16_t ReadWord();
    uint32_t ReadDword();
    float    ReadFloat();
    std::string ReadString(size_t len);
    void     Skip(size_t bytes);

    uint8_t  GetOpcode() const;
    const uint8_t* Data() const { return data_.data(); }
    size_t   Size() const { return data_.size(); }
    void     ResetRead() { readPos_ = 0; }

    std::string ToHexString() const;

private:
    std::vector<uint8_t> data_;
    size_t readPos_ = 0;
};

enum class PacketDirection { Send, Recv };

struct PacketEvent {
    PacketDirection direction;
    Packet          packet;
    bool            blocked;
};

using PacketCallback = std::function<void(PacketEvent&)>;

class PacketHook {
public:
    static PacketHook& Get();

    bool InstallSendHook(uintptr_t sendFnc);
    bool InstallRecvHook(uintptr_t recvFnc);
    void Remove();

    void RegisterCallback(PacketCallback cb);

    void OnSend(uint8_t* data, size_t size);
    void OnRecv(uint8_t* data, size_t size);

private:
    PacketHook() = default;
    std::vector<PacketCallback> callbacks_;
    std::mutex mutex_;
};

} // namespace core
