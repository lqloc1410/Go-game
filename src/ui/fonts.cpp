// src/ui/Fonts.cpp
#include "ui/Fonts.hpp"
#include <iostream>

bool Fonts::load(const std::string& dir) {
    const std::string titleFile = "CinzelDecorative-Regular.ttf";
    const std::string bodyFile  = "FaunaOne-Regular.ttf";

    bool ok = true;
    auto tryLoad = [&](sf::Font& f, const std::string& name) {
        if (!f.loadFromFile(dir + name)) {
            std::cerr << "Impossible to load font: " << dir + name << "\n";
            ok = false;
        }
    };
    tryLoad(title, titleFile);
    tryLoad(body,  bodyFile);
    return ok;
}