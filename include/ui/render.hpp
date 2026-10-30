#pragma once
#include <SFML/Graphics.hpp>
#include <string>

// AI-assisted: see AI43, AI44

inline void textCentered(sf::RenderWindow& w, const std::string& s, const sf::Font& f, unsigned size, sf::Vector2f pos, sf::Color c) {
    sf::Text t(s, f, size);
    sf::FloatRect b = t.getLocalBounds();
    t.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
    t.setPosition(pos);
    t.setFillColor(c);
    w.draw(t);
}