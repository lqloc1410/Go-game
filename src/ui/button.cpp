#include "ui/button.hpp"

// AI-assisted: see AI43, AI44

button::button(const std::string& label, sf::Vector2f center, sf::Vector2f size, const sf::Font& font) {
    box.setSize(size);
    box.setOrigin(size.x / 2.f, size.y / 2.f);
    box.setPosition(center);
    box.setOutlineThickness(2.f);
    text.setFont(font);
    text.setCharacterSize(24);
    setLabel(label);
}

void button::setLabel(const std::string& s) {
    text.setString(s);
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin(bounds.left + bounds.width / 2.f, bounds.top + bounds.height / 2.f);
    text.setPosition(box.getPosition());
}

void button::draw(sf::RenderWindow& w) {
    if (!enabled) {
        box.setFillColor(sf::Color(45, 45, 50));
        box.setOutlineColor(sf::Color(70, 70, 75));
        text.setFillColor(sf::Color(110, 110, 115));
    }
    else if (hover) {
        box.setFillColor(sf::Color(95, 95, 120));
        box.setOutlineColor(sf::Color(225, 210, 100));
        text.setFillColor(sf::Color(0, 0, 0));
    }
    else {
        box.setFillColor(sf::Color(60, 60, 72));
        box.setOutlineColor(sf::Color(130, 130, 145));
        text.setFillColor(sf::Color(235, 235, 235));
    }
    w.draw(box);
    w.draw(text);
}