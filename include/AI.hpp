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
#include <fstream>
#include <unordered_map>
#include <GameLogic.hpp>

static inline std::pair<int, int> transform_offset(int i, int j, int k) {
    switch (k) {
        case 0: return { i,  j}; // Identity (0 deg)
        case 1: return { j, -i}; // Rot 90 CW
        case 2: return {-i, -j}; // Rot 180
        case 3: return {-j,  i}; // Rot 270 CW
        case 4: return { i, -j}; // Flip Horizontal
        case 5: return { j,  i}; // Flip Diagonal (Transpose)
        case 6: return {-i,  j}; // Flip Vertical
        case 7: return {-j, -i}; // Flip Anti-diagonal
        default: return {i, j};
    }
}

inline uint16_t serializer3x3_raw(const Board& board, int16_t r_center, int16_t c_center, Color color, int k = 0){
    uint16_t encoded{};
    for (int i = -1; i <= 1; ++i){
        for (int j = -1; j <= 1; ++j){
            if (i == 0 && j == 0) continue;
            auto [di, dj] = transform_offset(i, j, k);
            int nr = r_center + di;
            int nc = c_center + dj;

            State cell_state = board.get_cell(nr, nc);
            if (cell_state == State::Empty) encoded <<= 2;
            else if (cell_state == State::OutOfBounds) encoded = (encoded << 2) + 3;
            else encoded = (encoded << 2) + (*StatetoColor(cell_state) == color ? 1 : 2);
        }
    }

    return encoded;
}

inline uint16_t serializer3x3(const Board& board, int16_t r_center, int16_t c_center, Color color){
    uint16_t encoded = 0xFFFF;
    for (int i = 0; i < 8; ++i){
        encoded = std::min(encoded, serializer3x3_raw(board, r_center, c_center, color, i));
    }

    return encoded;
}

inline uint64_t serializer5x5_raw(const Board& board, int16_t r_center, int16_t c_center, Color color, int k = 0){
    uint64_t encoded{};
    for (int i = -2; i <= 2; ++i){
        for (int j = -2; j <= 2; ++j){
            if (i == 0 && j == 0) continue;
            auto [di, dj] = transform_offset(i, j, k);
            int nr = r_center + di;
            int nc = c_center + dj;

            State cell_state = board.get_cell(nr, nc);
            if (cell_state == State::Empty) encoded <<= 2;
            else if (cell_state == State::OutOfBounds) encoded = (encoded << 2) + 3;
            else encoded = (encoded << 2) + (*StatetoColor(cell_state) == color ? 1 : 2);
        }
    }

    return encoded;
}

inline uint64_t serializer5x5(const Board& board, int16_t r_center, int16_t c_center, Color color){
    uint64_t encoded = 0xFFFFFFFFFFFFFFFFULL;
    for (int i = 0; i < 8; ++i){
        encoded = std::min(encoded, serializer5x5_raw(board, r_center, c_center, color, i));
    }

    return encoded;
}

class patterns3x3{
private: 
    std::array<int32_t, 65536> freq{};

public:
    patterns3x3(const std::string &filepath){
        std::ifstream in(filepath, std::ios::binary);
        if (!in.is_open()){
            std::cerr << "File doesn't exist: " << filepath << "\n";
            return;
        }

        in.read(reinterpret_cast<char*>(freq.data()), 65536 * sizeof(int32_t));
    }

    int32_t operator[](int id) const {
        if (id < 0 || id >= 65536) return 0;
        return freq[id];
    }
};

class patterns5x5{
private: 
    std::unordered_map<uint64_t, int32_t> freq;

public:
    patterns5x5(const std::string &filepath){
        std::ifstream in(filepath, std::ios::binary);
        if (!in.is_open()){
            std::cerr << "File doesn't exist: " << filepath << "\n";
            return;
        }

        uint64_t sz = 0;
        in.read(reinterpret_cast<char*>(&sz), sizeof(sz));
        freq.reserve(sz);

        for (uint64_t i = 0; i < sz; ++i){
            uint64_t id = 0;
            in.read(reinterpret_cast<char*>(&id), sizeof(id));
            
            int32_t cnt = 0;
            in.read(reinterpret_cast<char*>(&cnt), sizeof(cnt));
            freq[id] = cnt;
        }
    }

    int32_t operator[](uint64_t id) const {
        auto val = freq.find(id);
        if (val == freq.end()) return 0;
        return val->second;
    }
};

struct MCTSNode{
    bool moves_generated = false;
    MoveType type = MoveType::DEFAULT;
    int16_t id = -1;
    Color prev_player;
    int visits = 0;
    double wins = 0.0;
    double priority = 0.0;
    MCTSNode *parent = nullptr;
    std::vector<std::unique_ptr<MCTSNode>> children;

    MCTSNode() = default;

    MCTSNode(Board &board, Color prev_p, MoveType t = MoveType::DEFAULT, int16_t move_id = -1, MCTSNode* p = nullptr);

    bool is_fully_expanded() const { return moves_generated; }
    bool is_terminal() const {
        return (type == MoveType::Resign || (parent != nullptr && parent -> type == MoveType::Pass && type == MoveType::Pass));
    }

    MCTSNode* select_best_child(double c = 5) const;
};

struct ScoredCandidate{
    int16_t id;
    double score;

    ScoredCandidate() = default;
    
    ScoredCandidate(int16_t inp_id, double s){id = inp_id; score = s;}

    const bool operator>(const ScoredCandidate &rhs) const{return score > rhs.score;}
};

class MCTSInstance{
private:
    Color AIColor;
    Board board;
    std::unique_ptr<MCTSNode> root = nullptr;
    std::vector<UndoState> undo_stack;

    int max_expansion_size;
    std::vector<ScoredCandidate> move_priority;
    

    MCTSNode* selectChild();

    double rollout(MCTSNode *node);

    void backprop(MCTSNode *node, double score);

    void rollback();

    double score_candidate_move(int16_t id, Color player);

public:
    MCTSInstance() = delete;
    MCTSInstance(const Board &src_board, Color color, MoveType PrevMoveType = MoveType::DEFAULT, int expansion_size = 20);

    int16_t get_best_move(int time_limit_ms = 1000);

    const MCTSNode* get_root() const {return root.get();}
    
    Board& get_board(){return board;}

    void advance_tree(int16_t move_id, MoveType type, Color color);
};

class MCTSThreads{
private:
    unsigned int thread_cnt;
    int total_rollouts = 0;
    int new_rollouts = 0;
    std::vector<MCTSInstance> instances;
    std::vector<std::thread> threads;
public:
    MCTSThreads() = delete;
    MCTSThreads(const Board &board, Color color, MoveType type = MoveType::DEFAULT, unsigned int requested_cnt = 0);

    void advance_tree(int16_t move_id, MoveType type, Color color);
    int16_t get_best_move(int time_limit_ms = 1000);
    int get_total_rollouts() const { return total_rollouts; }
    int get_new_rollouts() const { return new_rollouts; }
};