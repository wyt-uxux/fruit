#define NOMINMAX
#include "ComboJudge.h"

#include <algorithm>

namespace FruitGame
{
    ComboJudge::ComboJudge(double maxHitGapSec)
        : maxHitGapSec_(std::max(0.02, maxHitGapSec))
    {
    }

    std::optional<ComboResolved> ComboJudge::update(double nowSec, bool slicing)
    {
        if (!chainActive_)
        {
            return std::nullopt;
        }

        if (!slicing)
        {
            return finalizeActiveChain();
        }

        if ((nowSec - lastHitTimeSec_) > maxHitGapSec_)
        {
            return finalizeActiveChain();
        }

        return std::nullopt;
    }

    std::optional<ComboResolved> ComboJudge::registerHit(double nowSec, const POINT& impactPoint, bool slicing)
    {
        if (!slicing)
        {
            return std::nullopt;
        }

        if (!chainActive_)
        {
            beginNewChain(nowSec, impactPoint);
            return std::nullopt;
        }

        if ((nowSec - lastHitTimeSec_) > maxHitGapSec_)
        {
            std::optional<ComboResolved> resolved = finalizeActiveChain();
            beginNewChain(nowSec, impactPoint);
            return resolved;
        }

        ++chainHits_;
        lastHitTimeSec_ = nowSec;
        lastHitPoint_ = impactPoint;
        return std::nullopt;
    }

    void ComboJudge::reset()
    {
        chainActive_ = false;
        chainHits_ = 0;
        lastHitTimeSec_ = 0.0;
        lastHitPoint_ = POINT{};
    }

    std::optional<ComboResolved> ComboJudge::finalizeActiveChain()
    {
        if (!chainActive_)
        {
            return std::nullopt;
        }

        const int hits = chainHits_;
        const POINT hitPoint = lastHitPoint_;

        chainActive_ = false;
        chainHits_ = 0;
        lastHitTimeSec_ = 0.0;
        lastHitPoint_ = POINT{};

        if (hits < ComboConfig::kMinComboCount)
        {
            return std::nullopt;
        }

        ComboResolved resolved;
        resolved.comboCount = std::min(hits, ComboConfig::kMaxComboCount);
        resolved.impactPoint = hitPoint;
        return resolved;
    }

    void ComboJudge::beginNewChain(double nowSec, const POINT& impactPoint)
    {
        chainActive_ = true;
        chainHits_ = 1;
        lastHitTimeSec_ = nowSec;
        lastHitPoint_ = impactPoint;
    }
}
