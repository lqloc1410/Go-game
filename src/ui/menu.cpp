#include "ui/menu.hpp"
#include "ui/render.hpp"
#include <cmath>
#include <algorithm>
#include "ui/theme.hpp"

// AI-assisted: see AI43, AI44

namespace {
    const sf::Vector2f coeffButton(380.f, 60.f);
    sf::Vector2f slot(int i) {
        return {575.f, 380.f + 78.f * i}; 
    }

    static sf::ConvexShape roundedRect(sf::Vector2f size, float radius, int pts = 10) {
        pts = std::max(2, pts);
        radius = std::min({radius, size.x * 0.5f, size.y * 0.5f});
        sf::ConvexShape shape(4 * pts);
        const float pi = 3.14159265f;
        const sf::Vector2f centers[4] = {
            {size.x - radius, radius},
            {size.x - radius, size.y - radius},
            {radius, size.y - radius},
            {radius, radius}
        };
        const float start[4] = {-90.f, 0.f, 90.f, 180.f}; // one quarter circle per corner
        int idx = 0;
        for (int k = 0; k < 4; ++k) {
            for (int i = 0; i < pts; ++i) {
                float a = (start[k] + 90.f * i / (pts - 1)) * pi / 180.f;
                shape.setPoint(idx++, {centers[k].x + radius * std::cos(a), centers[k].y + radius * std::sin(a)});
            }
        }
        return shape;
    }

    const char* themeName(boardTheme t) {
        switch (t) {
            case boardTheme::zen: return "Zen";
            case boardTheme::sumie:      return "Sumi-e";
            case boardTheme::dark:       return "Dark mode";
        }
        return "Zen";
    }

    const char* stoneName(stoneStyle s) {
        switch (s) {
            case stoneStyle::zen:       return "Zen";
            case stoneStyle::sumie: return "Sumi-e";
            case stoneStyle::dark:        return "Dark";
        }
        return "Zen";
    }

    const char* ruleName(ruleSet r) {
        switch (r) {
            case ruleSet::Japanese: return "Japanese (Experimental)";
            case ruleSet::Chinese:  return "Chinese";
            case ruleSet::AGA:      return "AGA (Experimental)";
        }
        return "Chinese";
    }
}

menuView::menuView(const Fonts& fonts, gameCfg& cfg) : fonts(fonts), cfg(cfg) {
    const sf::Font& f = fonts.body;

    main.emplace_back("New Game", slot(0), coeffButton, f);
    main.emplace_back("Resume Game", slot(1), coeffButton, f);
    main.emplace_back("Saved Game", slot(2), coeffButton, f);
    main.emplace_back("Settings", slot(3), coeffButton, f);
    
    mode.emplace_back("Player vs Player", slot(0), coeffButton, f);
    mode.emplace_back("Player vs AI", slot(1), coeffButton, f);
    mode.emplace_back("Back", slot(2), coeffButton, f);

    diff.emplace_back("Easy", slot(0), coeffButton, f);
    diff.emplace_back("Medium", slot(1), coeffButton, f);
    diff.emplace_back("Hard", slot(2), coeffButton, f);
    diff.emplace_back("Back", slot(3), coeffButton, f);

    size.emplace_back("9 x 9", slot(0), coeffButton, f);
    size.emplace_back("13 x 13", slot(1), coeffButton, f);
    size.emplace_back("19 x 19", slot(2), coeffButton, f);
    size.emplace_back("Back", slot(3), coeffButton, f);

    initSettingsButtons();
}

void menuView::initSettingsButtons() {
    const sf::Font& f = fonts.body;
    const sf::Vector2f rowSize(440.f, 48.f);
    const sf::Vector2f actionSize(195.f, 48.f);
    const float cx = 575.f;
    const float startY = 265.f;
    const float stepY = 60.f;

    // Indices 0..4: setting toggles/selectors
    settingsBtns.emplace_back("", sf::Vector2f{cx, startY + stepY * 0}, rowSize, f);
    settingsBtns.emplace_back("", sf::Vector2f{cx, startY + stepY * 1}, rowSize, f);
    settingsBtns.emplace_back("", sf::Vector2f{cx, startY + stepY * 2}, rowSize, f);
    settingsBtns.emplace_back("", sf::Vector2f{cx, startY + stepY * 3}, rowSize, f);
    settingsBtns.emplace_back("", sf::Vector2f{cx, startY + stepY * 4}, rowSize, f);

    // Indices 5..6: Reset & Back
    settingsBtns.emplace_back("Reset", sf::Vector2f{cx - 115.f, startY + stepY * 5 + 18.f}, actionSize, f);
    settingsBtns.emplace_back("Back",  sf::Vector2f{cx + 115.f, startY + stepY * 5 + 18.f}, actionSize, f);

    refreshSettingsLabels();
}

void menuView::refreshSettingsLabels() {
    settingsBtns[0].setLabel(std::string("Background Music: ") + (cfg.bgmOn ? "ON" : "OFF"));
    settingsBtns[1].setLabel(std::string("Sound Effects: ") + (cfg.sfxOn ? "ON" : "OFF"));
    settingsBtns[2].setLabel(std::string("Board Theme: ") + themeName(cfg.theme));
    settingsBtns[3].setLabel(std::string("Stone Style: ") + stoneName(cfg.stones));
    settingsBtns[4].setLabel(std::string("Rule Set: ") + ruleName(cfg.rules));
}

void menuView::onSettingsClick(int i) {
    switch (i) {
        case 0: // BGM toggle
            cfg.bgmOn = !cfg.bgmOn;
            settingsChanged = true;
            break;
        case 1: // SFX toggle
            cfg.sfxOn = !cfg.sfxOn;
            settingsChanged = true;
            break;
        case 2: // Cycle Board Theme
            cfg.theme = static_cast<boardTheme>((static_cast<int>(cfg.theme) + 1) % 3);
            cfg.stones = static_cast<stoneStyle>(static_cast<int>(cfg.theme));
            settingsChanged = true;
            break;
        case 3: // Cycle Stone Style
            cfg.stones = static_cast<stoneStyle>((static_cast<int>(cfg.stones) + 1) % 3);
            settingsChanged = true;
            break;
        case 4: // Cycle Rule Set (Chinese -> Japanese -> AGA)
            cfg.setRuleSet(static_cast<ruleSet>((static_cast<int>(cfg.rules) + 1) % 3));
            settingsChanged = true;
            break;
        case 5: // Reset
            cfg.resetSettings();
            settingsChanged = true;
            break;
        case 6: // Back
            settingsOpen = false;
            return;
    }
    refreshSettingsLabels();
}

std::vector<button>& menuView::active() {
    switch (page) {
        case page::Mode: return mode;
        case page::Difficulty: return diff;
        case page::boardSize: return size;
        default: return main;
    }
}

std::string menuView::modeText() const {
    if (cfg.mode == gameMode::PvP) return "Player vs Player";
    const char* names[] = {"Easy", "Medium", "Hard"};
    return std::string("Player vs AI (") + names[static_cast<int>(cfg.difficulty)] + ")";
}

void menuView::onClick(int i) {
    switch (page) {
        case page::Main:
            if (i == 0) page = page::Mode;
            else if (i == 1) action = menuAction::resume;
            else if (i == 2) action = menuAction::openSaved;
            else if (i == 3) {
                refreshSettingsLabels();
                settingsOpen = true;
            }
            break;

        case page::Mode:
            if (i == 0) {
                cfg.mode = gameMode::PvP;
                page = page::boardSize;
            }
            else if (i == 1) {
                cfg.mode = gameMode::PvAI;
                page = page::Difficulty;
            }
            else backtoMain();
            break;

        case page::boardSize:
            if (i == 3) backtoMain();
            else {
                const int sizes[] = {9, 13, 19};
                cfg.boardSize = sizes[i];
                action = menuAction::newGame;
                backtoMain();
            }
            break;

        case page::Difficulty:
            if (i == 3) page = page::Mode;
            else {
                cfg.difficulty = static_cast<difficulty>(i);
                page = page::boardSize;
            }
            break;
    }
}

void menuView::handleEvent(const sf::Event& e, sf::Vector2f mouse) {
    // Intercept all input while the Settings popup is open
    if (settingsOpen) {
        if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::Escape) {
            settingsOpen = false;
            return;
        }
        if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
            for (int i = 0; i < static_cast<int>(settingsBtns.size()); i++) {
                if (settingsBtns[i].contains(mouse)) {
                    onSettingsClick(i);
                    break;
                }
            }
        }
        return;
    }

    if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::Escape && page != page::Main) {
        backtoMain();
        return;
    }
    if (e.type != sf::Event::MouseButtonPressed || e.mouseButton.button != sf::Mouse::Left) return;

    std::vector<button>& btns = active();
    for (int i = 0; i < static_cast<int>(btns.size()); i++) {
        if (btns[i].contains(mouse)) {
            onClick(i);
            break;
        }
    }
}

void menuView::update(sf::Vector2f mouse, bool canResume) {
    if (settingsOpen) {
        // Clear hover states behind the modal
        for (auto& b : active()) b.setHover(false);
        for (auto& b : settingsBtns) b.setHover(b.contains(mouse));
        return;
    }

    main[1].setEnabled(canResume);
    for (auto& b : active()) b.setHover(b.contains(mouse));
}

void menuView::drawSettingsPopup(sf::RenderWindow& w) {
    const theme& th = themeManager::instance().current();
    // 1. Dimmed full-view backdrop
    sf::RectangleShape dim(sf::Vector2f(w.getView().getSize()));
    dim.setFillColor(sf::Color(0, 0, 0, 140));
    w.draw(dim);

    // 2. Rounded popup card centered at (575, 405)
    const sf::Vector2f boxSize(540.f, 520.f);
    sf::ConvexShape box = roundedRect(boxSize, 24.f, 12);
    box.setOrigin(boxSize.x * 0.5f, boxSize.y * 0.5f);
    box.setPosition(575.f, 405.f);
    box.setFillColor(withAlpha(th.hudPanel, 245));
    box.setOutlineThickness(3.f);
    box.setOutlineColor(th.accent);
    w.draw(box);

    // 3. Popup title & buttons
    textCentered(w, "SETTINGS", fonts.title_bold, 38, {575.f, 195.f}, th.accent);

    for (auto& b : settingsBtns) b.draw(w);
}

void menuView::draw(sf::RenderWindow& w) {
    const theme& th = themeManager::instance().current();

    textCentered(w, "GO", fonts.game_title, 130, {575.f, 150.f}, th.accent);
    textCentered(w, "Master your Go board", fonts.body_decor, 48, {575.f, 250.f}, th.textPrimary);
    
    const char* header = nullptr;
    if (page == page::Mode) header = "Select game mode";
    else if (page == page::Difficulty) header = "Select difficulty";
    else if (page == page::boardSize) header = "Select board size";
    
    if (header) textCentered(w, header, fonts.body, 26, {575.f, 320.f}, mixColor(th.windowBg, th.textPrimary, 0.85f));
    
    for (auto& b : active()) b.draw(w);

    if (page == page::Mode || page == page::Difficulty) {
        textCentered(w, "Current: " + modeText(), fonts.body, 18, {575.f, 720.f}, th.textPrimary);
    }

    if (settingsOpen) {
        drawSettingsPopup(w);
    }
}

menuAction menuView::takeAction() {
    menuAction a = action;
    action = menuAction::None;
    return a;
}

bool menuView::takeSettingsChanged() {
    bool c = settingsChanged;
    settingsChanged = false;
    return c;
}