#include "core/packets.h"
#include <sstream>
#include <iomanip>
#include <cstring>

namespace core {

// --- Packet ---

Packet::Packet(uint8_t opcode) {
    data_.push_back(opcode);
}

Packet::Packet(const uint8_t* data, size_t size)
    : data_(data, data + size) {}

Packet& Packet::WriteByte(uint8_t v) {
    data_.push_back(v);
    return *this;
}

Packet& Packet::WriteWord(uint16_t v) {
    data_.push_back(static_cast<uint8_t>(v & 0xFF));
    data_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    return *this;
}

Packet& Packet::WriteDword(uint32_t v) {
    data_.push_back(static_cast<uint8_t>(v & 0xFF));
    data_.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    data_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    data_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    return *this;
}

Packet& Packet::WriteFloat(float v) {
    uint32_t raw;
    memcpy(&raw, &v, sizeof(float));
    return WriteDword(raw);
}

Packet& Packet::WriteString(const std::string& s) {
    WriteWord(static_cast<uint16_t>(s.size()));
    data_.insert(data_.end(), s.begin(), s.end());
    return *this;
}

Packet& Packet::WriteBytes(const void* data, size_t size) {
    auto* bytes = static_cast<const uint8_t*>(data);
    data_.insert(data_.end(), bytes, bytes + size);
    return *this;
}

uint8_t Packet::ReadByte() {
    if (readPos_ >= data_.size()) return 0;
    return data_[readPos_++];
}

uint16_t Packet::ReadWord() {
    if (readPos_ + 1 >= data_.size()) return 0;
    uint16_t v = data_[readPos_] | (data_[readPos_ + 1] << 8);
    readPos_ += 2;
    return v;
}

uint32_t Packet::ReadDword() {
    if (readPos_ + 3 >= data_.size()) return 0;
    uint32_t v = data_[readPos_]
        | (data_[readPos_ + 1] << 8)
        | (data_[readPos_ + 2] << 16)
        | (data_[readPos_ + 3] << 24);
    readPos_ += 4;
    return v;
}

float Packet::ReadFloat() {
    uint32_t raw = ReadDword();
    float v;
    memcpy(&v, &raw, sizeof(float));
    return v;
}

std::string Packet::ReadString(size_t len) {
    if (readPos_ + len > data_.size()) return "";
    std::string s(reinterpret_cast<const char*>(&data_[readPos_]), len);
    readPos_ += len;
    return s;
}

void Packet::Skip(size_t bytes) {
    readPos_ += bytes;
    if (readPos_ > data_.size()) readPos_ = data_.size();
}

uint8_t Packet::GetOpcode() const {
    return data_.empty() ? 0 : data_[0];
}

std::string Packet::ToHexString() const {
    std::ostringstream ss;
    for (size_t i = 0; i < data_.size(); ++i) {
        if (i > 0) ss << ' ';
        ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
           << static_cast<int>(data_[i]);
    }
    return ss.str();
}

// --- PacketHook ---

PacketHook& PacketHook::Get() {
    static PacketHook instance;
    return instance;
}

void PacketHook::RegisterCallback(PacketCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(std::move(cb));
}

void PacketHook::OnSend(uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    PacketEvent event{PacketDirection::Send, Packet(data, size), false};
    for (auto& cb : callbacks_)
        cb(event);
}

void PacketHook::OnRecv(uint8_t* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    PacketEvent event{PacketDirection::Recv, Packet(data, size), false};
    for (auto& cb : callbacks_)
        cb(event);
}

bool PacketHook::InstallSendHook(uintptr_t sendFnc) {
    // Hook kurulumu game.cpp veya dllmain.cpp'den yapilacak
    // Burada sadece adres kaydediliyor, gercek hook HookManager uzerinden
    return sendFnc != 0;
}

bool PacketHook::InstallRecvHook(uintptr_t recvFnc) {
    return recvFnc != 0;
}

void PacketHook::Remove() {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.clear();
}

} // namespace core
