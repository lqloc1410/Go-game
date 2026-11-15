#include "mode/gameSession.hpp"

// AI-assisted: see AI34

void GameSession::newGame(int boardSize, float komi, int handicap_cnt) {
    game_ = Game(boardSize, komi, handicap_cnt);
    size_ = game_.get_size();
    handicap = game_.get_history().get_handicap_cnt();
    komi_ = static_cast<float>(game_.get_komi());
    ai_.reset();
}

MoveType GameSession::lastMoveType() const {
    const auto& hist = game_.get_history().get_history();
    return hist.empty() ? MoveType::DEFAULT : hist.back().type;
}

MoveInfo GameSession::play(int x, int y) {
    if (game_.is_game_over() != EndgameStatus::NotOver) return {MoveStatus::GameEnded};
    if (x < 0 || y < 0 || x >= size_ || y >= size_)      return {MoveStatus::OutOfBounds};
    if (game_.get_cell(y, x) != State::Empty)          return {MoveStatus::Occupied};

    Color me = game_.get_turn();                      // lấy lượt từ logic
    auto oppCount = [&] {
        return me == Color::Black ? game_.get_white_cnt() : game_.get_black_cnt();
    };
    int before = oppCount();

    MoveStatus stat = game_.play_move(y, x);
    if (stat != MoveStatus::Success) return {stat}; 

    int16_t move_id = game_.get_board().to_id(y, x);
    ai_.sync_move(game_.get_board(), move_id, MoveType::Place, me);
    return {stat, before - oppCount(), x, y, false};
}

MoveInfo GameSession::playAIMove() {
    if (game_.is_game_over() != EndgameStatus::NotOver) return {MoveStatus::GameEnded};

    Color me = game_.get_turn();
    int16_t move_id = ai_.choose_move(game_.get_board(), me, lastMoveType());
    if (move_id == -1) {
        if (!game_.pass()) return {MoveStatus::GameEnded};
        ai_.sync_move(game_.get_board(), -1, MoveType::Pass, me);
        return {MoveStatus::Success, 0, -1, -1, true};
    }

    auto oppCount = [&] {
        return me == Color::Black ? game_.get_white_cnt() : game_.get_black_cnt();
    };
    int before = oppCount();

    MoveStatus stat = game_.play_move(move_id);
    if (stat != MoveStatus::Success) return {stat};

    ai_.sync_move(game_.get_board(), move_id, MoveType::Place, me);
    Point p = toPoint(move_id, size_);
    return {stat, before - oppCount(), p.c, p.r, false};
}

bool GameSession::pass() {
    Color me = game_.get_turn();
    if (!game_.pass()) return false;
    ai_.sync_move(game_.get_board(), -1, MoveType::Pass, me);
    return true;
}

bool GameSession::undo() {
    if (!game_.undo()) return false;
    ai_.reset();
    return true;
}

bool GameSession::redo() {
    if (!game_.redo()) return false;
    ai_.reset();
    return true;
}

bool GameSession::resign() {
    if (!game_.resign()) return false;
    ai_.reset();
    return true;
}

bool GameSession::finishLoad(bool status) {
    if (!status) return false;
    size_ = game_.get_size();
    komi_ = static_cast<float>(game_.get_komi());
    handicap = game_.get_history().get_handicap_cnt();
    ai_.reset();
    return true;
}

bool GameSession::load(const std::string& path) {
    return finishLoad(game_.load_from_file(path));
}

bool GameSession::load() {
    return finishLoad(game_.load_from_file());
}

int GameSession::stoneAt(int x, int y) const {
    State s = game_.get_cell(y, x);
    return s == State::Black ? 1 : (s == State::White ? 2 : 0);
}