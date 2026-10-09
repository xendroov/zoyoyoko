#include "gui/bridge.h"
#include "gui/overlay.h"
#include "bot/engine.h"
#include "core/game.h"
#include <sstream>

namespace gui {

Bridge& Bridge::Get() {
    static Bridge instance;
    return instance;
}

void Bridge::Initialize() {
    Overlay::Get().SetMessageHandler([this](const std::string& json) {
        HandleMessage(json);
    });
}

static std::string ExtractString(const std::string& json, const std::string& key) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    auto start = json.find('"', pos + 1);
    if (start == std::string::npos) return "";
    auto end = json.find('"', start + 1);
    if (end == std::string::npos) return "";
    return json.substr(start + 1, end - start - 1);
}

void Bridge::HandleMessage(const std::string& json) {
    std::string op = ExtractString(json, "op");

    if (op.substr(0, 4) == "bot.")
        HandleBotCommand(op.substr(4));
    else if (op == "config.get")
        HandleConfigGet(ExtractString(json, "reason"));
    else if (op == "config.save")
        HandleConfigSave(json);
    else if (op == "config.set")
        HandleConfigSet(ExtractString(json, "path"), ExtractString(json, "value"));
    else if (op == "mobs.list")
        HandleMobsList();
    else if (op.substr(0, 7) == "window.")
        HandleWindowCommand(op.substr(7));
    else if (op == "subscribe") {
        std::string channel = ExtractString(json, "channel");
        if (channel == "playerState") SendPlayerState();
        else if (channel == "system") SendSystemState();
    }
}

void Bridge::HandleBotCommand(const std::string& action) {
    auto& engine = bot::Engine::Get();
    if (action == "start") {
        engine.Config().enabled = true;
        engine.Config().attack.enabled = true;
        engine.Start();
        SendLog("[BOT] Bot baslatildi");
    } else if (action == "stop") {
        engine.Stop();
        SendLog("[BOT] Bot durduruldu");
    }
    SendSystemState();
}

void Bridge::HandleConfigGet(const std::string& reason) {
    SendConfig();
}

void Bridge::HandleConfigSave(const std::string& configJson) {
    auto& engine = bot::Engine::Get();
    engine.ConfigFromJson(configJson);
    SendLog("[CFG] Config kaydedildi");
}

void Bridge::HandleConfigSet(const std::string& path, const std::string& value) {
    auto& engine = bot::Engine::Get();
    auto& cfg = engine.Config();

    if (path == "attack.enabled")       cfg.attack.enabled = (value == "true");
    else if (path == "attack.range")    cfg.attack.range = std::stof(value);
    else if (path == "buff.enabled")    cfg.buff.enabled = (value == "true");
    else if (path == "potion.hpEnabled") cfg.potion.hpEnabled = (value == "true");
    else if (path == "potion.hpThreshold") cfg.potion.hpThreshold = std::stof(value);
    else if (path == "potion.spEnabled") cfg.potion.spEnabled = (value == "true");
    else if (path == "potion.spThreshold") cfg.potion.spThreshold = std::stof(value);
    else if (path == "loot.autoLoot")   cfg.loot.autoLoot = (value == "true");
    else if (path == "target.radius")   cfg.target.radius = std::stof(value);

    SendConfig();
}

void Bridge::HandleMobsList() {
    SendMobList();
}

void Bridge::HandleWindowCommand(const std::string& action) {
    auto& overlay = Overlay::Get();
    if (action == "minimize") {
        ShowWindow(overlay.GetWindow(), SW_MINIMIZE);
    } else if (action == "close") {
        overlay.Shutdown();
    }
}

void Bridge::SendPlayerState() {
    auto& game = core::Game::Get();
    auto player = game.GetPlayer();

    std::ostringstream ss;
    ss << "{\"type\":\"event\",\"channel\":\"playerState\",\"data\":{";
    ss << "\"vid\":" << player.vid << ",";
    ss << "\"name\":\"" << player.name << "\",";
    ss << "\"level\":" << static_cast<int>(player.level) << ",";
    ss << "\"race\":" << static_cast<int>(player.race) << ",";
    ss << "\"curHp\":" << player.curHp << ",";
    ss << "\"maxHp\":" << player.maxHp << ",";
    ss << "\"curSp\":" << player.curSp << ",";
    ss << "\"maxSp\":" << player.maxSp << ",";
    ss << "\"exp\":" << player.exp << ",";
    ss << "\"gold\":" << player.gold << ",";
    ss << "\"posX\":" << player.posX << ",";
    ss << "\"posY\":" << player.posY << ",";
    ss << "\"isDead\":" << (player.isDead ? "true" : "false");
    ss << "}}";

    Overlay::Get().PostMessage(ss.str());
}

void Bridge::SendSystemState() {
    auto& engine = bot::Engine::Get();
    auto& game = core::Game::Get();

    std::ostringstream ss;
    ss << "{\"type\":\"event\",\"channel\":\"system\",\"data\":{";
    ss << "\"hookOk\":" << (game.IsInitialized() ? "true" : "false") << ",";
    ss << "\"isRunning\":" << (engine.IsRunning() ? "true" : "false") << ",";
    ss << "\"state\":\"" << engine.GetStateString() << "\"";
    ss << "}}";

    Overlay::Get().PostMessage(ss.str());
}

void Bridge::SendMobList() {
    auto& game = core::Game::Get();
    auto mobs = game.GetNearbyMobs();

    std::ostringstream ss;
    ss << "{\"type\":\"event\",\"channel\":\"mobsList\",\"data\":[";
    for (size_t i = 0; i < mobs.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "{\"vid\":" << mobs[i].vid << ",";
        ss << "\"name\":\"" << mobs[i].name << "\",";
        ss << "\"hp\":" << mobs[i].curHp << ",";
        ss << "\"maxHp\":" << mobs[i].maxHp << ",";
        ss << "\"distance\":" << mobs[i].distance << "}";
    }
    ss << "]}";

    Overlay::Get().PostMessage(ss.str());
}

void Bridge::SendSkillList() {
    std::ostringstream ss;
    ss << "{\"type\":\"event\",\"channel\":\"skillsList\",\"data\":[]}";
    Overlay::Get().PostMessage(ss.str());
}

void Bridge::SendBuffList() {
    SendSkillList();
}

void Bridge::SendConfig() {
    auto& engine = bot::Engine::Get();
    std::string configJson = engine.ConfigToJson();

    std::ostringstream ss;
    ss << "{\"type\":\"event\",\"channel\":\"config\",\"data\":" << configJson << "}";
    Overlay::Get().PostMessage(ss.str());
}

void Bridge::SendLog(const std::string& msg) {
    std::ostringstream ss;
    ss << "{\"type\":\"event\",\"channel\":\"log\",\"data\":{\"msg\":\"" << msg << "\"}}";
    Overlay::Get().PostMessage(ss.str());
}

} // namespace gui
