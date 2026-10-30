#include "ui/menu.hpp"
#include "ui/render.hpp"

namespace {
    const sf::Vector2f coeffButton(380.f, 60.f);
    sf::Vector2f slot(int i) {
        return {575.f, 380.f + 78.f * i}; 
    }
}

menuView::menuView(const Fonts& fonts, gameCfg& cfg) : fonts(fonts), cfg(cfg) {
    const sf::Font& f = fonts.body;

    main.emplace_back("New Game", slot(0), coeffButton, f);
    main.emplace_back("Saved Game", slot(1), coeffButton, f);
    main.emplace_back("On-going Game", slot(2), coeffButton, f);
    main.emplace_back("Game mode", slot(3), coeffButton, f);
    main.emplace_back("Settings", slot(4), coeffButton, f);
    
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
            if (i == 0) page = page::boardSize;
            else if (i == 1) action = menuAction::openSaved;
            else if (i == 2) action = menuAction::resume;
            else if (i == 3) page = page::Mode;
            else if (i == 4) action = menuAction::settings;
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

        case page::Difficulty:
            if (i==3) page = page::Mode;
            else {
                cfg.difficulty = static_cast<difficulty>(i);
                page = page::boardSize;
            }
            break;
    }
}

void menuView::handleEvent(const sf::Event& e, sf::Vector2f mouse) {
    if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::Escape && page != page::Main) {
        page = page::Main;
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
    main[2].setEnabled(canResume);
    for (auto& b : active()) b.setHover(b.contains(mouse));
}

void menuView::draw(sf::RenderWindow& w) {
    textCentered(w, "GO GAME", fonts.title, 68, {575.f, 200.f}, sf::Color(240, 220, 170));
    
    const char* header = nullptr;
    if (page == page::Mode) header = "Select game mode";
    else if (page == page::Difficulty) header = "Select difficulty";
    else if (page == page::boardSize) header = "Select board size";
    
    if (header) textCentered(w, header, fonts.body, 26, {575.f, 320.f}, sf::Color(200, 200, 200));
    
    for (auto& b : active()) b.draw(w);

    if (page == page::Mode || page == page::Difficulty) textCentered(w, "Current: " + modeText(), fonts.body, 18, {575.f, 720.f}, sf::Color(160, 160, 165));
}

menuAction menuView::takeAction() {
    menuAction a = action;
    action = menuAction::None;
    return a;
}