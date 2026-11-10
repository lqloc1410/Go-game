#include <AI.hpp>

MCTSNode::MCTSNode(Board &board, Color prev_p, MoveType t, int16_t move_id, MCTSNode* p)
    : type(t), id(move_id), prev_player(prev_p), parent(p){}

MCTSNode* MCTSNode::select_best_child(double c) const{
    if (this -> visits == 0) return nullptr;
    MCTSNode* res = nullptr;
    double best_UCB = -1.0;
    for (const std::unique_ptr<MCTSNode>& child : children){
        if (child -> visits == 0) return child.get();

        double curr_UCB = (child -> wins / child ->visits) + c * sqrt(log(this -> visits)/child -> visits);
        if (curr_UCB > best_UCB){
            res = child.get();
            best_UCB = curr_UCB;
        }
    }

    return res;
}

MCTSNode* MCTSInstance::selectChild(){
    MCTSNode* curr = root.get();
    if (!curr) return nullptr;

    while (!curr->is_terminal() && curr->is_fully_expanded()){
        curr = curr -> select_best_child();
        if (curr -> type == MoveType::Place){
            MoveState tmp = board.place(curr -> id, curr -> prev_player);
            undo_stack.push_back(std::move(*tmp.undo_state));
        }
    }

    if (curr->is_terminal()) return curr;

    Color next_player = invertColor(curr -> prev_player);
    if (!curr->moves_generated) {
        curr->untried_moves = board.getReasonablePtsShuffled(next_player);
        curr->untried_moves.push_back(-1); // Always O(1)

        // If the opponent DID NOT pass, we want Pass (-1) evaluated last.
        // Move it to index 0 so it's popped last (.pop_back()):
        if (curr->type != MoveType::Pass && curr->untried_moves.size() > 1) {
            std::swap(curr->untried_moves.front(), curr->untried_moves.back()); // O(1)
        }
        // If curr->type == MoveType::Pass, -1 stays at .back() and gets popped FIRST!
        curr->moves_generated = true;
    }

    int16_t id = (curr -> untried_moves).back();
    (curr -> untried_moves).pop_back();
    
    MoveType type;
    if (id == -1){
        type = MoveType::Pass;
    }else{
        type = MoveType::Place;
        MoveState tmp = board.place(id, next_player);
        undo_stack.push_back(std::move(*tmp.undo_state));
    }

    curr->children.push_back(std::make_unique<MCTSNode>(board, next_player, type, id, curr));
    return curr->children.back().get();
}

Color MCTSInstance::rollout(MCTSNode *node){
    if (node -> is_terminal()){
        if (node -> type == MoveType::Resign) return invertColor(node -> prev_player);
        double score = board.calculate_score();

        // No support for draws yet
        return score > 0.0 ? Color::White : Color::Black;
    }

    int consec_pass = node -> type == MoveType::Pass ? 1 : 0;
    Color curr_turn = invertColor(node -> prev_player);
    int max_steps = 3 * board.get_size() * board.get_size();
    int steps = 0;
    while (consec_pass < 2 && steps < max_steps){
        int16_t id = board.getRandPoint(curr_turn);

        if (id == -1) consec_pass++;
        else{
            consec_pass = 0;
            MoveState tmp = board.place(id, curr_turn);
            undo_stack.push_back(std::move(*tmp.undo_state));
        }

        curr_turn = invertColor(curr_turn);
        steps++;
    }

    double score = board.calculate_score();
    return score > 0.0 ? Color::White : Color::Black;
}

void MCTSInstance::backprop(MCTSNode *node, Color winner){
    while (node != nullptr){
        node -> visits++;

        if (node -> prev_player == winner){
            node -> wins++;
        }

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
}

int16_t MCTSInstance::get_best_move(int time_limit_ms){
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(time_limit_ms);

    do{
        MCTSNode* leaf = selectChild();
        Color winner = rollout(leaf);
        backprop(leaf, winner);
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

MCTSThreads::MCTSThreads(const Board &board, Color color, MoveType type, unsigned int requested_cnt){
    unsigned int hw = std::max(1u, std::thread::hardware_concurrency());
    unsigned int default_cnt = hw > 1 ? hw - 1 : 1;
    thread_cnt = requested_cnt ? std::min(requested_cnt, hw) : default_cnt;    
    instances.reserve(thread_cnt);
    for (unsigned int i = 0; i < thread_cnt; ++i){
        instances.emplace_back(board, color, type);
    }
    threads.reserve(thread_cnt);
}

int16_t MCTSThreads::get_best_move(int time_limit_ms){
    threads.clear();
    for (unsigned int i = 0; i < thread_cnt; ++i){
        threads.emplace_back(&MCTSInstance::get_best_move, &instances[i], time_limit_ms);
    }

    for (std::thread &t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    int cell_cnt = instances[0].get_board().get_cell_cnt();

    int cnt_vis_pass = 0;
    std::array<int, CELL_CNT> total_vis{};
    for (const MCTSInstance &instance: instances){
        for (const auto &child: instance.get_root() -> children){
            if (child -> id == -1) cnt_vis_pass += child -> visits;
            else{
                total_vis[child -> id] += child -> visits;
            }
        }
    }

    auto max_val = std::max_element(total_vis.begin(), total_vis.end());
    return *max_val > cnt_vis_pass ? max_val - total_vis.begin() : -1;
}