#pragma once
#include <GameLogic.hpp>
#include <GameParser.hpp>
#include <string>
#include <optional>
#include <filesystem>
#include <fstream>
namespace fs = std::filesystem;
class Game{
private:
    bool has_data = false;
    Board board;
    HistoryTracker history;
    GameLog log;
    const fs::path save_folder = "./data";
    const std::string save_file = "save.sgf", tmp_file = "save_tmp.sgf";
    const std::string full_path = save_folder.string() + '/' + save_file;
    const std::string tmp_path = save_folder.string() + '/' + tmp_file;

    std::string to_sgf() const;
public:
    Game(int board_size = 19, double komi = 7.5, int handicap = 0);

    bool load_from_file(const std::string &filepath);

    bool load_from_file(){return load_from_file(full_path);}

    MoveStatus play_move(int16_t id);
    MoveStatus play_move(int r, int c){return play_move(board.to_id(r, c));}
    MoveStatus play_move(Point p){return play_move(board.get(p));}

    Color get_turn() const {return history.get_turn();}
    bool undo() {return history.undo_move(board);}
    bool redo() {return history.redo_move(board);}
    bool pass() {return history.pass(history.get_turn());}
    bool resign() {return history.resign(history.get_turn());}
    std::optional<Color> get_resigned_player() const { return history.get_resigned_player(); }
    EndgameStatus is_game_over() const {return history.is_game_over();}
    int get_size() const { return board.get_size(); }
    double get_komi() const { return board.get_komi(); }
    int get_black_cnt() const { return board.get_black_cnt(); }
    int get_white_cnt() const { return board.get_white_cnt(); }
    double calculate_score() const { return board.calculate_score(); }
    State get_cell(int r, int c) const { return board.get_cell(r, c); }
    State get_cell(Point p) const { return board.get_cell(p.r, p.c); }
    const Board& get_board() const { return board; }
    const HistoryTracker& get_history() const { return history; }
    const GameLog& get_log() const { return log; }

    void display() const{board.display();}

    MoveStatus place_setup(int16_t id, Color color){
        return history.place_setup(board, id, color);
    }

    MoveStatus place_setup(Point p, Color color){
        return history.place_setup(board, board.get(p), color);
    }

    bool save() const;
};