#pragma once
#include <SFML/Graphics.hpp>
#include <functional>
#include <optional>

struct GridPos { int x, y; };

// AI-assisted: see AI29, AI33
class BoardView {
public:
    BoardView(int boardSize = 9, float pixelSize = 800.f, sf::Vector2f origin = {0.f, 0.f});

    void setBoardSize(int n);
    void setLayout(sf::Vector2f origin, float pixelSize);

    // giao điểm (x,y) -> toạ độ pixel; và ngược lại
    sf::Vector2f gridToPixel(int x, int y) const;
    std::optional<GridPos> pixelToGrid(sf::Vector2f p) const;

    void setHover(std::optional<GridPos> p) { hover_ = p; }

    // stoneAt(x,y) trả về: 0 = trống, 1 = đen, 2 = trắng
    // currentPlayer: 1 hoặc 2, dùng để vẽ quân mờ khi rê chuột
    void draw(sf::RenderWindow& w,
              const std::function<int(int, int)>& stoneAt,
              int currentPlayer) const;

private:
    void recompute();

    int n_;
    float size_;
    float cell_ = 0.f;
    sf::Vector2f origin_;
    std::optional<GridPos> hover_;
};