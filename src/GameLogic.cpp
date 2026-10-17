#include "GameLogic.hpp"

std::optional<Color> StatetoColor(State state){
    if (state == State::Black){
        return Color::Black;
    }else if (state == State::White){
        return Color::White;
    }else{
        return std::nullopt;
    }
}

State ColortoState(Color color){
    return color == Color::Black ? State::Black : State::White;
}

Color invertColor(Color color){
    return color == Color::White ? Color::Black : Color::White;
}

Point toPoint(int id, int sz){
    Point tmp;
    
    tmp.r = id / sz;
    tmp.c = id % sz;

    return tmp;
}

// ============================================================================
// Hash & HashLUT
// ============================================================================

Hash Hash::random(){
    return Hash{rng(), rng()};
}

Hash& Hash::operator^=(const Hash& rhs){
    h1 ^= rhs.h1;
    h2 ^= rhs.h2;

    return *this;
}

Hash Hash::operator^(const Hash& rhs) const {
    Hash tmp = *this; // Copy current object
    tmp ^= rhs;       // Use ^=
    return tmp;       // Return by value (optimized out via RVO)
}

const Hash& HashLUT::getHash(int id, Color color){
    return color == Color::Black ? LUT_black[id] : LUT_white[id];
}

// ============================================================================
// DSU
// ============================================================================

DSU::DSU(int sz){
    board_size = sz;
    int total_cells = board_size * board_size;
    for (int i = 0; i < total_cells; ++i){
        parent[i] = i;
        cmp_size[i] = 1;
        next_stone[i] = i;
    }
}

int DSU::get(int u){
    if (u == parent[u]){
        return u;
    }

    // May remove path compression here once I get to DSU rollback.
    return parent[u] = get(parent[u]);
}

bool DSU::join(int u, int v){
    int ru = get(u), rv = get(v);
    if (ru == rv) return false;

    if (cmp_size[ru] < cmp_size[rv]) std::swap(ru, rv);

    parent[rv] = ru;
    cmp_size[ru] += cmp_size[rv];
    liberties[ru] |= liberties[rv];

    /*
        This is an interesting way to group connected stones together.
        Some small cases:
        1. 
        We have sets A -> A and B -> B. After merging through A and B, A -> B -> A.

        2. 
        We have sets A -> B -> C -> A and D -> E -> D. 
        After merging through B and E, we have A -> B -> D -> E -> C -> A   
    */
    std::swap(next_stone[ru], next_stone[rv]);
    return true; 
}

int DSU::count_liberty(int u){
    int ru = get(u);
    return liberties[ru].count();
}

void DSU::add_liberty(int u, int lib_cell) {
    liberties[get(u)].set(lib_cell);
}

void DSU::remove_liberty(int u, int lib_cell) {
    liberties[get(u)].reset(lib_cell);
}

// Function to clear group DSU & find removed stones.
// This function is AI generated. See AI01
std::vector<int16_t> DSU::clear_group(int u) {
    std::vector<int16_t> dead_stones;
    int root = get(u);
    dead_stones.reserve(cmp_size[root]);

    // Traverses the group containing id u to reset DSU node states
    int curr = u;
    do {
        int next = next_stone[curr];
        dead_stones.push_back(static_cast<int16_t>(curr));

        // Reset DSU state so this cell can be played on again later
        parent[curr] = curr;
        cmp_size[curr] = 1;
        liberties[curr].reset();
        next_stone[curr] = curr;

        curr = next;
    } while (curr != u);

    return dead_stones;
}

// ============================================================================
// UndoState & RedoState
// ============================================================================

UndoState::UndoState(bool pass, Color c){
    is_pass = pass;
    color = c;
    id = -1;
}

RedoState::RedoState(int16_t inp_id, Color inp_color){
    id = inp_id;
    color = inp_color;
}

RedoState::RedoState(bool pass, Color c){
    is_pass = pass;
    color = c;
    id = -1;
}

// ============================================================================
// Board
// ============================================================================

int Board::get(int r, int c) const{
    return r*board_size + c;
}

int Board::score_cnt_helper(int16_t id) const{
    if (board[id] != State::Empty || visited[id]) return 0;

    bool touches_white = false;
    bool touches_black = false;

    st.push_back(id);
    visited[id] = true;
    int area = 1;
    while (!st.empty()){
        int curr_id = st.back();
        st.pop_back();
        int r = curr_id / board_size, c = curr_id % board_size;
        
        for (int i = 0; i < 4; ++i){
            int nr = r + dr[i], nc = c + dc[i];
            if (nr < 0 || nc < 0 || nr >= board_size || nc >= board_size) continue;

            int nid = get(nr, nc);
            if (visited[nid]) continue;

            if (board[nid] == State::Black) touches_black = true;
            else if (board[nid] == State::White) touches_white = true;
            else{
                visited[nid] = true;
                area += 1;
                st.push_back(nid);
            }
        }
    }

    if (touches_black && !touches_white) return -area;
    else if (touches_white && !touches_black) return area;
    else return 0;
}

// Helper for removing groups. This function is partly AI generated. See AI01
bool Board::remove_group(int id, std::array<State, CELL_CNT>& b, DSU& dsu, std::vector<int16_t>& captured) {
    if (id < 0 || id >= cell_cnt) return false;

    State captured_color = b[id];
    if (captured_color == State::Empty) return false;

    int root = dsu.get(id);
    if (dsu.count_liberty(root) != 0) return false;

    State opp_color = (captured_color == State::Black) ? State::White : State::Black;
    std::vector<int16_t> dead_stones = dsu.clear_group(root);

    for (int16_t curr : dead_stones) {
        int r = curr / board_size;
        int c = curr % board_size;

        // 1. Give `curr` as a liberty to all adjacent OPPONENT stones
        for (int i = 0; i < 4; ++i) {
            int nr = r + dr[i], nc = c + dc[i];
            if (nr < 0 || nc < 0 || nr >= board_size || nc >= board_size) continue;

            int n_idx = get(nr, nc);
            if (b[n_idx] == opp_color) dsu.add_liberty(n_idx, curr);
        }

        // 2. Clear this cell on the board & record for undo history
        b[curr] = State::Empty;
        captured.push_back(curr);
    }

    return true;
}

Board::Board(int sz){
    board_size = sz; 
    liberty_tracker = DSU(sz);  
    seen_hashes.insert(curr_hash);
    cell_cnt = board_size*board_size;
    st.reserve(cell_cnt);
}

std::optional<UndoState> Board::place(int16_t curr_id, Color color){
    /*
        The main bottleneck in this is it copies the entire DSU struct,
        rather than writing differences.
        I will implement DSU rollback later.
    */
    // Will implement rules later
    if (curr_id < 0 || curr_id >= cell_cnt || board[curr_id] != State::Empty){
        return std::nullopt;
    }

    State my_state = ColortoState(color);
    State opp_state = ColortoState(invertColor(color));
    DSU tmp_liberty = liberty_tracker;
    std::array<State, CELL_CNT> tmp_board = board;
    std::set<int> friendly, opponent, empty;
    int r = curr_id / board_size;
    int c = curr_id % board_size;
    for (int i = 0; i < 4; ++i){
        int nr = r + dr[i], nc = c + dc[i];
        if (nr < 0 || nc < 0 || nr >= board_size || nc >= board_size){
            continue;
        }

        int adj_id = get(nr, nc);

        if (board[adj_id] == State::Empty){
            empty.insert(tmp_liberty.get(adj_id));
        }else if (board[adj_id] == opp_state){
            opponent.insert(tmp_liberty.get(adj_id));
        }else if (board[adj_id] == my_state){
            friendly.insert(tmp_liberty.get(adj_id));
        }
    }

    for (int i: empty){
        tmp_liberty.add_liberty(curr_id, i);
    }

    for (int i: friendly){
        tmp_liberty.remove_liberty(i, curr_id);
        tmp_liberty.join(i, curr_id);
    }

    tmp_board[curr_id] = my_state;
    std::vector<int16_t> captured;
    for (int i: opponent){
        tmp_liberty.remove_liberty(i, curr_id);
        remove_group(i, tmp_board, tmp_liberty, captured);
    }

    if (tmp_liberty.count_liberty(curr_id)){
        Hash tmp_hash = curr_hash ^ HashLUT::getHash(curr_id, color);

        for (int16_t i: captured){
            tmp_hash ^= HashLUT::getHash(i, invertColor(color));
        }

        if (seen_hashes.contains(tmp_hash)){
            return std::nullopt;
        }

        UndoState res;
        res.color = color;
        res.id = curr_id;
        res.captured = captured;
        res.liberty_tracker = liberty_tracker;
        res.prev_hash = curr_hash;

        liberty_tracker = std::move(tmp_liberty);
        board = std::move(tmp_board);
        board[curr_id] = (color == Color::Black) ? State::Black : State::White;

        curr_hash = std::move(tmp_hash);
        seen_hashes.insert(curr_hash);

        if (color == Color::White){
            white_cnt += 1;
            black_cnt -= static_cast<int>(captured.size()); 
        }else{
            black_cnt += 1;
            white_cnt -= static_cast<int>(captured.size()); 
        }

        return res;
    }
    
    return std::nullopt;
}

void Board::undo(const UndoState &state){
    if (state.is_pass) return;
    board[state.id] = State::Empty;
    State opp_state = ColortoState(invertColor(state.color));
    for (int16_t i: state.captured){
        board[i] = opp_state;
    }
    liberty_tracker = state.liberty_tracker;
    seen_hashes.erase(curr_hash);
    curr_hash = state.prev_hash;

    if (state.color == Color::Black){
        black_cnt -= 1;
        white_cnt += static_cast<int>(state.captured.size());
    }else{
        white_cnt -= 1;
        black_cnt += static_cast<int>(state.captured.size());
    }
}

int Board::get_white_cnt() const{
    return white_cnt;
}

int Board::get_black_cnt() const{
    return black_cnt;
}

int Board::get_size() const{
    return board_size;
}

int16_t Board::to_id(int r, int c) const{
    if (r < 0 || c < 0 || r >= board_size || c >= board_size) return -1;
    return static_cast<int16_t>(get(r, c));
}

State Board::get_cell(int r, int c) const{
    if (r < 0 || c < 0 || r >= board_size || c >= board_size) return State::Empty;
    return board[get(r, c)];
}

void Board::display(std::ostream& os) const{
    os << "\n    ";
    for (int c = 0; c < board_size; ++c){
        if (c < 10) os << " " << c << " ";
        else os << c << " ";
    }
    os << "\n";

    for (int r = 0; r < board_size; ++r){
        if (r < 10) os << " " << r << "  ";
        else os << r << "  ";

        for (int c = 0; c < board_size; ++c){
            State s = board[get(r, c)];
            char ch = (s == State::Black) ? 'X' : (s == State::White ? 'O' : '.');
            os << " " << ch << " ";
        }

        int go_row = board_size - r;
        os << " " << go_row << "\n";
    }

    os << "    ";
    for (int c = 0; c < board_size; ++c){
        char col_ch = static_cast<char>((c < 8) ? ('A' + c) : ('A' + c + 1));
        os << " " << col_ch << " ";
    }
    os << "\n";
}

double Board::calculate_score(double komi) const{
    int score = white_cnt - black_cnt;

    visited.fill(false);
    for (int i = 0; i < cell_cnt; ++i){
        if (board[i] == State::Empty && !visited[i]){
            score += score_cnt_helper(i);
        }
    }

    return static_cast<double>(score) + komi;
}

// ============================================================================
// HistoryTracker
// ============================================================================

bool HistoryTracker::is_game_over() const{
    int sz = undo_stack.size();
    if (sz < 2) return false;
    return undo_stack[sz - 1].is_pass && undo_stack[sz - 2].is_pass;
}

Color HistoryTracker::get_turn() const{
    if (undo_stack.empty()) return Color::Black;
    return invertColor(undo_stack.back().color);
}

bool HistoryTracker::play_move(Board &board, int16_t id, Color color){
    if (is_game_over()) return false;

    auto res = board.place(id, color);

    if (!res.has_value()) return false;

    undo_stack.push_back(std::move(*res));

    redo_stack.clear();
    return true;
}

bool HistoryTracker::undo_move(Board &board){
    if (undo_stack.empty()) return false;

    UndoState tmp = undo_stack.back();
    if (!tmp.is_pass){
        board.undo(undo_stack.back());
        redo_stack.push_back(RedoState(tmp.id, tmp.color));
    }else{
        redo_stack.push_back(RedoState(tmp.is_pass, tmp.color));
    }
    undo_stack.pop_back();
    return true;
}

bool HistoryTracker::redo_move(Board &board){
    if (redo_stack.empty()) return false;
    
    RedoState tmp = redo_stack.back();

    if (!tmp.is_pass){
        auto res = board.place(redo_stack.back().id, redo_stack.back().color);
        if (!res.has_value()) return false;
        undo_stack.push_back(std::move(*res));
    }else{
        undo_stack.push_back(UndoState(tmp.is_pass, tmp.color));
    }

    redo_stack.pop_back();
    return true;
}

bool HistoryTracker::pass(Color color){
    if (is_game_over()) return false;
    undo_stack.push_back(UndoState(true, color));
    redo_stack.clear();
    return true;
}