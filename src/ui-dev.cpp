#include <SFML/Graphics.hpp> // AI-assisted: see AI30
#include <iomanip>
#include <sstream>
#include <thread>
#include "mode/gameSession.hpp"
#include "ui/board.hpp"
#include "ui/hudView.hpp"
#include "ui/fonts.hpp"
#include "ui/layout.hpp"
#include "ui/audio.hpp"
#include "ui/menu.hpp"
#include "ui/render.hpp"

int main() {

    sf::RenderWindow window(sf::VideoMode(1150, 800), "Go UI - dev");
    window.setFramerateLimit(60);

    window.setView(makeLetterboxView(window.getSize())); // AI-assisted: see AI36


    GameSession game(9);
    BoardView board(9, 800.f);

    Fonts fonts;
    if (!fonts.load()) return 1;
    hudView hud(fonts);

    // AI-assisted: see AI43, AI44

    enum class screen { Menu, Game, Placeholder}; //Placeholder is temporary for Saved game and Settings
    screen screen = screen::Menu;
    std::string placeholderTitle;
    bool gameStarted = false;
    gameCfg cfg;
    menuView menu(fonts, cfg);

    Audio audio;
    if (!audio.load()) std::cerr << "Some audio failed to load" << '\n';
    auto stoneAt = [&](int x, int y) { return game.stoneAt(x, y); };

    // Báo kết quả đúng một lần khi ván vừa kết thúc
    bool over = false;
    auto checkEnd = [&] {
        

        EndgameStatus now = game.endgameStatus();
        if (now == EndgameStatus::NotOver) over = false;
        if (over) return;
        if (now == EndgameStatus::Pass) {
            double s = game.score();
            std::ostringstream os;
            os << std::fixed << std::setprecision(1) << (s > 0 ? s : -s);
            hud.push(std::string(s > 0 ? "White" : "Black") + " wins by " + os.str() + " points", 8.f);
        }else if (now == EndgameStatus::Resignation){
            auto tmp = game.resignedPlayer();
            if (!tmp) return;
            
            Color resigned_player = *tmp;
            std::ostringstream os;
            os << (resigned_player == Color::Black ? "White" : "Black");
            os << " wins by resignation.";
            hud.push(os.str());
        }else return;
        over = true;
    };

    sf::Clock clock;
    bool hasSaveFile = std::filesystem::exists("./data/save.sgf");
    while (window.isOpen()) {
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Resized) {
                window.setView(makeLetterboxView({e.size.width, e.size.height}));
            }

            if (e.type == sf::Event::Closed) window.close();

            sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

            // AI-assisted: see AI43, AI44

            if (screen == screen::Menu) {
                menu.handleEvent(e, mouse);
                switch (menu.takeAction()) {
                    case menuAction::newGame:
                        game.newGame(cfg.boardSize, 7.5, 0); //mchau nho implement handicaps nhe :3
                        board.setBoardSize(cfg.boardSize);
                        board.setHover(std::nullopt);
                        over = false;
                        gameStarted = true;
                        screen = screen::Game;
                        break;
                    case menuAction::resume:
                        if (gameStarted){
                            screen = screen::Game;
                        }else if (game.load()) {
                            screen = screen::Game;
                            board.setBoardSize(game.size());
                            gameStarted = true;
                            over = game.isGameOver();
                            board.setHover(std::nullopt);
                        }
                        else screen = screen::Menu;
                        break;
                    case menuAction::openSaved: 
                        placeholderTitle = "Saved game";
                        screen = screen::Placeholder;
                        break;
                    case menuAction::settings:
                        placeholderTitle = "Settings";
                        screen = screen::Placeholder;
                        break;
                    case menuAction::None:
                        break;
                }
                continue;
            }

            if (screen == screen::Placeholder) {
                if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::Escape) screen = screen::Menu;
                continue;
            }

            if (e.type == sf::Event::MouseMoved)
                board.setHover(board.pixelToGrid(mouse));

            if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
                if (auto p = board.pixelToGrid(mouse)) {
                    MoveInfo m = game.play(p->x, p->y);
                    switch (m.result) {
                        case MoveStatus::Success:
                            if (m.captured > 0) {
                                hud.push("Captured " + std::to_string(m.captured) + " stones!");
                                audio.playCapture();
                            }
                            else audio.playStone();                            
                            break;
                        case MoveStatus::Occupied: 
                            hud.push("That point is already occupied!");
                            audio.playError(); 
                            break;
                        case MoveStatus::Ko:  
                            hud.push("Ko/Superko violation!");
                            audio.playError(); 
                            break;
                        case MoveStatus::Suicide:  
                            hud.push("Suicide is not allowed!");
                            audio.playError(); 
                            break;
                        case MoveStatus::GameEnded: 
                            hud.push("The game is over!"); 
                            audio.playError();
                            break;
                        case MoveStatus::OutOfBounds:  
                            hud.push("Out of bounds!");
                            audio.playError(); 
                            break;
                        case MoveStatus::InvalidCommand:  
                            hud.push("Unknown command. Try again!");
                            audio.playError(); 
                            break;
                        default: break;
                    }
                    checkEnd();
                }
            }

            if (e.type == sf::Event::KeyPressed) {
                switch (e.key.code) {
                    case sf::Keyboard::Escape: screen = screen::Menu; if (!game.isGameOver()) game.save(); break;
                    case sf::Keyboard::P: {
                        int who = game.currentPlayer();
                        if (game.pass()) hud.push(who == 1 ? "Black passed." : "White passed.");
                        break;
                    }
                    case sf::Keyboard::Left: if (!game.undo()) hud.push("Nothing to undo!"); break;
                    case sf::Keyboard::Right: if (!game.redo()) hud.push("Nothing to redo!"); break;
                    case sf::Keyboard::N: game.newGame(); hud.push("Board reset!"); break;
                    case sf::Keyboard::R: if (!game.resign()) hud.push("The game is already over!"); break;
                    case sf::Keyboard::S: if (game.save()) hud.push("Game saved!"); else hud.push("Save failed! Try again."); break;
                    default: break;
                }
                checkEnd();
            }
        }

        sf::Vector2f mouseNow = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        if (screen == screen::Menu) {
            bool canResume = (gameStarted && !game.isGameOver()) || (!gameStarted && hasSaveFile);
            menu.update(mouseNow, canResume);
        }
        hud.update(clock.restart().asSeconds());

        window.clear(sf::Color(40, 40, 40)); 
        switch (screen) { // AI-assisted: see AI43, AI44
            case screen::Menu: 
                menu.draw(window);
                break;

            case screen::Placeholder:
                textCentered(window, placeholderTitle, fonts.title, 48, {575.f, 250.f}, sf::Color(240, 220, 170));
                textCentered(window, "Coming soon :(", fonts.body, 26, {575.f, 340.f}, sf::Color(200, 200, 200));
                textCentered(window, "(Press Esc to go back)", fonts.body, 26, {575.f, 380.f}, sf::Color(150, 150, 155));
                break;

            case screen::Game:
                board.draw(window, stoneAt, game.currentPlayer());
                hud.draw(window, 800.f, game.currentPlayer(), game.isGameOver(), game.blackStones(), game.whiteStones());
        }
        window.display();
    }
}