#include "ui/theme.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

sf::Color hexColor(unsigned rgb, unsigned alpha) {
    return sf::Color(static_cast<sf::Uint8>((rgb >> 16) & 0xFF), static_cast<sf::Uint8>((rgb >> 8) & 0xFF),
                     static_cast<sf::Uint8>(rgb & 0xFF), static_cast<sf::Uint8>(alpha));
}
sf::Color mixColor(sf::Color a, sf::Color b, float t) {
    auto m = [&](sf::Uint8 x, sf::Uint8 y) { return static_cast<sf::Uint8>(x + (y - x) * t + 0.5f); };
    return sf::Color(m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a));
}
sf::Color withAlpha(sf::Color c, int alpha) { c.a = static_cast<sf::Uint8>(alpha); return c; }

themeManager& themeManager::instance() { static themeManager inst; return inst; }

themeManager::themeManager() {
    {   // Zen
        theme& t = themes_[0];
        t.id = themeID::zen;  t.name = "zen";  t.displayName = "Traditional Zen";
        t.windowBg = hexColor(0x2B2118);  t.hudPanel = hexColor(0x3B2E22);  t.boardTint = hexColor(0xD8A25E);
        t.grid = hexColor(0x231709);  t.textPrimary = hexColor(0xF7EFE5);
        t.accent = hexColor(0xffe696);  t.lastMove = hexColor(0xE04F33);
        t.blackFill = hexColor(0x2A2724);  t.blackShade = hexColor(0x100E0C);              
        t.blackHighlight = hexColor(0xFFF2DC, 45);  t.blackRim = hexColor(0x0F0D0B);
        t.whiteFill = hexColor(0xF3EBDC);  t.whiteShade = hexColor(0xCFC3AA);             
        t.whiteHighlight = hexColor(0xFFFFFF, 220);  t.whiteRim = hexColor(0xB5A88F);
        t.rimWidth = 1.f;  t.shadowAlpha = 75.f;
        t.gridThickness = 0.045f;  t.inkLines = false;  t.boardTexAlpha = 200;  t.menuDimAlpha = 75;
    }
    {   // Sumi-e
        theme& t = themes_[1];
        t.id = themeID::sumie;  t.name = "sumie";  t.displayName = "Sumi-e Ink Wash";
        t.windowBg = hexColor(0xEAE3D2);  t.hudPanel = hexColor(0xDFD6C1);  t.boardTint = hexColor(0xF4EFE6);
        t.grid = hexColor(0x2B2B2B);  t.textPrimary = hexColor(0x1C1C1C);
        t.accent = hexColor(0x630000);  t.lastMove = hexColor(0xB8292F);
        t.blackFill = hexColor(0x151515);  t.blackShade = hexColor(0x050505);              
        t.blackHighlight = hexColor(0xFFFFFF, 25);  t.blackRim = hexColor(0x000000);
        t.whiteFill = hexColor(0xF7F1E3);  t.whiteShade = hexColor(0xDCD2BC);           
        t.whiteHighlight = hexColor(0xFFFFFF, 110);  t.whiteRim = hexColor(0x2B2B2B);
        t.rimWidth = 1.6f;  t.shadowAlpha = 45.f;
        t.gridThickness = 0.040f;  t.inkLines = true;  t.boardTexAlpha = 150;  t.menuDimAlpha = 0;
    }
    {   // Dark mode
        theme& t = themes_[2];
        t.id = themeID::dark;  t.name = "dark";  t.displayName = "Dark Mode";
        t.windowBg = hexColor(0x121215);  t.hudPanel = hexColor(0x1E1E24);  t.boardTint = hexColor(0x24242B);
        t.grid = hexColor(0x001d2e);  t.textPrimary = hexColor(0xF4F4F6);
        t.accent = hexColor(0x015382);  t.lastMove = hexColor(0x38BDF8);
        t.blackFill = hexColor(0x0A0A0C);  t.blackShade = hexColor(0x000000);              // black with a visible rim
        t.blackHighlight = hexColor(0xFFFFFF, 30);  t.blackRim = hexColor(0x5A5A68);
        t.whiteFill = hexColor(0xE4E4E7);  t.whiteShade = hexColor(0xB4B4BC);              // soft off-white
        t.whiteHighlight = hexColor(0xFFFFFF, 110);  t.whiteRim = hexColor(0xA1A1AA);
        t.rimWidth = 1.5f;  t.shadowAlpha = 90.f;
        t.gridThickness = 0.035f;  t.inkLines = false;  t.boardTexAlpha = 190;  t.menuDimAlpha = 40;
    }
    for (theme& t : themes_) {                                   // asset paths follow one naming rule
        const std::string d = "assets/themes/" + t.name + "/";
        t.menuBg = d + "bg.jpg";  t.boardTex = d + "board.jpg";  t.bgm = d + "bgm.ogg";
        t.sfxStone = d + "stone.wav"; t.sfxCapture = d + "capture.wav"; t.sfxError = d + "error.wav";
    }
}

void themeManager::ensureLoaded(int i) {
    Resources& r = res_[i];
    if (r.loaded) return;                                        // images are loaded once, on first use
    r.loaded = true;
    const theme& t = themes_[i];
    r.menuOk  = std::filesystem::exists(t.menuBg)  && r.menuBg.loadFromFile(t.menuBg);
    r.boardOk = std::filesystem::exists(t.boardTex) && r.board.loadFromFile(t.boardTex);
    if (r.menuOk)  r.menuBg.setSmooth(true);
    if (r.boardOk) r.board.setSmooth(true);
    if (!r.menuOk)  std::cerr << "[theme " << t.name << "] no menu image, using a plain color\n";
    if (!r.boardOk) std::cerr << "[theme " << t.name << "] no board image, using the tint only\n";
}

const sf::Texture* themeManager::menuBg() {
    int i = static_cast<int>(id_); ensureLoaded(i);
    return res_[i].menuOk ? &res_[i].menuBg : nullptr;
}
const sf::Texture* themeManager::boardTex() {
    int i = static_cast<int>(id_); ensureLoaded(i);
    return res_[i].boardOk ? &res_[i].board : nullptr;
}

void themeManager::set(themeID id) {
    if (id == id_) return;
    id_ = id;
    ensureLoaded(static_cast<int>(id_));
    ++version_;                                                  // every cache compares this number
    for (auto& cb : listeners_) cb(current());
}

void themeManager::loadChoice(const std::string& path) {
    std::filesystem::path p = path.empty() ? defaultThemeConfigPath() : std::filesystem::path(path);
    std::ifstream in(p);
    if (!in) {
        saveChoice(p.string()); // creates data/ and writes default theme (zen / zen)
        return;
    }
    std::string boardName, stoneName;
    if (!(in >> boardName)) return;
    if (!(in >> stoneName)) stoneName = boardName;

    auto find = [&](const std::string& n, themeID& out) {
        for (const theme& t : themes_)
            if (t.name == n) {
                out = t.id;
                return true;
            }
        return false;
    };
    themeID id = id_;
    if (find(boardName, id)) set(id);
    id = id_;
    if (find(stoneName, id)) stoneId_ = id;
}

void themeManager::saveChoice(const std::string& path) const {
    std::filesystem::path p = path.empty() ? defaultThemeConfigPath() : std::filesystem::path(path);
    std::error_code ec;
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
    }
    std::ofstream out(p);
    if (!out) return;
    out << current().name << "\n" << stones().name << "\n";
}