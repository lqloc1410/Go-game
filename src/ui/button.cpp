#include "ui/button.hpp"
#include "ui/theme.hpp"

// AI-assisted: see AI43, AI44

button::button(const std::string& label, sf::Vector2f center, sf::Vector2f size, const sf::Font& font) {
    box.setSize(size);
    box.setOrigin(size.x / 2.f, size.y / 2.f);
    box.setPosition(center);
    box.setOutlineThickness(2.f);
    text.setFont(font);
    text.setCharacterSize(22);
    setLabel(label);
}

void button::setLabel(const std::string& s) {
    text.setString(s);
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
    text.setPosition(box.getPosition());
}

void button::draw(sf::RenderWindow& w) {
    const theme& th = themeManager::instance().current();
    if (!enabled) {
        box.setFillColor(mixColor(th.hudPanel, th.windowBg, 0.5f));
        box.setOutlineColor(mixColor(th.hudPanel, th.textPrimary, 0.12f));
        text.setFillColor(mixColor(th.hudPanel, th.textPrimary, 0.35f));
    } 
    else if (hover) {
        box.setFillColor(mixColor(th.hudPanel, th.accent, 0.45f));
        box.setOutlineColor(th.accent);
        text.setFillColor(th.textPrimary);
    } 
    else {
        box.setFillColor(th.hudPanel);
        box.setOutlineColor(mixColor(th.hudPanel, th.textPrimary, 0.35f));
        text.setFillColor(th.textPrimary);
    }
    w.draw(box);
    w.draw(text);
}