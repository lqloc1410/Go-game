#pragma once
#include <vector>
#include <iostream>
#include <cstdint>
#include <array>
#include <bitset>
#include <algorithm>
#include <stdexcept>
#include <set>
#include <optional>
#include <random>
#include <utility>
#include <queue>

// Because Go can have a 19x19 board, we set max size here.
const int MAX_BOARD_WIDTH = 19;
const int CELL_CNT = MAX_BOARD_WIDTH * MAX_BOARD_WIDTH;

// For easy grid traversals;
const int dr[] = {1, -1, 0, 0};
const int dc[] = {0, 0, 1, -1};

/*
    Defines enums for board states and player colors. 
    Improves type safety and prevents malformed inputs
*/
enum class State : int8_t{
    Black = -1,
    White = 1,
    Empty = 0,
};

enum class Color : int8_t{
    Black = -1,
    White = 1,
};

std::optional<Color> StatetoColor(State state);
State ColortoState(Color color);
Color invertColor(Color color);

// idk why tf this is here but keep it for later
struct Point{
    int r = 0, c = 0;
    Point() = default;

    Point(int row, int col);
};
Point toPoint(int id, int sz);

// Defines classes for Zorbist hashing for Superko (currently positional)
class Hash{
private:
    /*
        This hash implementation uses double hashing.

        The probability of a collision of N positions is approximately N^2/M,
        where M is the maximum possible hash value.

        Using 2 64-bit hashes is equivalent to a 128-bit hash, 
        which will make collision rates insignificant.
    */
    uint64_t h1{};
    uint64_t h2{};

    // Keeping seed fixed for now to ensure consistent generation across games.
    // idk if I need to randomize seed. 
    static inline std::mt19937_64 rng{250208141008};
public:
    Hash() = default;    

    Hash(uint64_t high, uint64_t low); 

    // Generates random hash for the LUT
    static Hash random();

    // Hash combine operators
    Hash& operator^=(const Hash& rhs);
    Hash operator^(const Hash& rhs) const;

    // Spaceship operator to allow for all types of comparisons
    // Comparing hashes by value is meaningless but for now it allows std::set usage.
    auto operator<=>(const Hash& rhs) const = default;
};

class HashLUT{
private:
    /*
        Generates static lookup tables for hashing and will not change throughout runtime.
        WILL NOT CHANGE WHEN MULTIPLE BOARDS ARE USED.
    */

    static inline auto LUT_black = []() {
        std::array<Hash, CELL_CNT> arr;
        for (auto& h : arr) h = Hash::random();
        return arr;
    }();
    static inline auto LUT_white = []() {
        std::array<Hash, CELL_CNT> arr;
        for (auto& h : arr) h = Hash::random();
        return arr;
    }();
public:
    HashLUT() = delete; 

    static const Hash& getHash(int id, Color color);
};

// DSU for easy group joining & finding liberties
class DSU{
private:
    //Standard DSU stuff
    int board_size{};
    std::array<int16_t, CELL_CNT> parent{};
    std::array<int16_t, CELL_CNT> cmp_size{};

    /*
        Fixed-size bitset array for checking liberties. 

        Used bitset because it has native bitwise operations + 
        can process multiple data points at once.
    */
    std::array<std::bitset<CELL_CNT>, CELL_CNT> liberties{};
    
    // Forms an implicit linked list for easy stone removal.
    std::array<int16_t, CELL_CNT> next_stone{};
    

    // Storage for easy rollbacks
    enum class OpType : uint8_t{Reset, Join, BitFlip};

    struct OpRecord {
        OpType type;
        int16_t u;      // Reset: node u | Join: ru | BitFlip: ru
        int16_t v;      // Reset: old_parent | Join: rv | BitFlip: lib_cell
        int16_t extra1; // Reset: old_cmp_size
        int16_t extra2; // Reset: old_next_stone
    };

    std::vector<OpRecord> op_history;
    std::vector<std::bitset<CELL_CNT>> bitset_history; // Only pushed on Reset & Join

    
public:
    struct Checkpoint { size_t op_idx; size_t bitset_idx; };
    Checkpoint checkpoint() const;
    DSU() = default;
    DSU(int sz);
    
    // Standard DSU stuff
    int get(int16_t u);
    bool join(int16_t u, int16_t v);

    // Helper operations for liberties
    int count_liberty(int16_t u);
    void add_liberty(int16_t u, int16_t lib_cell);
    void remove_liberty(int16_t u, int16_t lib_cell);

    // Function to clear a connected group containing u
    void clear_group(int u);

    // Function to get a connected group.
    std::vector<int16_t> get_group(int u);

    // CAUTION: Only safe to call on an isolated empty cell, 
    // or when looping over ALL members of an entire captured group.
    // Calling this on a single stone inside an active group will break everything!
    void reset(int16_t u);
    void rollback(Checkpoint cp);
};

enum class MoveType : int8_t{Place, Pass, Resign, DEFAULT};

enum class MoveStatus : int8_t{Success, Suicide, Ko, Occupied, OutOfBounds, GameEnded};

enum class EndgameStatus : int8_t{Pass, Resignation, NotOver};

struct UndoState{
    // Storing past moves for undos.
    MoveType type = MoveType::DEFAULT;
    Color color;
    int16_t id{-1};
    std::vector<int16_t> captured;
    DSU::Checkpoint dsu_cp;

    Hash prev_hash;
    UndoState() = default;

    UndoState(MoveType t, Color c);
};

struct MoveState{
    MoveStatus status = MoveStatus::Success;
    std::optional<UndoState> undo_state = std::nullopt;
};

struct RedoState{
    // Storing past moves for redos.
    MoveType type = MoveType::DEFAULT;
    int16_t id{};
    Color color{};

    RedoState() = default;
    RedoState(int16_t inp_id, Color inp_color, MoveType inp_type);
};

class Board{
private:
    int board_size{};
    int cell_cnt{};

    // Counting stones for scoring.
    int white_cnt{};
    int black_cnt{};

    // implemeted flat array for better cache locality.
    std::array<State, CELL_CNT> board{};

    // Structures to implement Zorbist Hashing.
    Hash curr_hash;
    std::set<Hash> seen_hashes;

    // DSU for liberty tracking
    DSU liberty_tracker;

    // Reusable stack & visited array for DFS
    mutable std::vector<int> st;
    mutable std::array<bool, CELL_CNT> visited{};

    // Helper to get cell id from (r, c)
    int get(int r, int c) const;

    // Helper for territory counting
    int score_cnt_helper(int16_t id) const;

    // Helper function to check if a move is suicide. 
    // This function also collects data of empty, friendly and opponent cells 
    bool is_suicide(int16_t id, Color color, std::vector<int16_t> &empty, std::vector<int16_t> &friendly, std::vector<int16_t> &opponent);
    // Helper to check if a placement is suicide.
    // Also collects opponent roots to check hash.
    bool is_suicide(int16_t id, Color color, std::vector<int16_t> &opponent);

public:
    Board(int sz);

    bool is_legal(int16_t curr_id, Color color);

    std::vector<int16_t> get_legal_moves(Color color);

    MoveState place(int16_t curr_id, Color color);

    void undo(const UndoState &state);

    int get_white_cnt() const;

    int get_black_cnt() const;

    int get_size() const;

    int16_t to_id(int r, int c) const;

    State get_cell(int r, int c) const;

    void display(std::ostream& os = std::cout) const;

    double calculate_score(double komi = 7.5) const;
};

class HistoryTracker{
private:
    std::vector<UndoState> undo_stack;
    std::vector<RedoState> redo_stack;
public:
    EndgameStatus is_game_over() const;

    Color get_turn() const;

    MoveStatus play_move(Board &board, int16_t id, Color color);

    bool undo_move(Board &board);

    bool redo_move(Board &board);

    bool pass(Color color);

    bool resign(Color color);

    std::optional<Color> get_resigned_player();
};