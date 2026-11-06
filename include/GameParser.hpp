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
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include "GameLogic.hpp"

enum class ResultType {Score, Resignation, Timeout, Disqualification, Void, Unknown, Draw};
enum class Ruleset{Chinese, Japanese, AGA, Other};

std::string strip(const std::string& str);

Ruleset string_to_ruleset(std::string s);

std::string ruleset_to_string(Ruleset ruleset);

struct GameResult{
    ResultType reason = ResultType::Unknown;
    std::optional<Color> winner = std::nullopt; 
    double score = 0;

    std::string to_sgf_string() const;
};

struct Move{
    MoveType type = MoveType::DEFAULT;
    Color color = Color::Black;
    Point point;
};

std::vector<std::string> split(const std::string &input, char separator);

enum class TokenType{TreeOpen, TreeClose, NodeStart, TagName, Value, NoType};
struct Token{
    TokenType type = TokenType::NoType;
    std::string value{};

    Token() = default;
    Token(TokenType t){
        type = t;
    }

    inline void clear(){
        type = TokenType::NoType;
        value.clear();
    }
};

// AI generated
struct ReplayResult {
    bool ok = true;
    int move_idx = -1;
    MoveStatus status = MoveStatus::Success;
    Color color = Color::Black;
    Point point{-1, -1};
    std::string message;

    explicit operator bool() const { return ok; }
    static ReplayResult success();
    static ReplayResult failure(int idx, MoveStatus st, Color c, Point p, const std::string &msg);
};


// AI-generated.
std::optional<std::string> read_file_to_string(const std::string& filepath);

std::optional<std::vector<Token>> tokenizer(const std::string &sgf);

std::optional<int> to_int(std::string s);

std::optional<double> to_double(std::string s);

// AI-generated
std::string point_to_sgf(Point p);

// AI-generated
std::string escape_sgf_text(const std::string& in);

class GameLog{
private: 
    int FileFormat = 4; // FF[4] for now
    int GameMode = 1; // GM[1] for Go
    Ruleset ruleset = Ruleset::Chinese; // Currently only officially supports Chinese rules


    int BoardSize = 19;

    std::string PlayerBlack{};
    std::string PlayerWhite{};

    double komi = 7.5;

    GameResult result;
    
    int HandicapBlack = 0;

    std::vector<std::string> RawPositionsBlack;
    std::vector<std::string> RawPositionsWhite;

    std::vector<Point> AddedPositionsBlack;
    std::vector<Point> AddedPositionsWhite;

    bool set_with_PL = false;
    Color starting_player = Color::Black;
    std::vector<Move> moves_list;

    std::optional<Point> parse_sgf_point(const std::string &val);

    std::optional<GameResult> parse_sgf_result(const std::string &val);

    bool handle_property(const std::string &tag, const std::vector<std::string> &vals);
public:
    GameLog() = default;

    bool parse(const std::string &raw_sgf); 
    

    bool load_board_data(Board &board, HistoryTracker &history);

    // Getters for SGF export
    int get_file_format() const { return FileFormat; }
    int get_game_mode() const { return GameMode; }
    Ruleset get_ruleset() const { return ruleset; }
    std::string get_ruleset_str() const { return ruleset_to_string(ruleset); }
    int get_board_size() const { return BoardSize; }
    const std::string& get_player_black() const { return PlayerBlack; }
    const std::string& get_player_white() const { return PlayerWhite; }
    double get_komi() const { return komi; }
    const GameResult& get_result() const { return result; }
    std::string get_result_str() const { return result.to_sgf_string(); }
    int get_handicap() const { return HandicapBlack; }
    const std::vector<std::string>& get_raw_black() const { return RawPositionsBlack; }
    const std::vector<std::string>& get_raw_white() const { return RawPositionsWhite; }
    bool has_explicit_pl() const { return set_with_PL; }
    Color get_starting_player() const { return starting_player; }
    const std::vector<Move>& get_moves() const { return moves_list; }
    size_t get_move_count() const { return moves_list.size(); }
    bool has_moves() const { return !moves_list.empty(); }
    
    // AI-generated
    friend std::ostream& operator<<(std::ostream& os, const GameLog &log);
};

