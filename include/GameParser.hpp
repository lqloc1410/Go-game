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
    
    // AI-generated
    friend std::ostream& operator<<(std::ostream& os, const GameLog &log);
};

