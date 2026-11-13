#include "mode/gameSession.hpp"

// AI-assisted: see AI34

void GameSession::newGame(int boardSize, float komi, int handicap_cnt) {
    game_ = Game(boardSize, komi, handicap_cnt);
    size_ = game_.get_size();
    handicap = game_.get_history().get_handicap_cnt();
    komi_ = static_cast<float>(game_.get_komi());
    moves_.clear();
    redoMoves_.clear();
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
    moves_.push_back({x, y, false});
    redoMoves_.clear();

    return {stat, before - oppCount()};
}

int GameSession::stoneAt(int x, int y) const {
    State s = game_.get_cell(y, x);
    return s == State::Black ? 1 : (s == State::White ? 2 : 0);
}