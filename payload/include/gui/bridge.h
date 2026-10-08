#pragma once
#include <string>

namespace gui {

// JS <-> C++ mesaj koprusu
// JS'den gelen komutlari isler, C++'dan JS'ye durum gonderir
class Bridge {
public:
    static Bridge& Get();

    void Initialize();

    // JS'den gelen mesaji isle
    void HandleMessage(const std::string& json);

    // Durum guncellemelerini JS'ye gonder
    void SendPlayerState();
    void SendSystemState();
    void SendMobList();
    void SendSkillList();
    void SendBuffList();
    void SendConfig();
    void SendLog(const std::string& msg);

private:
    Bridge() = default;

    void HandleBotCommand(const std::string& action);
    void HandleConfigGet(const std::string& reason);
    void HandleConfigSave(const std::string& configJson);
    void HandleConfigSet(const std::string& path, const std::string& value);
    void HandleMobsList();
    void HandleSkillsList();
    void HandleWindowCommand(const std::string& action);
};

} // namespace gui
