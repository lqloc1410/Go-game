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

// AI-assisted: see AI03
Point::Point(int row, int col){
    r = row; c = col;
}

// AI-assisted: see AI11
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


// AI-assisted: see AI20
// DSU for easy group joining & finding liberties
DSU::Checkpoint DSU::checkpoint() const {
    return {op_history.size(), bitset_history.size()};
}

// AI-assisted: see AI03
DSU::DSU(int sz){
    board_size = sz;
    int total_cells = board_size * board_size;
    for (int i = 0; i < total_cells; ++i){
        parent[i] = i;
        cmp_size[i] = 1;
        next_stone[i] = i;
    }
}

// AI-assisted: see AI22
// Standard DSU stuff
int DSU::get(int16_t u){
    // No path compression here to make it easy to roll back to a previous state.
    // Changes time complexity of DSU to O(logn) instead of O(a(n)).
    while (u != parent[u]) u = parent[u];
    return u;
}

// AI-assisted: see AI02, AI20
bool DSU::join(int16_t u, int16_t v){
    int16_t ru = get(u), rv = get(v);
    if (ru == rv) return false;

    if (cmp_size[ru] < cmp_size[rv]) std::swap(ru, rv);
    
    // AI-assisted: see AI20
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
    // AI-assisted: see AI02
    std::swap(next_stone[ru], next_stone[rv]);
    return true; 
}

// Helper operations for liberties
// AI-assisted: see AI20, AI23
int DSU::count_liberty(int16_t u){
    int16_t ru = get(u);
    return liberties[ru].count();
}


// AI-assisted: see AI20, AI23
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

// AI-assisted: see AI20, AI23
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

// AI-assisted: see AI09
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

// AI-assisted: see AI10
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

// AI-assisted: see AI10, AI19, AI20
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

// AI-assisted: see AI20
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

UndoState::UndoState(MoveType t, Color c){
    type = t;
    color = c;
    id = -1;
}

RedoState::RedoState(int16_t inp_id, Color inp_color, MoveType inp_type){
    id = inp_id;
    color = inp_color;
    type = inp_type;
}

int16_t Board::get(int r, int c) const{
    return r*board_size + c;
}

int16_t Board::get(Point point) const{
    return to_id(point.r, point.c);
}

// AI-assisted: see AI08
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

// AI-assisted: see AI04, AI13, AI16, AI18
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

        // AI-assisted: see AI04
        int root_nid = liberty_tracker.get(nid);
        if (board[nid] == my_state){
            if (std::find(friendly.begin(), friendly.end(), root_nid) == friendly.end()){
                friendly.push_back(root_nid);
                // AI-assisted: see AI16
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

// AI-assisted: see AI18, AI19, AI21
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
        

        // AI-assisted: see AI19
        if (board[nid] == State::Empty){
            empty_cnt++;
            continue;
        }
        int root_nid = liberty_tracker.get(nid);
        if (board[nid] == my_state){
            // AI-assisted: see AI16
            if (liberty_tracker.count_liberty(root_nid) > 1) valid_friendly_cnt++;
        }else{
            // AI-assisted: see AI04
            if (std::find(opponent.begin(), opponent.end(), root_nid) == opponent.end()){
                opponent.push_back(root_nid);
                if (liberty_tracker.count_liberty(root_nid) == 1) valid_opponent_cnt++;
            } 
        }
    }

    return empty_cnt == 0 && valid_friendly_cnt == 0 && valid_opponent_cnt == 0;
}

// AI-assisted: see AI03
Board::Board(int sz, double km){
    board_size = sz; 
    komi = km;
    liberty_tracker = DSU(sz);  
    seen_hashes.insert(curr_hash);
    cell_cnt = board_size*board_size;
    st.reserve(cell_cnt);
}

// AI-assisted: see AI17
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

// AI-assisted: see AI17
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

// AI-assisted: see AI02, AI03, AI04, AI05, AI08, AI19
MoveState Board::place(int16_t curr_id, Color color) {
    if (curr_id < 0 || curr_id >= cell_cnt) {
        return {MoveStatus::OutOfBounds};
    }

    if (board[curr_id] != State::Empty){
        return {MoveStatus::Occupied};
    }

    State my_state = ColortoState(color);
    State opp_state = ColortoState(invertColor(color));
    Color opp_color = invertColor(color);

    std::vector<int16_t> friendly, opponent, empty;
    friendly.reserve(4);
    opponent.reserve(4);
    empty.reserve(4);

    if (is_suicide(curr_id, color, empty, friendly, opponent)) return {MoveStatus::Suicide};
    

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

    if (seen_hashes.contains(tmp_hash)) return MoveState{MoveStatus::Ko};

    MoveState res;
    res.undo_state.emplace();
    res.undo_state -> type = MoveType::Place;
    res.undo_state -> color = color;
    res.undo_state -> id = curr_id;
    res.undo_state -> prev_hash = curr_hash;                // Save PREVIOUS hash
    res.undo_state -> dsu_cp = liberty_tracker.checkpoint();    // Save PREVIOUS DSU

    // AI-assisted: see AI19
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

    // AI-assisted: see AI03
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

    // AI-assisted: see AI08
    if (color == Color::White) {
        white_cnt += 1;
        black_cnt -= static_cast<int>(captured.size());
    } else {
        black_cnt += 1;
        white_cnt -= static_cast<int>(captured.size());
    }

    res.undo_state -> captured = std::move(captured);
    return res;
}

MoveState Board::place(Point point, Color color){
    return place(get(point), color);
}


// AI-assisted: see AI05, AI08, AI20, AI23
void Board::undo(const UndoState &state){
    if (state.type != MoveType::Place) return;
    board[state.id] = State::Empty;
    State opp_state = ColortoState(invertColor(state.color));
    for (int16_t i: state.captured){
        board[i] = opp_state;
    }
    // AI-assisted: see AI20
    liberty_tracker.rollback(state.dsu_cp);
    seen_hashes.erase(curr_hash);
    curr_hash = state.prev_hash;

    // AI-assisted: see AI08
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

double Board::get_komi() const {return komi;}

int16_t Board::to_id(int r, int c) const {
    if (r < 0 || c < 0 || r >= board_size || c >= board_size) return -1;
    return static_cast<int16_t>(get(r, c));
}

State Board::get_cell(int r, int c) const {
    if (r < 0 || c < 0 || r >= board_size || c >= board_size) return State::Empty;
    return board[get(r, c)];
}

// AI-assisted: see AI12
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

// AI-assisted: see AI08
double Board::calculate_score() const{
    int score = white_cnt - black_cnt;

    visited.fill(false);
    for (int i = 0; i < cell_cnt; ++i){
        if (board[i] == State::Empty && !visited[i]){
            score += score_cnt_helper(i);
        }
    }

    return static_cast<double>(score) + komi;
}

HistoryTracker::HistoryTracker(int handicap, Color start_turn) {
    handicap_cnt = handicap;
    current_turn = start_turn;
}

// AI-assisted: see AI26
EndgameStatus HistoryTracker::is_game_over() const {
    int sz = undo_stack.size();
    if (sz == 0) return EndgameStatus::NotOver;
    if (undo_stack.back().type == MoveType::Resign) return EndgameStatus::Resignation;
    if (sz >= 2 && undo_stack[sz - 1].type == MoveType::Pass && undo_stack[sz - 2].type == MoveType::Pass) {
        return EndgameStatus::Pass;
    }
    return EndgameStatus::NotOver;
}

Color HistoryTracker::get_turn() const {
    if (static_cast<int>(undo_stack.size()) < handicap_cnt) return Color::Black;
    if (static_cast<int>(undo_stack.size()) == handicap_cnt) return current_turn;
    return invertColor(undo_stack.back().color);
}

// AI-assisted: see AI05
MoveStatus HistoryTracker::play_move(Board &board, int16_t id, Color color){
    if (is_game_over() != EndgameStatus::NotOver) return MoveStatus::GameEnded;

    MoveState res = board.place(id, color);

    if (res.status != MoveStatus::Success) return res.status;

    undo_stack.push_back(std::move(*res.undo_state));

    redo_stack.clear();
    return MoveStatus::Success;
}

// AI-assisted: see AI05, AI23
bool HistoryTracker::undo_move(Board &board){
    if (undo_stack.empty()) return false;

    const UndoState& tmp = undo_stack.back();
    if (tmp.type == MoveType::Place){
        board.undo(undo_stack.back());
    }
    redo_stack.push_back(RedoState(tmp.id, tmp.color, tmp.type));

    undo_stack.pop_back();
    return true;
}

// AI-assisted: see AI05
bool HistoryTracker::redo_move(Board &board){
    if (redo_stack.empty()) return false;
    
    RedoState tmp = redo_stack.back();

    if (tmp.type == MoveType::Place){
        auto res = board.place(redo_stack.back().id, redo_stack.back().color);
        if (res.status != MoveStatus::Success) return false;
        undo_stack.push_back(std::move(*res.undo_state));
    }else{
        undo_stack.push_back(UndoState(tmp.type, tmp.color));
    }

    redo_stack.pop_back();
    return true;
}

// AI-assisted: see AI26
bool HistoryTracker::pass(Color color){
    if (is_game_over() != EndgameStatus::NotOver) return false;
    undo_stack.push_back(UndoState(MoveType::Pass, color));
    redo_stack.clear();
    return true;
}

bool HistoryTracker::resign(Color color){
    if (is_game_over() != EndgameStatus::NotOver) return false;
    undo_stack.push_back(UndoState(MoveType::Resign, color));
    redo_stack.clear();
    return true;
}

std::optional<Color> HistoryTracker::get_resigned_player(){
    if (is_game_over() != EndgameStatus::Resignation) return std::nullopt;
    return undo_stack.back().color;
}