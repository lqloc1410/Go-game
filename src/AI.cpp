#include <AI.hpp>

static const patterns3x3 global_p3("lookup-tables/pattern3x3.bin");
static const patterns5x5 global_p5("lookup-tables/pattern5x5.bin");

MCTSNode::MCTSNode(Board &board, Color prev_p, MoveType t, int16_t move_id, MCTSNode* p)
    : type(t), id(move_id), prev_player(prev_p), parent(p){}

MCTSNode* MCTSNode::select_best_child(double c_puct) const{
    if (children.empty()) return nullptr;
    MCTSNode* best_child = nullptr;
    double best_score = -1e9;

    double fpu_reduction = 0.25;
    double parent_q = (this->visits > 0) 
        ? std::max(0.0, (1.0 - (this->wins / this->visits)) - fpu_reduction) 
        : 0.5;
    double sqrt_parent_visits = std::sqrt(std::max(1, this->visits));

    for (const std::unique_ptr<MCTSNode>& child : children){
        double q = (child->visits > 0) ? (child->wins / child->visits) : parent_q;

        double u = c_puct * child -> priority * (sqrt_parent_visits / (1.0 + child->visits));

        double puct_score = q + u;
        if (puct_score > best_score) {
            best_score = puct_score;
            best_child = child.get();
        }
    }

    return best_child;
}

double MCTSInstance::score_candidate_move(int16_t id, Color player) {
    if (board.is_eye(id, player)) return 0.001;
    int r = id / board.get_size(), c = id % board.get_size();
    auto [h3, h5] = serialize_patterns(board, r, c, player);

    double s3 = pattern3x3_has_stone(h3) ? std::log1p(static_cast<double>(global_p3[h3])) : 0.0;
    double s5 = pattern5x5_has_stone(h5) ? std::log1p(static_cast<double>(global_p5[h5])) : 0.0;
    double move_score = 1.0 + s3 + 3.0 * s5;

    // Tactical bonuses for saving our own groups or capturing/pressuring opponent groups
    int save_sz = 0, cap_sz = 0, atari_sz = 0;
    int empty_neighbors = 0, max_friendly_libs = 0, friendly_gsz_sum = 0;
    int16_t empty_ids[4]{};
    Color opp = invertColor(player);

    SmallVec4 distinct_groups;
    SmallVec4 friendly_roots;

    for (int i = 0; i < 4; ++i) {
        int nr = r + dr[i], nc = c + dc[i];
        State cell_state = board.get_cell(nr, nc);
        if (cell_state == State::OutOfBounds) continue;
        if (cell_state == State::Empty) {
            empty_ids[empty_neighbors++] = board.get(nr, nc);
            continue;
        }
        Color ncol = (cell_state == State::Black) ? Color::Black : Color::White;
        int16_t n_root = board.get_root(nr, nc);
        if (distinct_groups.find(n_root) == -1) {
            int libs = board.count_lib(n_root);
            int gsz = board.get_group_size(n_root);
            if (ncol == player) {
                if (libs > max_friendly_libs) max_friendly_libs = libs;
                if (libs == 1) save_sz += gsz;
                friendly_roots.push_back(n_root);
                friendly_gsz_sum += gsz;
            } else {
                if (libs == 1) cap_sz += gsz;
                else if (libs == 2) atari_sz += gsz;
            }
            distinct_groups.push_back(n_root);
        }
    }

    if (empty_neighbors >= 2 || max_friendly_libs >= 3) {
        // Guaranteed new_libs >= 2 without placing/undoing
        if (cap_sz > 0) move_score += 20.0 * cap_sz;
        if (save_sz > 0) move_score += 22.0 * save_sz;
        if (atari_sz > 0) move_score += 5.0 * atari_sz;
    } else {
        int new_libs = 0;
        int new_gsz = 0;
        bool have_result = false;

        if (cap_sz == 0) {
            if (friendly_roots.sz == 0) {
                new_libs = empty_neighbors;
                new_gsz = 1;
            } else {
                std::bitset<CELL_CNT> merged_libs = board.get_liberties(friendly_roots.data[0]);
                for (int i = 1; i < friendly_roots.sz; ++i) {
                    merged_libs |= board.get_liberties(friendly_roots.data[i]);
                }
                for (int i = 0; i < empty_neighbors; ++i) {
                    merged_libs.set(empty_ids[i]);
                }
                merged_libs.reset(id);
                new_libs = static_cast<int>(merged_libs.count());
                new_gsz = 1 + friendly_gsz_sum;
            }
            have_result = true;
        } else {
            MoveState sim = board.place(id, player);
            if (sim.status == MoveStatus::Success) {
                new_libs = board.count_lib(id);
                new_gsz = board.get_group_size(id);
                board.undo(*sim.undo_state);
                have_result = true;
            }
        }

        if (have_result) {
            if (new_libs == 1 && cap_sz < new_gsz) {
                // Self-atari (or snapback losing more stones than captured): heavily penalize
                move_score = (new_gsz >= 2) ? move_score*0.02 : move_score;
            } else {
                // Safe move, Ko capture (cap_sz == 1, new_gsz == 1), or net-positive capture
                if (cap_sz > 0) move_score += 20.0 * cap_sz;
                if (new_libs > 1) {
                    if (save_sz > 0) move_score += 22.0 * save_sz;
                    if (atari_sz > 0) move_score += 5.0 * atari_sz;
                }
            }
        }
    }

    int sz = board.get_size();
    int total_stones = board.get_white_cnt() + board.get_black_cnt();
    int opening_limit = std::max(8, board.get_cell_cnt() / 12); // 8 (9x9), 14 (13x13), 30 (19x19)
    int strategic_limit = board.get_cell_cnt() / 3;            // 27 (9x9), 56 (13x13), 120 (19x19)
    bool local_stone = pattern5x5_has_stone(h5);

    if (save_sz == 0 && cap_sz == 0 && atari_sz == 0 && total_stones < strategic_limit) {
        int dist_r = std::min(r, sz - 1 - r);
        int dist_c = std::min(c, sz - 1 - c);
        int line = std::min(dist_r, dist_c);
        int other = std::max(dist_r, dist_c);
        int corner_max = (sz <= 9) ? 3 : 4; // 3-3/3-4/4-4 on 9x9, up to 3-5/4-5 on 13x13/19x19

        if (total_stones < opening_limit) {
            bool is_tengen = (r == sz / 2 && c == sz / 2 && (sz % 2 == 1));

            if (line == 0) {
                move_score *= 0.1;
            } else if (line == 1) {
                move_score *= local_stone ? 0.6 : 0.35;
            } else if (line == 2 || line == 3) {
                move_score *= (other <= corner_max) ? 2.5 : 1.6;
            } else if (is_tengen) {
                move_score *= (sz <= 9) ? 1.8 : (sz <= 13 ? 1.2 : 1.0);
            } else if (!local_stone) {
                move_score *= 0.6; // Discourage unmotivated floating 5th+ line moves on 13x13/19x19
            }
        } else if (!local_stone && (line == 2 || line == 3)) {
            // Keep open 3rd/4th-line corners & extensions competitive past opening_limit
            move_score *= (other <= corner_max) ? 2.0 : 1.4;
        }
    }

    return std::pow(move_score, 1.25);
}

static void remove_child(MCTSNode* parent, const MCTSNode* target) {
    if (!parent || !target) return;
    auto& children = parent->children;
    for (size_t i = 0; i < children.size(); ++i) {
        if (children[i].get() == target) {
            if (i + 1 != children.size()) {
                std::swap(children[i], children.back());
            }
            children.pop_back();
            return;
        }
    }
}

MCTSNode* MCTSInstance::selectChild(){
    MCTSNode* curr = root.get();
    if (!curr) return nullptr;

    int select_depth = 0;
    while (!curr->is_terminal() && curr->moves_generated && select_depth < 300){
        if (!curr->unexpanded.empty()){
            int new_child_cnt = curr->max_child_org + static_cast<int>(0.5 * std::sqrt(curr->visits));
            if (curr->max_child_cnt < new_child_cnt){
                Color next_player = invertColor(curr->prev_player);
                while (curr->max_child_cnt < new_child_cnt && !curr->unexpanded.empty()){
                    ScoredCandidate cand = curr->unexpanded.back();
                    curr->unexpanded.pop_back();
                    if (curr->unexpanded.empty()) std::vector<ScoredCandidate>().swap(curr->unexpanded);
                    MoveState tmp = board.place(cand.id, next_player);
                    if (tmp.status != MoveStatus::Success) continue;
                    undo_stack.push_back(std::move(*tmp.undo_state));
                    auto child = std::make_unique<MCTSNode>(board, next_player, MoveType::Place, cand.id, curr);
                    child->priority = cand.score / curr->total_child_score;
                    MCTSNode* raw = child.get();
                    curr->children.push_back(std::move(child));
                    curr->max_child_cnt++;
                    return raw;
                }
            }
        }

        MCTSNode* next_child = curr->select_best_child();
        if (!next_child) break;
        if (next_child->type == MoveType::Place){
            MoveState tmp = board.place(next_child->id, next_child->prev_player);
            if (tmp.status != MoveStatus::Success) {
                curr->max_child_cnt = std::max(0, curr->max_child_cnt - 1);
                remove_child(curr, next_child);
                continue;
            }
            undo_stack.push_back(std::move(*tmp.undo_state));
        }
        curr = next_child;
        ++select_depth;
    }

    if (curr->is_terminal()) return curr;

    if (curr != root.get() && curr->visits == 0) {
        return curr;
    }

    Color next_player = invertColor(curr->prev_player);
    if (!curr->moves_generated){
        board.getReasonablePts(next_player, raw_candidates);
        curr->unexpanded.reserve(raw_candidates.size());

        for (int16_t id : raw_candidates) {
            double score = score_candidate_move(id, next_player) + board.rand_below(1024) * 1e-6;
            curr->unexpanded.push_back(ScoredCandidate(id, score));
            curr->total_child_score += score;
        }

        // Ascending order so best moves sit at .back() for O(1) pop_back()
        std::sort(curr->unexpanded.begin(), curr->unexpanded.end(), std::less<ScoredCandidate>());

        double pass_score = (curr->type == MoveType::Pass) ? 16.0 : 0.01;
        curr->total_child_score += pass_score;

        int init_cnt = std::min(curr->max_child_cnt, static_cast<int>(curr->unexpanded.size()));
        curr->children.reserve(init_cnt + 1);
        for (int i = 0; i < init_cnt; ++i){
            ScoredCandidate cand = curr->unexpanded.back();
            curr->unexpanded.pop_back();
            auto child = std::make_unique<MCTSNode>(board, next_player, MoveType::Place, cand.id, curr);
            child->priority = cand.score / curr->total_child_score; // Properly normalized prior!
            curr->children.push_back(std::move(child));
        }
        curr->max_child_cnt = init_cnt;

        auto child = std::make_unique<MCTSNode>(board, next_player, MoveType::Pass, -1, curr);
        child->priority = pass_score / curr->total_child_score;
        curr->children.push_back(std::move(child));

        curr->moves_generated = true;
    }

    // 3. Let PUCT pick the best child (highest prior first, then guided by winrate):
    MCTSNode* best = nullptr;
    while ((best = curr->select_best_child()) != nullptr) {
        if (best->type != MoveType::Place) break;
        MoveState tmp = board.place(best->id, next_player);
        if (tmp.status == MoveStatus::Success) {
            undo_stack.push_back(std::move(*tmp.undo_state));
            break;
        }
        curr->max_child_cnt = std::max(0, curr->max_child_cnt - 1);
        remove_child(curr, best);
    }
    return best ? best : curr;
}

bool MCTSInstance::is_bad_self_atari(int16_t cid, const UndoState& u) {
    int gsz = board.get_group_size(cid);
    return gsz >= 2 &&
           board.get_only_lib(cid) != -1 &&
           static_cast<int>(u.captured.size()) < gsz;
}

int16_t MCTSInstance::play_rand_move(Color color) {
    const int n = board.get_empty_cnt();
    if (n == 0) return -1;

    auto try_play = [&](int16_t cand) -> bool {
        if (board.is_eye(cand, color)) return false;
        MoveState tmp = board.place(cand, color);
        if (tmp.status != MoveStatus::Success) return false;
        if (is_bad_self_atari(cand, *tmp.undo_state)) {
            board.undo(*tmp.undo_state);
            return false;
        }
        undo_stack.push_back(std::move(*tmp.undo_state));
        return true;
    };

    if (n > 8){
        // Fast path: uniform random probes (zero pool copy)
        for (int tries = 0; tries < 8; ++tries) {
            int idx = static_cast<int>(board.rand_below(static_cast<uint32_t>(n)));
            int16_t cand = board.get_empty_cell(idx);
            if (try_play(cand)) return cand;
        }
    }

    // Exhaustive fallback: sample without replacement over empty_cells
    std::array<int16_t, CELL_CNT> pool;
    board.copy_empty_cells(pool.data());
    int rem = n;
    while (rem > 0) {
        int idx = static_cast<int>(board.rand_below(static_cast<uint32_t>(rem)));
        int16_t cand = pool[idx];
        pool[idx] = pool[--rem];
        if (try_play(cand)) return cand;
    }

    return -1;
}

double MCTSInstance::rollout(MCTSNode *node){
    if (node -> type == MoveType::Resign) {
        return (node -> prev_player == Color::Black) ? 1000.0 : -1000.0;
    }

    // Even if node->type == MoveType::Pass (or Pass->Pass), play out remaining non-eye moves
    // so dead stones are captured before Tromp-Taylor area scoring runs.
    const bool is_double_pass = node -> is_double_pass();
    if (is_double_pass && (board.get_black_cnt() + board.get_white_cnt()) <= 2 * board.get_size()) {
        return board.calculate_score();
    }

    int consec_pass = 0;
    Color curr_turn = (is_double_pass && (node->visits & 1))
        ? node->prev_player
        : invertColor(node->prev_player);
    int max_steps = 3 * board.get_size() * board.get_size();
    int steps = 0;
    int16_t prev_move = node -> id;
    while (consec_pass < 2 && steps < max_steps){
        bool is_special_case = false;
        if (prev_move != -1){
            int16_t pos_lib = board.get_only_lib(prev_move);
            if (pos_lib != -1 && !board.is_eye(pos_lib, curr_turn)){
                MoveState tmp = board.place(pos_lib, curr_turn);
                if (tmp.status == MoveStatus::Success){
                    if (is_bad_self_atari(pos_lib, *tmp.undo_state)) {
                        board.undo(*tmp.undo_state);
                    } else {
                        prev_move = pos_lib;
                        undo_stack.push_back(std::move(*tmp.undo_state));
                        is_special_case = true;
                        consec_pass = 0;
                    }
                }
            }
            if (!is_special_case){
                int r = prev_move/board.get_size(), c = prev_move%board.get_size();
                int best_opp_sz = -1; int16_t best_opp_id = -1;
                int best_fr_sz = -1; int16_t best_fr_id = -1;
                for (int i = 0; i < 4; ++i){
                    int nr = r + dr[i], nc = c + dc[i];
                    if (nr < 0 || nr >= board.get_size() || nc < 0 || nc >= board.get_size()) continue;

                    std::optional<Color> neighbor_color = StatetoColor(board.get_cell(nr, nc));
                    if (!neighbor_color.has_value()) continue;

                    int16_t nid = board.get(nr, nc);
                    int16_t libpos = board.get_only_lib(nid);
                    int group_sz = board.get_group_size(nid);
                    if (*neighbor_color == curr_turn && libpos != -1 && best_fr_sz < group_sz){
                        best_fr_sz = group_sz;
                        best_fr_id = libpos;
                    }else if (*neighbor_color == invertColor(curr_turn) && libpos != -1 && best_opp_sz < group_sz){
                        best_opp_sz = group_sz;
                        best_opp_id = libpos;
                    }
                }

                bool prioritize_saving = (best_fr_sz > best_opp_sz);

                if (prioritize_saving && best_fr_sz != -1 && !board.is_eye(best_fr_id, curr_turn)) {
                    MoveState tmp = board.place(best_fr_id, curr_turn);
                    if (tmp.status == MoveStatus::Success) {
                        if (is_bad_self_atari(best_fr_id, *tmp.undo_state)) {
                            board.undo(*tmp.undo_state); // Still atari
                        } else {
                            undo_stack.push_back(std::move(*tmp.undo_state));
                            prev_move = best_fr_id;
                            is_special_case = true;
                            consec_pass = 0;
                        }
                    }
                }

                if (!is_special_case && best_opp_sz != -1 && !board.is_eye(best_opp_id, curr_turn)) {
                    MoveState tmp = board.place(best_opp_id, curr_turn);
                    if (tmp.status == MoveStatus::Success) {
                        if (is_bad_self_atari(best_opp_id, *tmp.undo_state)) {
                            board.undo(*tmp.undo_state);
                        } else {
                            undo_stack.push_back(std::move(*tmp.undo_state));
                            prev_move = best_opp_id;
                            is_special_case = true;
                            consec_pass = 0;
                        }
                    }
                }

                // Fallback to saving if capturing wasn't possible or friendly was smaller
                if (!is_special_case && !prioritize_saving && best_fr_sz != -1 && !board.is_eye(best_fr_id, curr_turn)) {
                    MoveState tmp = board.place(best_fr_id, curr_turn);
                    if (tmp.status == MoveStatus::Success) {
                        if (is_bad_self_atari(best_fr_id, *tmp.undo_state)) {
                            board.undo(*tmp.undo_state);
                        } else {
                            undo_stack.push_back(std::move(*tmp.undo_state));
                            prev_move = best_fr_id;
                            is_special_case = true;
                            consec_pass = 0;
                        }
                    }
                }
            }
        }

        if (!is_special_case && prev_move != -1 && board.rand_below(3) != 0) {
            int r = prev_move / board.get_size(), c = prev_move % board.get_size();
            int16_t cands[8];
            uint32_t weights[8];
            int cnt = 0;
            uint32_t total_w = 0;
            for (int di = -1; di <= 1; ++di) {
                for (int dj = -1; dj <= 1; ++dj) {
                    if (di == 0 && dj == 0) continue;
                    int nr = r + di, nc = c + dj;
                    if (!board.is_empty_cell(nr, nc)) continue;
                    int16_t nid = board.get(nr, nc);
                    if (board.is_eye(nid, curr_turn)) continue;
                    uint16_t h3 = serialize_3x3(board, nr, nc, curr_turn);
                    uint32_t w = global_p3.weight(h3);
                    if (w == 0) continue;
                    cands[cnt] = nid;
                    weights[cnt] = w;
                    total_w += w;
                    ++cnt;
                }
            }
            while (cnt > 0 && total_w > 0) {
                uint32_t pick = board.rand_below(total_w);
                uint32_t acc = 0;
                int chosen = 0;
                for (int i = 0; i < cnt; ++i) {
                    acc += weights[i];
                    if (pick < acc) {
                        chosen = i;
                        break;
                    }
                }
                int16_t cid = cands[chosen];
                MoveState tmp = board.place(cid, curr_turn);
                if (tmp.status == MoveStatus::Success) {
                    // Reject self-atari / snapback moves losing more stones than captured
                    if (is_bad_self_atari(cid, *tmp.undo_state)) {
                        board.undo(*tmp.undo_state);
                    } else {
                        undo_stack.push_back(std::move(*tmp.undo_state));
                        prev_move = cid;
                        is_special_case = true;
                        consec_pass = 0;
                        break;
                    }
                }
                // Swap-remove illegal / self-atari candidate and try another local neighbor
                total_w -= weights[chosen];
                cands[chosen] = cands[cnt - 1];
                weights[chosen] = weights[cnt - 1];
                --cnt;
            }
        }

        if (!is_special_case){
            int16_t id = play_rand_move(curr_turn);
            if (id == -1) {consec_pass++; prev_move = -1;}
            else{
                consec_pass = 0;
                prev_move = id;
            }
        }

        curr_turn = invertColor(curr_turn);
        steps++;
    }

    return board.calculate_score();
}

void MCTSInstance::backprop(MCTSNode *node, double score){
    const double scale = 0.15 * board.get_cell_cnt();
    while (node != nullptr){
        node -> visits++;

        double margin = (node -> prev_player == Color::White) ? score : -score;
        double win = (margin > 0.0) ? 1.0 : 0.0;
        double bonus = 0.5 + 0.5 * std::tanh(margin / scale);
        node -> wins += 0.85 * win + 0.15 * bonus;

        node = node -> parent;
    }
}

void MCTSInstance::rollback(){
    while (!undo_stack.empty()){
        board.undo(undo_stack.back());
        undo_stack.pop_back();
    }
}

MCTSInstance::MCTSInstance(const Board &src_board, Color color, MoveType PrevMoveType){
    AIColor = color;
    board = src_board;
    undo_stack.reserve(3 * board.get_size() * board.get_size());
    root = std::make_unique<MCTSNode>(board, invertColor(AIColor), PrevMoveType);
    raw_candidates.reserve(CELL_CNT);
}

int16_t MCTSInstance::get_best_move(int time_limit_ms){
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(time_limit_ms);

    do{
        MCTSNode* leaf = selectChild();
        double score = rollout(leaf);
        backprop(leaf, score);
        rollback();
    }while (std::chrono::steady_clock::now() <= deadline);


    MCTSNode *best_child = nullptr;
    int max_visits = -1;
    for (const auto& child : root -> children){
        if (child -> visits > max_visits){
            best_child = child.get();
            max_visits = child -> visits;
        }
    }

    return best_child ? best_child->id : -1;
}

// AI-assisted: see AI78
void MCTSInstance::advance_tree(int16_t move_id, MoveType type, Color color){
    if (type == MoveType::Place){board.place(move_id, color);}

    std::unique_ptr<MCTSNode> new_root = nullptr;
    if (root) {
        for (auto& child: root -> children){
            if (child -> id == move_id && child -> type == type){
                new_root = std::move(child);
                new_root -> parent = nullptr;
                break;
            } 
        }
    }

    if (new_root){
        root = std::move(new_root);
    }else{
        root = std::make_unique<MCTSNode>(board, color, type, move_id);
    }
    undo_stack.clear();
}

MCTSThreads::MCTSThreads(const Board &board, Color color, MoveType type, unsigned int requested_cnt){
    unsigned int hw = std::max(1u, std::thread::hardware_concurrency());
    unsigned int default_cnt = hw > 1 ? (hw/2) : 1;
    thread_cnt = requested_cnt ? std::min(requested_cnt, hw) : default_cnt;    
    instances.reserve(thread_cnt);
    std::random_device rd;
    uint32_t base_seed = rd();
    for (unsigned int i = 0; i < thread_cnt; ++i){
        instances.emplace_back(board, color, type);
        instances.back().get_board().seed_gen(base_seed + i * 10007 + 1);
    }
    threads.reserve(thread_cnt);
}

// AI-assisted: see AI78
void MCTSThreads::advance_tree(int16_t move_id, MoveType type, Color color){
    for (unsigned int i = 0; i < thread_cnt; ++i){
        instances[i].advance_tree(move_id, type, color);
    }
}

// AI-assisted: see AI78
int16_t MCTSThreads::get_best_move(int time_limit_ms){
    int start_rollouts = 0;
    for (const MCTSInstance &instance : instances) {
        if (instance.get_root()) {
            start_rollouts += instance.get_root()->visits;
        }
    }

    threads.clear();
    for (unsigned int i = 0; i < thread_cnt; ++i){
        threads.emplace_back(&MCTSInstance::get_best_move, &instances[i], time_limit_ms);
    }

    for (std::thread &t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    total_rollouts = 0;
    int cnt_vis_pass = 0;
    double wins_pass = 0.0;
    std::array<int, CELL_CNT> total_vis{};
    std::array<double, CELL_CNT> total_wins{};
    for (const MCTSInstance &instance: instances){
        const MCTSNode* r = instance.get_root();
        if (!r) continue;
        total_rollouts += r->visits;
        for (const auto &child: r -> children){
            if (child -> id == -1) {
                cnt_vis_pass += child -> visits;
                wins_pass += child -> wins;
            } else {
                total_vis[child -> id] += child -> visits;
                total_wins[child -> id] += child -> wins;
            }
        }
    }
    new_rollouts = total_rollouts - start_rollouts;

    auto max_it = std::max_element(total_vis.begin(), total_vis.end());
    int max_vis = *max_it;
    int16_t best_stone_id = (max_vis > 0) ? static_cast<int16_t>(max_it - total_vis.begin()) : -1;

    bool opp_passed = (instances[0].get_root() && instances[0].get_root()->type == MoveType::Pass);
    if (opp_passed) {
        if (cnt_vis_pass > 0 && (wins_pass / cnt_vis_pass) > 0.55) {
            return -1; // Ahead: pass to win!
        }
        // Behind: do not pass if there is any viable stone move!
        return (best_stone_id != -1) ? best_stone_id : -1;
    }

    if (max_vis == 0) return -1;
    return (max_vis >= cnt_vis_pass) ? best_stone_id : -1;
}