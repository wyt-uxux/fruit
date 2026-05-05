
#pragma once

#include <Windows.h>

#include <optional>

#include "ComboConfig.h"

namespace FruitGame
{
    struct ComboResolved
    {
        int comboCount = 0;
        POINT impactPoint{};
    };

    class ComboJudge
    {
    public:
        explicit ComboJudge(double maxHitGapSec = ComboConfig::kMaxHitGapSec);

        std::optional<ComboResolved> update(double nowSec, bool slicing);
        std::optional<ComboResolved> registerHit(double nowSec, const POINT& impactPoint, bool slicing);
        void reset();

    private:
        std::optional<ComboResolved> finalizeActiveChain();
        void beginNewChain(double nowSec, const POINT& impactPoint);

        double maxHitGapSec_ = ComboConfig::kMaxHitGapSec;
        bool chainActive_ = false;
        int chainHits_ = 0;
        double lastHitTimeSec_ = 0.0;
        POINT lastHitPoint_{};
    };
}
