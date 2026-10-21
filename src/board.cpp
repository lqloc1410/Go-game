#include "ui/board.hpp"
#include <cmath>
#include <vector>

BoardView::BoardView(int boardSize, float pixelSize, sf::Vector2f origin)
    : n_(boardSize), size_(pixelSize), origin_(origin) {
    recompute();
}

void BoardView::setBoardSize(int n) { n_ = n; recompute(); }
void BoardView::setLayout(sf::Vector2f origin, float pixelSize) {
    origin_ = origin; size_ = pixelSize; recompute();
}

void BoardView::recompute() { cell_ = size_ / (n_ + 1); }

sf::Vector2f BoardView::gridToPixel(int x, int y) const {
    return { origin_.x + cell_ * (x + 1), origin_.y + cell_ * (y + 1) };
}

std::optional<GridPos> BoardView::pixelToGrid(sf::Vector2f p) const {
    float fx = (p.x - origin_.x) / cell_ - 1.f;
    float fy = (p.y - origin_.y) / cell_ - 1.f;
    int gx = (int)std::lround(fx);
    int gy = (int)std::lround(fy);
    if (gx < 0 || gy < 0 || gx >= n_ || gy >= n_) return std::nullopt;
    // chỉ nhận khi click đủ gần giao điểm (trong 0.45 ô)
    if (std::hypot(fx - gx, fy - gy) > 0.45f) return std::nullopt;
    return GridPos{gx, gy};
}

static std::vector<GridPos> starPoints(int n) {
    if (n == 19) {
        std::vector<GridPos> v;
        for (int a : {3, 9, 15}) for (int b : {3, 9, 15}) v.push_back({a, b});
        return v;
    }
    if (n == 13) return {{3,3},{3,9},{9,3},{9,9},{6,6}};
    if (n == 9)  return {{2,2},{2,6},{6,2},{6,6},{4,4}};
    return {};
}

void BoardView::draw(sf::RenderWindow& w,
                     const std::function<int(int, int)>& stoneAt,
                     int currentPlayer) const {
    // 1) nền gỗ
    sf::RectangleShape bg({size_, size_});
    bg.setPosition(origin_);
    bg.setFillColor(sf::Color(222, 184, 135));
    w.draw(bg);

    // 2) các đường kẻ
    sf::VertexArray lines(sf::Lines);
    for (int i = 0; i < n_; ++i) {
        sf::Vector2f a = gridToPixel(0, i), b = gridToPixel(n_ - 1, i);   // ngang
        sf::Vector2f c = gridToPixel(i, 0), d = gridToPixel(i, n_ - 1);   // dọc
        lines.append({a, sf::Color::Black}); lines.append({b, sf::Color::Black});
        lines.append({c, sf::Color::Black}); lines.append({d, sf::Color::Black});
    }
    w.draw(lines);

    // 3) điểm sao
    float starR = cell_ * 0.12f;
    for (auto s : starPoints(n_)) {
        sf::CircleShape dot(starR);
        dot.setOrigin(starR, starR);
        dot.setPosition(gridToPixel(s.x, s.y));
        dot.setFillColor(sf::Color::Black);
        w.draw(dot);
    }

    // 4) quân cờ
    float r = cell_ * 0.46f;
    sf::CircleShape stone(r);
    stone.setOrigin(r, r);
    for (int y = 0; y < n_; ++y)
        for (int x = 0; x < n_; ++x) {
            int s = stoneAt(x, y);
            if (s == 0) continue;
            stone.setPosition(gridToPixel(x, y));
            stone.setFillColor(s == 1 ? sf::Color(20, 20, 20) : sf::Color(245, 245, 245));
            stone.setOutlineThickness(1.f);
            stone.setOutlineColor(sf::Color(0, 0, 0, 120));
            w.draw(stone);
        }

    // 5) quân mờ khi rê chuột vào ô trống
    if (hover_ && stoneAt(hover_->x, hover_->y) == 0) {
        stone.setPosition(gridToPixel(hover_->x, hover_->y));
        sf::Color c = currentPlayer == 1 ? sf::Color(20, 20, 20, 120)
                                         : sf::Color(245, 245, 245, 160);
        stone.setFillColor(c);
        stone.setOutlineThickness(0.f);
        w.draw(stone);
    }
}