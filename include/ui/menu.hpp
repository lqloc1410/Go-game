#pragma once
#include <string>
#include <vector>
#include <SFML/Graphics.hpp>
#include "ui/button.hpp"
#include "ui/fonts.hpp"
#include "ui/gameCfg.hpp"

enum class menuAction { None, newGame, resume, openSaved, settings };

class menuView {
    public: 
        menuView(const Fonts& fonts, gameCfg& cfg);

        void handleEvent(const sf::Event& e, sf::Vector2f mouse);
        void update(sf::Vector2f mouse, bool canResume);
        void draw(sf::RenderWindow& w);
        menuAction takeAction();
    
    private:
        enum class page { Main, Mode, Difficulty, boardSize };

        std::vector<button>& active();
        void onClick(int index);
        void backtoMain() {page = page::Main; };
        std::string modeText() const;

        const Fonts& fonts;
        gameCfg& cfg;
        page page = page::Main;
        std::vector<button> main, mode, diff, size;
        menuAction action = menuAction::None;
};


