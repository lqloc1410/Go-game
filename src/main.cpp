#include "GameLogic.hpp"
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
              << "  pass  (p)    : Pass turn (2 consecutive passes end the game)\n"
              << "  undo  (u)    : Undo the last move\n"
              << "  redo  (r)    : Redo the last undone move\n"
              << "  score (s)    : Calculate current Chinese Area Score\n"
              << "  help  (h)    : Show this help message\n"
              << "  quit  (q)    : Exit the game\n";
}

static void print_score(const Board& board, double komi) {
    double score = board.calculate_score(komi);
    std::cout << "\n================ Area Score (Komi: " << komi << ") ================\n";
    std::cout << "Stones on Board -> Black (X): " << board.get_black_cnt()
              << " | White (O): " << board.get_white_cnt() << "\n";
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

static bool parse_move(const std::string& line, const Board& board, int16_t& out_id) {
    int sz = board.get_size();
    std::istringstream iss(line);

    // 1. Try parsing two integers: "r c" (0-indexed)
    int r, c;
    if (iss >> r >> c) {
        std::string extra;
        if (!(iss >> extra)) {
            int16_t id = board.to_id(r, c);
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
        int16_t id = board.to_id(row, col);
        if (id != -1) {
            out_id = id;
            return true;
        }
    } catch (...) {
        return false;
    }

    return false;
}

int main() {
    std::cout << "==============================================================\n";
    std::cout << "                     Go Game CLI Engine                       \n";
    std::cout << "==============================================================\n";

    int board_size = 9;
    std::cout << "Enter board size [2-" << MAX_BOARD_WIDTH << "] (default 9): ";
    std::string sz_line;
    if (std::getline(std::cin, sz_line) && !sz_line.empty()) {
        try {
            int val = std::stoi(sz_line);
            if (val >= 2 && val <= MAX_BOARD_WIDTH) {
                board_size = val;
            } else {
                std::cout << "Invalid size, defaulting to 9x9.\n";
            }
        } catch (...) {
            std::cout << "Invalid input, defaulting to 9x9.\n";
        }
    }

    double komi = 7.5;
    Board board(board_size);
    HistoryTracker history;

    print_help();

    while (true) {
        board.display(std::cout);

        Color turn = history.get_turn();
        const char* turn_str = (turn == Color::Black) ? "Black (X)" : "White (O)";

        std::cout << "\nStones -> Black (X): " << board.get_black_cnt()
                  << " | White (O): " << board.get_white_cnt() << "\n";

        if (history.is_game_over()) {
            std::cout << "\n*** Game Over (Both players passed consecutively) ***\n";
            print_score(board, komi);
            std::cout << "Enter 'undo' (u) to resume or 'quit' (q) to exit: ";
        } else {
            std::cout << "[" << turn_str << "] Enter move or command: ";
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
            print_score(board, komi);
        } else if (cmd_lower == "undo" || cmd_lower == "u") {
            if (history.undo_move(board)) {
                std::cout << ">> Undid last move.\n";
            } else {
                std::cout << ">> Nothing to undo!\n";
            }
        } else if (cmd_lower == "redo" || cmd_lower == "r") {
            if (history.redo_move(board)) {
                std::cout << ">> Redid move.\n";
            } else {
                std::cout << ">> Nothing to redo!\n";
            }
        } else if (cmd_lower == "pass" || cmd_lower == "p") {
            if (history.pass(turn)) {
                std::cout << ">> " << turn_str << " passed.\n";
            } else {
                std::cout << ">> Cannot pass: game is already over.\n";
            }
        } else {
            if (history.is_game_over()) {
                std::cout << ">> Game is over! Use 'undo' or 'quit'.\n";
                continue;
            }

            int16_t move_id = -1;
            if (!parse_move(cmd, board, move_id)) {
                std::cout << ">> Invalid command or coordinate! Type 'help' for usage.\n";
                continue;
            }

            if (history.play_move(board, move_id, turn)) {
                Point pt = toPoint(move_id, board_size);
                std::cout << ">> " << turn_str << " placed at (" << pt.r << ", " << pt.c << ").\n";
            } else {
                std::cout << ">> Illegal move (occupied, suicide, or superko violation)!\n";
            }
        }
    }

    return 0;
}
