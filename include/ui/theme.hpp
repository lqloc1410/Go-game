#pragma once
#include <array>
#include <functional>
#include <string>
#include <vector>
#include <filesystem>
#include <SFML/Graphics.hpp>

enum class themeID {
    zen = 0,
    sumie = 1,
    dark = 2
};
constexpr int kThemeCount = 3;

struct theme {
    themeID id;
    std::string name;
    std::string displayName;

    sf::Color windowBg, hudPanel, boardTint, grid, textPrimary, accent, lastMove;

    sf::Color blackFill, blackShade, blackHighlight, blackRim;
    sf::Color whiteFill, whiteShade, whiteHighlight, whiteRim;
    float rimWidth;
    float shadowAlpha;

    float gridThickness;
    bool inkLines;
    int boardTexAlpha;
    int menuDimAlpha;

    std::string menuBg, boardTex, bgm, sfxStone, sfxCapture, sfxError; 
};

sf::Color hexColor(unsigned rgb, unsigned alpha = 255);
sf::Color mixColor(sf::Color a, sf::Color b, float t);
sf::Color withAlpha(sf::Color c, int alpha);


class themeManager {
public:
    static themeManager& instance();
    const theme& current() const {
        return themes_[static_cast<int>(id_)];
    }
    themeID currentId() const { return id_; }
    const theme& get(themeID id) const {
        return themes_[static_cast<int>(id)];
    }
    unsigned version() const {
        return version_;
    }

    void set(themeID id);
    void next() {
        set(static_cast<themeID>((static_cast<int>(id_) + 1) % kThemeCount));
    }

    const sf::Texture* menuBg();
    const sf::Texture* boardTex();

    void onChange(std::function<void(const theme&)> cb) {
        listeners_.push_back(std::move(cb));
    }

    void loadChoice(const std::string& path = "");
    void saveChoice(const std::string& path = "") const;
    
    const theme& stones() const {
        return themes_[static_cast<int>(stoneId_)]; }
        themeID stoneId() const { return stoneId_; }
        void setStoneStype(themeID id) { stoneId_ = id; }
    
private:
    themeManager();
    struct Resources {
        sf::Texture menuBg, board;
        bool menuOk = false, boardOk = false, loaded = false;
    };
    void ensureLoaded(int index);
    std::array<theme, kThemeCount> themes_;
    std::array<Resources, kThemeCount> res_;
    std::vector<std::function<void(const theme&)>> listeners_;
    themeID id_ = themeID::zen;
    unsigned version_ = 1;
    themeID stoneId_ = themeID::zen;

    static std::filesystem::path defaultThemeConfigPath() {
    #ifdef GO_PROJECT_ROOT
        std::filesystem::path root(GO_PROJECT_ROOT);
        if (std::filesystem::exists(root)) {
            return root / "data" / "theme.cfg"; // or "themes.cfg"
        }
    #endif
        return std::filesystem::path("data") / "theme.cfg";
    }
};