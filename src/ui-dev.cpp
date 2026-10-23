#include <SFML/Graphics.hpp> // AI-assisted: see AI30
#include <iomanip>
#include <sstream>
#include "mode/gameSession.hpp"
#include "ui/board.hpp"
#include "ui/hudView.hpp"
#include "ui/fonts.hpp"
#include "ui/layout.hpp"

int main() {
    
    sf::RenderWindow window(sf::VideoMode(1150, 800), "Go UI - dev");
    window.setFramerateLimit(60);

    window.setView(makeLetterboxView(window.getSize())); // AI-assisted: see AI36


    GameSession game(9);
    BoardView board(9, 800.f);

    Fonts fonts;
    if (!fonts.load()) return 1;
    hudView hud(fonts);

    auto stoneAt = [&](int x, int y) { return game.stoneAt(x, y); };

    // Báo kết quả đúng một lần khi ván vừa kết thúc
    bool over = false;
    auto checkEnd = [&] {
        bool now = game.isGameOver();
        if (now && !over) {
            double s = game.score();
            std::ostringstream os;
            os << std::fixed << std::setprecision(1) << (s > 0 ? s : -s);
            hud.push(std::string(s > 0 ? "White" : "Black") + " wins by " + os.str() + " points", 8.f);
        }
        over = now;
    };

    sf::Clock clock;
    while (window.isOpen()) {
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Resized) {
                window.setView(makeLetterboxView({e.size.width, e.size.height}));
            }

            if (e.type == sf::Event::Closed) window.close();

            sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

            if (e.type == sf::Event::MouseMoved)
                board.setHover(board.pixelToGrid(mouse));

            if (e.type == sf::Event::MouseButtonPressed &&
                e.mouseButton.button == sf::Mouse::Left) {
                if (auto p = board.pixelToGrid(mouse)) {
                    MoveInfo m = game.play(p->x, p->y);
                    switch (m.result) {
                        case MoveResult::Ok:
                            if (m.captured > 0)
                                hud.push("Captured " + std::to_string(m.captured) + " stones!");
                            break;
                        case MoveResult::Occupied: hud.push("That point is already occupied"); break;
                        case MoveResult::Invalid:  hud.push("Invalid move (Ko or suicide)"); break;
                        case MoveResult::GameOver: hud.push("The game is over"); break;
                        default: break;
                    }
                    checkEnd();
                }
            }

            if (e.type == sf::Event::KeyPressed) {
                switch (e.key.code) {
                    case sf::Keyboard::P: {
                        int who = game.currentPlayer();
                        if (game.pass()) hud.push(who == 1 ? "Black passed" : "White passed");
                        break;
                    }
                    case sf::Keyboard::U: if (!game.undo()) hud.push("Nothing to undo"); break;
                    case sf::Keyboard::R: if (!game.redo()) hud.push("Nothing to redo"); break;
                    case sf::Keyboard::N: game.newGame(19); hud.push("New game"); break;
                    default: break;
                }
                checkEnd();
            }
        }

        hud.update(clock.restart().asSeconds());

        window.clear(sf::Color(40, 40, 40));
        board.draw(window, stoneAt, game.currentPlayer());
        hud.draw(window, 800.f, game.currentPlayer(), game.isGameOver(),
                 game.blackStones(), game.whiteStones());
        window.display();
    }
}