// include/ui/Fonts.hpp
#pragma once
#include <SFML/Graphics.hpp>
#include <string>

struct Fonts {
    sf::Font  title;
    sf::Font body;  

    bool load(const std::string& dir = "assets/fonts/");
};