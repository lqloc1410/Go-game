// include/ui/Fonts.hpp
#pragma once
#include <SFML/Graphics.hpp>
#include <string>

struct Fonts {
    sf::Font game_title;
    sf::Font  title_bold;
    sf::Font title_regular;
    sf::Font body_decor;
    sf::Font body;
    
    bool load(const std::string& dir = "assets/fonts/");
};