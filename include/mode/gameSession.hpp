#pragma once
#include <memory>
#include "GameLogic.hpp"
#include "Game.hpp"
#include "GameParser.hpp"
#include <optional>
#include <utility>
#include <vector>

//enum class MoveResult { Ok, Occupied, Invalid, GameOver, OutOfBoard };

struct MoveInfo {
    MoveStatus result;
    int captured{};      
};

class GameSession {
public:
    explicit GameSession(int boardSize = 19, float komi = 7.5, int handicap_cnt = 0) { newGame(boardSize, komi, handicap_cnt); }

    void newGame(int boardSize, float komi, int handicap_cnt);
    void newGame() {newGame(size_, komi_, handicap);}
    void reset() { newGame(size_, komi_, handicap); }          
    
    MoveInfo play(int x, int y);              // x = cột, y = hàng
    bool pass() {
        if (game_.pass()) {
            moves_.push_back({-1, -1, true});
            redoMoves_.clear();
        }
        return game_.pass();
    }
    bool undo() {
        if(game_.undo() && !moves_.empty()) {
            redoMoves_.push_back(moves_.back());
            moves_.pop_back();
        }
        return game_.undo(); 
    }
    bool redo() {
        if (game_.undo() && !redoMoves_.empty()) {
            moves_.push_back(redoMoves_.back());
            redoMoves_.pop_back();
        }
        return game_.redo(); 
    }
    bool resign() {return game_.resign();}

    int    size() const { return size_; }
    int    stoneAt(int x, int y) const;       // 0 trống, 1 đen, 2 trắng

    int  currentPlayer() const { return game_.get_turn() == Color::Black ? 1 : 2; } // 1 đen, 2 trắng
    bool isGameOver() const    { return game_.is_game_over() != EndgameStatus::NotOver; }
    int  blackStones() const   { return game_.get_black_cnt(); }
    int  whiteStones() const   { return game_.get_white_cnt(); }
    double score() const { return game_.calculate_score(); } // > 0: Trắng hơn, < 0: Đen hơn

    bool save() const{return game_.save();}
    bool load(const std::string& path) {
        bool status = game_.load_from_file(path); 
        if (!status) return false;
        size_ = game_.get_size();
        komi_ = static_cast<float>(game_.get_komi());
        handicap = game_.get_history().get_handicap_cnt();
        return status;
    }
    bool load() {
        bool status = game_.load_from_file(); 
        if (!status) return false;
        size_ = game_.get_size();
        komi_ = static_cast<float>(game_.get_komi());
        handicap = game_.get_history().get_handicap_cnt();
        return status;
    }

    std::optional<std::pair<int, int>> lastMove() const {
        if (moves_.empty() || moves_.back().pass) return std::nullopt;
        return std::make_pair(moves_.back().x, moves_.back().y);
    }

    EndgameStatus endgameStatus() const {return game_.is_game_over();}
    std::optional<Color> resignedPlayer() const {return game_.get_resigned_player();}


private:
    int size_{};
    float komi_{};
    int handicap{};
    Game game_;
    struct Move {
        int x, y;
        bool pass;
    };
    std::vector<Move> moves_, redoMoves_;
};