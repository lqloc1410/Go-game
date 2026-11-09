#include "Game.hpp"
#include "AI.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <cctype>
#include <iomanip>
#include <cmath>
#include <cstdlib>

static void clear_screen() {
#ifdef _WIN32
    std::system("cls");
#else
    std::system("clear");
#endif
}

static void print_help() {
    std::cout << "\nCommands:\n"
              << "  <r> <c>      : Play at 0-indexed (row, col), e.g. '3 4'\n"
              << "  <Col><Row>   : Play at Go coordinate, e.g. 'D4', 'E5', 'Q16'\n"
              << "  ai [ms]      : Have MCTS AI play the current turn (default 1000 ms)\n"
              << "  pvai [w|b|off]: Auto-play AI as White (default), Black, or turn off\n"
              << "  aivai [ms]   : Watch AI play against AI until game over (or 'bvb [ms]')\n"
              << "  gtp [ms]     : Enter Go Text Protocol (GTP v2) mode (or run 'main --gtp [ms]')\n"
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

static std::string to_lower_str(std::string s) {
    for (char& ch : s) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return s;
}

static std::string id_to_gtp_vertex(int16_t id, int sz) {
    if (id == -1) return "pass";
    Point pt = toPoint(id, sz);
    char col_ch = static_cast<char>((pt.c < 8) ? ('A' + pt.c) : ('A' + pt.c + 1));
    int go_row = sz - pt.r;
    return std::string(1, col_ch) + std::to_string(go_row);
}

static bool parse_gtp_color(const std::string& token, Color& out_color) {
    std::string lower = to_lower_str(token);
    if (lower == "b" || lower == "black") {
        out_color = Color::Black;
        return true;
    }
    if (lower == "w" || lower == "white") {
        out_color = Color::White;
        return true;
    }
    return false;
}

static std::vector<Point> get_fixed_handicap_points(int sz, int count) {
    if (count < 2 || count > 9 || sz < 7) return {};
    if (sz % 2 == 0 && count > 4) return {};

    int offset = (sz >= 13) ? 3 : 2;
    int mid = sz / 2;

    // Standard GTP v2 star-point ordering (Section 4.1.1):
    // 1: D4 (bottom-left), 2: Q16 (top-right), 3: D16 (top-left), 4: Q4 (bottom-right)
    Point bl{sz - 1 - offset, offset};
    Point tr{offset, sz - 1 - offset};
    Point tl{offset, offset};
    Point br{sz - 1 - offset, sz - 1 - offset};
    Point ml{mid, offset};
    Point mr{mid, sz - 1 - offset};
    Point mt{offset, mid};
    Point mb{sz - 1 - offset, mid};
    Point cc{mid, mid};

    std::vector<Point> pts = {bl, tr, tl, br};
    if (count <= 4) {
        pts.resize(count);
        return pts;
    }
    if (count == 5) {
        pts.push_back(cc);
        return pts;
    }
    pts.push_back(ml);
    pts.push_back(mr);
    if (count == 6) return pts;
    if (count == 7) {
        pts.push_back(cc);
        return pts;
    }
    pts.push_back(mt);
    pts.push_back(mb);
    if (count == 8) return pts;
    pts.push_back(cc);
    return pts;
}

static void run_gtp(Game& game, int default_time_ms = 1000) {
    int current_board_size = game.get_size();
    double current_komi = game.get_komi();
    int ai_time_ms = default_time_ms;

    const std::vector<std::string> supported_cmds = {
        "protocol_version",
        "name",
        "version",
        "known_command",
        "list_commands",
        "quit",
        "boardsize",
        "clear_board",
        "komi",
        "fixed_handicap",
        "place_free_handicap",
        "set_free_handicap",
        "play",
        "genmove",
        "undo",
        "time_settings",
        "time_left",
        "final_score",
        "showboard"
    };

    auto reply_ok = [](const std::string& id_str, const std::string& msg = "") {
        std::cout << "=" << id_str;
        if (!msg.empty()) std::cout << " " << msg;
        std::cout << "\n\n";
        std::cout.flush();
    };

    auto reply_err = [](const std::string& id_str, const std::string& msg) {
        std::cout << "?" << id_str << " " << msg << "\n\n";
        std::cout.flush();
    };

    std::string raw_line;
    while (std::getline(std::cin, raw_line)) {
        // GTP v2 preprocessing: strip control characters except HT/LF, convert HT to space, strip comments (#)
        std::string cleaned;
        cleaned.reserve(raw_line.size());
        for (unsigned char ch : raw_line) {
            if (ch == '#') break;
            if (ch == '\t') {
                cleaned.push_back(' ');
            } else if (ch >= 32 && ch != 127) {
                cleaned.push_back(static_cast<char>(ch));
            }
        }

        cleaned = strip(cleaned);
        if (cleaned.empty()) continue;

        std::istringstream iss(cleaned);
        std::string token;
        if (!(iss >> token)) continue;

        std::string id_str;
        std::string cmd_name;

        // Check if the first token is an integer command ID
        bool is_id = !token.empty() && std::all_of(token.begin(), token.end(), [](unsigned char c) {
            return std::isdigit(c) != 0;
        });
        if (is_id) {
            id_str = token;
            if (!(iss >> cmd_name)) continue;
        } else {
            cmd_name = token;
        }

        cmd_name = to_lower_str(cmd_name);

        if (cmd_name == "protocol_version") {
            reply_ok(id_str, "2");
        } else if (cmd_name == "name") {
            reply_ok(id_str, "GoGameMCTS");
        } else if (cmd_name == "version") {
            reply_ok(id_str, "1.0");
        } else if (cmd_name == "known_command") {
            std::string query;
            iss >> query;
            query = to_lower_str(query);
            bool found = std::find(supported_cmds.begin(), supported_cmds.end(), query) != supported_cmds.end();
            reply_ok(id_str, found ? "true" : "false");
        } else if (cmd_name == "list_commands") {
            std::ostringstream oss;
            for (size_t i = 0; i < supported_cmds.size(); ++i) {
                if (i > 0) oss << "\n";
                oss << supported_cmds[i];
            }
            reply_ok(id_str, oss.str());
        } else if (cmd_name == "quit") {
            reply_ok(id_str);
            break;
        } else if (cmd_name == "boardsize") {
            std::string sz_tok;
            if (!(iss >> sz_tok)) {
                reply_err(id_str, "syntax error");
                continue;
            }
            auto maybe_sz = to_int(sz_tok);
            if (!maybe_sz.has_value() || *maybe_sz < 2 || *maybe_sz > MAX_BOARD_WIDTH) {
                reply_err(id_str, "unacceptable size");
                continue;
            }
            current_board_size = *maybe_sz;
            game = Game(current_board_size, current_komi, 0);
            reply_ok(id_str);
        } else if (cmd_name == "clear_board") {
            game = Game(current_board_size, current_komi, 0);
            reply_ok(id_str);
        } else if (cmd_name == "komi") {
            std::string km_tok;
            if (!(iss >> km_tok)) {
                reply_err(id_str, "syntax error");
                continue;
            }
            auto maybe_km = to_double(km_tok);
            if (!maybe_km.has_value()) {
                reply_err(id_str, "syntax error");
                continue;
            }
            current_komi = *maybe_km;
            game.set_komi(current_komi);
            reply_ok(id_str);
        } else if (cmd_name == "fixed_handicap" || cmd_name == "place_free_handicap") {
            std::string n_tok;
            if (!(iss >> n_tok)) {
                reply_err(id_str, "syntax error");
                continue;
            }
            auto maybe_n = to_int(n_tok);
            if (!maybe_n.has_value() || *maybe_n < 2 || *maybe_n > 9) {
                reply_err(id_str, "invalid number of stones");
                continue;
            }
            if (game.get_black_cnt() > 0 || game.get_white_cnt() > 0 || game.get_history().has_started()) {
                reply_err(id_str, "board not empty");
                continue;
            }
            auto pts = get_fixed_handicap_points(current_board_size, *maybe_n);
            if (pts.empty()) {
                reply_err(id_str, "invalid number of stones");
                continue;
            }
            game = Game(current_board_size, current_komi, *maybe_n);
            std::ostringstream oss;
            for (size_t i = 0; i < pts.size(); ++i) {
                int16_t id = game.get_board().to_id(pts[i].r, pts[i].c);
                game.place_setup(id, Color::Black);
                if (i > 0) oss << " ";
                oss << id_to_gtp_vertex(id, current_board_size);
            }
            reply_ok(id_str, oss.str());
        } else if (cmd_name == "set_free_handicap") {
            if (game.get_black_cnt() > 0 || game.get_white_cnt() > 0 || game.get_history().has_started()) {
                reply_err(id_str, "board not empty");
                continue;
            }
            std::vector<int16_t> ids;
            std::string v_tok;
            bool bad_vertex = false;
            while (iss >> v_tok) {
                int16_t id = -1;
                if (!parse_move(v_tok, game, id)) {
                    bad_vertex = true;
                    break;
                }
                ids.push_back(id);
            }
            if (bad_vertex || ids.size() < 2 || ids.size() > 9) {
                reply_err(id_str, "bad vertex list");
                continue;
            }
            game = Game(current_board_size, current_komi, static_cast<int>(ids.size()));
            bool ok = true;
            for (int16_t id : ids) {
                if (game.place_setup(id, Color::Black) != MoveStatus::Success) {
                    ok = false;
                    break;
                }
            }
            if (!ok) {
                game = Game(current_board_size, current_komi, 0);
                reply_err(id_str, "bad vertex list");
            } else {
                reply_ok(id_str);
            }
        } else if (cmd_name == "play") {
            std::string col_tok, vert_tok;
            if (!(iss >> col_tok >> vert_tok)) {
                reply_err(id_str, "syntax error");
                continue;
            }
            Color color;
            if (!parse_gtp_color(col_tok, color)) {
                reply_err(id_str, "syntax error");
                continue;
            }
            std::string vert_lower = to_lower_str(vert_tok);
            if (vert_lower == "pass") {
                if (!game.pass(color)) {
                    reply_err(id_str, "illegal move");
                } else {
                    reply_ok(id_str);
                }
            } else if (vert_lower == "resign") {
                if (!game.resign(color)) {
                    reply_err(id_str, "illegal move");
                } else {
                    reply_ok(id_str);
                }
            } else {
                int16_t id = -1;
                if (!parse_move(vert_tok, game, id)) {
                    reply_err(id_str, "syntax error");
                    continue;
                }
                MoveStatus st = game.play_move(id, color);
                if (st != MoveStatus::Success) {
                    reply_err(id_str, "illegal move");
                } else {
                    reply_ok(id_str);
                }
            }
        } else if (cmd_name == "genmove") {
            std::string col_tok;
            if (!(iss >> col_tok)) {
                reply_err(id_str, "syntax error");
                continue;
            }
            Color color;
            if (!parse_gtp_color(col_tok, color)) {
                reply_err(id_str, "syntax error");
                continue;
            }
            if (game.is_game_over() != EndgameStatus::NotOver) {
                reply_ok(id_str, "pass");
                continue;
            }
            MoveType prev_type = MoveType::DEFAULT;
            const auto& hist = game.get_history().get_history();
            if (!hist.empty()) {
                prev_type = hist.back().type;
            }
            MCTSThreads mcts(game.get_board(), color, prev_type);
            int16_t best_id = mcts.get_best_move(ai_time_ms);
            if (best_id == -1) {
                game.pass(color);
                reply_ok(id_str, "pass");
            } else {
                MoveStatus st = game.play_move(best_id, color);
                if (st == MoveStatus::Success) {
                    reply_ok(id_str, id_to_gtp_vertex(best_id, game.get_size()));
                } else {
                    game.pass(color);
                    reply_ok(id_str, "pass");
                }
            }
        } else if (cmd_name == "undo") {
            if (!game.undo()) {
                reply_err(id_str, "cannot undo");
            } else {
                reply_ok(id_str);
            }
        } else if (cmd_name == "time_settings") {
            int main_time = 0, byo_time = 0, byo_stones = 0;
            if (iss >> main_time >> byo_time >> byo_stones) {
                if (byo_time > 0 && byo_stones > 0) {
                    ai_time_ms = std::clamp((byo_time * 1000) / byo_stones, 100, 10000);
                } else if (main_time > 0) {
                    ai_time_ms = std::clamp((main_time * 1000) / 30, 100, 10000);
                }
            }
            reply_ok(id_str);
        } else if (cmd_name == "time_left") {
            reply_ok(id_str);
        } else if (cmd_name == "final_score") {
            if (game.is_game_over() == EndgameStatus::Resignation) {
                auto resigned = game.get_resigned_player();
                if (resigned.has_value()) {
                    reply_ok(id_str, (*resigned == Color::Black) ? "W+R" : "B+R");
                    continue;
                }
            }
            double score = game.calculate_score();
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1);
            if (score > 0.0) {
                oss << "W+" << score;
            } else if (score < 0.0) {
                oss << "B+" << (-score);
            } else {
                oss << "0";
            }
            reply_ok(id_str, oss.str());
        } else if (cmd_name == "showboard") {
            std::ostringstream oss;
            game.get_board().display(oss);
            std::string out = oss.str();
            while (!out.empty() && out.back() == '\n') {
                out.pop_back();
            }
            reply_ok(id_str, out);
        } else {
            reply_err(id_str, "unknown command");
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string first_arg = to_lower_str(argv[1]);
        if (first_arg == "--gtp" || first_arg == "-gtp" || first_arg == "gtp") {
            int gtp_time_ms = 1000;
            if (argc > 2) {
                auto maybe_ms = to_int(argv[2]);
                if (maybe_ms.has_value() && *maybe_ms > 0) {
                    gtp_time_ms = *maybe_ms;
                }
            }
            Game gtp_game(19, 7.5, 0);
            run_gtp(gtp_game, gtp_time_ms);
            return 0;
        }
    }

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

    if (!loaded) {
        clear_screen();
    }
    print_help();

    std::optional<Color> auto_ai_color = std::nullopt;
    bool bot_vs_bot = false;
    int ai_time_ms = 1000;

    auto play_ai_turn = [&](Color turn_color, int limit_ms) {
        const char* ai_str = (turn_color == Color::Black) ? "Black (X)" : "White (O)";
        MoveType prev_type = MoveType::DEFAULT;
        const auto& hist = game.get_history().get_history();
        if (!hist.empty()) {
            prev_type = hist.back().type;
        }

        std::cout << ">> AI [" << ai_str << "] is thinking (" << limit_ms << " ms)...\n";
        MCTSThreads mcts(game.get_board(), turn_color, prev_type);
        int16_t best_id = mcts.get_best_move(limit_ms);

        clear_screen();
        if (best_id == -1) {
            game.pass();
            std::cout << ">> AI [" << ai_str << "] passed.\n";
        } else {
            MoveStatus st = game.play_move(best_id);
            if (st == MoveStatus::Success) {
                Point pt = toPoint(best_id, game.get_size());
                char col_ch = static_cast<char>((pt.c < 8) ? ('A' + pt.c) : ('A' + pt.c + 1));
                int go_row = game.get_size() - pt.r;
                std::cout << ">> AI [" << ai_str << "] placed at (" << pt.r << ", " << pt.c
                          << ") [" << col_ch << go_row << "].\n";
            } else {
                game.pass();
                std::cout << ">> AI [" << ai_str << "] fallback passed.\n";
            }
        }
    };

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
        if (endgame == EndgameStatus::NotOver && total_placed >= hand &&
            (bot_vs_bot || (auto_ai_color.has_value() && *auto_ai_color == turn))) {
            int max_moves = game.get_size() * game.get_size() * 3;
            if (static_cast<int>(game.get_history().get_history().size()) >= max_moves) {
                clear_screen();
                std::cout << ">> Max move limit reached (" << max_moves << " moves). Forcing pass.\n";
                game.pass();
            } else {
                play_ai_turn(turn, ai_time_ms);
            }
            continue;
        }
        if (endgame != EndgameStatus::NotOver) {
            bot_vs_bot = false;
        }

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
        if (first == std::string::npos) {
            clear_screen();
            continue;
        }
        size_t last = line.find_last_not_of(" \t\r\n");
        std::string cmd = line.substr(first, last - first + 1);

        std::string cmd_lower = cmd;
        for (char& ch : cmd_lower) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }

        if (cmd_lower == "quit" || cmd_lower == "q" || cmd_lower == "exit") {
            std::cout << "Goodbye!\n";
            break;
        } else if (cmd_lower == "ai" || cmd_lower.rfind("ai ", 0) == 0) {
            if (game.is_game_over() != EndgameStatus::NotOver) {
                clear_screen();
                std::cout << ">> Cannot run AI: game is already over.\n";
                continue;
            }
            if (total_placed < hand) {
                clear_screen();
                std::cout << ">> Finish placing handicap stones first!\n";
                continue;
            }
            int limit_ms = ai_time_ms;
            if (cmd_lower.size() > 3) {
                auto maybe_ms = to_int(strip(cmd.substr(3)));
                if (maybe_ms.has_value() && *maybe_ms > 0) {
                    limit_ms = *maybe_ms;
                }
            }
            play_ai_turn(turn, limit_ms);
        } else if (cmd_lower == "aivai" || cmd_lower.rfind("aivai ", 0) == 0 ||
                   cmd_lower == "bvb"   || cmd_lower.rfind("bvb ", 0) == 0) {
            if (game.is_game_over() != EndgameStatus::NotOver) {
                clear_screen();
                std::cout << ">> Cannot run Bot-vs-Bot: game is already over.\n";
                continue;
            }
            if (total_placed < hand) {
                clear_screen();
                std::cout << ">> Finish placing handicap stones first!\n";
                continue;
            }
            size_t space_pos = cmd_lower.find(' ');
            if (space_pos != std::string::npos) {
                auto maybe_ms = to_int(strip(cmd_lower.substr(space_pos + 1)));
                if (maybe_ms.has_value() && *maybe_ms > 0) {
                    ai_time_ms = *maybe_ms;
                }
            }
            bot_vs_bot = true;
            auto_ai_color = std::nullopt;
            std::cout << ">> Bot-vs-Bot started (" << ai_time_ms << " ms per move).\n";
            play_ai_turn(turn, ai_time_ms);
        } else if (cmd_lower == "gtp" || cmd_lower.rfind("gtp ", 0) == 0) {
            if (cmd_lower.size() > 4) {
                auto maybe_ms = to_int(strip(cmd_lower.substr(4)));
                if (maybe_ms.has_value() && *maybe_ms > 0) {
                    ai_time_ms = *maybe_ms;
                }
            }
            run_gtp(game, ai_time_ms);
            break;
        } else {
            clear_screen();
            if (cmd_lower == "help" || cmd_lower == "h") {
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
                    if (auto_ai_color.has_value() && game.get_turn() == *auto_ai_color) {
                        game.undo();
                    }
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
            } else if (cmd_lower == "pvai" || cmd_lower.rfind("pvai ", 0) == 0) {
                std::string arg = (cmd_lower.size() > 5) ? strip(cmd_lower.substr(5)) : "w";
                if (arg == "off" || arg == "none" || arg == "pvp") {
                    auto_ai_color = std::nullopt;
                    std::cout << ">> PvAI disabled (PvP mode).\n";
                } else if (arg == "b" || arg == "black") {
                    auto_ai_color = Color::Black;
                    std::cout << ">> PvAI enabled: AI playing as Black (X).\n";
                } else {
                    auto_ai_color = Color::White;
                    std::cout << ">> PvAI enabled: AI playing as White (O).\n";
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
    }

    return 0;
}
