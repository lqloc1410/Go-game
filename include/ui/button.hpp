// AI-assisted: see AI43, AI44
#pragma once
#include <SFML/Graphics.hpp>
#include <string>

class button {
    public:
        button(const std::string& label, sf::Vector2f center, sf::Vector2f size, const sf::Font& font);
        void setLabel(const std::string& s);
        void setEnabled(bool b) { enabled = b; }
        void setHover(bool h) { hover = h && enabled; }
        bool contains(sf::Vector2f p) const { return enabled && box.getGlobalBounds().contains(p); };
        void draw(sf::RenderWindow& w);

    private:
        sf::RectangleShape box;
        sf::Text text;
        bool hover = false, enabled = true;
    
};