#include "GameLogic.hpp"

// Definitions for Color-State conversion.
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


Point::Point(int row, int col){
    r = row; c = col;
}
Point toPoint(int id, int sz){
    Point tmp;
    
    tmp.r = id / sz;
    tmp.c = id % sz;

    return tmp;
}


Hash::Hash(uint64_t high, uint64_t low) {
    h1= high, h2 = low;
}
Hash Hash::random(){
    return Hash{rng(), rng()};
}
Hash& Hash::operator^=(const Hash& rhs){
    h1 ^= rhs.h1;
    h2 ^= rhs.h2;
    return *this;
}
Hash Hash::operator^(const Hash& rhs) const {
    Hash tmp = *this;
    tmp ^= rhs;
    return tmp;
}

const Hash& HashLUT::getHash(int id, Color color){
    return color == Color::Black ? LUT_black[id] : LUT_white[id];
}


// DSU for easy group joining & finding liberties
DSU::Checkpoint DSU::checkpoint() const {
    return {op_history.size(), bitset_history.size()};
}
DSU::DSU(int sz){
    board_size = sz;
    int total_cells = board_size * board_size;
    for (int i = 0; i < total_cells; ++i){
        parent[i] = i;
        cmp_size[i] = 1;
        next_stone[i] = i;
    }
}

// Standard DSU stuff
int DSU::get(int16_t u){
    // No path compression here to make it easy to roll back to a previous state.
    // Changes time complexity of DSU to O(logn) instead of O(a(n)).
    while (u != parent[u]) u = parent[u];
    return u;
}
bool DSU::join(int16_t u, int16_t v){
    int16_t ru = get(u), rv = get(v);
    if (ru == rv) return false;

    if (cmp_size[ru] < cmp_size[rv]) std::swap(ru, rv);
    
    op_history.push_back({
        OpType::Join, ru, rv, 0, 0
    });
    bitset_history.push_back(liberties[ru]);

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

// Helper operations for liberties
int DSU::count_liberty(int16_t u){
    int16_t ru = get(u);
    return liberties[ru].count();
}
void DSU::add_liberty(int16_t u, int16_t lib_cell) {
    int16_t ru = get(u);
    if (!liberties[ru].test(lib_cell)){
        op_history.push_back({
            OpType::BitFlip,
            ru,
            lib_cell,
            0,
            0
        });
        liberties[ru].set(lib_cell);
    }
}
void DSU::remove_liberty(int16_t u, int16_t lib_cell) {
    int16_t ru = get(u);
    if (liberties[ru].test(lib_cell)){
        op_history.push_back({
            OpType::BitFlip,
            ru,
            lib_cell,
            0,
            0
        });
        liberties[ru].reset(lib_cell);
    }
}

// Function to clear a connected group containing u
void DSU::clear_group(int u) {
    // Traverses the group containing id u to reset DSU node states
    int curr = u;
    do {
        int next = next_stone[curr];

        // Reset DSU state so this cell can be played on again later
        parent[curr] = curr;
        cmp_size[curr] = 1;
        liberties[curr].reset();
        next_stone[curr] = curr;

        curr = next;
    } while (curr != u);
}

// Function to get a connected group.
std::vector<int16_t> DSU::get_group(int u) {
    std::vector<int16_t> group_members;
    group_members.reserve(cmp_size[get(u)]);

    // Traverses the group containing id u
    int curr = u;
    do {
        int next = next_stone[curr];
        group_members.push_back(static_cast<int16_t>(curr));
        curr = next;
    } while (curr != u);

    return group_members;
}

// CAUTION: Only safe to call on an isolated empty cell, 
// or when looping over ALL members of an entire captured group.
// Calling this on a single stone inside an active group will break everything!
void DSU::reset(int16_t u){
    op_history.push_back({
        OpType::Reset,
        u,
        parent[u],
        cmp_size[u],
        next_stone[u]
    });
    bitset_history.push_back(liberties[u]);

    parent[u] = u;
    cmp_size[u] = 1;
    liberties[u].reset();
    next_stone[u] = u;
}

void DSU::rollback(Checkpoint cp) {
    while (op_history.size() > cp.op_idx) {
        OpRecord op = op_history.back();
        op_history.pop_back();
        switch (op.type) {
            case OpType::BitFlip:
                liberties[op.u].flip(op.v);
                break;
            case OpType::Join: {
                int ru = op.u, rv = op.v;
                std::swap(next_stone[ru], next_stone[rv]);
                liberties[ru] = bitset_history.back();
                bitset_history.pop_back();
                cmp_size[ru] -= cmp_size[rv];
                parent[rv] = rv;
                break;
            }
            case OpType::Reset: {
                int u = op.u;
                parent[u] = op.v;
                cmp_size[u] = op.extra1;
                next_stone[u] = op.extra2;
                liberties[u] = bitset_history.back();
                bitset_history.pop_back();
                break;
            }
        }
    }
}

UndoState::UndoState(bool pass, Color c){
    is_pass = pass;
    color = c;
    id = -1;
}

RedoState::RedoState(int16_t inp_id, Color inp_color){
    id = inp_id;
    color = inp_color;
}
RedoState::RedoState(Color c, bool pass){
    is_pass = pass;
    color = c;
    id = -1;
}

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

bool Board::is_suicide(int16_t id, Color color, std::vector<int16_t> &empty, std::vector<int16_t> &friendly, std::vector<int16_t> &opponent){       
    empty.clear();
    friendly.clear();
    opponent.clear();
    
    int r = id / board_size, c = id % board_size;

    int empty_cnt = 0;
    int valid_friendly_cnt = 0;
    int valid_opponent_cnt = 0;

    State my_state = ColortoState(color);
    for (int i = 0; i < 4; ++i){
        int nr = r + dr[i], nc = c + dc[i];

        if (nr < 0 || nr >= board_size) continue;
        if (nc < 0 || nc >= board_size) continue;

        int nid = get(nr, nc);
        

        if (board[nid] == State::Empty){
            empty.push_back(nid);
            empty_cnt++;
            continue;
        }

        int root_nid = liberty_tracker.get(nid);
        if (board[nid] == my_state){
            if (std::find(friendly.begin(), friendly.end(), root_nid) == friendly.end()){
                friendly.push_back(root_nid);
                if (liberty_tracker.count_liberty(root_nid) > 1) valid_friendly_cnt++;
            } 
        }else{
            if (std::find(opponent.begin(), opponent.end(), root_nid) == opponent.end()){
                opponent.push_back(root_nid);

                if (liberty_tracker.count_liberty(root_nid) == 1) valid_opponent_cnt++;
            } 
        }
    }

    return empty_cnt == 0 && valid_friendly_cnt == 0 && valid_opponent_cnt == 0;
}

bool Board::is_suicide(int16_t id, Color color, std::vector<int16_t> &opponent){       
    opponent.clear();
    
    int r = id / board_size, c = id % board_size;

    int empty_cnt = 0;
    int valid_friendly_cnt = 0;
    int valid_opponent_cnt = 0;

    State my_state = ColortoState(color);

    for (int i = 0; i < 4; ++i){
        int nr = r + dr[i], nc = c + dc[i];

        if (nr < 0 || nr >= board_size) continue;
        if (nc < 0 || nc >= board_size) continue;

        int nid = get(nr, nc);
        

        
        if (board[nid] == State::Empty){
            empty_cnt++;
            continue;
        }
        int root_nid = liberty_tracker.get(nid);
        if (board[nid] == my_state){
            if (liberty_tracker.count_liberty(root_nid) > 1) valid_friendly_cnt++;
        }else{
            if (std::find(opponent.begin(), opponent.end(), root_nid) == opponent.end()){
                opponent.push_back(root_nid);
                if (liberty_tracker.count_liberty(root_nid) == 1) valid_opponent_cnt++;
            } 
        }
    }

    return empty_cnt == 0 && valid_friendly_cnt == 0 && valid_opponent_cnt == 0;
}

Board::Board(int sz){
    board_size = sz; 
    liberty_tracker = DSU(sz);  
    seen_hashes.insert(curr_hash);
    cell_cnt = board_size*board_size;
    st.reserve(cell_cnt);
}

bool Board::is_legal(int16_t curr_id, Color color)  {
    if (curr_id < 0 || curr_id >= cell_cnt || board[curr_id] != State::Empty) {
        return false;
    }

    Color opp_color = invertColor(color);

    std::vector<int16_t> opponent;
    opponent.reserve(4);

    if (is_suicide(curr_id, color, opponent)) return false;
    
    Hash tmp_hash = curr_hash ^ HashLUT::getHash(curr_id, color);
    for (int opp_root : opponent) {
        if (liberty_tracker.count_liberty(opp_root) == 1) { // Only groups with 1 liberty die!
            std::vector<int16_t> tmp = liberty_tracker.get_group(opp_root);
            for (int16_t id : tmp) {
                tmp_hash ^= HashLUT::getHash(id, opp_color);
            }
        }
    }

    if (seen_hashes.contains(tmp_hash)) return false;

    return true;
}

std::vector<int16_t> Board::get_legal_moves(Color color) {
    std::vector<int16_t> ans;
    ans.reserve(cell_cnt);
    for (int i = 0; i < cell_cnt; ++i){
        if (board[i] == State::Empty && is_legal(i, color)){
            ans.push_back(i);
        }
    }

    return ans;
}

std::optional<UndoState> Board::place(int16_t curr_id, Color color) {
    if (curr_id < 0 || curr_id >= cell_cnt || board[curr_id] != State::Empty) {
        return std::nullopt;
    }

    State my_state = ColortoState(color);
    State opp_state = ColortoState(invertColor(color));
    Color opp_color = invertColor(color);

    std::vector<int16_t> friendly, opponent, empty;
    friendly.reserve(4);
    opponent.reserve(4);
    empty.reserve(4);

    if (is_suicide(curr_id, color, empty, friendly, opponent)) return std::nullopt;
    

    Hash tmp_hash = curr_hash ^ HashLUT::getHash(curr_id, color);
    std::vector<int16_t> captured;

    for (int opp_root : opponent) {
        if (liberty_tracker.count_liberty(opp_root) == 1) { // Only groups with 1 liberty die!
            std::vector<int16_t> tmp = liberty_tracker.get_group(opp_root);
            for (int16_t id : tmp) {
                tmp_hash ^= HashLUT::getHash(id, opp_color);
            }
            captured.insert(captured.end(), tmp.begin(), tmp.end());
        }
    }

    if (seen_hashes.contains(tmp_hash)) return std::nullopt;

    UndoState res;
    res.color = color;
    res.id = curr_id;
    res.captured = captured;
    res.prev_hash = curr_hash;                // Save PREVIOUS hash
    res.dsu_cp = liberty_tracker.checkpoint();    // Save PREVIOUS DSU

    liberty_tracker.reset(curr_id);
    board[curr_id] = my_state;
    curr_hash = tmp_hash;
    seen_hashes.insert(curr_hash);

    for (int e : empty) {
        liberty_tracker.add_liberty(curr_id, e);
    }
    for (int f : friendly) {
        liberty_tracker.remove_liberty(f, curr_id);
        liberty_tracker.join(f, curr_id);
    }

    for (int opp_root : opponent) {
        liberty_tracker.remove_liberty(opp_root, curr_id);
    }

    for (int16_t dead_id : captured) {
        board[dead_id] = State::Empty;

        // idk why i commented this line out but i'll figure it out.

        //liberty_tracker.reset(dead_id);

        int r = dead_id / board_size, c = dead_id % board_size;
        for (int i = 0; i < 4; ++i) {
            int nr = r + dr[i], nc = c + dc[i];
            if (nr < 0 || nr >= board_size || nc < 0 || nc >= board_size) continue;

            int nid = get(nr, nc);
            if (board[nid] == my_state) {
                liberty_tracker.add_liberty(nid, dead_id);
            }
        }
    }

    if (color == Color::White) {
        white_cnt += 1;
        black_cnt -= static_cast<int>(captured.size());
    } else {
        black_cnt += 1;
        white_cnt -= static_cast<int>(captured.size());
    }

    return res;
}

void Board::undo(const UndoState &state){
    if (state.is_pass) return;
    board[state.id] = State::Empty;
    State opp_state = ColortoState(invertColor(state.color));
    for (int16_t i: state.captured){
        board[i] = opp_state;
    }
    liberty_tracker.rollback(state.dsu_cp);
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

int Board::get_white_cnt() const {return white_cnt;}

int Board::get_black_cnt() const {return black_cnt;}

int Board::get_size() const {return board_size;}

int16_t Board::to_id(int r, int c) const {
    if (r < 0 || c < 0 || r >= board_size || c >= board_size) return -1;
    return static_cast<int16_t>(get(r, c));
}

State Board::get_cell(int r, int c) const {
    if (r < 0 || c < 0 || r >= board_size || c >= board_size) return State::Empty;
    return board[get(r, c)];
}

void Board::display(std::ostream& os) const {
    os << "\n    ";
    for (int c = 0; c < board_size; ++c) {
        if (c < 10) os << " " << c << " ";
        else os << c << " ";
    }
    os << "\n";

    for (int r = 0; r < board_size; ++r) {
        if (r < 10) os << " " << r << "  ";
        else os << r << "  ";

        for (int c = 0; c < board_size; ++c) {
            State s = board[get(r, c)];
            char ch = (s == State::Black) ? 'X' : (s == State::White ? 'O' : '.');
            os << " " << ch << " ";
        }

        int go_row = board_size - r;
        os << " " << go_row << "\n";
    }

    os << "    ";
    for (int c = 0; c < board_size; ++c) {
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


bool HistoryTracker::is_game_over() const{
    int sz = undo_stack.size();
    if (sz < 2) return false;
    return undo_stack[sz - 1].is_pass && undo_stack[sz - 2].is_pass;
}

Color HistoryTracker::get_turn() const {
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
        redo_stack.push_back(RedoState(tmp.color, tmp.is_pass));
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
