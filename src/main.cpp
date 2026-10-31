#include "Game.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <cctype>
#include <iomanip>
#include <cmath>

static void print_help() {
    std::cout << "\nCommands:\n"
              << "  <r> <c>      : Play at 0-indexed (row, col), e.g. '3 4'\n"
              << "  <Col><Row>   : Play at Go coordinate, e.g. 'D4', 'E5', 'Q16'\n"
              << "  load <file>  : Load game from an SGF file (or 'l <file>')\n"
              << "  save         : Save game to SGF (default: data/save.sgf)\n"
              << "  pass   (p)   : Pass turn (2 consecutive passes end the game)\n"
              << "  resign       : Resign the game\n"
              << "  undo   (u)   : Undo the last move\n"
              << "  redo   (r)   : Redo the last undone move\n"
              << "  score  (s)   : Calculate current Chinese Area Score\n"
              << "  help   (h)   : Show this help message\n"
              << "  quit   (q)   : Exit the game\n";
}

static void print_score(const Game& game) {
    double score = game.calculate_score();
    double komi = game.get_komi();
    std::cout << "\n================ Area Score (Komi: " << komi << ") ================\n";
    std::cout << "Stones on Board -> Black (X): " << game.get_black_cnt()
              << " | White (O): " << game.get_white_cnt() << "\n";
    std::cout << "Net Score (White - Black + Komi): "
              << std::fixed << std::setprecision(1) << score << "\n";
    if (score > 0.0) {
        std::cout << ">>> Result: WHITE (O) leads by " << score << " points!\n";
    } else if (score < 0.0) {
        std::cout << ">>> Result: BLACK (X) leads by " << -score << " points!\n";
    } else {
        std::cout << ">>> Result: Draw (Jigo)!\n";
    }
    std::cout << "==============================================================\n";
}

static bool parse_move(const std::string& line, const Game& game, int16_t& out_id) {
    int sz = game.get_size();
    std::istringstream iss(line);

    // 1. Try parsing two integers: "r c" (0-indexed)
    int r, c;
    if (iss >> r >> c) {
        std::string extra;
        if (!(iss >> extra)) {
            int16_t id = game.get_board().to_id(r, c);
            if (id != -1) {
                out_id = id;
                return true;
            }
        }
        return false;
    }

    // 2. Try parsing standard Go coordinate: e.g. "D4", "Q16" (skipping 'I')
    std::istringstream token_ss(line);
    std::string token;
    if (!(token_ss >> token) || token.size() < 2) return false;
    std::string extra;
    if (token_ss >> extra) return false;

    char col_ch = static_cast<char>(std::toupper(static_cast<unsigned char>(token[0])));
    if (col_ch < 'A' || col_ch > 'T' || col_ch == 'I') return false;

    int col = (col_ch < 'I') ? (col_ch - 'A') : (col_ch - 'A' - 1);
    if (col < 0 || col >= sz) return false;

    try {
        size_t idx = 0;
        int go_row = std::stoi(token.substr(1), &idx);
        if (idx != token.size() - 1) return false;
        if (go_row < 1 || go_row > sz) return false;

        int row = sz - go_row;
        int16_t id = game.get_board().to_id(row, col);
        if (id != -1) {
            out_id = id;
            return true;
        }
    } catch (...) {
        return false;
    }

    return false;
}

int main(int argc, char* argv[]) {
    std::cout << "==============================================================\n";
    std::cout << "                     Go Game CLI Engine                       \n";
    std::cout << "==============================================================\n";

    Game game(9);
    bool loaded = false;

    // Check command line arguments:
    // 1. main <file.sgf>
    // 2. main <board_size> [handicap]
    if (argc > 1) {
        std::string arg1 = argv[1];
        auto maybe_sz = to_int(arg1);
        if (maybe_sz.has_value() && *maybe_sz >= 2 && *maybe_sz <= MAX_BOARD_WIDTH) {
            int board_size = *maybe_sz;
            int handicap = 0;
            if (argc > 2) {
                auto maybe_ha = to_int(argv[2]);
                if (maybe_ha.has_value() && (*maybe_ha == 0 || (*maybe_ha >= 2 && *maybe_ha <= 9))) {
                    handicap = *maybe_ha;
                }
            }
            double komi = (handicap >= 2) ? 0.5 : 7.5;
            game = Game(board_size, komi, handicap);
            loaded = true;
        } else {
            if (game.load_from_file(arg1)) {
                loaded = true;
                std::cout << ">> Successfully loaded: " << arg1 << "\n";
                std::cout << game.get_log() << "\n";
            } else {
                std::cout << ">> Failed to load: " << arg1 << "\n";
                return 1;
            }
        }
    }

    if (!loaded) {
        std::cout << "Enter board size [2-" << MAX_BOARD_WIDTH << "] (default 9) [handicap 2-9], or 'load <filepath>': ";
        std::string sz_line;
        if (std::getline(std::cin, sz_line)) {
            sz_line = strip(sz_line);
            std::string sz_lower = sz_line;
            for (char& ch : sz_lower) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }

            // Check if user entered 'load <file>' or a direct .sgf file path
            if (sz_lower.rfind("load ", 0) == 0 || sz_lower.rfind("l ", 0) == 0 ||
                (sz_line.size() > 4 && sz_line.substr(sz_line.size() - 4) == ".sgf")) {
                std::string path = sz_line;
                if (sz_lower.rfind("load ", 0) == 0) path = strip(sz_line.substr(5));
                else if (sz_lower.rfind("l ", 0) == 0) path = strip(sz_line.substr(2));

                if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
                    path = path.substr(1, path.size() - 2);
                }
                if (game.load_from_file(path)) {
                    loaded = true;
                    std::cout << ">> Successfully loaded: " << path << "\n";
                    std::cout << game.get_log() << "\n";
                } else {
                    std::cout << ">> SGF loading failed: invalid syntax or unsupported format.\n";
                    return 1;
                }
            }

            if (!loaded) {
                int board_size = 9;
                int handicap = 0;
                bool handicap_specified = false;

                if (!sz_line.empty()) {
                    std::istringstream iss(sz_line);
                    int s_val = 0, h_val = 0;
                    if (iss >> s_val) {
                        if (s_val >= 2 && s_val <= MAX_BOARD_WIDTH) {
                            board_size = s_val;
                        } else {
                            std::cout << "Invalid board size, defaulting to 9x9.\n";
                        }
                        if (iss >> h_val) {
                            handicap_specified = true;
                            if (h_val == 0 || (h_val >= 2 && h_val <= 9 && h_val < board_size * board_size)) {
                                handicap = h_val;
                            } else {
                                std::cout << "Invalid handicap count (must be 0 or 2-9), defaulting to 0.\n";
                            }
                        }
                    }
                }

                // If handicap wasn't given on the same line, prompt for it
                if (!handicap_specified) {
                    std::cout << "Enter handicap stones [0, 2-9] (default 0): ";
                    std::string h_line;
                    if (std::getline(std::cin, h_line)) {
                        h_line = strip(h_line);
                        if (!h_line.empty()) {
                            try {
                                int val = std::stoi(h_line);
                                if (val == 0 || (val >= 2 && val <= 9 && val < board_size * board_size)) {
                                    handicap = val;
                                } else {
                                    std::cout << "Invalid handicap count (must be 0 or 2-9), defaulting to 0.\n";
                                }
                            } catch (...) {
                                std::cout << "Invalid input, defaulting to 0 handicap.\n";
                            }
                        }
                    }
                }

                double komi = (handicap >= 2) ? 0.5 : 7.5;
                try {
                    game = Game(board_size, komi, handicap);
                } catch (const std::exception& e) {
                    std::cout << ">> Error initializing game: " << e.what() << ", defaulting to 9x9 even game.\n";
                    game = Game(9);
                }
            }
        } else {
            game = Game(9);
        }
    }

    print_help();

    while (true) {
        game.display();

        int hand = game.get_history().get_handicap_cnt();
        int total_placed = static_cast<int>(game.get_history().get_added_black().size() + game.get_history().get_history().size());

        Color turn = game.get_turn();
        const char* turn_str = (turn == Color::Black) ? "Black (X)" : "White (O)";

        std::cout << "\nStones -> Black (X): " << game.get_black_cnt()
                  << " | White (O): " << game.get_white_cnt() << "\n";
        if (hand >= 2) {
            std::cout << "Handicap: " << hand << " stones | Komi: " << game.get_komi() << "\n";
        }

        EndgameStatus endgame = game.is_game_over();
        if (endgame == EndgameStatus::Pass) {
            std::cout << "\n*** Game Over (Both players passed consecutively) ***\n";
            print_score(game);
            std::cout << "Enter 'undo' (u) to resume, 'save' to save, 'load <file>' to load, or 'quit' (q) to exit: ";
        } else if (endgame == EndgameStatus::Resignation) {
            std::optional<Color> resigned = game.get_resigned_player();
            const char* loser_str = (resigned == Color::Black) ? "Black (X)" : "White (O)";
            const char* winner_str = (resigned == Color::Black) ? "WHITE (O)" : "BLACK (X)";
            const char* code_str = (resigned == Color::Black) ? "W+R" : "B+R";
            std::cout << "\n*** Game Over (" << loser_str << " resigned) ***\n";
            std::cout << ">>> Result: " << winner_str << " wins by resignation (" << code_str << ")!\n";
            std::cout << "Enter 'undo' (u) to resume, 'save' to save, 'load <file>' to load, or 'quit' (q) to exit: ";
        } else {
            if (total_placed < hand) {
                std::cout << "[Black (X) - Handicap stone " << (total_placed + 1) << "/" << hand << "] Enter move: ";
            } else {
                std::cout << "[" << turn_str << "] Enter move or command: ";
            }
        }

        std::string line;
        if (!std::getline(std::cin, line)) break;

        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        size_t last = line.find_last_not_of(" \t\r\n");
        std::string cmd = line.substr(first, last - first + 1);

        std::string cmd_lower = cmd;
        for (char& ch : cmd_lower) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }

        if (cmd_lower == "quit" || cmd_lower == "q" || cmd_lower == "exit") {
            std::cout << "Goodbye!\n";
            break;
        } else if (cmd_lower == "help" || cmd_lower == "h") {
            print_help();
        } else if (cmd_lower == "score" || cmd_lower == "s") {
            print_score(game);
        } else if (cmd_lower == "save") {
            if (game.save()) {
                std::cout << ">> Game saved successfully.\n";
            } else {
                std::cout << ">> Failed to save game.\n";
            }
        } else if (cmd_lower.rfind("load ", 0) == 0 || cmd_lower.rfind("l ", 0) == 0) {
            size_t space_pos = cmd.find(' ');
            std::string path = strip(cmd.substr(space_pos + 1));
            if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
                path = path.substr(1, path.size() - 2);
            }
            Game temp_game;
            if (temp_game.load_from_file(path)) {
                game = std::move(temp_game);
                std::cout << ">> Successfully loaded: " << path << "\n";
                std::cout << game.get_log() << "\n";
            } else {
                std::cout << ">> SGF loading failed: could not open or parse file.\n";
            }
        } else if (cmd_lower == "undo" || cmd_lower == "u") {
            if (game.undo()) {
                std::cout << ">> Undid last move.\n";
            } else {
                std::cout << ">> Nothing to undo!\n";
            }
        } else if (cmd_lower == "redo" || cmd_lower == "r") {
            if (game.redo()) {
                std::cout << ">> Redid move.\n";
            } else {
                std::cout << ">> Nothing to redo!\n";
            }
        } else if (cmd_lower == "pass" || cmd_lower == "p") {
            if (game.pass()) {
                std::cout << ">> " << turn_str << " passed.\n";
            } else {
                std::cout << ">> Cannot pass: game is already over.\n";
            }
        } else if (cmd_lower == "resign") {
            if (game.resign()) {
                std::cout << ">> " << turn_str << " resigned.\n";
            } else {
                std::cout << ">> Cannot resign: game is already over.\n";
            }
        } else {
            if (game.is_game_over() != EndgameStatus::NotOver) {
                std::cout << ">> Game is over! Use 'undo', 'save', 'load <file>', or 'quit'.\n";
                continue;
            }

            int16_t move_id = -1;
            if (!parse_move(cmd, game, move_id)) {
                std::cout << ">> Invalid command or coordinate! Type 'help' for usage.\n";
                continue;
            }

            MoveStatus status = game.play_move(move_id);
            switch (status) {
                case MoveStatus::Success: {
                    Point pt = toPoint(move_id, game.get_size());
                    if (total_placed < hand) {
                        std::cout << ">> Black (X) placed handicap stone " << (total_placed + 1) << "/" << hand
                                  << " at (" << pt.r << ", " << pt.c << ").\n";
                    } else {
                        std::cout << ">> " << turn_str << " placed at (" << pt.r << ", " << pt.c << ").\n";
                    }
                    break;
                }
                case MoveStatus::Occupied:
                    std::cout << ">> Illegal move: intersection is already occupied!\n";
                    break;
                case MoveStatus::Suicide:
                    std::cout << ">> Illegal move: suicide is not allowed!\n";
                    break;
                case MoveStatus::Ko:
                    std::cout << ">> Illegal move: positional superko / ko violation!\n";
                    break;
                case MoveStatus::OutOfBounds:
                    std::cout << ">> Illegal move: coordinate is out of bounds!\n";
                    break;
                case MoveStatus::GameEnded:
                    std::cout << ">> Cannot play: game is already over!\n";
                    break;
                case MoveStatus::InvalidCommand:
                    std::cout << ">> Illegal move: invalid command!\n";
                    break;
            }
        }
    }

    return 0;
}
