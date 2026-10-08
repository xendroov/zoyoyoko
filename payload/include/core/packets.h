#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <mutex>

namespace core {

// KO packet opcode'lari
namespace Opcode {
    constexpr uint8_t WIZ_LOGIN             = 0x01;
    constexpr uint8_t WIZ_NEW_CHAR          = 0x02;
    constexpr uint8_t WIZ_SEL_CHAR          = 0x04;
    constexpr uint8_t WIZ_GAMESTART         = 0x05;
    constexpr uint8_t WIZ_MOVE              = 0x06;
    constexpr uint8_t WIZ_ROTATE            = 0x07;
    constexpr uint8_t WIZ_ATTACK            = 0x08;
    constexpr uint8_t WIZ_CHAT              = 0x0A;
    constexpr uint8_t WIZ_REGENE            = 0x0C;
    constexpr uint8_t WIZ_DEAD              = 0x0D;
    constexpr uint8_t WIZ_HP_CHANGE         = 0x13;
    constexpr uint8_t WIZ_MAGIC_PROCESS     = 0x14;
    constexpr uint8_t WIZ_MAGIC_FLYING      = 0x15;
    constexpr uint8_t WIZ_MAGIC_EFFECT      = 0x16;
    constexpr uint8_t WIZ_NPC_INFO          = 0x1A;
    constexpr uint8_t WIZ_NPC_MOVE          = 0x1B;
    constexpr uint8_t WIZ_ITEM_MOVE         = 0x20;
    constexpr uint8_t WIZ_ITEM_USE          = 0x21;
    constexpr uint8_t WIZ_ITEM_GET          = 0x22;
    constexpr uint8_t WIZ_ZONE_CHANGE       = 0x25;
    constexpr uint8_t WIZ_POINT_CHANGE      = 0x26;
    constexpr uint8_t WIZ_STATE_CHANGE      = 0x29;
    constexpr uint8_t WIZ_TARGET_HP         = 0x2E;
    constexpr uint8_t WIZ_TRADE             = 0x30;
    constexpr uint8_t WIZ_ITEM_DROP         = 0x33;
    constexpr uint8_t WIZ_WARP              = 0x3C;
    constexpr uint8_t WIZ_PARTY             = 0x42;
    constexpr uint8_t WIZ_SKILL_USE         = 0x4D;
    constexpr uint8_t WIZ_OBJECT_EVENT      = 0x57;
    constexpr uint8_t WIZ_SELECT_TARGET     = 0x5E;
    constexpr uint8_t WIZ_NPC_REGION        = 0x63;
    constexpr uint8_t WIZ_GOLD_CHANGE       = 0x64;
}

// Packet builder
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

// Packet hook callback tipleri
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

    // Hook'lardan cagrilan fonksiyonlar
    void OnSend(uint8_t* data, size_t size);
    void OnRecv(uint8_t* data, size_t size);

private:
    PacketHook() = default;
    std::vector<PacketCallback> callbacks_;
    std::mutex mutex_;
};

} // namespace core
