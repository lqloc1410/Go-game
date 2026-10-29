#include "mode/gameSession.hpp"

// AI-assisted: see AI34

void GameSession::newGame(int boardSize) {
    size_ = boardSize;
    board_ = std::make_unique<Board>(boardSize);
    history_ = HistoryTracker();
}

MoveInfo GameSession::play(int x, int y) {
    if (history_.is_game_over())                         return {MoveResult::GameOver};
    if (x < 0 || y < 0 || x >= size_ || y >= size_)      return {MoveResult::OutOfBoard};
    if (board_->get_cell(y, x) != State::Empty)          return {MoveResult::Occupied};

    Color me = history_.get_turn();                      // lấy lượt từ logic
    auto oppCount = [&] {
        return me == Color::Black ? board_->get_white_cnt() : board_->get_black_cnt();
    };
    int before = oppCount();

    if (!history_.play_move(*board_, board_->to_id(y, x), me))
        return {MoveResult::Invalid};                    // Ko hoặc suicide

    return {MoveResult::Ok, before - oppCount()};
}

bool GameSession::pass() { return history_.pass(history_.get_turn()); }
bool GameSession::undo() { return history_.undo_move(*board_); }
bool GameSession::redo() { return history_.redo_move(*board_); }

int GameSession::stoneAt(int x, int y) const {
    State s = board_->get_cell(y, x);
    return s == State::Black ? 1 : (s == State::White ? 2 : 0);
}
int  GameSession::currentPlayer() const { return history_.get_turn() == Color::Black ? 1 : 2; }
bool GameSession::isGameOver() const    { return history_.is_game_over(); }
int  GameSession::blackStones() const   { return board_->get_black_cnt(); }
int  GameSession::whiteStones() const   { return board_->get_white_cnt(); }
double GameSession::score(double komi) const { return board_->calculate_score(komi); }