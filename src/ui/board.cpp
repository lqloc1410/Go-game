#include "ui/board.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {
constexpr float kFadeSeconds = 0.30f;

std::vector<GridPos> starPoints(int n) {
    if (n == 19) { std::vector<GridPos> v; for (int a : {3, 9, 15}) for (int b : {3, 9, 15}) v.push_back({a, b}); return v; }
    if (n == 13) return {{3, 3}, {3, 9}, {9, 3}, {9, 9}, {6, 6}};
    if (n == 9)  return {{2, 2}, {2, 6}, {6, 2}, {6, 6}, {4, 4}};
    return {};
}

float noise(int a, int b) {
    std::uint32_t h = static_cast<std::uint32_t>(a) * 73856093u ^ static_cast<std::uint32_t>(b) * 19349663u;
    h ^= h >> 13; h *= 0x85ebca6bu; h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFu) / 65535.f;
}
}

BoardView::BoardView(int boardSize, float pixelSize, sf::Vector2f origin)
    : n_(boardSize), size_(pixelSize), origin_(origin) { recompute(); }

void BoardView::recompute() { 
    cell_ = size_ / (n_ + 1); 
}
void BoardView::setBoardSize(int n) { 
    n_ = n; 
    recompute(); 
    staticDirty_ = true; 
    resetAnimation(); 
}
void BoardView::setLayout(sf::Vector2f origin, float pixelSize) {
    origin_ = origin; 
    size_ = pixelSize; 
    recompute(); 
    staticDirty_ = true;
}
void BoardView::resetAnimation() { 
    prev_.clear(); 
    fading_.clear(); 
    animPrimed_ = false; 
}

sf::Vector2f BoardView::gridToPixel(int x, int y) const {
    return {origin_.x + cell_ * (x + 1), origin_.y + cell_ * (y + 1)};
}

std::optional<GridPos> BoardView::pixelToGrid(sf::Vector2f p) const {
    float fx = (p.x - origin_.x) / cell_ - 1.f, fy = (p.y - origin_.y) / cell_ - 1.f;
    int gx = static_cast<int>(std::lround(fx)), gy = static_cast<int>(std::lround(fy));
    if (gx < 0 || gy < 0 || gx >= n_ || gy >= n_) return std::nullopt;
    if (std::hypot(fx - gx, fy - gy) > 0.45f) return std::nullopt;
    return GridPos{gx, gy};
}

void BoardView::rebuildStatic() const {
    themeManager& tm = themeManager::instance();
    const theme& th = tm.current();
    const unsigned px = static_cast<unsigned>(size_) * kSuperSample;
    if (!staticTex_.create(px, px)) return;
    staticTex_.setSmooth(true);
    staticTex_.setView(sf::View(sf::FloatRect(0.f, 0.f, size_, size_)));   // draw in logical units, stored at 2x pixels
    staticTex_.clear(sf::Color::Transparent);

    auto P = [&](int x, int y) { return sf::Vector2f(cell_ * (x + 1), cell_ * (y + 1)); };

    // 1) surface: the tint first, the texture over it
    sf::RectangleShape surface({size_, size_});
    surface.setFillColor(th.boardTint);
    staticTex_.draw(surface);
    if (const sf::Texture* tex = tm.boardTex()) {
        sf::RectangleShape img({size_, size_});
        img.setTexture(tex);
        img.setFillColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(th.boardTexAlpha)));
        staticTex_.draw(img);
    }

    // 2) grid: every line is a thin rectangle
    const float base = std::max(1.5f, cell_ * th.gridThickness);
    const float edge = base * 1.8f;                                          // thicker outer border
    auto segment = [&](sf::Vector2f a, sf::Vector2f b, bool horizontal, float t, sf::Color c) {
        float len = horizontal ? (b.x - a.x) : (b.y - a.y);
        sf::RectangleShape r(horizontal ? sf::Vector2f(len + t, t) : sf::Vector2f(t, len + t));
        r.setOrigin(t / 2.f, t / 2.f);                                       // extended by t/2 so corners have no gaps
        r.setPosition(a);
        r.setFillColor(c);
        staticTex_.draw(r);
    };
    for (int i = 0; i < n_; ++i) {
        const float t = (i == 0 || i == n_ - 1) ? edge : base;
        if (!th.inkLines) {
            segment(P(0, i), P(n_ - 1, i), true,  t, th.grid);
            segment(P(i, 0), P(i, n_ - 1), false, t, th.grid);
        } else {                                                             // Sumi-e: thickness and ink density vary
            for (int s = 0; s + 1 < n_; ++s) {                       
                sf::Color ch = withAlpha(th.grid, static_cast<int>(205 + 50 * noise(i + 31, s)));
                sf::Color cv = withAlpha(th.grid, static_cast<int>(205 + 50 * noise(s + 57, i)));
                segment(P(s, i), P(s + 1, i), true,  t * (0.80f + 0.40f * noise(i, s)),       ch);
                segment(P(i, s), P(i, s + 1), false, t * (0.80f + 0.40f * noise(s + 101, i)), cv);
            }
        }
    }

    // 3) star points
    const float sr = std::max(2.5f, cell_ * 0.13f);
    for (auto s : starPoints(n_)) {
        sf::CircleShape dot(sr);
        dot.setOrigin(sr, sr);
        dot.setPosition(P(s.x, s.y));
        dot.setFillColor(th.grid);
        staticTex_.draw(dot);
    }
    staticTex_.display();
    staticDirty_ = false;
    themeVersion_ = tm.version();
}

void BoardView::drawStone(sf::RenderTarget& t, sf::Vector2f p, int who, float a01, float scale) const {
    const theme& th = themeManager::instance().current();
    const bool black = (who == 1);
    const float r  = cell_ * 0.46f * scale;
    const float rw = std::min(th.rimWidth, r * 0.3f);
    const float rd = r - rw;                                                 // disc radius; the rim adds rw outside
    auto fade = [&](sf::Color c) { c.a = static_cast<sf::Uint8>(c.a * a01); return c; };

    sf::CircleShape disc(rd);
    disc.setOrigin(rd, rd);

    if (a01 > 0.99f && th.shadowAlpha > 0.f) {                               // drop shadow (none on the ghost)
        disc.setPosition(p.x + r * 0.10f, p.y + r * 0.14f);
        disc.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(th.shadowAlpha)));
        t.draw(disc);
    }
    disc.setPosition(p);                                                     // base disc in the shade colour, with the rim
    disc.setFillColor(fade(black ? th.blackShade : th.whiteShade));
    disc.setOutlineThickness(rw);
    disc.setOutlineColor(fade(black ? th.blackRim : th.whiteRim));
    t.draw(disc);

    const float ir = rd * 0.90f;                   
    sf::CircleShape inner(ir);
    inner.setOrigin(ir, ir);
    inner.setPosition(p.x - rd * 0.07f, p.y - rd * 0.07f);
    inner.setFillColor(fade(black ? th.blackFill : th.whiteFill));
    t.draw(inner);

    const float hr = rd * 0.28f;                                             // soft highlight (shell / lacquer sheen)
    sf::CircleShape hl(hr);
    hl.setOrigin(hr, hr);
    hl.setPosition(p.x - rd * 0.33f, p.y - rd * 0.33f);
    hl.setFillColor(fade(black ? th.blackHighlight : th.whiteHighlight));
    t.draw(hl);

    if (th.inkLines && a01 > 0.99f) {                                        // Sumi-e: faint ink bleed around the stone
        sf::CircleShape bleed(r + 1.5f);
        bleed.setOrigin(r + 1.5f, r + 1.5f);
        bleed.setPosition(p);
        bleed.setFillColor(sf::Color::Transparent);
        bleed.setOutlineThickness(2.f);
        bleed.setOutlineColor(withAlpha(black ? th.blackRim : th.whiteRim, 55));
        t.draw(bleed);
    }
}

void BoardView::draw(sf::RenderWindow& w, const std::function<int(int, int)>& stoneAt, 
                    int currentPlayer, std::optional<GridPos> lastMove, 
                    const std::function<bool(int, int)>& isDead,
                    const std::function<int(int, int)>& territoryAt) const {
    themeManager& tm = themeManager::instance();
    const theme& th = tm.current();
    if (staticDirty_ || themeVersion_ != tm.version()) rebuildStatic();     // only when the size, layout or theme changed

    sf::Sprite layer(staticTex_.getTexture());
    layer.setScale(1.f / kSuperSample, 1.f / kSuperSample);
    layer.setPosition(origin_);
    w.draw(layer);

    // capture animation: stones that vanished since the last frame fade out
    const float dt = animClock_.restart().asSeconds();
    const std::size_t total = static_cast<std::size_t>(n_) * n_;
    if (prev_.size() != total) { prev_.assign(total, 0); animPrimed_ = false; }
    if (animPrimed_)
        for (int y = 0; y < n_; ++y)
            for (int x = 0; x < n_; ++x)
                if (prev_[y * n_ + x] != 0 && stoneAt(x, y) == 0) fading_.push_back({x, y, prev_[y * n_ + x], 0.f});
    for (int y = 0; y < n_; ++y)
        for (int x = 0; x < n_; ++x) prev_[y * n_ + x] = stoneAt(x, y);
    animPrimed_ = true;
    for (auto& f : fading_) f.age += dt;
    fading_.erase(std::remove_if(fading_.begin(), fading_.end(),
                                 [](const fading& f) { return f.age >= kFadeSeconds; }), fading_.end());

    const float r = cell_ * 0.46f;

    // optional territory markers
    if (territoryAt)
        for (int y = 0; y < n_; ++y)
            for (int x = 0; x < n_; ++x)
                if (stoneAt(x, y) == 0)
                    if (int o = territoryAt(x, y)) {
                        const float s = cell_ * 0.30f;
                        sf::RectangleShape sq({s, s});
                        sq.setOrigin(s / 2.f, s / 2.f);
                        sq.setPosition(gridToPixel(x, y));
                        sq.setFillColor(withAlpha(o == 1 ? th.blackFill : th.whiteFill, 230));
                        sq.setOutlineThickness(1.f);
                        sq.setOutlineColor(o == 1 ? th.blackRim : th.whiteRim);
                        w.draw(sq);
                    }

    // stones (dead stones faded with an X) 
    auto cross = [&](sf::Vector2f p) {
        const float len = r * 1.1f, t = std::max(2.f, r * 0.12f);
        for (float ang : {45.f, -45.f}) {
            sf::RectangleShape bar({len, t});
            bar.setOrigin(len / 2.f, t / 2.f);
            bar.setPosition(p);
            bar.setRotation(ang);
            bar.setFillColor(th.accent);
            w.draw(bar);
        }
    };
    for (int y = 0; y < n_; ++y)
        for (int x = 0; x < n_; ++x)
            if (int who = stoneAt(x, y)) {
                const bool dead = isDead && isDead(x, y);
                drawStone(w, gridToPixel(x, y), who, dead ? 0.43f : 1.f);
                if (dead) cross(gridToPixel(x, y));
            }

    // fading (captured / undone) stones 
    for (const fading& f : fading_) {
        const float k = 1.f - f.age / kFadeSeconds;                          // 1 -> 0
        drawStone(w, gridToPixel(f.x, f.y), f.who, k, 0.85f + 0.15f * k);
    }

    // last move marker 
    if (lastMove && stoneAt(lastMove->x, lastMove->y) != 0) {
        int who = stoneAt(lastMove -> x, lastMove -> y);
        if (who != 0) {
            sf::Color ring = (who == 1) ? th.whiteFill : th.blackFill;
            ring.a = 255;
            float mr = r * 0.6f;
            sf::CircleShape mark(mr);
            mark.setOrigin(mr, mr);
            mark.setPosition(gridToPixel(lastMove -> x, lastMove -> y));
            mark.setFillColor(sf::Color::Transparent);
            mark.setOutlineThickness(std::max(2.f, r * 0.10f));
            mark.setOutlineColor(ring);
            w.draw(mark);
        }
    }

    // hover ghost: about 40% opacity 
    if (hover_ && stoneAt(hover_->x, hover_->y) == 0)
        drawStone(w, gridToPixel(hover_->x, hover_->y), currentPlayer, 0.40f);
}