#pragma once
#include <memory>
#include "GameLogic.hpp"

enum class MoveResult { Ok, Occupied, Invalid, GameOver, OutOfBoard };

struct MoveInfo {
    MoveResult result;
    int captured = 0;      
};

class GameSession {
public:
    explicit GameSession(int boardSize = 19) { newGame(boardSize); }

    void newGame(int boardSize);
    void reset() { newGame(size_); }          
    
    MoveInfo play(int x, int y);              // x = cột, y = hàng
    bool pass();
    bool undo();
    bool redo();

    int    size() const { return size_; }
    int    stoneAt(int x, int y) const;       // 0 trống, 1 đen, 2 trắng
    int    currentPlayer() const;             // 1 đen, 2 trắng
    bool   isGameOver() const;
    int    blackStones() const;
    int    whiteStones() const;
    double score(double komi = 7.5) const;    // > 0: Trắng hơn, < 0: Đen hơn

private:
    int size_ = 9;
    std::unique_ptr<Board> board_;
    HistoryTracker history_;
};