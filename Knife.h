#pragma once

#include <Windows.h>

#include <deque>
#include <memory>
#include <vector>

#include <graphics.h>

#include "KnifeEffect.h"

namespace FruitGame
{
    class FruitKnife
    {
    public:
        FruitKnife();

        void update(double dt, HWND hwnd);
        void render() const;
        void triggerFlash(const POINT& impactPoint, double angle, double durationSec = 0.20);

        [[nodiscard]] bool isSlicing() const noexcept;
        [[nodiscard]] bool hasCursorPoint() const noexcept;
        [[nodiscard]] POINT cursorPoint() const noexcept;
        [[nodiscard]] double cutAngle() const noexcept;
        [[nodiscard]] double bladeSpeed() const noexcept;
        [[nodiscard]] bool hasBladeSegment() const noexcept;
        void bladeSegment(POINT* from, POINT* to) const;

    private:
        struct TrailPoint
        {
            POINT p{};
            double life = 0.0;
        };

        void pushPoint(const POINT& p);
        void trimTrailByLength();
        [[nodiscard]] std::vector<POINT> buildSmoothedTrail() const;

        std::deque<TrailPoint> trail_;
        POINT currentPoint_{};
        POINT filteredPoint_{};
        POINT flashPoint_{};
        POINT segmentFrom_{};
        POINT segmentTo_{};
        double cutAngle_ = -1.57079632679;
        double flashAngle_ = 0.0;
        double flashDuration_ = 0.0;
        double bladeSpeed_ = 0.0;
        bool hasFilteredPoint_ = false;
        bool hasBladeSegment_ = false;
        bool flashVisible_ = false;
        double flashTimer_ = 0.0;
        double sampleTimer_ = 0.0;
        bool strokeActive_ = false;
        bool hasPoint_ = false;
        bool slicing_ = false;
        std::unique_ptr<KnifeEffect> effect_;
    };
}

