#include "GameLogic.hpp"
#include "GameParser.hpp"

Ruleset string_to_ruleset(std::string s) {
    s = strip(s);
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (s == "chinese") return Ruleset::Chinese;
    else if (s == "japanese") return Ruleset::Japanese;
    else if (s == "aga") return Ruleset::AGA;
    else return Ruleset::Other;
}

std::string ruleset_to_string(Ruleset ruleset){
    if (ruleset == Ruleset::Chinese) return "Chinese";
    else if (ruleset == Ruleset::Japanese) return "Japanese";
    else if (ruleset == Ruleset::AGA) return "AGA";
    else return "Other";
}

// AI-generated
std::string GameResult::to_sgf_string() const {
    if (reason == ResultType::Draw) return "Draw";
    if (reason == ResultType::Void) return "Void";
    if (reason == ResultType::Unknown) return "?";
    if (!winner.has_value()) return "";

    std::string prefix = (*winner == Color::Black) ? "B+" : "W+";
    if (reason == ResultType::Resignation) return prefix + "R";
    if (reason == ResultType::Timeout) return prefix + "T";
    if (reason == ResultType::Disqualification) return prefix + "F";
    if (reason == ResultType::Score) {
        std::ostringstream ss;
        ss << score;
        return prefix + ss.str();
    }
    return "";
}

// AI-generated
std::string point_to_sgf(Point p) {
    if (p.r < 0 || p.c < 0) return "";
    return std::string{static_cast<char>('a' + p.c), static_cast<char>('a' + p.r)};
}

// AI-generated
std::string escape_sgf_text(const std::string& in) {
    std::string out;
    for (char c : in) {
        if (c == ']' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

std::vector<std::string> split(const std::string &input, char separator){
    std::vector<std::string> res;
    std::string curr{};
    for (char i:input){
        if (i == separator){
            res.push_back(curr);
            curr.clear();
        }else{
            curr += i;
        }
    }

    res.push_back(curr);
    return res;
}

std::string strip(const std::string& str) {
    auto start = str.find_first_not_of(" \t\n\r\v\f");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\n\r\v\f");
    return str.substr(start, end - start + 1);
}

// AI-generated.
std::optional<std::string> read_file_to_string(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return std::nullopt; // File doesn't exist or couldn't be opened
    }
    std::ostringstream ss;
    ss << file.rdbuf(); // Reads the entire file buffer in one go
    return ss.str();
}

std::optional<std::vector<Token>> tokenizer(const std::string &sgf){
    std::vector<Token> tokens;
    Token curr{};
    bool escaped = false;
    for (char i: sgf){
        if (curr.type == TokenType::Value){
            if (escaped) {curr.value += i; escaped = false;}
            else if (i == '\\') escaped = true;
            else if (i == ']') {tokens.push_back(curr); curr.clear();}
            else curr.value += i;

            continue;
        }
        
        if (curr.type == TokenType::TagName){
            if (i >= 'A' && i <= 'Z'){curr.value += i; continue;}
            else{tokens.push_back(curr); curr.clear();}
        }
        if (std::isspace(static_cast<unsigned char>(i))) continue;
        else if (i == ';') tokens.push_back(Token(TokenType::NodeStart));
        else if (i == '(') tokens.push_back(Token(TokenType::TreeOpen));
        else if (i == ')') tokens.push_back(Token(TokenType::TreeClose));
        else if (i == '[') curr.type = TokenType::Value;
        else if (i >= 'A' && i <= 'Z') {
            curr.type = TokenType::TagName;
            curr.value += i;
        }else return std::nullopt;
    }

    if (curr.type != TokenType::NoType) return std::nullopt;
    return tokens;
}

std::optional<int> to_int(std::string s){
    s = strip(s);
    try{
        size_t id = 0;
        int ans = std::stoi(s, &id);

        if (id != s.size()) return std::nullopt;

        return ans;
    }catch(const std::exception&){return std::nullopt;}
}

std::optional<double> to_double(std::string s){
    s = strip(s);
    try{
        size_t id = 0;
        double ans = std::stod(s, &id);

        if (id != s.size() || !std::isfinite(ans)) return std::nullopt;

        return ans;
    }catch(const std::exception&){return std::nullopt;}
}

std::optional<Point> GameLog::parse_sgf_point(const std::string &val){
        if (val.size() != 2) return std::nullopt;
        int c = val[0] - 'a', r = val[1] - 'a';
        if (r < 0 || r >= BoardSize || c < 0 || c >= BoardSize) return std::nullopt;

        return Point(r, c);
    }

std::optional<GameResult> GameLog::parse_sgf_result(const std::string &val){
    if (strip(val).empty()) return GameResult();
    std::vector<std::string> tokens = split(val, '+');
    for (std::string &s: tokens){
        s = strip(s);
    }
    GameResult res;
    if (tokens[0] == "B" || tokens[0] == "W"){
        res.winner = tokens[0] == "B" ? Color::Black : Color::White;

        if (tokens.size() != 2 || tokens[1].empty()) return std::nullopt;

        if (tokens[1] == "R" || tokens[1] == "Resign") res.reason = ResultType::Resignation;
        else if (tokens[1] == "T" || tokens[1] == "Time") res.reason = ResultType::Timeout;
        else if (tokens[1] == "F" || tokens[1] == "Forfeit") res.reason = ResultType::Disqualification;
        else{
            std::optional<double> score = to_double(tokens[1]);
            if (!score.has_value() || *score <= 0) return std::nullopt;
            res.reason = ResultType::Score;
            res.score = *score;
        }
    }else if (tokens[0] == "0" || tokens[0] == "Draw"){
        if (tokens.size() != 1) return std::nullopt;
        res.reason = ResultType::Draw;
    }else if (tokens[0] == "Void"){
        if (tokens.size() != 1) return std::nullopt;
        res.reason = ResultType::Void;
    }else if (tokens[0] == "?"){
        if (tokens.size() != 1) return std::nullopt;
        res.reason = ResultType::Unknown;
    }else return std::nullopt;

    return res;
}

bool GameLog::handle_property(const std::string &tag, const std::vector<std::string> &vals){
    if (vals.empty()) return false;
    if (tag == "SZ"){
        if (moves_list.size() != 0) return false;
        if (vals.size() != 1) return false;
        if (vals[0].empty()) return true;
        auto sz = to_int(vals[0]);

        if (!sz.has_value()) return false;

        if (2 > *sz || *sz > MAX_BOARD_WIDTH) return false;
        BoardSize = *sz;
    } else if (tag == "KM"){
        if (moves_list.size() != 0) return false;
        if (vals.size() != 1) return false;
        if (vals[0].empty()) return true;
        auto km = to_double(vals[0]);
        if (!km.has_value()) return false;
        komi = *km;
    } else if (tag == "FF"){
        if (moves_list.size() != 0) return false;
        if (vals.size() != 1) return false;
        if (vals[0].empty()) return true;
        auto ff = to_int(vals[0]);
        if (!ff.has_value() || *ff < 1 || *ff > 4) return false;
        FileFormat = *ff;
    } else if (tag == "GM"){
        if (moves_list.size() != 0) return false;
        if (vals.size() != 1) return false;
        if (vals[0].empty()) return true;
        auto gm = to_int(vals[0]);
        if (!gm.has_value() || *gm != 1) return false;
        GameMode = *gm;
    } else if (tag == "HA"){
        if (moves_list.size() != 0) return false;
        if (vals.size() != 1) return false;
        if (vals[0].empty()) return true;
        auto ha = to_int(vals[0]);
        if (!ha.has_value() || *ha < 0 || *ha == 1 || *ha > 9) return false;
        HandicapBlack = *ha;
    } else if (tag == "PB" || tag == "PW"){
        if (moves_list.size() != 0) return false;
        if (vals.size() != 1) return false;
        if (vals[0].empty()) return true;
        if (tag == "PB"){
            PlayerBlack = vals[0];
        }else{
            PlayerWhite = vals[0];
        }
    } else if (tag == "RE"){
        if (moves_list.size() != 0) return false;
        if (vals.size() != 1) return false;
        if (vals[0].empty()) return true;
        auto res = parse_sgf_result(vals[0]);
        if (!res.has_value()) return false;
        result = *res;
    } else if (tag == "RU"){
        if (moves_list.size() != 0) return false;
        if (vals.size() != 1) return false;
        if (vals[0].empty()) return true;
        ruleset = string_to_ruleset(vals[0]);
    } else if (tag == "AW" || tag == "AB"){
        if (moves_list.size() != 0) return false;
        if (vals.size() == 0) return false;
        if (vals.size() == 1 && vals[0].empty()) return true;

        auto &target = (tag == "AB") ? RawPositionsBlack : RawPositionsWhite;
        for (const std::string &pos: vals){
            if (pos.empty()) return false;
            target.push_back(pos);
        }
    } else if (tag == "B" || tag == "W"){
        if (vals.size() != 1) return false;
        Color c = tag == "B" ? Color::Black : Color::White;
        Move res;
        res.color = c;
        
        if (vals[0].empty() || vals[0] == "tt"){
            res.type = MoveType::Pass;
        }else{
            res.type = MoveType::Place;
            auto point = parse_sgf_point(vals[0]);
            if (!point.has_value()) return false;
            res.point = *point;
        }
        
        moves_list.push_back(res);
    } else if (tag == "PL") {
        if (moves_list.size() != 0) return false;
        if (vals.size() != 1) return false;
        if (vals[0].empty()) return true;
        std::string p = strip(vals[0]);
        if (p == "B" || p == "b") {starting_player = Color::Black; set_with_PL = true;}
        else if (p == "W" || p == "w") {starting_player = Color::White; set_with_PL = true;}
        else return false;
    }
    return true;
}

bool GameLog::parse(const std::string &raw_sgf){
    *this = GameLog();
    auto tokens_raw = tokenizer(raw_sgf);

    if (!tokens_raw.has_value()) return false;

    std::vector<Token> tokens = *tokens_raw;
    int sz = tokens.size();
    int pos = 0;

    int node_id = 0;
    int open_trees = 0;
    if (sz == 0 || tokens[0].type != TokenType::TreeOpen) return false;

    while (pos < sz && tokens[pos].type != TokenType::TreeClose){
        if (tokens[pos].type == TokenType::TreeOpen){
            open_trees++;
            pos++;
        }else if (tokens[pos].type == TokenType::NodeStart){
            node_id += 1;
            pos += 1;
        }else if (tokens[pos].type == TokenType::TagName) {
            if (node_id == 0) return false; // Tag before any ';'
            std::string tag = tokens[pos].value;
            pos++;
            // Collect all consecutive [Value] tokens belonging to this tag
            std::vector<std::string> vals;
            while (pos < sz && tokens[pos].type == TokenType::Value) {
                vals.push_back(tokens[pos].value);
                pos++;
            }
            if (vals.empty()) return false; // Tag with no '[...]' value!
            if (!handle_property(tag, vals)) return false;
        } else return false;
    }

    
    for (const std::string &pos_p: RawPositionsBlack){
        auto conv_pos = parse_sgf_point(pos_p);
        if (!conv_pos.has_value()) return false;
        AddedPositionsBlack.push_back(*conv_pos);
    }

    for (const std::string &pos_p: RawPositionsWhite){
        auto conv_pos = parse_sgf_point(pos_p);
        if (!conv_pos.has_value()) return false;
        AddedPositionsWhite.push_back(*conv_pos);
    }

    for (; pos < sz; ++pos) {
        if (tokens[pos].type == TokenType::TreeOpen) {
            open_trees++;
        } else if (tokens[pos].type == TokenType::TreeClose) {
            open_trees--;
            if (open_trees < 0) return false; // Extra ')'
        }
    }

    if (HandicapBlack >= 2) {
        if (static_cast<int>(AddedPositionsBlack.size()) > HandicapBlack) return false;
        if (!AddedPositionsWhite.empty()) return false;
        if (!set_with_PL) starting_player = Color::White;
    }
    
    if (open_trees != 0 || node_id == 0) return false; // Unclosed '(' or empty '()'
    return true;
} 


bool GameLog::load_board_data(Board &board_org, HistoryTracker &history_org){
    Board board = Board(BoardSize, komi);
    HistoryTracker history = HistoryTracker(HandicapBlack, starting_player);

    for (const Point &p: AddedPositionsBlack){
        MoveStatus tmp = history.place_setup(board, p, Color::Black);
        if (tmp != MoveStatus::Success) return false;
    }
    for (const Point &p: AddedPositionsWhite){
        MoveStatus tmp = history.place_setup(board, p, Color::White);
        if (tmp != MoveStatus::Success) return false;
    }

    
    for (const Move &m: moves_list){
        if (m.color != history.get_turn()) return false;
        if (m.type == MoveType::Pass){
            if (!history.pass(m.color)) return false;
        }else if (m.type == MoveType::Place){
            MoveStatus status = history.play_move(board, board.get(m.point), m.color);
            if (status != MoveStatus::Success) return false;
        }else{
            return false;
        }
    }

    if (result.reason == ResultType::Resignation && result.winner.has_value()) {
        if (history.is_game_over() == EndgameStatus::NotOver) {
            history.resign(invertColor(*result.winner));
        }
    }

    board_org = std::move(board);
    history_org = std::move(history) ;
    return true;
}

// AI-generated
std::ostream& operator<<(std::ostream& os, const GameLog &log){
    os << "File Format: " << log.FileFormat << '\n';
    os << "Game Mode: " << log.GameMode << '\n';
    os << "Ruleset: " << ruleset_to_string(log.ruleset) << '\n';
    os << "Board Size: " << log.BoardSize << '\n';
    os << "Black: " << log.PlayerBlack << '\n';
    os << "White: " << log.PlayerWhite << '\n';
    os << "Komi: " << log.komi << '\n';
    os << "Result: ";
    if (log.result.reason == ResultType::Draw) os << "Draw\n";
    else if (log.result.reason == ResultType::Void) os << "Void\n";
    else if (log.result.reason == ResultType::Unknown || !log.result.winner.has_value()) os << "?\n";
    else {
        os << (*log.result.winner == Color::Black ? "B+" : "W+");
        if (log.result.reason == ResultType::Resignation) os << "R\n";
        else if (log.result.reason == ResultType::Timeout) os << "T\n";
        else if (log.result.reason == ResultType::Disqualification) os << "F\n";
        else if (log.result.reason == ResultType::Score) os << log.result.score << '\n';
    }

    os << "Handicap (Black): " << log.HandicapBlack << '\n';
    os << "Added Black Stones (" << log.AddedPositionsBlack.size() << "):";
    for (const Point &p : log.AddedPositionsBlack) {
        os << " (" << p.r << ", " << p.c << ")";
    }
    os << '\n';

    os << "Added White Stones (" << log.AddedPositionsWhite.size() << "):";
    for (const Point &p : log.AddedPositionsWhite) {
        os << " (" << p.r << ", " << p.c << ")";
    }
    os << '\n';

    os << "Starting Player: " << (log.starting_player == Color::Black ? "Black" : "White") << '\n';
    auto format_move = [](const Move &m) -> std::string {
        if (m.type == MoveType::Pass) return "pass";
        return std::string{static_cast<char>('a' + m.point.c), static_cast<char>('a' + m.point.r)};
    };

    os << "Moves (" << log.moves_list.size() << "):\n";
    int turn = 1;
    for (size_t i = 0; i < log.moves_list.size(); ++i, ++turn) {
        if (log.moves_list[i].color == Color::Black) {
            os << turn << ". " << format_move(log.moves_list[i]);
            if (i + 1 < log.moves_list.size() && log.moves_list[i + 1].color == Color::White) {
                os << ' ' << format_move(log.moves_list[++i]);
            }
        } else {
            os << turn << ". .. " << format_move(log.moves_list[i]);
        }
        os << '\n';
    }
    return os;
    
}

