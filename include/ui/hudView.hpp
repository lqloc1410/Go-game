#pragma once
#include <SFML/Graphics.hpp>
#include <array>
#include <deque>
#include <string>
#include "ui/button.hpp"
#include "ui/fonts.hpp"

enum class hudAction { None, Undo, Redo, Save, Reset };

class hudView {
public:
    explicit hudView(const Fonts& fonts); // AI-assisted: see AI35
    void push(const std::string& utf8, float seconds = 5.f);
    void update(float dt);

    hudAction handleClick(sf::Vector2f mouse, float panelX, bool canUndo, bool canRedo, bool canReset);
    void updateButtons(sf::Vector2f mouse, float panelX, bool canUndo, bool canRedo, bool canReset);
    void draw(sf::RenderWindow& w, float panelX, int currentPlayer,
              bool gameOver, int blackStones, int whiteStones,
              const std::string& headline = "", const std::string& subline = "");
private:
    struct Toast { std::string text; float left; };
    const Fonts& fonts_;
    std::deque<Toast> toasts_;
    std::array<button, 4> btns_;

    float builtPanelX = -1.f;

    void ensureButtons(float panelX);
};