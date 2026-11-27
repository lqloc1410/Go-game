#include "ui/hudView.hpp"
#include "ui/layout.hpp"
#include "ui/theme.hpp"

static sf::String u8(const std::string& s) {
    return sf::String::fromUtf8(s.begin(), s.end());
}

hudView::hudView(const Fonts& fonts) : fonts_(fonts), btns_ {
    button("Undo",  {0.f, 0.f}, {66.f, 36.f}, fonts.body),
    button("Redo",  {0.f, 0.f}, {66.f, 36.f}, fonts.body),
    button("Save",  {0.f, 0.f}, {66.f, 36.f}, fonts.body),
    button("Reset", {0.f, 0.f}, {66.f, 36.f}, fonts.body)
    } {}

void hudView::ensureButtons(float panelX) {
    if (builtPanelX == panelX) return;
    builtPanelX = panelX;

    float colX = panelX + 250.f;
    sf::Vector2f sz{150.f, 36.f};

    btns_[0] = button("Undo", {colX,  600.f}, sz, fonts_.body);
    btns_[1] = button("Redo", {colX, 650.f}, sz, fonts_.body);
    btns_[2] = button("Save", {colX,  700.f}, sz, fonts_.body);
    btns_[3] = button("Reset", {colX, 750.f}, sz, fonts_.body);
}

void hudView::push(const std::string& t, float sec) {
    for (auto& old : toasts_)
        if (old.text == t) { old.left = sec; return; }
    toasts_.push_back({t, sec});
    if (toasts_.size() > 5) toasts_.pop_front();
}

void hudView::update(float dt) {
    for (auto& t : toasts_) t.left -= dt;
    while (!toasts_.empty() && toasts_.front().left <= 0.f) toasts_.pop_front();
}

void hudView::updateButtons(sf::Vector2f mouse, float panelX, bool canUndo, bool canRedo, bool canReset) {
    ensureButtons(panelX);
    bool states[4] = { canUndo, canRedo, canReset, canReset };
    for (int i = 0; i < 4; i++) {
        btns_[i].setEnabled(states[i]);
        btns_[i].setHover(btns_[i].contains(mouse));
    }
}
hudAction hudView::handleClick(sf::Vector2f mouse, float panelX, bool canUndo, bool canRedo, bool canReset) {
    updateButtons(mouse, panelX, canUndo, canRedo, canReset);
    if (btns_[0].contains(mouse)) return hudAction::Undo;
    if (btns_[1].contains(mouse)) return hudAction::Redo;
    if (btns_[2].contains(mouse)) return hudAction::Save;
    if (btns_[3].contains(mouse)) return hudAction::Reset;
    return hudAction::None;
}

// AI-assisted: see AI20. AI35
void hudView::draw(sf::RenderWindow& w, float panelX, int currentPlayer,
                   bool gameOver, int blackStones, int whiteStones, 
                   const std::string& headline, const std::string& subline) {
    ensureButtons(panelX);

    const theme& th = themeManager::instance().current();
    const sf::Color muted  = mixColor(th.hudPanel, th.textPrimary, 0.55f);
    
    auto text = [&](const sf::Font& font, const std::string& s, float x, float y,
    unsigned size, sf::Color c) {
        sf::Text t(u8(s), font, size);
        t.setPosition(x, y);
        t.setFillColor(c);
        w.draw(t);
    };

    sf::RectangleShape panel({kVirtualW - panelX, kVirtualH});
    panel.setPosition(panelX, 0.f);
    panel.setFillColor(th.hudPanel);
    w.draw(panel);

    float x = panelX + 24.f;
    if (!headline.empty()) {
        text(fonts_.title_bold, headline, x, 30.f, 26, th.accent);
        if (!subline.empty()) text(fonts_.title_bold, subline, x, 70.f, 24, th.accent);
    }
    else if (gameOver) {
        text(fonts_.title_bold, "Game Over", x, 30.f, 28, th.accent);
    } else {
        sf::CircleShape s(18.f);
        s.setPosition(x, 30.f);
        s.setFillColor(currentPlayer == 1 ? th.blackShade : th.whiteHighlight);
        s.setOutlineThickness(2.f);
        s.setOutlineColor(currentPlayer == 1 ? th.blackRim : th.whiteRim);
        w.draw(s);
        text(fonts_.title_bold, currentPlayer == 1 ? "Turn: Black" : "Turn: White", x + 50.f, 32.f, 26, th.accent);
    }

    text(fonts_.body, "Black stones: "   + std::to_string(blackStones), x, 130.f, 20, th.textPrimary);
    text(fonts_.body, "White stones: " + std::to_string(whiteStones), x, 160.f, 20, th.textPrimary);

    float y = 220.f;
    for (auto& t : toasts_) { text(fonts_.body, t.text, x, y, 17, muted); y += 32.f; }

    text(fonts_.body, "Shortcuts: ", x, 575.f, 15, th.textPrimary);
    text(fonts_.body, "P = Pass", x,600.f, 15, th.textPrimary);
    text(fonts_.body, "LArrow = Undo",x,625.f, 15, th.textPrimary);
    text(fonts_.body, "RArrow = Redo", x, 650.f, 15, th.textPrimary);
    text(fonts_.body, "N = Reset board", x, 675.f, 15, th.textPrimary);
    text(fonts_.body, "R = Resign", x,700.f, 15, th.textPrimary);
    text(fonts_.body, "S = Save Game", x, 725.f, 15, th.textPrimary);

    for (auto& b : btns_) b.draw(w);
}
