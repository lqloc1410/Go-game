#pragma once
#include <vector>
#include <cstdint>
#include <random>
#include <queue>
#include <chrono>
#include <random>
#include <chrono>
#include <memory>
#include <cmath>
#include <thread>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <GameLogic.hpp>

namespace AIConstants {
    // PUCT & selection
    inline constexpr double kInitialBestScore = -1e9;
    inline constexpr double kFpuReduction = 0.25;
    inline constexpr double kDefaultParentWinrate = 0.5;
    inline constexpr int kMaxSelectDepth = 300;
    inline constexpr double kProgressiveWideningCoeff = 0.5;
    inline constexpr uint32_t kTieBreakerRange = 1024;
    inline constexpr double kTieBreakerScale = 1e-6;
    inline constexpr double kRootPassScore = 16.0;
    inline constexpr double kMidTreePassScore = 0.01;

    // Candidate move scoring: pattern & tactical base
    inline constexpr double kOwnEyeScore = 0.001;
    inline constexpr double kBaseMoveScore = 1.0;
    inline constexpr double kPattern5x5Weight = 3.0;
    inline constexpr int kInfLiberties = 999;
    inline constexpr int kMaxCounterCaptureChain = 16;
    inline constexpr int kSafeCaptureMinLibs = 3;

    // Candidate move scoring: penalties & bonuses
    inline constexpr double kMultiStoneSelfAtariPenalty = 0.01;
    inline constexpr double kThrowInAtariPenalty = 0.15;
    inline constexpr double kSingleStoneSelfAtariPenalty = 0.02;
    inline constexpr double kSelfLibertyTighteningPenalty = 0.05;
    inline constexpr double kPureSelfLibertyFillPenalty = 0.02;
    inline constexpr double kSharedLibertyDisadvantagePenalty = 0.25;
    inline constexpr double kCaptureBonusPerStone = 20.0;
    inline constexpr double kSaveAtariSafeBonusPerStone = 22.0;
    inline constexpr double kSaveAtariTightBonusPerStone = 12.0;
    inline constexpr double kDefendTwoLibBonusPerStone = 6.0;
    inline constexpr double kConnectGroupsBonusPerStone = 2.5;
    inline constexpr double kAtariSafeBonusPerStone = 5.0;
    inline constexpr double kAtariTightBonusPerStone = 2.5;

    // Candidate move scoring: opening & positional geometry
    inline constexpr int kMinOpeningStoneLimit = 8;
    inline constexpr int kOpeningCellDivisor = 12;
    inline constexpr int kStrategicCellDivisor = 3;
    inline constexpr int kSmallBoardSize = 9;
    inline constexpr int kMediumBoardSize = 13;
    inline constexpr int kSmallBoardCornerMax = 3;
    inline constexpr int kLargeBoardCornerMax = 4;
    inline constexpr double kOpeningFirstLinePenalty = 0.1;
    inline constexpr double kOpeningSecondLineAnchoredPenalty = 0.6;
    inline constexpr double kOpeningSecondLineEmptyPenalty = 0.35;
    inline constexpr double kOpeningCornerBonus = 2.5;
    inline constexpr double kOpeningSideBonus = 1.6;
    inline constexpr double kTengenSmallBoardBonus = 1.8;
    inline constexpr double kTengenMediumBoardBonus = 1.2;
    inline constexpr double kTengenLargeBoardBonus = 1.0;
    inline constexpr double kOpeningFloatingCenterPenalty = 0.6;
    inline constexpr double kMidgameFirstLineAnchoredPenalty = 0.35;
    inline constexpr double kMidgameFirstLineEmptyPenalty = 0.15;
    inline constexpr double kMidgameOpenCornerBonus = 2.0;
    inline constexpr double kMidgameOpenSideBonus = 1.4;
    inline constexpr double kMoveScoreExponent = 1.25;

    // Rollout & random play
    inline constexpr int kFastRandomProbeThreshold = 8;
    inline constexpr int kFastRandomProbeAttempts = 8;
    inline constexpr double kResignScore = 1000.0;
    inline constexpr int kEarlyDoublePassStoneFactor = 2;
    inline constexpr int kRolloutMaxStepsFactor = 3;
    inline constexpr int kRolloutCounterCaptureMaxSteps = 12;
    inline constexpr uint32_t kLocalPatternRolloutDenom = 3;

    // Backpropagation
    inline constexpr double kScoreMarginScaleFactor = 0.15;
    inline constexpr double kWinrateWeight = 0.85;
    inline constexpr double kScoreMarginWeight = 0.15;

    // Multi-threading & pass decision thresholds
    inline constexpr uint32_t kThreadSeedStride = 10007U;
    inline constexpr double kPassAfterOppPassWinrate = 0.50;
    inline constexpr double kHopelessPositionWinrate = 0.15;
    inline constexpr double kPassWinrateMargin = 0.03;
    inline constexpr double kNormalTurnPassWinrate = 0.85;
}

static inline std::pair<int, int> transform_offset(int i, int j, int k) {
    switch (k) {
        case 0: return { i,  j}; // Identity (0 deg)
        case 1: return { j, -i}; // Rot 90 CW
        case 2: return {-i, -j}; // Rot 180
        case 3: return {-j,  i}; // Rot 270 CW
        case 4: return { i, -j}; // Flip Horizontal
        case 5: return { j,  i}; // Flip Diagonal (Transpose)
        case 6: return {-i,  j}; // Flip Vertical
        case 7: return {-j, -i}; // Flip Anti-diagonal
        default: return {i, j};
    }
}

inline uint16_t serialize_3x3(
    const Board& board, int16_t r_center, int16_t c_center, Color color)
{
    static constexpr uint8_t kMapBlack[4] = {0, 1, 2, 3};
    static constexpr uint8_t kMapWhite[4] = {0, 2, 1, 3};
    const uint8_t* cmap = (color == Color::Black) ? kMapBlack : kMapWhite;

    uint8_t patch[3][3]{};
    const int sz = board.get_size();

    if (r_center >= 1 && r_center < sz - 1 && c_center >= 1 && c_center < sz - 1) {
        const int base = (r_center - 1) * sz + (c_center - 1);
        for (int dr = 0; dr < 3; ++dr) {
            const int row_base = base + dr * sz;
            for (int dc = 0; dc < 3; ++dc) {
                patch[dr][dc] = cmap[static_cast<uint8_t>(board.get_cell_unchecked(row_base + dc))];
            }
        }
    } else {
        for (int dr = 0; dr < 3; ++dr) {
            for (int dc = 0; dc < 3; ++dc) {
                State s = board.get_cell(r_center + dr - 1, c_center + dc - 1);
                patch[dr][dc] = cmap[static_cast<uint8_t>(s)];
            }
        }
    }

    uint16_t min3 = 0xFFFFU;

    for (int k = 0; k < 8; ++k) {
        uint16_t enc3 = 0;
        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (i == 0 && j == 0) continue;
                auto [di, dj] = transform_offset(i, j, k);
                enc3 = static_cast<uint16_t>((enc3 << 2) | patch[1 + di][1 + dj]);
            }
        }
        min3 = std::min(min3, enc3);
    }

    return min3;
}

/// Computes canonical (D4 symmetry-minimized) 3x3 and 5x5 pattern hashes in a single 5x5 patch read.
inline std::pair<uint16_t, uint64_t> serialize_patterns(
    const Board& board, int16_t r_center, int16_t c_center, Color color)
{
    static constexpr uint8_t kMapBlack[4] = {0, 1, 2, 3};
    static constexpr uint8_t kMapWhite[4] = {0, 2, 1, 3};
    const uint8_t* cmap = (color == Color::Black) ? kMapBlack : kMapWhite;

    uint8_t patch[5][5]{};
    const int sz = board.get_size();

    if (r_center >= 2 && r_center < sz - 2 && c_center >= 2 && c_center < sz - 2) {
        const int base = (r_center - 2) * sz + (c_center - 2);
        for (int dr = 0; dr < 5; ++dr) {
            const int row_base = base + dr * sz;
            for (int dc = 0; dc < 5; ++dc) {
                patch[dr][dc] = cmap[static_cast<uint8_t>(board.get_cell_unchecked(row_base + dc))];
            }
        }
    } else {
        for (int dr = 0; dr < 5; ++dr) {
            for (int dc = 0; dc < 5; ++dc) {
                State s = board.get_cell(r_center + dr - 2, c_center + dc - 2);
                patch[dr][dc] = cmap[static_cast<uint8_t>(s)];
            }
        }
    }

    uint16_t min3 = 0xFFFFU;
    uint64_t min5 = 0xFFFFFFFFFFFFFFFFULL;

    for (int k = 0; k < 8; ++k) {
        uint16_t enc3 = 0;
        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                if (i == 0 && j == 0) continue;
                auto [di, dj] = transform_offset(i, j, k);
                enc3 = static_cast<uint16_t>((enc3 << 2) | patch[2 + di][2 + dj]);
            }
        }
        min3 = std::min(min3, enc3);

        uint64_t enc5 = 0;
        for (int i = -2; i <= 2; ++i) {
            for (int j = -2; j <= 2; ++j) {
                if (i == 0 && j == 0) continue;
                auto [di, dj] = transform_offset(i, j, k);
                enc5 = (enc5 << 2) | patch[2 + di][2 + dj];
            }
        }
        min5 = std::min(min5, enc5);
    }

    return {min3, min5};
}

/// Returns true iff the 3x3 pattern hash contains at least one stone (01/10),
/// ignoring Empty (00) and OutOfBounds (11) where both bits in the pair match.
inline bool pattern3x3_has_stone(uint16_t h){
    return ((h ^ (h >> 1)) & 0x5555U) != 0;
}

/// Returns true iff the 5x5 pattern hash (48 bits) contains at least one stone (01/10).
inline bool pattern5x5_has_stone(uint64_t h){
    return ((h ^ (h >> 1)) & 0x0000555555555555ULL) != 0;
}

class patterns3x3{
private: 
    std::array<int32_t, 65536> freq{};
    std::array<uint32_t, 65536> rollout_w{};

public:
    patterns3x3(const std::string &filepath){
        std::ifstream in(filepath, std::ios::binary);
        if (!in.is_open()){
            std::cerr << "File doesn't exist: " << filepath << "\n";
            return;
        }

        in.read(reinterpret_cast<char*>(freq.data()), 65536 * sizeof(int32_t));

        for (size_t i = 0; i < 65536; ++i) {
            if (freq[i] > 0) {
                rollout_w[i] = static_cast<uint32_t>(1.0 + 10.0 * std::log1p(static_cast<double>(freq[i])));
            }
        }
    }

    int32_t operator[](int id) const {
        if (id < 0 || id >= 65536) return 0;
        return freq[id];
    }

    uint32_t weight(uint16_t id) const { return rollout_w[id]; }
};

class patterns5x5{
private:
    std::vector<uint64_t> keys;
    std::vector<int32_t> counts;

public:
    /// Loads 5x5 pattern frequencies into sorted parallel arrays for cache-friendly O(log N) lookup.
    patterns5x5(const std::string &filepath){
        std::ifstream in(filepath, std::ios::binary);
        if (!in.is_open()){
            std::cerr << "File doesn't exist: " << filepath << "\n";
            return;
        }

        uint64_t sz = 0;
        in.read(reinterpret_cast<char*>(&sz), sizeof(sz));
        if (!in || sz == 0) return;

        constexpr size_t kRecordBytes = sizeof(uint64_t) + sizeof(int32_t);
        std::vector<char> raw(static_cast<size_t>(sz) * kRecordBytes);
        if (!in.read(raw.data(), static_cast<std::streamsize>(raw.size()))){
            std::cerr << "Failed to read 5x5 pattern table: " << filepath << "\n";
            return;
        }

        keys.resize(static_cast<size_t>(sz));
        counts.resize(static_cast<size_t>(sz));
        bool is_sorted = true;

        for (size_t i = 0; i < static_cast<size_t>(sz); ++i){
            const char* ptr = raw.data() + i * kRecordBytes;
            std::memcpy(&keys[i], ptr, sizeof(uint64_t));
            std::memcpy(&counts[i], ptr + sizeof(uint64_t), sizeof(int32_t));
            if (i > 0 && keys[i] < keys[i - 1]) is_sorted = false;
        }

        if (!is_sorted){
            std::vector<std::pair<uint64_t, int32_t>> entries(static_cast<size_t>(sz));
            for (size_t i = 0; i < static_cast<size_t>(sz); ++i){
                entries[i] = {keys[i], counts[i]};
            }
            std::sort(entries.begin(), entries.end(),
                      [](const auto& a, const auto& b){ return a.first < b.first; });
            for (size_t i = 0; i < static_cast<size_t>(sz); ++i){
                keys[i] = entries[i].first;
                counts[i] = entries[i].second;
            }
        }
    }

    int32_t operator[](uint64_t id) const {
        auto it = std::lower_bound(keys.begin(), keys.end(), id);
        if (it == keys.end() || *it != id) return 0;
        return counts[static_cast<size_t>(it - keys.begin())];
    }
};

struct ScoredCandidate{
    int16_t id;
    double score;

    ScoredCandidate() = default;
    
    ScoredCandidate(int16_t inp_id, double s){id = inp_id; score = s;}

    bool operator<(const ScoredCandidate &rhs) const{return score < rhs.score;}
    bool operator>(const ScoredCandidate &rhs) const{return score > rhs.score;}
};

struct MCTSNode{
    bool moves_generated = false;
    MoveType type = MoveType::DEFAULT;
    int16_t id = -1;
    Color prev_player;
    int visits = 0;
    double wins = 0.0;
    double priority = 0.0;
    MCTSNode *parent = nullptr;

    static constexpr int max_child_org = 10;

    int max_child_cnt = 10;
    std::vector<ScoredCandidate> unexpanded;
    std::vector<std::unique_ptr<MCTSNode>> children;

    double total_child_score = 0.0;
    MCTSNode() = default;

    MCTSNode(Board &board, Color prev_p, MoveType t = MoveType::DEFAULT, int16_t move_id = -1, MCTSNode* p = nullptr);

    bool is_fully_expanded() const { return moves_generated && unexpanded.empty(); }
    bool is_terminal() const {
        return (type == MoveType::Resign || (parent != nullptr && parent -> type == MoveType::Pass && type == MoveType::Pass));
    }
    bool is_double_pass() const{return parent != nullptr && parent -> type == MoveType::Pass && type == MoveType::Pass;}

    MCTSNode* select_best_child(double c = 1) const;
};

class MCTSInstance{
private:
    Color AIColor;
    Board board;
    std::unique_ptr<MCTSNode> root = nullptr;
    std::vector<UndoState> undo_stack;

    int max_expansion_size;
    std::vector<int16_t> raw_candidates;   

    MCTSNode* selectChild();

    double rollout(MCTSNode *node);

    void backprop(MCTSNode *node, double score);

    void rollback();

    double score_candidate_move(int16_t id, Color player);

    bool is_bad_self_atari(int16_t cid, const UndoState& u);

    int16_t play_rand_move(Color color);

public:
    MCTSInstance() = delete;
    MCTSInstance(const Board &src_board, Color color, MoveType PrevMoveType = MoveType::DEFAULT);

    int16_t get_best_move(int time_limit_ms = 1000);

    const MCTSNode* get_root() const {return root.get();}
    
    Board& get_board(){return board;}

    void advance_tree(int16_t move_id, MoveType type, Color color);
};

class MCTSThreads{
private:
    unsigned int thread_cnt;
    int total_rollouts = 0;
    int new_rollouts = 0;
    std::vector<MCTSInstance> instances;
    std::vector<std::thread> threads;
public:
    MCTSThreads() = delete;
    MCTSThreads(const Board &board, Color color, MoveType type = MoveType::DEFAULT, unsigned int requested_cnt = 0);

    void advance_tree(int16_t move_id, MoveType type, Color color);
    int16_t get_best_move(int time_limit_ms = 1000);
    int get_total_rollouts() const { return total_rollouts; }
    int get_new_rollouts() const { return new_rollouts; }
};