#include <GameLogic.hpp>
#include <GameParser.hpp>
#include <string>
#include <optional>
class Game{
private:
    Board board;
    HistoryTracker history;
    GameLog log;
    std::string autosave_path;
public:
    Game(int board_size = 19, double komi = 7.5, int handicap = 0){
        board = Board(board_size, komi);
        history = HistoryTracker(handicap);
    }

};