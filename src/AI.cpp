#include <AI.hpp>

using namespace AIConstants;

static const patterns3x3 global_p3("lookup-tables/pattern3x3.bin");
static const patterns5x5 global_p5("lookup-tables/pattern5x5.bin");

MCTSNode::MCTSNode(Board &board, Color prev_p, MoveType t, int16_t move_id, MCTSNode* p)
    : type(t), id(move_id), prev_player(prev_p), parent(p){}

MCTSNode* MCTSNode::select_best_child(double c_puct) const{
    if (children.empty()) return nullptr;
    MCTSNode* best_child = nullptr;
    double best_score = kInitialBestScore;

    // First Play Urgency (FPU): unvisited children start slightly below parent's expected winrate
    double parent_q = (this->visits > 0) 
        ? std::max(0.0, (1.0 - (this->wins / this->visits)) - kFpuReduction) 
        : kDefaultParentWinrate;
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
    // Never prioritize filling our own true eye
    if (board.is_eye(id, player)) return kOwnEyeScore;
    int r = id / board.get_size(), c = id % board.get_size();
    auto [h3, h5] = serialize_patterns(board, r, c, player);

    // Base score from 3x3 and 5x5 pro/master pattern frequencies (only if stones exist nearby)
    double s3 = pattern3x3_has_stone(h3) ? std::log1p(static_cast<double>(global_p3[h3])) : 0.0;
    double s5 = pattern5x5_has_stone(h5) ? std::log1p(static_cast<double>(global_p5[h5])) : 0.0;
    double move_score = kBaseMoveScore + s3 + kPattern5x5Weight * s5;

    // Scan the 4 orthogonal neighbors to collect tactical context around `id`
    int save_sz = 0, defend_2lib_sz = 0, cap_sz = 0, atari_sz = 0;
    int empty_neighbors = 0, max_friendly_libs = 0, friendly_gsz_sum = 0;
    int min_opp_libs = kInfLiberties, opp_group_cnt = 0;
    int16_t empty_ids[4]{};

    SmallVec4 distinct_groups;
    SmallVec4 friendly_roots;
    SmallVec4 cap_roots;

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
                if (libs == 1) save_sz += gsz;              // Friendly group in atari (1 lib)
                else if (libs == 2) defend_2lib_sz += gsz;  // Friendly group in pre-atari (2 libs)
                friendly_roots.push_back(n_root);
                friendly_gsz_sum += gsz;
            } else {
                opp_group_cnt++;
                if (libs < min_opp_libs) min_opp_libs = libs;
                if (libs == 1) {
                    cap_sz += gsz;                          // Opponent group we immediately capture
                    cap_roots.push_back(n_root);
                } else if (libs == 2) {
                    atari_sz += gsz;                        // Opponent group we put into atari
                }
            }
            distinct_groups.push_back(n_root);
        }
    }

    // Counter-capture check: if capturing an opponent group (`cap_roots`) also rescues
    // nearby friendly 1-liberty groups not directly adjacent to `id`, count them in `save_sz`.
    if (cap_roots.sz > 0) {
        State my_state = ColortoState(player);
        int bsz = board.get_size();
        int16_t extra_saved[kMaxCounterCaptureChain]{};
        int extra_saved_cnt = 0;
        for (int ci = 0; ci < cap_roots.sz; ++ci) {
            int16_t start = cap_roots.data[ci];
            int16_t curr = start;
            int chain_steps = 0;
            do {
                int sr = curr / bsz, sc = curr % bsz;
                for (int d = 0; d < 4; ++d) {
                    int nr = sr + dr[d], nc = sc + dc[d];
                    if (board.get_cell(nr, nc) != my_state) continue;
                    int16_t f_root = board.get_root(nr, nc);
                    if (board.count_lib(f_root) == 1 && friendly_roots.find(f_root) == -1) {
                        bool seen = false;
                        for (int k = 0; k < extra_saved_cnt; ++k) {
                            if (extra_saved[k] == f_root) {
                                seen = true;
                                break;
                            }
                        }
                        if (!seen) {
                            save_sz += board.get_group_size(f_root);
                            if (extra_saved_cnt < kMaxCounterCaptureChain) extra_saved[extra_saved_cnt++] = f_root;
                        }
                    }
                }
                curr = board.get_next_stone(curr);
            } while (curr != start && ++chain_steps < kMaxCounterCaptureChain);
        }
    }

    // Compute exact post-move liberties (`new_libs`) and merged group size (`new_gsz`)
    int new_libs = 0;
    int new_gsz = 0;
    bool have_result = false;

    if (cap_sz == 0) {
        if (friendly_roots.sz == 0) {
            // Isolated stone: liberties = empty neighbors
            new_libs = empty_neighbors;
            new_gsz = 1;
        } else if (friendly_roots.sz == 1 && empty_neighbors == 0) {
            // Extending a single group with no new empty neighbors loses 1 liberty (occupied by `id`)
            new_libs = max_friendly_libs - 1;
            new_gsz = 1 + friendly_gsz_sum;
        } else {
            // Union liberties of all merged friendly groups + new empty neighbors, minus `id` itself
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
    } else if (empty_neighbors >= 2 || max_friendly_libs >= kSafeCaptureMinLibs) {
        // Capturing >=1 opponent stone while having >=2 empty neighbors or a >=3-lib ally guarantees >=3 libs
        new_libs = kSafeCaptureMinLibs;
        new_gsz = 1 + friendly_gsz_sum;
        have_result = true;
    } else {
        // Capturing in tight quarters: simulate place+undo to get exact liberties after removals
        MoveState sim = board.place(id, player);
        if (sim.status == MoveStatus::Success) {
            new_libs = board.count_lib(id);
            new_gsz = board.get_group_size(id);
            board.undo(*sim.undo_state);
            have_result = true;
        }
    }

    if (have_result) {
        bool apply_bonuses = false;
        if (new_libs == 1 && cap_sz < new_gsz) {
            // Case A: Self-atari or losing snapback (1 liberty left and fewer stones captured than risked)
            if (new_gsz >= 2) {
                // Multi-stone self-atari: almost always a blunder
                move_score *= kMultiStoneSelfAtariPenalty;
            } else {
                // Single-stone self-atari (`cap_sz == 0`): only mildly viable if throwing in to put opp in atari
                move_score *= (atari_sz > 0) ? kThrowInAtariPenalty : kSingleStoneSelfAtariPenalty;
            }
        } else if (cap_sz == 0 && new_libs >= 2) {
            // Case B: Non-capturing safe move (`new_libs >= 2`), check for self-liberty tightening
            if (new_libs == 2 && max_friendly_libs >= kSafeCaptureMinLibs &&
                (save_sz == 0 || (friendly_gsz_sum - save_sz) >= save_sz)) {
                // Avoid dragging a >=3-liberty group down to 2 liberties (unless rescuing a bigger atari group)
                move_score *= kSelfLibertyTighteningPenalty;
            } else if (friendly_roots.sz == 1 && empty_neighbors == 0 &&
                       save_sz == 0 && atari_sz == 0) {
                // Playing on an internal/adjacent point of a single friendly group with no new empty libs:
                if (opp_group_cnt == 0) {
                    // Purely filling our own group's liberties without touching any opponent stone
                    move_score *= kPureSelfLibertyFillPenalty;
                } else if (new_libs < min_opp_libs - 1) {
                    // Filling a shared liberty when we already have fewer liberties than the opponent
                    move_score *= kSharedLibertyDisadvantagePenalty;
                } else {
                    apply_bonuses = true;
                }
            } else {
                apply_bonuses = true;
            }
        } else {
            // Case C: Safe capture, Ko capture (`cap_sz == 1, new_gsz == 1`), or net-positive capture
            apply_bonuses = true;
        }

        if (apply_bonuses) {
            if (cap_sz > 0) move_score += kCaptureBonusPerStone * cap_sz;
            if (new_libs >= 2) {
                if (save_sz > 0) move_score += (new_libs >= kSafeCaptureMinLibs ? kSaveAtariSafeBonusPerStone : kSaveAtariTightBonusPerStone) * save_sz;
                if (defend_2lib_sz > 0 && new_libs >= kSafeCaptureMinLibs) move_score += kDefendTwoLibBonusPerStone * defend_2lib_sz;
                if (friendly_roots.sz >= 2 && new_libs >= kSafeCaptureMinLibs) move_score += kConnectGroupsBonusPerStone * friendly_gsz_sum; // Connecting >=2 groups safely
                if (atari_sz > 0) move_score += (new_libs >= kSafeCaptureMinLibs ? kAtariSafeBonusPerStone : kAtariTightBonusPerStone) * atari_sz;
            }
        }
    }

    // Opening & early-midgame positional geometry (only when move has no local tactical urgency)
    int sz = board.get_size();
    int total_stones = board.get_white_cnt() + board.get_black_cnt();
    int opening_limit = std::max(kMinOpeningStoneLimit, board.get_cell_cnt() / kOpeningCellDivisor); // 8 (9x9), 14 (13x13), 30 (19x19)
    int strategic_limit = board.get_cell_cnt() / kStrategicCellDivisor;                             // 27 (9x9), 56 (13x13), 120 (19x19)
    bool local_stone = pattern5x5_has_stone(h5);

    if (save_sz == 0 && cap_sz == 0 && atari_sz == 0 && defend_2lib_sz == 0 && total_stones < strategic_limit) {
        int dist_r = std::min(r, sz - 1 - r);
        int dist_c = std::min(c, sz - 1 - c);
        int line = std::min(dist_r, dist_c);
        int other = std::max(dist_r, dist_c);
        int corner_max = (sz <= kSmallBoardSize) ? kSmallBoardCornerMax : kLargeBoardCornerMax; // 3-3/3-4/4-4 on 9x9, up to 3-5/4-5 on 13x13/19x19

        if (total_stones < opening_limit) {
            bool is_tengen = (r == sz / 2 && c == sz / 2 && (sz % 2 == 1));

            if (line == 0) {
                // Opening: strongly avoid 1st-line edge moves
                move_score *= kOpeningFirstLinePenalty;
            } else if (line == 1) {
                // Opening: discourage 2nd-line moves unless anchored by a nearby stone
                move_score *= local_stone ? kOpeningSecondLineAnchoredPenalty : kOpeningSecondLineEmptyPenalty;
            } else if (line == 2 || line == 3) {
                // Opening: favor 3rd/4th-line corner enclosures and side points
                move_score *= (other <= corner_max) ? kOpeningCornerBonus : kOpeningSideBonus;
            } else if (is_tengen) {
                move_score *= (sz <= kSmallBoardSize) ? kTengenSmallBoardBonus : (sz <= kMediumBoardSize ? kTengenMediumBoardBonus : kTengenLargeBoardBonus);
            } else if (!local_stone) {
                move_score *= kOpeningFloatingCenterPenalty; // Discourage unmotivated floating 5th+ line moves on 13x13/19x19
            }
        } else {
            if (line == 0 && friendly_roots.sz < 2) {
                // Early-midgame: still discourage non-connecting 1st-line crawls
                move_score *= local_stone ? kMidgameFirstLineAnchoredPenalty : kMidgameFirstLineEmptyPenalty;
            } else if (!local_stone && (line == 2 || line == 3)) {
                // Keep open 3rd/4th-line corners & extensions competitive past opening_limit
                move_score *= (other <= corner_max) ? kMidgameOpenCornerBonus : kMidgameOpenSideBonus;
            }
        }
    }

    return std::pow(move_score, kMoveScoreExponent);
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
    while (!curr->is_terminal() && curr->moves_generated && select_depth < kMaxSelectDepth){
        // Progressive widening: unlock more unexpanded moves as `curr->visits` grows
        if (!curr->unexpanded.empty()){
            int new_child_cnt = curr->max_child_org + static_cast<int>(kProgressiveWideningCoeff * std::sqrt(curr->visits));
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
            double score = score_candidate_move(id, next_player) + board.rand_below(kTieBreakerRange) * kTieBreakerScale;
            curr->unexpanded.push_back(ScoredCandidate(id, score));
            curr->total_child_score += score;
        }

        // Ascending order so best moves sit at .back() for O(1) pop_back()
        std::sort(curr->unexpanded.begin(), curr->unexpanded.end(), std::less<ScoredCandidate>());

        // Give Pass a strong prior only if opponent passed at the root or no board moves exist;
        // mid-tree pass nodes stay low-priority so MCTS doesn't assume the opponent will pass back.
        double pass_score = ((curr == root.get() && curr->type == MoveType::Pass) || curr->unexpanded.empty()) ? kRootPassScore : kMidTreePassScore;
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
    int16_t only_lib = board.get_only_lib(cid);
    if (only_lib == -1) return false; // Group has >=2 liberties -> safe
    int gsz = board.get_group_size(cid);
    int cap_cnt = static_cast<int>(u.captured.size());
    if (cap_cnt >= gsz) return false; // Net-positive capture or valid Ko capture
    if (gsz >= 2) return true;        // Multi-stone self-atari losing more stones than captured

    // Single-stone self-atari capturing nothing: only allow as a throw-in if it puts
    // an adjacent >=2-stone opponent group into 1-liberty atari.
    int sz = board.get_size();
    int r = cid / sz, c = cid % sz;
    for (int i = 0; i < 4; ++i) {
        int nr = r + dr[i], nc = c + dc[i];
        if (nr < 0 || nr >= sz || nc < 0 || nc >= sz) continue;
        int16_t nid = board.get(nr, nc);
        if (nid != only_lib &&
            board.get_only_lib(nid) != -1 &&
            board.get_group_size(nid) >= 2) {
            return false;
        }
    }
    return true;
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

    if (n > kFastRandomProbeThreshold){
        // Fast path: uniform random probes (zero pool copy)
        for (int tries = 0; tries < kFastRandomProbeAttempts; ++tries) {
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
        return (node -> prev_player == Color::Black) ? kResignScore : -kResignScore;
    }

    // Even if node->type == MoveType::Pass (or Pass->Pass), play out remaining non-eye moves
    // so dead stones are captured before Tromp-Taylor area scoring runs.
    const bool is_double_pass = node -> is_double_pass();
    if (is_double_pass && (board.get_black_cnt() + board.get_white_cnt()) <= kEarlyDoublePassStoneFactor * board.get_size()) {
        return board.calculate_score();
    }

    int consec_pass = 0;
    Color curr_turn = (is_double_pass && (node->visits & 1))
        ? node->prev_player
        : invertColor(node->prev_player);
    int max_steps = kRolloutMaxStepsFactor * board.get_size() * board.get_size();
    int steps = 0;
    int16_t prev_move = node -> id;
    while (consec_pass < 2 && steps < max_steps){
        bool is_special_case = false;
        if (prev_move != -1){
            // Rollout heuristic 1: if the last move left itself in 1-liberty atari, capture it
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
            // Rollout heuristic 2: check orthogonal neighbors of `prev_move` for friendly or
            // opponent groups in atari; save our largest group (or counter-capture) vs capture theirs.
            if (!is_special_case){
                int r = prev_move/board.get_size(), c = prev_move%board.get_size();
                int best_opp_sz = -1; int16_t best_opp_id = -1;
                int best_fr_sz = -1; int16_t best_fr_id = -1;
                int16_t best_fr_stone = -1;
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
                        best_fr_stone = nid;
                    }else if (*neighbor_color == invertColor(curr_turn) && libpos != -1 && best_opp_sz < group_sz){
                        best_opp_sz = group_sz;
                        best_opp_id = libpos;
                    }
                }

                auto try_play_tactical = [&](int16_t move_id) -> bool {
                    if (move_id == -1 || board.is_eye(move_id, curr_turn)) return false;
                    MoveState tmp = board.place(move_id, curr_turn);
                    if (tmp.status != MoveStatus::Success) return false;
                    if (is_bad_self_atari(move_id, *tmp.undo_state)) {
                        board.undo(*tmp.undo_state);
                        return false;
                    }
                    undo_stack.push_back(std::move(*tmp.undo_state));
                    prev_move = move_id;
                    is_special_case = true;
                    consec_pass = 0;
                    return true;
                };

                auto try_save_or_counter_capture = [&]() -> bool {
                    if (best_fr_sz == -1) return false;
                    if (try_play_tactical(best_fr_id)) return true;

                    // Extending failed or is self-atari; check adjacent opponent groups in atari for counter-capture
                    State opp_state = ColortoState(invertColor(curr_turn));
                    int sz = board.get_size();
                    int16_t curr = best_fr_stone;
                    int chain_steps = 0;
                    do {
                        int sr = curr / sz, sc = curr % sz;
                        for (int d = 0; d < 4; ++d) {
                            int nr = sr + dr[d], nc = sc + dc[d];
                            if (board.get_cell(nr, nc) != opp_state) continue;
                            int16_t opp_id = board.get(nr, nc);
                            int16_t cap_lib = board.get_only_lib(opp_id);
                            if (cap_lib != -1 && try_play_tactical(cap_lib)) {
                                return true;
                            }
                        }
                        curr = board.get_next_stone(curr);
                    } while (curr != best_fr_stone && ++chain_steps < kRolloutCounterCaptureMaxSteps);
                    return false;
                };

                bool prioritize_saving = (best_fr_sz > best_opp_sz);

                if (prioritize_saving) {
                    try_save_or_counter_capture();
                }

                if (!is_special_case && best_opp_sz != -1) {
                    try_play_tactical(best_opp_id);
                }

                // Fallback to saving or counter-capturing if capturing wasn't possible or friendly was smaller
                if (!is_special_case && !prioritize_saving) {
                    try_save_or_counter_capture();
                }
            }
        }

        // Rollout heuristic 3: 2/3 chance to pick a 3x3 pattern-weighted neighbor of `prev_move`
        if (!is_special_case && prev_move != -1 && board.rand_below(kLocalPatternRolloutDenom) != 0) {
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

        // Rollout fallback: uniform random legal non-eye, non-self-atari move
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
    const double scale = kScoreMarginScaleFactor * board.get_cell_cnt();
    while (node != nullptr){
        node -> visits++;

        // `score` is (White - Black + komi); flip sign so `margin > 0` means `node->prev_player` won
        double margin = (node -> prev_player == Color::White) ? score : -score;
        double win = (margin > 0.0) ? 1.0 : 0.0;
        double bonus = 0.5 + 0.5 * std::tanh(margin / scale);
        // 85% binary win/loss + 15% smooth score margin to break ties in clearly won/lost games
        node -> wins += kWinrateWeight * win + kScoreMarginWeight * bonus;

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
    undo_stack.reserve(kRolloutMaxStepsFactor * board.get_size() * board.get_size());
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
        instances.back().get_board().seed_gen(base_seed + i * kThreadSeedStride + 1);
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

    double pass_wr = (cnt_vis_pass > 0) ? (wins_pass / cnt_vis_pass) : 0.0;
    double best_stone_wr = (best_stone_id != -1 && max_vis > 0) ? (total_wins[best_stone_id] / max_vis) : 0.0;

    bool opp_passed = (instances[0].get_root() && instances[0].get_root()->type == MoveType::Pass);
    if (opp_passed) {
        if (best_stone_id == -1 || max_vis == 0) return -1;
        // Case 1: Ahead after opponent passed -> pass to end the game and win
        if (cnt_vis_pass > 0 && pass_wr > kPassAfterOppPassWinrate) {
            return -1;
        }
        // Case 2: Hopelessly behind (<15% winrate) or no stone move beats passing -> pass cleanly
        if (best_stone_wr < kHopelessPositionWinrate || (cnt_vis_pass > 0 && best_stone_wr <= pass_wr + kPassWinrateMargin)) {
            return -1;
        }
        return best_stone_id;
    }

    // Normal turn (opponent did not pass): only pass if no stone moves exist or Pass clearly dominates
    if (best_stone_id == -1 || max_vis == 0) return -1;
    if (cnt_vis_pass > max_vis && pass_wr > kNormalTurnPassWinrate) return -1;
    return best_stone_id;
}