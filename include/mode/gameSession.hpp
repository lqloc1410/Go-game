#pragma once
#include <memory>
#include "GameLogic.hpp"
#include "Game.hpp"
#include "GameParser.hpp"
#include "AI.hpp"
#include <optional>
#include <utility>

//enum class MoveResult { Ok, Occupied, Invalid, GameOver, OutOfBoard };

enum class Phase { Playing, Marking, Finished };

struct MoveInfo {
    MoveStatus result;
    int captured{};
    int x{-1};
    int y{-1};
    bool is_pass{false};
};


class GameSession {
public:
    explicit GameSession(int boardSize = 19, float komi = 7.5, int handicap_cnt = 0) { newGame(boardSize, komi, handicap_cnt); }

    void newGame(int boardSize, float komi, int handicap_cnt);
    void newGame() {newGame(size_, komi_, handicap);}
    void reset() { newGame(size_, komi_, handicap); }          
    
    MoveInfo play(int x, int y);              // x = cột, y = hàng
    MoveInfo playAIMove();
    void setAIDifficulty(AIDifficulty diff) { ai_.set_difficulty(diff); }
    AIDifficulty aiDifficulty() const { return ai_.get_difficulty(); }

    bool pass();
    bool undo();
    bool redo();
    bool resign();

    int    size() const { return size_; }
    int    stoneAt(int x, int y) const;       // 0 trống, 1 đen, 2 trắng

    int  currentPlayer() const { return game_.get_turn() == Color::Black ? 1 : 2; } // 1 đen, 2 trắng
    bool isGameOver() const    { return game_.is_game_over() != EndgameStatus::NotOver; }
    int  blackStones() const   { return game_.get_black_cnt(); }
    int  whiteStones() const   { return game_.get_white_cnt(); }
    double score() const { return game_.calculate_score(); } // > 0: Trắng hơn, < 0: Đen hơn
    bool toggleDeadGroup(int x, int y) { return game_.toggle_dead_group(y, x); }
    bool isDeadStone(int x, int y) const { return game_.is_dead_stone(y, x); }
    bool hasDeadStones() const { return game_.has_dead_stones(); }
    void clearDeadStones() { game_.clear_dead_stones(); }
    void autoMarkDeadStones(int rollout_cnt = 300) { game_.auto_mark_dead_stones(rollout_cnt); }

    bool save() const{return game_.save();}
    bool save(const std::string& path) const { return game_.save(path); }
    bool load(const std::string& path);
    bool load();

    EndgameStatus endgameStatus() const {return game_.is_game_over();}
    
    std::optional<std::pair<int, int>> lastMove() const {
        const auto& h = game_.get_history().get_history();
        if (h.empty()) return std::nullopt;
        const UndoState& u = h.back();
        if (u.type != MoveType::Place || u.id < 0 || size_ <= 0) return std::nullopt;
        return std::make_pair(u.id % size_, u.id / size_);
    }

    Phase phase() const {
        switch (game_.is_game_over()) {
            case EndgameStatus::NotOver: return Phase::Playing;
            case EndgameStatus::Resignation: return Phase::Finished;
            default: return scoreConfirmed_ ? Phase::Finished : Phase::Marking;
        }
    }
    bool confirmScore() {
        if (phase() != Phase::Marking) return false;
        scoreConfirmed_ = true;
        return true;
    }
    
    bool resumePlay() {
        if (phase() != Phase::Marking) return false;
        clearDeadStones();
        scoreConfirmed_ = false;
        return game_.undo();
    }
    void syncPhase() {
        if (!isGameOver()) scoreConfirmed_ = false;
    }
    std::optional<Color> resignedPlayer() const {return game_.get_resigned_player();}
private:
    MoveType lastMoveType() const;
    bool finishLoad(bool status);

    int size_{};
    float komi_{};
    int handicap{};
    Game game_;
    AIEngine ai_;

    bool scoreConfirmed_ = false;
};