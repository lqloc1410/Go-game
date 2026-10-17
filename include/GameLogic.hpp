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

    Point(int row, int col){
        r = row; c = col;
    }
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

    Hash(uint64_t high, uint64_t low) : h1(high), h2(low) {}

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
    std::array<int, CELL_CNT> parent{};
    std::array<int, CELL_CNT> cmp_size{};

    /*
        Fixed-size bitset array for checking liberties. 

        Used bitset because it has native bitwise operations + 
        can process multiple data points at once.
    */
    std::array<std::bitset<CELL_CNT>, CELL_CNT> liberties{};
    
    // Forms an implicit linked list for easy stone removal.
    std::array<int, CELL_CNT> next_stone{};
    
public:
    DSU() = default;
    DSU(int sz);
    
    // Standard DSU stuff
    int get(int u);
    bool join(int u, int v);

    // Helper operations for liberties
    int count_liberty(int u);
    void add_liberty(int u, int lib_cell);
    void remove_liberty(int u, int lib_cell);

    // Function to clear group DSU & find removed stones.
    // This function is AI generated. See AI01
    std::vector<int16_t> clear_group(int u);
};

struct UndoState{
    // Storing past moves for undos.
    bool is_pass{};
    Color color;
    int16_t id;
    std::vector<int16_t> captured;
    DSU liberty_tracker;
    Hash prev_hash;
    UndoState() = default;

    UndoState(bool pass, Color c);
};

struct RedoState{
    // Storing past moves for redos.
    bool is_pass{};
    int16_t id{};
    Color color{};

    RedoState() = default;
    RedoState(int16_t inp_id, Color inp_color);

    RedoState(bool pass, Color c);
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

    // Helper for removing groups. This function is partly AI generated. See AI01
    bool remove_group(int id, std::array<State, CELL_CNT>& b, DSU& dsu, std::vector<int16_t>& captured);

public:
    Board(int sz);

    std::optional<UndoState> place(int16_t curr_id, Color color);

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
    bool is_game_over() const;

    Color get_turn() const;

    bool play_move(Board &board, int16_t id, Color color);

    bool undo_move(Board &board);

    bool redo_move(Board &board);

    bool pass(Color color);
};