#include "ui/hudView.hpp"
#include "ui/layout.hpp"
#include "ui/theme.hpp"

static sf::String u8(const std::string& s) {
    return sf::String::fromUtf8(s.begin(), s.end());
}

namespace {
    struct ButtonSpec {
        hudAction action;
        const char* label;
    };

    constexpr std::array<ButtonSpec, hudView::kButtonCount> kButtonSpecs = {{
        { hudAction::Pass,   "Pass (P)"   },
        { hudAction::Resign, "Resign (R)" },
        { hudAction::Undo,   "Undo (LArr)"   },
        { hudAction::Redo,   "Redo (RArr)"   },
        { hudAction::Save,   "Save (S)"   },
        { hudAction::Load,   "Load (L)"   },
        { hudAction::Reset,  "Reset (N)"  },
        { hudAction::Menu,   "Menu (Esc)"   },
    }};
}

hudView::hudView(const Fonts& fonts) : fonts_(fonts), btns_ {
    button(kButtonSpecs[0].label, {0.f, 0.f}, {144.f, 38.f}, fonts.body),
    button(kButtonSpecs[1].label, {0.f, 0.f}, {144.f, 38.f}, fonts.body),
    button(kButtonSpecs[2].label, {0.f, 0.f}, {144.f, 38.f}, fonts.body),
    button(kButtonSpecs[3].label, {0.f, 0.f}, {144.f, 38.f}, fonts.body),
    button(kButtonSpecs[4].label, {0.f, 0.f}, {144.f, 38.f}, fonts.body),
    button(kButtonSpecs[5].label, {0.f, 0.f}, {144.f, 38.f}, fonts.body),
    button(kButtonSpecs[6].label, {0.f, 0.f}, {144.f, 38.f}, fonts.body),
    button(kButtonSpecs[7].label, {0.f, 0.f}, {144.f, 38.f}, fonts.body)
    } {}

void hudView::ensureButtons(float panelX) {
    if (builtPanelX == panelX) return;
    builtPanelX = panelX;

    const float colX[2] = { panelX + 96.f, panelX + 254.f };
    const float startY = 570.f;
    const float rowStep = 50.f;
    const sf::Vector2f sz{144.f, 38.f};

    for (std::size_t i = 0; i < kButtonCount; ++i) {
        const std::size_t col = i % 2;
        const std::size_t row = i / 2;
        btns_[i] = button(kButtonSpecs[i].label, {colX[col], startY + row * rowStep}, sz, fonts_.body);
    }
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

void hudView::updateButtons(sf::Vector2f mouse, float panelX, const hudButtonStates& states) {
    ensureButtons(panelX);
    const bool enabled[kButtonCount] = {
        states.canPass,
        states.canResign,
        states.canUndo,
        states.canRedo,
        states.canSave,
        states.canLoad,
        states.canReset,
        states.canMenu
    };
    for (std::size_t i = 0; i < kButtonCount; ++i) {
        btns_[i].setEnabled(enabled[i]);
        btns_[i].setHover(btns_[i].contains(mouse));
    }
}

hudAction hudView::handleClick(sf::Vector2f mouse, float panelX, const hudButtonStates& states) {
    updateButtons(mouse, panelX, states);
    for (std::size_t i = 0; i < kButtonCount; ++i) {
        if (btns_[i].contains(mouse)) return kButtonSpecs[i].action;
    }
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

    for (auto& b : btns_) b.draw(w);
}
