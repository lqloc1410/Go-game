#pragma once
#include <memory>
#include "GameLogic.hpp"
#include "Game.hpp"
#include "GameParser.hpp"
#include "AI.hpp"

//enum class MoveResult { Ok, Occupied, Invalid, GameOver, OutOfBoard };

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

    bool save() const{return game_.save();}
    bool save(const std::string& path) const { return game_.save(path); }
    bool load(const std::string& path);
    bool load();

    EndgameStatus endgameStatus() const {return game_.is_game_over();}
    std::optional<Color> resignedPlayer() const {return game_.get_resigned_player();}
private:
    MoveType lastMoveType() const;
    bool finishLoad(bool status);

    int size_{};
    float komi_{};
    int handicap{};
    Game game_;
    AIEngine ai_;
};