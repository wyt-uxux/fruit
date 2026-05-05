#define NOMINMAX
#include "Knife.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace FruitGame
{
    namespace
    {
        constexpr double kTrailLife = 0.18;
        constexpr std::size_t kMaxTrailPoints = 180;
        constexpr double kSmoothingAlpha = 0.34;
        constexpr double kTrailSampleInterval = 0.002;
        constexpr double kTrailSampleStepPx = 2.4;
        constexpr double kTrailMaxLengthRatio = 0.72;
        constexpr double kReleaseFadeSpeed = 1;

        int clampInt(int value, int low, int high)
        {
            return std::max(low, std::min(value, high));
        }

        inline int lerpInt(int a, int b, double t)
        {
            return static_cast<int>(std::round(a + (b - a) * t));
        }

        POINT lerpPoint(const POINT& a, const POINT& b, double t)
        {
            POINT p{};
            p.x = lerpInt(a.x, b.x, t);
            p.y = lerpInt(a.y, b.y, t);
            return p;
        }

        inline double clamp01(double value)
        {
            return std::max(0.0, std::min(1.0, value));
        }

        inline double lerp(double a, double b, double t)
        {
            return a + (b - a) * t;
        }

        POINT makePoint(double x, double y)
        {
            POINT p{};
            p.x = static_cast<LONG>(std::round(x));
            p.y = static_cast<LONG>(std::round(y));
            return p;
        }

        POINT catmullRomPoint(const POINT& p0, const POINT& p1, const POINT& p2, const POINT& p3, double t)
        {
            const double t2 = t * t;
            const double t3 = t2 * t;

            const double x = 0.5 * ((2.0 * p1.x) +
                (-p0.x + p2.x) * t +
                (2.0 * p0.x - 5.0 * p1.x + 4.0 * p2.x - p3.x) * t2 +
                (-p0.x + 3.0 * p1.x - 3.0 * p2.x + p3.x) * t3);
            const double y = 0.5 * ((2.0 * p1.y) +
                (-p0.y + p2.y) * t +
                (2.0 * p0.y - 5.0 * p1.y + 4.0 * p2.y - p3.y) * t2 +
                (-p0.y + 3.0 * p1.y - 3.0 * p2.y + p3.y) * t3);
            return makePoint(x, y);
        }

        void drawBeamLayer(int cx, int cy, double angle, double halfLength, double halfWidth, COLORREF color)
        {
            const double vx = std::cos(angle);
            const double vy = std::sin(angle);
            const double nx = -vy;
            const double ny = vx;

            POINT quad[4] = {
                makePoint(cx - vx * halfLength, cy - vy * halfLength),
                makePoint(cx - nx * halfWidth, cy - ny * halfWidth),
                makePoint(cx + vx * halfLength, cy + vy * halfLength),
                makePoint(cx + nx * halfWidth, cy + ny * halfWidth) };

            setfillcolor(color);
            solidpolygon(quad, 4);
        }

        void drawProceduralFlash(int centerX, int centerY, double angle, double progress)
        {
            const double pulse = std::sin(progress * 3.14159265358979323846);
            const double stretch = lerp(0.92, 1.18, clamp01(pulse));
            const double halfLen = 130.0 * stretch;

            drawBeamLayer(centerX, centerY, angle, halfLen * 1.08, 8.0, RGB(198, 149, 40));
            drawBeamLayer(centerX, centerY, angle, halfLen * 0.98, 5.0, RGB(241, 210, 112));
            drawBeamLayer(centerX, centerY, angle, halfLen * 0.86, 2.0, RGB(255, 252, 230));

            const int lineLen = static_cast<int>(std::round(halfLen * 1.04));
            const int x1 = centerX - static_cast<int>(std::round(std::cos(angle) * lineLen));
            const int y1 = centerY - static_cast<int>(std::round(std::sin(angle) * lineLen));
            const int x2 = centerX + static_cast<int>(std::round(std::cos(angle) * lineLen));
            const int y2 = centerY + static_cast<int>(std::round(std::sin(angle) * lineLen));

            setlinecolor(RGB(255, 248, 210));
            setlinestyle(PS_SOLID, 2);
            line(x1, y1, x2, y2);

            setfillcolor(RGB(255, 247, 230));
            solidcircle(centerX, centerY, 4);

            const double nx = -std::sin(angle);
            const double ny = std::cos(angle);
            setfillcolor(RGB(255, 219, 118));
            solidcircle(
                centerX + static_cast<int>(std::round(nx * 6.0)),
                centerY + static_cast<int>(std::round(ny * 6.0)),
                3);
            solidcircle(
                centerX - static_cast<int>(std::round(nx * 5.0)),
                centerY - static_cast<int>(std::round(ny * 5.0)),
                2);
        }
    }

    FruitKnife::FruitKnife()
        : effect_(createBambooKnifeEffect())
    {
    }

    void FruitKnife::setAdvancedEffect(bool enabled)
    {
        if (enabled)
        {
            if (!effect_)
            {
                effect_ = createBambooKnifeEffect();
            }
        }
        else
        {
            // disable by releasing the heavyweight effect; trail rendering still occurs.
            effect_.reset();
        }
    }

    bool FruitKnife::isAdvancedEffectEnabled() const noexcept
    {
        return effect_ != nullptr;
    }

    void FruitKnife::update(double dt, HWND hwnd)
    {
        hasBladeSegment_ = false;

        POINT p{};
        if (GetCursorPos(&p) && ScreenToClient(hwnd, &p))
        {
            currentPoint_.x = clampInt(p.x, 0, getwidth());
            currentPoint_.y = clampInt(p.y, 0, getheight());
            hasPoint_ = true;
        }

        slicing_ = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        const double decayMul = slicing_ ? 1.0 : kReleaseFadeSpeed;

        for (auto& point : trail_)
        {
            point.life -= dt * decayMul;
        }

        while (!trail_.empty() && trail_.front().life <= 0.0)
        {
            trail_.pop_front();
        }

        if (effect_)
        {
            effect_->update(dt, slicing_, hasPoint_, currentPoint_);
        }

        if (flashVisible_)
        {
            flashTimer_ -= dt;
            if (flashTimer_ <= 0.0)
            {
                flashVisible_ = false;
                flashTimer_ = 0.0;
            }
        }
        if (!slicing_ || !hasPoint_)
        {
            strokeActive_ = false;
            hasFilteredPoint_ = false;
            sampleTimer_ = 0.0;
            bladeSpeed_ = 0.0;
            return;
        }

        if (!strokeActive_)
        {
            // Start a new independent stroke; this avoids connecting with fading leftovers.
            trail_.clear();
            strokeActive_ = true;
            hasFilteredPoint_ = false;
            sampleTimer_ = 0.0;
        }

        if (!hasFilteredPoint_)
        {
            filteredPoint_ = currentPoint_;
            hasFilteredPoint_ = true;
            pushPoint(filteredPoint_);
            sampleTimer_ = 0.0;
            bladeSpeed_ = 0.0;
            return;
        }

        const POINT prevFiltered = filteredPoint_;
        filteredPoint_.x = lerpInt(filteredPoint_.x, currentPoint_.x, kSmoothingAlpha);
        filteredPoint_.y = lerpInt(filteredPoint_.y, currentPoint_.y, kSmoothingAlpha);

        if (trail_.empty())
        {
            pushPoint(filteredPoint_);
        }

        segmentFrom_ = prevFiltered;
        segmentTo_ = filteredPoint_;
        hasBladeSegment_ = true;

        const POINT& last = trail_.back().p;
        const int dx = filteredPoint_.x - last.x;
        const int dy = filteredPoint_.y - last.y;
        if (dx != 0 || dy != 0)
        {
            cutAngle_ = std::atan2(static_cast<double>(dy), static_cast<double>(dx));
        }

        const double segDx = static_cast<double>(segmentTo_.x - segmentFrom_.x);
        const double segDy = static_cast<double>(segmentTo_.y - segmentFrom_.y);
        bladeSpeed_ = std::sqrt(segDx * segDx + segDy * segDy) / std::max(1e-4, dt);

        sampleTimer_ += dt;
        const double distSq = static_cast<double>(dx * dx + dy * dy);
        const bool shouldSample = sampleTimer_ >= kTrailSampleInterval || distSq >= 36.0;
        if (shouldSample)
        {
            sampleTimer_ = 0.0;

            const POINT lastTrail = trail_.back().p;
            const double sx = static_cast<double>(filteredPoint_.x - lastTrail.x);
            const double sy = static_cast<double>(filteredPoint_.y - lastTrail.y);
            const double segLen = std::sqrt(sx * sx + sy * sy);
            if (segLen > kTrailSampleStepPx)
            {
                const int steps = static_cast<int>(std::floor(segLen / kTrailSampleStepPx));
                for (int i = 1; i <= steps; ++i)
                {
                    const double t = static_cast<double>(i) / static_cast<double>(steps + 1);
                    POINT p{};
                    p.x = static_cast<LONG>(std::round(lastTrail.x + sx * t));
                    p.y = static_cast<LONG>(std::round(lastTrail.y + sy * t));
                    pushPoint(p);
                }
            }

            pushPoint(filteredPoint_);
        }
    }

    void FruitKnife::render() const
    {
        const std::vector<POINT> smoothTrail = buildSmoothedTrail();
        if (effect_)
        {
            effect_->renderTrail(smoothTrail);
        }

        if (flashVisible_)
        {
            const double progress = clamp01(1.0 - flashTimer_ / std::max(0.0001, flashDuration_));
            drawProceduralFlash(flashPoint_.x, flashPoint_.y, flashAngle_, progress);
        }
    }

    void FruitKnife::triggerFlash(const POINT& impactPoint, double angle, double durationSec)
    {
        flashPoint_.x = clampInt(impactPoint.x, 0, getwidth());
        flashPoint_.y = clampInt(impactPoint.y, 0, getheight());
        flashAngle_ = angle;
        flashDuration_ = std::max(0.06, durationSec);
        flashTimer_ = flashDuration_;
        flashVisible_ = true;
    }

    bool FruitKnife::isSlicing() const noexcept
    {
        return slicing_;
    }

    bool FruitKnife::hasCursorPoint() const noexcept
    {
        return hasPoint_;
    }

    POINT FruitKnife::cursorPoint() const noexcept
    {
        return currentPoint_;
    }

    double FruitKnife::cutAngle() const noexcept
    {
        return cutAngle_;
    }

    double FruitKnife::bladeSpeed() const noexcept
    {
        return bladeSpeed_;
    }

    bool FruitKnife::hasBladeSegment() const noexcept
    {
        return hasBladeSegment_;
    }

    void FruitKnife::bladeSegment(POINT* from, POINT* to) const
    {
        if (from != nullptr)
        {
            *from = segmentFrom_;
        }
        if (to != nullptr)
        {
            *to = segmentTo_;
        }
    }

    std::vector<POINT> FruitKnife::buildSmoothedTrail() const
    {
        std::vector<POINT> base;
        base.reserve(trail_.size());
        for (const auto& t : trail_)
        {
            base.push_back(t.p);
        }

        if (base.size() < 2)
        {
            return base;
        }

        std::vector<POINT> resampled;
        resampled.reserve(base.size() * 2);
        resampled.push_back(base.front());

        for (std::size_t i = 1; i < base.size(); ++i)
        {
            const POINT a = base[i - 1];
            const POINT b = base[i];
            const double dx = static_cast<double>(b.x - a.x);
            const double dy = static_cast<double>(b.y - a.y);
            const double len = std::sqrt(dx * dx + dy * dy);
            const int inserts = std::max(0, static_cast<int>(std::floor(len / kTrailSampleStepPx)) - 1);
            for (int s = 1; s <= inserts; ++s)
            {
                const double t = static_cast<double>(s) / static_cast<double>(inserts + 1);
                resampled.push_back(makePoint(a.x + dx * t, a.y + dy * t));
            }
            resampled.push_back(b);
        }

        if (resampled.size() < 3)
        {
            return resampled;
        }

        std::vector<POINT> smooth = resampled;
        for (int pass = 0; pass < 2; ++pass)
        {
            if (smooth.size() < 3)
            {
                break;
            }

            std::vector<POINT> next;
            next.reserve(smooth.size() * 2);
            next.push_back(smooth.front());

            for (std::size_t i = 0; i + 1 < smooth.size(); ++i)
            {
                const POINT& a = smooth[i];
                const POINT& b = smooth[i + 1];
                next.push_back(makePoint(0.75 * static_cast<double>(a.x) + 0.25 * static_cast<double>(b.x),
                    0.75 * static_cast<double>(a.y) + 0.25 * static_cast<double>(b.y)));
                next.push_back(makePoint(0.25 * static_cast<double>(a.x) + 0.75 * static_cast<double>(b.x),
                    0.25 * static_cast<double>(a.y) + 0.75 * static_cast<double>(b.y)));
            }

            next.push_back(smooth.back());
            smooth = std::move(next);
        }

        return smooth;
    }

    void FruitKnife::pushPoint(const POINT& p)
    {
        trail_.push_back(TrailPoint{ p, kTrailLife });

        trimTrailByLength();

        while (trail_.size() > kMaxTrailPoints)
        {
            trail_.pop_front();
        }
    }

    void FruitKnife::trimTrailByLength()
    {
        if (trail_.size() < 2)
        {
            return;
        }

        const double maxLen = static_cast<double>(getwidth()) * kTrailMaxLengthRatio;
        if (maxLen <= 1.0)
        {
            return;
        }

        double accum = 0.0;
        for (std::size_t i = trail_.size() - 1; i > 0; --i)
        {
            const POINT& a = trail_[i].p;
            const POINT& b = trail_[i - 1].p;
            const double dx = static_cast<double>(a.x - b.x);
            const double dy = static_cast<double>(a.y - b.y);
            accum += std::sqrt(dx * dx + dy * dy);
            if (accum > maxLen)
            {
                trail_.erase(trail_.begin(), trail_.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
        }
    }
}

