#pragma once
#include <SFML/Graphics.hpp>
#include <functional>
#include <optional>

struct GridPos { int x, y; };

struct boardStyle {
    sf::Color boardColor{222, 184, 125};
    sf::Color lineColor{0, 0, 0};
    sf::Color blackStone{20, 20, 20};
    sf::Color whiteStone{245, 245, 245};
    sf::Color markColor{220, 50, 50};
    const sf::Texture* boardTex = nullptr;
    const sf::Texture* blackTex = nullptr;
    const sf::Texture* whiteTex = nullptr;
};

// AI-assisted: see AI29, AI33
class BoardView {
public:
    BoardView(int boardSize = 9, float pixelSize = 800.f, sf::Vector2f origin = {0.f, 0.f});

    void setBoardSize(int n);
    void setLayout(sf::Vector2f origin, float pixelSize);

    // giao điểm (x,y) -> toạ độ pixel; và ngược lại
    sf::Vector2f gridToPixel(int x, int y) const;
    std::optional<GridPos> pixelToGrid(sf::Vector2f p) const;

    void setStyle(const boardStyle& s) { style = s; staticDirty_ = true; }

    void setHover(std::optional<GridPos> p) { hover_ = p; }

    // stoneAt(x,y) trả về: 0 = trống, 1 = đen, 2 = trắng
    // currentPlayer: 1 hoặc 2, dùng để vẽ quân mờ khi rê chuột
    void draw(sf::RenderWindow& w,
              const std::function<int(int, int)>& stoneAt,
              int currentPlayer,
              std::optional<GridPos> lastMove = std::nullopt) const;

private:
    void recompute();
    void rebuildStatic() const;
    mutable sf::RenderTexture staticTex_;
    mutable bool staticDirty_ = true;
    static constexpr unsigned kSuperSample = 2;
    int n_;
    float size_;
    float cell_ = 0.f;
    sf::Vector2f origin_;
    std::optional<GridPos> hover_;
    boardStyle style;
};