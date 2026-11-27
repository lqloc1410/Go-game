#include "mode/gameSession.hpp"

// AI-assisted: see AI34

namespace {
    Difficulty toParserDifficulty(AIDifficulty d) {
        switch (d) {
            case AIDifficulty::Easy:   return Difficulty::Easy;
            case AIDifficulty::Medium: return Difficulty::Medium;
            case AIDifficulty::Hard:   return Difficulty::Hard;
        }
        return Difficulty::Hard;
    }

    AIDifficulty toAIDifficulty(Difficulty d) {
        switch (d) {
            case Difficulty::Easy:   return AIDifficulty::Easy;
            case Difficulty::Medium: return AIDifficulty::Medium;
            case Difficulty::Hard:   return AIDifficulty::Hard;
        }
        return AIDifficulty::Hard;
    }
}

void GameSession::newGame(int boardSize, float komi, int handicap_cnt) {
    const bool wasAI = game_.get_is_ai();
    const Color prevAIColor = game_.get_ai_color();
    const AIDifficulty prevDiff = ai_.get_difficulty();

    game_ = Game(boardSize, komi, handicap_cnt);
    size_ = game_.get_size();
    handicap = game_.get_history().get_handicap_cnt();
    komi_ = static_cast<float>(game_.get_komi());
    game_.set_ai_config(wasAI, toParserDifficulty(prevDiff), prevAIColor);
    ai_.reset();
}

void GameSession::setAIConfig(bool isAI, AIDifficulty diff, Color aiColor) {
    game_.set_ai_config(isAI, toParserDifficulty(diff), aiColor);
    ai_.set_difficulty(diff);
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

AISearchInput GameSession::captureAIInput() const {
    return { game_.get_board(), game_.get_turn(), lastMoveType() };
}

int16_t GameSession::computeAIMove(const Board& boardCopy, Color aiColor, MoveType lastType) {
    return ai_.choose_move(boardCopy, aiColor, lastType);
}

MoveInfo GameSession::applyAIMove(int16_t move_id) {
    if (game_.is_game_over() != EndgameStatus::NotOver) return {MoveStatus::GameEnded};

    Color me = game_.get_turn();
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

MoveInfo GameSession::playAIMove() {
    if (game_.is_game_over() != EndgameStatus::NotOver) return {MoveStatus::GameEnded};
    AISearchInput in = captureAIInput();
    return applyAIMove(computeAIMove(in.board, in.color, in.lastMoveType));
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
    ai_.set_difficulty(toAIDifficulty(game_.get_ai_difficulty()));
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