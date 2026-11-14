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
#include <string>
#include <random>
#include <chrono>
// Because Go can have a 19x19 board, we set max size here.
const int MAX_BOARD_WIDTH = 19;
const int CELL_CNT = MAX_BOARD_WIDTH * MAX_BOARD_WIDTH;

// For easy grid traversals;
const int dr[] = {1, -1, 0, 0};
const int dc[] = {0, 0, 1, -1};
const int drd[] = {1, 1, -1, -1};
const int dcd[] = {-1, 1, -1, 1};

/*
    Defines enums for board states and player colors. 
    Improves type safety and prevents malformed inputs
*/
enum class State : int8_t{
    Black = 1,
    White = 2,
    Empty = 0,
    OutOfBounds = 3
};

enum class Color : int8_t{
    Black = -1,
    White = 1,
};

std::optional<Color> StatetoColor(State state);
State ColortoState(Color color);
Color invertColor(Color color);

// AI-assisted: see AI03
// idk why tf this is here but keep it for later
struct Point{
    int r = 0, c = 0;
    Point() = default;

    Point(int row, int col);
};


// AI-assisted: see AI11
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
    
    // AI-assisted: see AI02
    // Forms an implicit linked list for easy stone removal.
    std::array<int16_t, CELL_CNT> next_stone{};
    

    // AI-assisted: see AI20
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
    // AI-assisted: see AI20
    struct Checkpoint { size_t op_idx; size_t bitset_idx; };
    Checkpoint checkpoint() const;
    DSU() = default;
    DSU(int sz);
    
    // Standard DSU stuff
    const int get(int16_t u) const;
    bool join(int16_t u, int16_t v);

    // Helper operations for liberties
    const int count_liberty(int16_t u) const;
    void add_liberty(int16_t u, int16_t lib_cell);
    void remove_liberty(int16_t u, int16_t lib_cell);

    // AI-assisted: see AI09
    // Function to clear a connected group containing u
    void clear_group(int u);

    // AI-assisted: see AI10
    // Function to get a connected group.
    const std::vector<int16_t> get_group(int u) const;

    // AI-assisted: see AI10, AI19, AI20
    // CAUTION: Only safe to call on an isolated empty cell, 
    // or when looping over ALL members of an entire captured group.
    // Calling this on a single stone inside an active group will break everything!
    void reset(int16_t u);
    // AI-assisted: see AI20
    void rollback(Checkpoint cp);

    const int16_t get_single_liberty(int16_t u) const;

    const int get_group_size(int16_t u) const {return cmp_size[get(u)];}

    const std::bitset<CELL_CNT>& get_liberties(int16_t u) const { return liberties[get(u)]; }

    int16_t get_next_stone(int16_t u) const { return next_stone[u]; }
};

enum class MoveType : int8_t{Place, Pass, Resign, Setup, DEFAULT};

enum class MoveStatus : int8_t{Success, Suicide, Ko, Occupied, OutOfBounds, GameEnded, InvalidCommand};
inline std::string MStatus_to_str(MoveStatus status){
    switch (status){
        case MoveStatus::Success: return "Success";
        case MoveStatus::Suicide: return "Suicide";
        case MoveStatus::Ko: return "Ko/Superko violation";
        case MoveStatus::Occupied: return "Cell already occupied";
        case MoveStatus::GameEnded: return "Game already ended";
        case MoveStatus::OutOfBounds: return "Out of bounds";
        case MoveStatus::InvalidCommand: return "Invalid command";
        default: return "Unknown error";
    }
}

enum class EndgameStatus : int8_t{Pass, Resignation, NotOver};

// AI-assisted: see AI05, AI19
struct UndoState{
    // Storing past moves for undos.
    MoveType type = MoveType::DEFAULT;
    Color color = Color::Black;
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

// AI-assisted: see AI26
struct RedoState{
    // Storing past moves for redos.
    MoveType type = MoveType::DEFAULT;
    int16_t id{};
    Color color{};

    RedoState() = default;
    RedoState(int16_t inp_id, Color inp_color, MoveType inp_type);
};

// Fixed-capacity 4-element stack buffer to avoid heap allocations for orthogonal neighbors
struct SmallVec4 {
    int16_t data[4]{};
    int sz = 0;

    void clear() { sz = 0; }
    void push_back(int16_t val) { data[sz++] = val; }
    int16_t* begin() { return data; }
    int16_t* end() { return data + sz; }
    const int16_t* begin() const { return data; }
    const int16_t* end() const { return data + sz; }
    int find(int16_t val){
        for (int i = 0; i < sz; ++i){
            if (data[i] == val) return i;
        }

        return -1;
    }
};

class Board{
private:
    int board_size{};
    int cell_cnt{};

    // AI-assisted: see AI08
    // Counting stones for scoring.
    int white_cnt{};
    int black_cnt{};

    // Komi tracker
    double komi = 7.5;

    // implemeted flat array for better cache locality.
    std::array<State, CELL_CNT> board{};

    // Structures to implement Zorbist Hashing.
    Hash curr_hash;
    std::vector<Hash> seen_hashes;

    bool has_seen_hash(const Hash &h) const {
        return std::find(seen_hashes.rbegin(), seen_hashes.rend(), h) != seen_hashes.rend();
    }

    // DSU for liberty tracking
    DSU liberty_tracker;

    // Reusable stack & visited array for DFS
    mutable std::vector<int> st;
    mutable std::array<bool, CELL_CNT> visited{};

    // AI-assisted: see AI08
    // Helper for territory counting
    int score_cnt_helper(int16_t id) const;

    // AI-assisted: see AI04, AI13, AI16, AI18
    // Helper function to check if a move is suicide. 
    // This function also collects data of empty, friendly and opponent cells 
    bool is_suicide(int16_t id, Color color, SmallVec4 &empty, SmallVec4 &friendly, SmallVec4 &opponent);
    // AI-assisted: see AI18, AI19, AI21
    // Helper to check if a placement is suicide.
    // Also collects opponent roots to check hash.
    bool is_suicide(int16_t id, Color color, SmallVec4 &opponent);

    

    //std::random_device rd;

    std::mt19937 gen;

    std::uniform_int_distribution<int> get_rand;

    std::array<int16_t, CELL_CNT> empty_cells{};
    std::array<int16_t, CELL_CNT> empty_idx{};
    int empty_cnt = 0;

    inline void remove_empty(int16_t id) {
        int16_t idx = empty_idx[id];                 // 1. Where is curr_id located?
        int16_t last_id = empty_cells[--empty_cnt];  // 2. Decrement count & grab the last cell
        empty_cells[idx] = last_id;                  // 3. Move the last cell into the hole
        empty_idx[last_id] = idx;                    // 4. Update the moved cell's index
    }

    inline void add_empty(int16_t id){
        empty_cells[empty_cnt] = id;
        empty_idx[id] = static_cast<int16_t>(empty_cnt);
        empty_cnt++;
    }
public:
    void seed_gen(uint64_t s) { gen.seed(s); }

    inline uint32_t rand_below(uint32_t bound) {
        return static_cast<uint32_t>((static_cast<uint64_t>(gen()) * bound) >> 32);
    }

    inline uint64_t rand(){
        return static_cast<uint64_t>(gen());
    }
    // AI-assisted: see AI03
    Board(int sz = 19, double km = 7.5);

    // Helper to get cell id from Point
    int16_t get(Point point) const;

    // Helper to get cell id from r, c
    inline int16_t get(int r, int c) const { return static_cast<int16_t>(r * board_size + c); }

    // AI-assisted: see AI17
    bool is_legal(int16_t curr_id, Color color);

    // AI-assisted: see AI17
    std::vector<int16_t> get_legal_moves(Color color);

    bool is_eye(int16_t id, Color color);

    inline int get_empty_cnt() const { return empty_cnt; }
    inline int16_t get_empty_cell(int idx) const { return empty_cells[idx]; }
    inline void copy_empty_cells(int16_t* dst) const {
        std::copy_n(empty_cells.data(), empty_cnt, dst);
    }

    void getReasonablePts(Color color, std::vector<int16_t> &out);

    // AI-assisted: see AI02, AI03, AI04, AI05, AI08, AI19
    MoveState place(int16_t curr_id, Color color);

    MoveState place(Point point, Color color);

    // AI-assisted: see AI05, AI08, AI20, AI23
    void undo(const UndoState &state);

    inline int get_white_cnt() const { return white_cnt; }

    inline int get_black_cnt() const { return black_cnt; }

    inline int get_size() const { return board_size; }

    double get_komi() const;

    void set_komi(double km) { komi = km; }

    inline int16_t to_id(int r, int c) const {
        if (r < 0 || c < 0 || r >= board_size || c >= board_size) return -1;
        return get(r, c);
    }

    inline State get_cell(int r, int c) const {
        if (static_cast<unsigned>(r) >= static_cast<unsigned>(board_size) ||
            static_cast<unsigned>(c) >= static_cast<unsigned>(board_size)) {
            return State::OutOfBounds;
        }
        return board[get(r, c)];
    }

    inline State get_cell_unchecked(int id) const { return board[id]; }

    inline bool is_empty_cell(int r, int c){
        return get_cell(r, c) == State::Empty;
    }
    // AI-assisted: see AI12
    void display(std::ostream& os = std::cout) const;

    // AI-assisted: see AI08
    double calculate_score() const;

    int get_cell_cnt(){return cell_cnt;}

    int16_t get_only_lib(int16_t u){ return liberty_tracker.get_single_liberty(u);}

    int count_lib(int16_t u) const { return liberty_tracker.count_liberty(u); }

    const std::bitset<CELL_CNT>& get_liberties(int16_t u) const { return liberty_tracker.get_liberties(u); }

    int get_group_size(int16_t u){return liberty_tracker.get_group_size(u);}

    int16_t get_next_stone(int16_t u) const { return liberty_tracker.get_next_stone(u); }

    int16_t get_root(int id){return liberty_tracker.get(id);}
    int16_t get_root(int r, int c) {return get_root(get(r, c));}
    int16_t get_root(Point point) {return get_root(get(point));}
};

// AI-assisted: see AI05
class HistoryTracker{
private:
    int handicap_cnt{};
    Color current_turn = Color::Black;

    std::vector<Point> AddedBlack;
    std::vector<Point> AddedWhite;

    std::vector<UndoState> undo_stack;
    std::vector<RedoState> redo_stack;
public:
    HistoryTracker(int handicap = 0, Color start_turn = Color::Black);

    // AI-assisted: see AI26
    EndgameStatus is_game_over() const;
    bool has_started() const {return !undo_stack.empty();}
    Color get_turn() const;

    // AI-assisted: see AI05
    MoveStatus play_move(Board &board, int16_t id, Color color);

    MoveStatus play_move(Board &board, Point p, Color color){return play_move(board, board.get(p), color);}
    MoveStatus place_setup(Board &board, int16_t id, Color color);

    MoveStatus place_setup(Board &board, Point p, Color color){return place_setup(board, board.get(p), color);}
    // AI-assisted: see AI05, AI23
    bool undo_move(Board &board);

    // AI-assisted: see AI05
    bool redo_move(Board &board);

    // AI-assisted: see AI26
    bool pass(Color color);

    bool resign(Color color);

    std::optional<Color> get_resigned_player() const;

    const std::vector<UndoState>& get_history() const;

    const std::vector<Point>& get_added_black() const {return AddedBlack;}
    const std::vector<Point>& get_added_white() const {return AddedWhite;}
    Color get_start_turn() const {return current_turn;}
    int get_handicap_cnt() const {return handicap_cnt;}
};
