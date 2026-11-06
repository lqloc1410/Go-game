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
        box.setFillColor(sf::Color(68, 58, 53));
        box.setOutlineColor(sf::Color(37, 37, 37));
        text.setFillColor(sf::Color(228, 221, 204));
    }
    else if (hover) {
        box.setFillColor(sf::Color(239, 201, 148));
        box.setOutlineColor(sf::Color(82, 59, 46));
        text.setFillColor(sf::Color(82, 59, 46));
    }
    else {
        box.setFillColor(sf::Color(90, 61, 41, 200));
        box.setOutlineColor(sf::Color(121, 84, 52));
        text.setFillColor(sf::Color(241, 218, 191));
    }
    w.draw(box);
    w.draw(text);
}