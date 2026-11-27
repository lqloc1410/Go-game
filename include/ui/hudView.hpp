#pragma once
#include <SFML/Graphics.hpp>
#include <array>
#include <cstddef>
#include <deque>
#include <string>
#include "ui/button.hpp"
#include "ui/fonts.hpp"

enum class hudAction { None, Pass, Resign, Undo, Redo, Save, Load, Reset, Menu };

struct hudButtonStates {
    bool canPass   = false;
    bool canResign = false;
    bool canUndo   = false;
    bool canRedo   = false;
    bool canSave   = false;
    bool canLoad   = false;
    bool canReset  = false;
    bool canMenu   = true;
};

class hudView {
public:
    static constexpr std::size_t kButtonCount = 8;

    explicit hudView(const Fonts& fonts); // AI-assisted: see AI35
    void push(const std::string& utf8, float seconds = 5.f);
    void update(float dt);

    hudAction handleClick(sf::Vector2f mouse, float panelX, const hudButtonStates& states);
    void updateButtons(sf::Vector2f mouse, float panelX, const hudButtonStates& states);
    void draw(sf::RenderWindow& w, float panelX, int currentPlayer,
              bool gameOver, int blackStones, int whiteStones,
              const std::string& headline = "", const std::string& subline = "");
private:
    struct Toast { std::string text; float left; };
    const Fonts& fonts_;
    std::deque<Toast> toasts_;
    std::array<button, kButtonCount> btns_;

    float builtPanelX = -1.f;

    void ensureButtons(float panelX);
};