#include "ui/hudView.hpp"
#include "ui/layout.hpp"

static sf::String u8(const std::string& s) {
    return sf::String::fromUtf8(s.begin(), s.end());
}

void hudView::push(const std::string& t, float sec) {
    toasts_.push_back({t, sec});
    if (toasts_.size() > 5) toasts_.pop_front();
}

void hudView::update(float dt) {
    for (auto& t : toasts_) t.left -= dt;
    while (!toasts_.empty() && toasts_.front().left <= 0.f) toasts_.pop_front();
}
// AI-assisted: see AI20. AI35
void hudView::draw(sf::RenderWindow& w, float panelX, int currentPlayer,
                   bool gameOver, int blackStones, int whiteStones) const {
    auto text = [&](const sf::Font& font, const std::string& s, float x, float y,
                    unsigned size, sf::Color c = sf::Color::White) {
        sf::Text t(u8(s), font, size);
        t.setPosition(x, y);
        t.setFillColor(c);
        w.draw(t);
    };

    sf::RectangleShape panel({kVirtualW - panelX, kVirtualH});
    panel.setPosition(panelX, 0.f);
    panel.setFillColor(sf::Color(30, 30, 34));
    w.draw(panel);

    float x = panelX + 24.f;
    if (gameOver) {
        text(fonts_.title, "Game Over", x, 30.f, 28, sf::Color(255, 200, 80));
    } else {
        sf::CircleShape s(18.f);
        s.setPosition(x, 30.f);
        s.setFillColor(currentPlayer == 1 ? sf::Color(20, 20, 20) : sf::Color(245, 245, 245));
        s.setOutlineThickness(2.f);
        s.setOutlineColor(sf::Color(150, 150, 150));
        w.draw(s);
        text(fonts_.title, currentPlayer == 1 ? "Turn: Black" : "Turn: White", x + 50.f, 32.f, 26);
    }

    text(fonts_.body, "Black stones: "   + std::to_string(blackStones), x, 100.f, 20);
    text(fonts_.body, "White stones: " + std::to_string(whiteStones), x, 130.f, 20);

    float y = 220.f;
    for (auto& t : toasts_) { text(fonts_.body, t.text, x, y, 17, sf::Color(255, 230, 150)); y += 32.f; }

    text(fonts_.body, "Shortcuts: P = Pass, U = Undo", x, 700.f, 15, sf::Color(150, 150, 150));
    text(fonts_.body, "R = Redo, N = New Game",        x, 725.f, 15, sf::Color(150, 150, 150));
}