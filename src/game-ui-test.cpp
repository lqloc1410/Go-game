#include <SFML/Graphics.hpp>

int main() {
    sf::RenderWindow window(sf::VideoMode(800, 800), "Go Game");
    window.setFramerateLimit(60);

    sf::CircleShape stone(20.f);
    stone.setOrigin(20.f, 20.f);          
    stone.setFillColor(sf::Color::Black);

    while (window.isOpen()) {
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed) window.close();
            if (e.type == sf::Event::MouseMoved)
                stone.setPosition((float)e.mouseMove.x, (float)e.mouseMove.y);
        }
        window.clear(sf::Color(222, 184, 135));   
        window.draw(stone);
        window.display();
    }
}