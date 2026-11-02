#include "ui/fonts.hpp"
#include <iostream>

bool Fonts::load(const std::string& dir) {
    const std::string game_titleFile = "TokyoTaiyakiDEMO-Regular.otf";
    const std::string title_boldFile = "CinzelDecorative-Bold.ttf";
    const std::string title_regularFile = "CinzelDecorative-Regular.ttf";
    const std::string body_decorFile = "Blacksword.otf";
    const std::string bodyFile  = "FaunaOne-Regular.ttf";

    bool ok = true;
    auto tryLoad = [&](sf::Font& f, const std::string& name) {
        if (!f.loadFromFile(dir + name)) {
            std::cerr << "Impossible to load font: " << dir + name << "\n";
            ok = false;
        }
    };

    tryLoad(game_title, game_titleFile);
    tryLoad(title_bold, title_boldFile);
    tryLoad(title_regular, title_regularFile);
    tryLoad(body_decor, body_decorFile);
    tryLoad(body,  bodyFile);
    return ok;
}