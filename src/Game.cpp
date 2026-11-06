#include <GameLogic.hpp>
#include <GameParser.hpp>
#include <Game.hpp>
namespace fs = std::filesystem;

std::string Game::to_sgf() const{
    std::ostringstream ss;

    ss << "(;FF[4]GM[1]";
    ss << "RU[" << log.get_ruleset_str() << "]";
    ss << "SZ[" << board.get_size() << "]";
    std::string pb = log.get_player_black().empty() ? "Black" : escape_sgf_text(log.get_player_black());
    std::string pw = log.get_player_white().empty() ? "White" : escape_sgf_text(log.get_player_white());
    ss << "PB[" << pb << "]PW[" << pw << "]";
    ss << "KM[" << board.get_komi() << "]";

    if (history.get_handicap_cnt() > 1){
        ss << "HA[" << (history.get_handicap_cnt()) << "]";
    }

    if (!history.get_added_black().empty()){
        ss << "AB";
        for (const Point &p: history.get_added_black()){
            ss << '[' << point_to_sgf(p) << ']';
        }
    }

    if (!history.get_added_white().empty()){
        ss << "AW";
        for (const Point &p: history.get_added_white()){
            ss << '[' << point_to_sgf(p) << ']';
        }
    }

    if (history.is_game_over() == EndgameStatus::Resignation) {
        auto resigned = history.get_resigned_player();
        if (resigned) {
            ss << "RE[" << (*resigned == Color::Black ? "W+R" : "B+R") << "]";
        }
    } else if (history.is_game_over() == EndgameStatus::Pass) {
        double score = board.calculate_score();
        if (score > 0) ss << "RE[W+" << score << "]";
        else if (score < 0) ss << "RE[B+" << -score << "]";
        else ss << "RE[0]";
    } else if (!log.get_result_str().empty() && log.get_result_str() != "?") {
        ss << "RE[" << log.get_result_str() << "]";
    }
    
    ss << "PL[" << (history.get_start_turn() == Color::Black ? "B" : "W") << "]";

    ss << "\n";

    for (const UndoState &move: history.get_history()){
        if (move.type == MoveType::Setup || move.type == MoveType::DEFAULT) continue;
        else if (move.type == MoveType::Resign) break;
        else{
            ss << ';';
            ss << (move.color == Color::Black ? "B" : "W");
            if (move.type == MoveType::Place) ss << '[' << point_to_sgf(toPoint(move.id, board.get_size())) << ']';
            else ss << "[]";
        }ss << '\n';
    }

    ss << ")";

    return ss.str();

}

Game::Game(int board_size, double komi, int handicap){
    if (board_size < 2 || board_size > MAX_BOARD_WIDTH) throw std::invalid_argument("Board size must be between 2 and MAX_BOARD_WIDTH");
    else if (handicap < 0 || handicap == 1) throw std::invalid_argument("Invalid handicap count.");
    board = Board(board_size, komi);
    Color start_turn = (handicap >= 2) ? Color::White : Color::Black;
    history = HistoryTracker(handicap, start_turn);
}

bool Game::load_from_file(const std::string &filepath){
    GameLog log_tmp;
    std::optional<std::string> raw_sgf = read_file_to_string(filepath);
    if (!raw_sgf) return false;
    if (!log_tmp.parse(*raw_sgf)) return false;
    if (!log_tmp.load_board_data(board, history)) return false;

    log = std::move(log_tmp);
    has_data = true;
    return true;
}

MoveStatus Game::play_move(int16_t id) {
    if (is_game_over() != EndgameStatus::NotOver) return MoveStatus::GameEnded;
    return history.play_move(board, id, history.get_turn());
}

bool Game::save() const {
    try {
        std::error_code ec;
        fs::create_directories(save_folder, ec);
        if (ec) return false;

        fs::path target_path = save_folder / save_file;
        fs::path temp_path = save_folder / tmp_file;

        std::ofstream out(temp_path);
        if (!out.is_open()) return false;

        out << to_sgf();
        out.close();
        if (!out.good()) return false;

        // Atomically replace target file
        fs::rename(temp_path, target_path, ec);
        if (ec) return false;

        return true;
    } catch (...) {
        return false;
    }
}