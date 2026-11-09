#pragma once
#include <vector>
#include <cstdint>
#include <random>
#include <queue>
#include <chrono>
#include <random>
#include <chrono>
#include <memory>
#include <cmath>
#include <thread>
#include <GameLogic.hpp>
struct MCTSNode{
    bool moves_generated = false;
    MoveType type = MoveType::DEFAULT;
    int16_t id = -1;
    Color prev_player;
    int visits = 0;
    double wins = 0.0;
    MCTSNode *parent = nullptr;
    std::vector<std::unique_ptr<MCTSNode>> children;
    std::vector<int16_t> untried_moves;

    MCTSNode() = default;

    MCTSNode(Board &board, Color prev_p, MoveType t = MoveType::DEFAULT, int16_t move_id = -1, MCTSNode* p = nullptr);

    bool is_fully_expanded() const { return moves_generated && untried_moves.empty(); }
    bool is_terminal() const {
        return (type == MoveType::Resign || (parent != nullptr && parent -> type == MoveType::Pass && type == MoveType::Pass));
    }

    MCTSNode* select_best_child(double c = 1.414) const;
};

class MCTSInstance{
private:
    Color AIColor;
    Board board;
    std::unique_ptr<MCTSNode> root = nullptr;
    std::vector<UndoState> undo_stack;

    MCTSNode* selectChild();

    Color rollout(MCTSNode *node);

    void backprop(MCTSNode *node, Color winner);

    void rollback();

public:
    MCTSInstance() = delete;
    MCTSInstance(const Board &src_board, Color color, MoveType PrevMoveType = MoveType::DEFAULT);

    int16_t get_best_move(int time_limit_ms = 1000);

    const MCTSNode* get_root() const {return root.get();}
    
    Board& get_board(){return board;}
};

class MCTSThreads{
private:
    unsigned int thread_cnt;
    std::vector<MCTSInstance> instances;
    std::vector<std::thread> threads;
public:
    MCTSThreads() = delete;
    MCTSThreads(const Board &board, Color color, MoveType type = MoveType::DEFAULT);

    int16_t get_best_move(int time_limit_ms = 1000);
};