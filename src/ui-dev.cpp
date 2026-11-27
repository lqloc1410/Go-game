#include <SFML/Graphics.hpp> // AI-assisted: see AI30
#include <SFML/Window/ContextSettings.hpp>
#include <iomanip>
#include <sstream>
#include <thread>
#include <algorithm> // AI-assisted: seeAI79
#include <cmath>
#include <chrono>
#include <future>
#include "mode/gameSession.hpp"
#include "ui/board.hpp"
#include "ui/hudView.hpp"
#include "ui/fonts.hpp"
#include "ui/layout.hpp"
#include "ui/audio.hpp"
#include "ui/menu.hpp"
#include "ui/render.hpp"
#include "ui/theme.hpp"

static std::string marginText(double s) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(1) << (s > 0 ? s : -s);
    return std::string(s > 0 ? "White" : "Black") + " leads by " + os.str();
}
static sf::Sprite coverSprite(const sf::Texture& tex, float W, float H) {
    sf::Sprite s(tex);
    float k = std::max(W / tex.getSize().x, H / tex.getSize().y);
    s.setScale(k, k);
    s.setPosition((W - tex.getSize().x * k) / 2.f, (H - tex.getSize().y * k) / 2.f);
    return s;
}
static sf::ConvexShape roundedRect(sf::Vector2f size, float radius, int pts = 10) {
    sf::ConvexShape shape(4 * pts);
    const float pi = 3.14159265f;
    const sf::Vector2f centers[4] = {{size.x - radius, radius}, {size.x - radius, size.y - radius},
                                     {radius, size.y - radius}, {radius, radius}};
    const float start[4] = {-90.f, 0.f, 90.f, 180.f};              // one quarter circle per corner
    int idx = 0;
    for (int k = 0; k < 4; ++k)
        for (int i = 0; i < pts; ++i) {
            float a = (start[k] + 90.f * i / (pts - 1)) * pi / 180.f;
            shape.setPoint(idx++, {centers[k].x + radius * std::cos(a), centers[k].y + radius * std::sin(a)});
        }
    return shape;
}

static std::string pointsText(double s) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(1) << (s > 0 ? s : -s);
    return os.str();
}
int main() {
    sf::ContextSettings settings;
    settings.antialiasingLevel = 8; // 4 or 8 (driver clamps to max supported)
    sf::RenderWindow window(sf::VideoMode(1150, 800), "Go UI - dev", sf::Style::Default, settings);
    window.setFramerateLimit(60);
    //window.setVerticalSyncEnabled(true);
    window.setView(makeLetterboxView(window.getSize())); // AI-assisted: see AI36


    GameSession game(9);
    BoardView board(9, 800.f);


    Fonts fonts;
    if (!fonts.load()) return 1;
    hudView hud(fonts);

    sf::RectangleShape dim({1150.f, 800.f});
    dim.setFillColor(sf::Color(0, 0, 0, 75));

    // AI-assisted: see AI43, AI44

    enum class screen { Menu, Game, GameOver, Settings, Placeholder}; //Placeholder is temporary for Saved game and Settings
    screen screen = screen::Menu;
    std::string placeholderTitle;
    bool gameStarted = false;
    Phase prevPhase = game.phase();
    gameCfg cfg;
    menuView menu(fonts, cfg);

    Audio audio;
    if (!audio.load()) std::cerr << "Some audio failed to load" << '\n';
    
    auto stoneAt = [&](int x, int y) { return game.stoneAt(x, y); };

    button resumeBtn("Resume play", {975.f, 400.f}, {260.f, 46.f}, fonts.body);
    button doneBtn("Done", {975.f, 465.f}, {260.f, 46.f}, fonts.body);
    
    button goNewBtn ("New Game", {575.f, 440.f}, {300.f, 54.f}, fonts.body);
    button goViewBtn ("View board", {575.f, 510.f}, {300.f, 54.f}, fonts.body);
    button goMenuBtn ("Main menu", {575.f, 580.f}, {300.f, 54.f}, fonts.body);

    themeManager& themes = themeManager::instance();
    themes.loadChoice();

    sf::RectangleShape overlay({1150.f, 800.f});
    overlay.setFillColor(sf::Color(0, 0, 0, 170));
    
    const theme& th = themeManager::instance().current();

    sf::ConvexShape box = roundedRect({520.f, 460.f}, 24.f);
    box.setPosition(575.f - 260.f, 400.f - 230.f); 
    box.setFillColor(withAlpha(th.hudPanel, 245));
    box.setOutlineThickness(3.f);
    box.setOutlineColor(th.accent);

    sf::RectangleShape divider({440.f, 2.f});
    divider.setOrigin(220.f, 1.f);
    divider.setPosition(575.f, 395.f);
    divider.setFillColor(withAlpha(th.accent, 120));
    
    bool over = false;
    bool viewingFinalBoard = false;
    bool isAI = false;
    std::future<int16_t> aiFuture;
    bool aiThinking = false;
    int aiPlayer = 2; // 1 = Black, 2 = White

    auto waitForAI = [&]{
        if (aiThinking && aiFuture.valid()) { aiFuture.wait(); aiThinking = false; }
    };

    auto isAITurn = [&]{
        return isAI && game.phase() == Phase::Playing && game.currentPlayer() == aiPlayer;
    };

    auto isDead = [&](int x, int y) {
        return game.isDeadStone(x, y);
    };

    auto toAIDiff = [](difficulty d) {
        switch (d) {
            case difficulty::Easy:   return AIDifficulty::Easy;
            case difficulty::Medium: return AIDifficulty::Medium;
            case difficulty::Hard:   return AIDifficulty::Hard;
        }
        return AIDifficulty::Easy;
    };

    auto syncAIFromGame = [&] {
        isAI = game.isAI();
        aiPlayer = (game.aiColor() == Color::Black) ? 1 : 2;
    };

    auto performUndo = [&] {
        if (!game.undo()) {
            hud.push("Nothing to undo!");
            return;
        }
        if (isAI && isAITurn()) {
            // Undo the preceding human move too so Undo doesn't immediately re-trigger AI.
            // If only the AI's opening move existed, restore it rather than forcing a re-search.
            if (!game.undo()) {
                game.redo();
                hud.push("Nothing to undo!");
            }
        }
    };

    auto performRedo = [&] {
        if (!game.redo()) {
            hud.push("Nothing to redo!");
            return;
        }
        if (isAI && isAITurn()) {
            game.redo();
        }
    };

    auto startNewGame = [&] {
        waitForAI();
        game.newGame(cfg.boardSize, cfg.komi, cfg.handicap); //mchau nho implement handicaps nhe :3
        game.setAIConfig(cfg.mode == gameMode::PvAI, toAIDiff(cfg.difficulty), Color::White);
        syncAIFromGame();
        board.setBoardSize(cfg.boardSize);
        board.setHover(std::nullopt);
        viewingFinalBoard = false;
        over = false;
        gameStarted = true;
        screen = screen::Game;
        prevPhase = game.phase();
        board.resetAnimation();
    };

    // Báo kết quả đúng một lần khi ván vừa kết thúc
    
    auto checkEnd = [&] {
        

        EndgameStatus now = game.endgameStatus();
    if (now == EndgameStatus::NotOver) {
        over = false;
    }
        if (over) return;
        if (now == EndgameStatus::Pass) {
            double s = game.score();
            std::ostringstream os;
            os << std::fixed << std::setprecision(1) << (s > 0 ? s : -s);
            hud.push(std::string(s > 0 ? "White" : "Black") + " wins by " + os.str() + " points", 8.f);
        }
        else if (now == EndgameStatus::Resignation){
            auto tmp = game.resignedPlayer();
            if (!tmp) return;
            
            Color resigned_player = *tmp;
            std::ostringstream os;
            os << (resigned_player == Color::Black ? "White" : "Black");
            os << " wins by resignation.";
            hud.push(os.str());
        }
        else return;
        over = true;
    };

    sf::Clock clock;
    bool hasSaveFile = std::filesystem::exists("./data/save.sgf");

    auto makeHudStates = [&] {
        const bool playing = (game.phase() == Phase::Playing);
        return hudButtonStates{
            playing && !aiThinking && !isAITurn(),      // canPass
            playing && !aiThinking,                     // canResign
            playing && !aiThinking,                     // canUndo
            playing && !aiThinking,                     // canRedo
            gameStarted && !aiThinking,                 // canSave
            hasSaveFile && !aiThinking,                 // canLoad
            gameStarted && !aiThinking,                 // canReset
            true                                        // canMenu
        };
    };
    
    audio.load();
    themes.onChange([&](const theme& t) { 
        audio.applyTheme(t);  
    });

    cfg.theme = static_cast<boardTheme>(static_cast<int>(themes.currentId()));
    cfg.stones = static_cast<stoneStyle>(static_cast<int>(themes.stoneId()));

    auto applySettings = [&] {
        themes.set(static_cast<themeID>(static_cast<int>(cfg.theme)));
        themes.setStoneStype(static_cast<themeID>(static_cast<int>(cfg.stones)));
        audio.setMusicOn(cfg.bgmOn);
        audio.setSfxVolume(cfg.sfxOn ? 70.f : 0.f);
        themes.saveChoice();
    };
    applySettings();

    while (window.isOpen()) {
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Resized) {
                window.setView(makeLetterboxView({e.size.width, e.size.height}));
            }

            if (e.type == sf::Event::Closed) {
                waitForAI();
                window.close();
            }

            sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));

            // AI-assisted: see AI43, AI44

            if (screen == screen::Menu) {
                menu.handleEvent(e, mouse);
                switch (menu.takeAction()) {
                    case menuAction::newGame:
                        startNewGame();
                        break;
                    case menuAction::resume:
                        if (gameStarted){
                            screen = screen::Game;
                        }else {
                            waitForAI();
                            if (game.load()) {
                                screen = screen::Game;
                                board.setBoardSize(game.size());
                                gameStarted = true;
                                over = game.isGameOver();
                                prevPhase = game.phase();
                                board.setHover(std::nullopt);
                                syncAIFromGame();
                            }
                            else screen = screen::Menu;
                        }
                        break;
                    case menuAction::openSaved: 
                        placeholderTitle = "Saved game";
                        screen = screen::Placeholder;
                        break;
                    case menuAction::Settings:
                        placeholderTitle = "Settings";
                        screen = screen::Settings;
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
                board.setHover((game.phase() == Phase::Playing && !aiThinking && !isAITurn()) ? board.pixelToGrid(mouse) : std::nullopt);

            if (screen == screen::GameOver) {
                if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
                    if (goNewBtn.contains(mouse)) {
                        viewingFinalBoard = false;
                        startNewGame();
                    } 
                    else if (goViewBtn.contains(mouse)) {
                        viewingFinalBoard = true;
                        screen = screen::Game;   
                    }
                    else if (goMenuBtn.contains(mouse)) {
                        viewingFinalBoard = false;
                        screen = screen::Menu;
                    }
                }
                if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::Escape) {
                    viewingFinalBoard = false;
                    screen = screen::Menu;
                }
                continue;
            }


            if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
                bool clickedBoardOrMarking = false;
                switch (game.phase()) {
                    case Phase::Marking:
                        if (doneBtn.contains(mouse)) {
                            game.confirmScore();
                            clickedBoardOrMarking = true;
                        }
                        else if (resumeBtn.contains(mouse)) {
                            game.resumePlay();
                            hud.push("Back to play");
                            clickedBoardOrMarking = true;
                        }
                        else if (auto p = board.pixelToGrid(mouse)) {
                            game.toggleDeadGroup(p -> x, p -> y);
                            clickedBoardOrMarking = true;
                        }
                        break;
                    case Phase::Playing:
                        if (aiThinking || isAITurn()) break;
                        if (auto p = board.pixelToGrid(mouse)) {
                            clickedBoardOrMarking = true;
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
                            }
                            break;
                        case Phase::Finished: 
                            break;
                }
                if (!clickedBoardOrMarking) {
                    hudAction act = hud.handleClick(mouse, 800.f, makeHudStates());
                    switch (act) {
                        case hudAction::Pass:
                            if (game.phase() == Phase::Playing) {
                                int who = game.currentPlayer();
                                if (game.pass()) hud.push(who == 1 ? "Black passed." : "White passed.");
                            }
                            break;
                        case hudAction::Resign:
                            if (!game.resign()) hud.push("The game is already over!");
                            break;
                        case hudAction::Undo:
                            if (game.phase() == Phase::Playing && !aiThinking) performUndo();
                            break;
                        case hudAction::Redo:
                            if (game.phase() == Phase::Playing && !aiThinking) performRedo();
                            break;
                        case hudAction::Save:
                            if (game.save()) {
                                hud.push("Game saved!");
                                hasSaveFile = true;
                            } 
                            else {
                                hud.push("Save failed! Try again.");
                            }
                            break;
                        case hudAction::Load:
                            // mchau pls add this when you have time    

                            hud.push("Load not implemented yet.");
                            /*if (game.load()) {
                                board.setBoardSize(game.size());
                                board.setHover(std::nullopt);
                                board.resetAnimation();
                                viewingFinalBoard = false;
                                over = game.isGameOver();
                                prevPhase = game.phase();
                                gameStarted = true;
                                syncAIFromGame();
                                hud.push("Game loaded!");
                            }
                            else {
                                hud.push("Load failed! No valid save found.");
                            }*/
                            break;
                        case hudAction::Reset:
                            waitForAI();
                            game.newGame();
                            viewingFinalBoard = false;
                            over = false;
                            prevPhase = game.phase();
                            hud.push("Board reset!");
                            break;
                        case hudAction::Menu:
                            waitForAI();
                            screen = screen::Menu;
                            if (!game.isGameOver() && game.save()) {
                                hasSaveFile = true;
                            }
                            break;
                        case hudAction::None:
                            break;
                    }
                }
            }
            if (e.type == sf::Event::KeyPressed) {
                switch (e.key.code) {
                    case sf::Keyboard::Escape: 
                        waitForAI();
                        screen = screen::Menu; 
                        if (!game.isGameOver()) {
                            game.save(); 
                            hasSaveFile = true;
                        }
                        break;
                    case sf::Keyboard::P: {
                        if (game.phase() == Phase::Playing && !aiThinking && !isAITurn()) {
                            int who = game.currentPlayer();
                            if (game.pass()) hud.push(who == 1 ? "Black passed." : "White passed.");
                        }
                        break;
                    }
                    case sf::Keyboard::Left: 
                        if (game.phase() == Phase::Playing && !aiThinking) {
                            performUndo();
                        }
                        break;
                    case sf::Keyboard::Right: 
                        if (game.phase() == Phase::Playing && !aiThinking) {
                            performRedo();
                        }
                        break;
                    case sf::Keyboard::N: 
                        if (aiThinking) break;
                        waitForAI();
                        game.newGame(); 
                        viewingFinalBoard = false;
                        over = false;
                        prevPhase = game.phase();
                        hud.push("Board reset!"); 
                        break;
                    case sf::Keyboard::R: 
                        if (aiThinking) break;
                        if (!game.resign()) hud.push("The game is already over!"); 
                        break;
                    case sf::Keyboard::S: 
                        if (aiThinking) break;
                        if (game.save()) {
                            hud.push("Game saved!"); 
                            hasSaveFile = true;
                        }
                        else hud.push("Save failed! Try again."); 
                        break;
                    default: break;
                }
            }
        }
        sf::Vector2f mouseNow = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        if (screen == screen::Game && isAITurn() && !aiThinking) {
            board.setHover(std::nullopt);
            AISearchInput input = game.captureAIInput();
            aiThinking = true;
            aiFuture = std::async(std::launch::async, [&game, input]() {
                return game.computeAIMove(input.board, input.color, input.lastMoveType);
            });
        }
        if (aiThinking && aiFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            int16_t moveId = aiFuture.get();
            aiThinking = false;
            if (screen == screen::Game && isAITurn()) {
                MoveInfo m = game.applyAIMove(moveId);
                if (m.result == MoveStatus::Success) {
                    if (m.is_pass) {
                        hud.push("AI passed.");
                    } else if (m.captured > 0) {
                        hud.push("Captured " + std::to_string(m.captured) + " stones!");
                        audio.playCapture();
                    } else {
                        audio.playStone();
                    }
                }
            }
        }

        game.syncPhase();
        Phase ph = game.phase();
        if (ph != prevPhase) {
            // std::cout << "phase " << static_cast<int>(prevPhase) << " -> " << static_cast<int>(ph) << "\n";   // temporary
            if (ph == Phase::Marking) {
                viewingFinalBoard = false;
                hud.push("Both players passed.\nClick the stone to mark dead.", 4.f);
            }
            if (ph == Phase::Finished && screen == screen::Game && !viewingFinalBoard) screen = screen::GameOver;
            prevPhase = ph;
        }
        if (screen == screen::GameOver) {
            goNewBtn.setHover(goNewBtn.contains(mouseNow));
            goViewBtn.setHover(goViewBtn.contains(mouseNow));
            goMenuBtn.setHover(goMenuBtn.contains(mouseNow));
        }
        
        bool marking = (ph == Phase::Marking);
        resumeBtn.setHover(marking && resumeBtn.contains(mouseNow));
        doneBtn.setHover(marking && doneBtn.contains(mouseNow));
        hud.updateButtons(mouseNow, 800.f, makeHudStates());
        
        bool canResume = (gameStarted && game.phase() != Phase::Finished || hasSaveFile);
        menu.update(mouseNow, canResume);
        if (menu.takeSettingsChanged()) {
            applySettings();
        }

        float dt = clock.restart().asSeconds();
        hud.update(dt);
        audio.update(dt);

        const theme& th = themes.current();
        window.clear(th.windowBg);

        if (screen != screen::Game) {
            if (const sf::Texture* tex = themes.menuBg()) window.draw(coverSprite(*tex, 1150.f, 800.f));
            if (th.menuDimAlpha > 0) { dim.setFillColor(sf::Color(0, 0, 0, th.menuDimAlpha)); window.draw(dim); }
        }

        switch (screen) { // AI-assisted: see AI43, AI44
            case screen::Menu: 
                menu.draw(window);
                break;

            case screen::Placeholder:
                textCentered(window, placeholderTitle, fonts.title_bold, 48, {575.f, 250.f}, sf::Color(44, 38, 30));
                textCentered(window, "Coming soon", fonts.body, 26, {575.f, 340.f}, sf::Color(90, 61, 41));
                textCentered(window, "(Press Esc to go back)", fonts.body, 26, {575.f, 380.f}, sf::Color(121, 84, 52));
                break;

            case screen::Game: {
                Phase dp = game.phase();
                std::string headline, subline;
                if (dp == Phase::Marking) {
                    headline = "Mark dead stones";
                    subline = marginText(game.score());
                }
                else if (dp == Phase::Finished) {
                    if (auto who = game.resignedPlayer()) {
                        subline = (*who == Color::Black) ? "Black resigned" : "White resigned";
                        headline = (*who == Color::Black) ? "White wins" : "Black wins";
                    }
                    else {
                        double s = game.score();
                        headline = (s > 0) ? "White wins" : "Black wins";
                        subline = marginText(s);
                    }
                }
                std::optional<GridPos> last;
                if (auto m = game.lastMove()) last = GridPos{m -> first, m -> second};
                
                board.draw(window, stoneAt, game.currentPlayer(), last, isDead);
                hud.draw(window, 800.f, game.currentPlayer(), game.isGameOver(), game.blackStones(), game.whiteStones(), headline, subline);
                
                if (dp == Phase::Marking) {
                    resumeBtn.draw(window);
                    doneBtn.draw(window);
                }
                break;
            }
            
            case screen::GameOver: {
                board.draw(window, stoneAt, game.currentPlayer(), std::nullopt, isDead);
                window.draw(overlay);
                window.draw(box);

                std::string result, detail;
                if (auto who = game.resignedPlayer()) {
                    double s = game.score();
                    result = (*who == Color::Black) ? "Black resigned" : "White resigned";
                    detail = (*who == Color::Black) ? "White wins" : "Black wins";
                }
                else {
                    double s = game.score();
                    if (s > 0.0) {
                        result = "White wins";
                        detail = marginText(s);
                    }
                    else if (s < 0.0) {
                        result = "Black wins";
                        detail = marginText(s);
                    }
                    else {
                        result = "Tie game";
                        detail = "Score is even (0.0)";
                    }
                }
                textCentered(window, "GAME OVER", fonts.title_bold, 52, {575.f, 230.f}, th.accent);
                textCentered(window, result, fonts.title_bold, 36, {575.f, 305.f}, th.textPrimary);
                textCentered(window, detail, fonts.body, 22, {575.f, 365.f}, mixColor(th.hudPanel, th.textPrimary, 0.8f));
                window.draw(divider);
                goNewBtn.draw(window);
                goViewBtn.draw(window);
                goMenuBtn.draw(window);
                break;
            }
        }   
        window.display();
    }
}
