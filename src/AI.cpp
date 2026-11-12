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
    int r = id / board.get_size(), c = id % board.get_size();
    uint16_t h3 = serializer3x3(board, r, c, player);
    uint64_t h5 = serializer5x5(board, r, c, player);

    double s3 = (h3 == 0) ? 0.0 : std::log1p(static_cast<double>(global_p3[h3]));
    double s5 = (h5 == 0) ? 0.0 : std::log1p(static_cast<double>(global_p5[h5]));
    double move_score = 1.0 + s3 + 3.0 * s5;

    // Tactical bonuses for saving our own groups or capturing/pressuring opponent groups
    int save_sz = 0, cap_sz = 0, atari_sz = 0;
    Color opp = invertColor(player);

    SmallVec4 distinct_groups;

    for (int i = 0; i < 4; ++i) {
        int nr = r + dr[i], nc = c + dc[i];
        if (nr < 0 || nr >= board.get_size() || nc < 0 || nc >= board.get_size()) continue;
        auto ncol = StatetoColor(board.get_cell(nr, nc));
        if (!ncol.has_value()) continue;
        int16_t n_root = board.get_root(nr, nc);
        if (distinct_groups.find(n_root) == -1) {
            int libs = board.count_lib(n_root);
            int gsz = board.get_group_size(n_root);
            if (*ncol == player && libs == 1) save_sz += gsz;
            else if (*ncol == opp && libs == 1) cap_sz += gsz;
            else if (*ncol == opp && libs == 2) atari_sz += gsz;
            distinct_groups.push_back(n_root);
        }
    }
    if (cap_sz > 0) move_score += 20.0 * cap_sz;

    // Simulate move to check resulting liberties of the placed stone's group
    MoveState sim = board.place(id, player);
    if (sim.status == MoveStatus::Success) {
        int new_libs = board.count_lib(id);
        int new_gsz = board.get_group_size(id);
        board.undo(*sim.undo_state);

        if (new_libs == 1) {
            // Self-atari: heavily penalize unless capturing a larger group
            if (cap_sz == 0) {
                move_score = (new_gsz >= 2) ? 0.02 : 0.25;
            }
        } else {
            if (save_sz > 0) move_score += 22.0 * save_sz;
            if (atari_sz > 0) move_score += 5.0 * atari_sz;
        }
    }

    int opening_limit = (board.get_size() <= 9) ? 12 : 30;
    if ((save_sz == 0 && cap_sz == 0 && atari_sz == 0) && board.get_white_cnt() + board.get_black_cnt() < opening_limit) {
        int dist_r = std::min(r, board.get_size() - 1 - r);
        int dist_c = std::min(c, board.get_size() - 1 - c);
        int line = std::min(dist_r, dist_c); // 0 = 1st line (edge), 1 = 2nd line, 2 = 3rd line...

        if (line == 0) move_score *= 0.1;       // Heavily penalize 1st line edge
        else if (line == 1) move_score *= 0.4;  // Penalize 2nd line
        else if (line >= 2) move_score *= 2.5;  // Reward 3rd & 4th line (corners / Tengen)
    }

    return move_score * move_score;
}

MCTSNode* MCTSInstance::selectChild(){
    MCTSNode* curr = root.get();
    if (!curr) return nullptr;

    int select_depth = 0;
    while (!curr->is_terminal() && curr->is_fully_expanded() && ++select_depth < 300){
        MCTSNode* next_child = curr->select_best_child();
        if (!next_child) break;
        curr = next_child;
        if (curr->type == MoveType::Place){
            MoveState tmp = board.place(curr->id, curr->prev_player);
            if (tmp.status != MoveStatus::Success) {
                curr->type = MoveType::Pass;
                curr->id = -1;
                break;
            }
            undo_stack.push_back(std::move(*tmp.undo_state));
        }
    }

    if (curr->is_terminal()) return curr;

    if (curr != root.get() && curr->visits == 0) {
        return curr;
    }

    Color next_player = invertColor(curr->prev_player);
    if (!curr->moves_generated) {
        move_priority.clear();
        std::vector<int16_t> candidates = board.getReasonablePtsShuffled(next_player);

        for (int16_t id : candidates) {
            double score = score_candidate_move(id, next_player);
            move_priority.push_back(ScoredCandidate(id, score));
        }

        if (static_cast<int>(move_priority.size()) > max_expansion_size){
            std::nth_element(move_priority.begin(), move_priority.begin() + max_expansion_size, move_priority.end(), std::greater<ScoredCandidate>());
            move_priority.resize(max_expansion_size);
        }

        double total_score = 0.0;
        for (const ScoredCandidate& prospectives: move_priority){
            total_score += prospectives.score;
        }

        double pass_score = (curr->type == MoveType::Pass) ? 16.0 : 0.01;
        total_score += pass_score;

        for (const ScoredCandidate& prospectives: move_priority){
            int16_t id = prospectives.id;
            MoveType type = MoveType::Place;
            auto child = std::make_unique<MCTSNode>(board, next_player, type, id, curr);
            child->priority = prospectives.score / (total_score); // Properly normalized prior!
            curr->children.push_back(std::move(child));
        }

        auto child = std::make_unique<MCTSNode>(board, next_player, MoveType::Pass, -1, curr);
        child->priority = pass_score / (total_score); 
        curr->children.push_back(std::move(child));

        curr->moves_generated = true;
    }

    // 3. Let PUCT pick the best child (highest prior first, then guided by winrate):
    MCTSNode* best = curr->select_best_child();
    if (best && best->type == MoveType::Place) {
        MoveState tmp = board.place(best->id, next_player);
        if (tmp.status != MoveStatus::Success) {
            best->type = MoveType::Pass;
            best->id = -1;
        } else {
            undo_stack.push_back(std::move(*tmp.undo_state));
        }
    }
    return best ? best : curr;
}

double MCTSInstance::rollout(MCTSNode *node){
    if (node -> type == MoveType::Resign) {
        return (node -> prev_player == Color::Black) ? 1000.0 : -1000.0;
    }

    // Even if node->type == MoveType::Pass (or Pass->Pass), play out remaining non-eye moves
    // so dead stones are captured before Tromp-Taylor area scoring runs.
    int consec_pass = 0;
    Color curr_turn = invertColor(node -> prev_player);
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
                    prev_move = pos_lib;
                    undo_stack.push_back(std::move(*tmp.undo_state));
                    is_special_case = true;
                    consec_pass = 0;
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
                        if (board.get_only_lib(best_fr_id) != -1) {
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
                        undo_stack.push_back(std::move(*tmp.undo_state));
                        prev_move = best_opp_id;
                        is_special_case = true;
                        consec_pass = 0;
                    }
                }

                // Fallback to saving if capturing wasn't possible or friendly was smaller
                if (!is_special_case && !prioritize_saving && best_fr_sz != -1 && !board.is_eye(best_fr_id, curr_turn)) {
                    MoveState tmp = board.place(best_fr_id, curr_turn);
                    if (tmp.status == MoveStatus::Success) {
                        if (board.get_only_lib(best_fr_id) != -1) {
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

        if (!is_special_case){
            int16_t id = -1;
            for (int tries = 0; tries < 8; ++tries) {
                int16_t cand = board.getRandPoint(curr_turn);
                if (cand == -1) break;
                MoveState tmp = board.place(cand, curr_turn);
                if (tmp.status != MoveStatus::Success) continue;
                // Reject self-atari of 2+ stones so groups don't kill themselves in playouts
                if (tries < 7 && board.get_only_lib(cand) != -1 && board.get_group_size(cand) >= 2) {
                    board.undo(*tmp.undo_state);
                    continue;
                }
                undo_stack.push_back(std::move(*tmp.undo_state));
                id = cand;
                break;
            }

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

MCTSInstance::MCTSInstance(const Board &src_board, Color color, MoveType PrevMoveType, int expansion_size){
    AIColor = color;
    board = src_board;
    undo_stack.reserve(3 * board.get_size() * board.get_size());
    root = std::make_unique<MCTSNode>(board, invertColor(AIColor), PrevMoveType);
    max_expansion_size = expansion_size;
    move_priority.reserve(361);
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