#pragma once
#include <SFML/Graphics.hpp>
#include <deque>
#include <string>
#include "ui/fonts.hpp"

class hudView {
public:
    explicit hudView(const Fonts& fonts) : fonts_(fonts) {} // AI-assisted: see AI35
    void push(const std::string& utf8, float seconds = 2.5f);
    void update(float dt);
    void draw(sf::RenderWindow& w, float panelX, int currentPlayer,
              bool gameOver, int blackStones, int whiteStones) const;
private:
    struct Toast { std::string text; float left; };
    const Fonts& fonts_;
    std::deque<Toast> toasts_;
};