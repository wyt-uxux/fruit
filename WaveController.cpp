#define NOMINMAX
#include "WaveController.h"

#include <algorithm>
#include <chrono>
#include <random>

namespace FruitGame
{
    namespace
    {
        inline double clamp01(double value)
        {
            return std::max(0.0, std::min(1.0, value));
        }

        SpawnBatch makeBatch(int count, bool coherentVertical, WaveType type, bool bomb)
        {
            SpawnBatch batch;
            batch.count = count;
            batch.coherentVertical = coherentVertical;
            batch.waveType = type;
            batch.bomb = bomb;
            return batch;
        }

        SpawnBatch makeBatch(int count, bool coherentVertical, WaveType type, bool bomb, bool sameFruit)
        {
            SpawnBatch batch = makeBatch(count, coherentVertical, type, bomb);
            batch.sameFruit = sameFruit;
            return batch;
        }
    }

    WaveController::WaveController()
        : currentGroup_(buildGroup(0))
    {
        const auto now = static_cast<unsigned long long>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::random_device rd;
        std::seed_seq seedSeq{
            rd(), rd(), rd(), rd(),
            static_cast<unsigned int>(now & 0xFFFFFFFFULL),
            static_cast<unsigned int>((now >> 32) & 0xFFFFFFFFULL) };
        rng_.seed(seedSeq);
        primeGroup();
    }

    WaveController::WaveGroup WaveController::buildGroup(std::size_t groupIndex)
    {
        WaveGroup group;
        std::uniform_real_distribution<double> rareRoll(0.0, 1.0);

        auto useSameFruitVariant = [&]() -> bool
            {
                return rareRoll(rng_) < 0.18;
            };

        auto addStep = [&](std::size_t index, double durationSec, int count, bool coherentVertical, WaveType type, bool bomb, bool sameFruit = false)
            {
                group.steps[index].durationSec = durationSec;
                group.steps[index].batch = makeBatch(count, coherentVertical, type, bomb, sameFruit);
                group.stepCount = std::max(group.stepCount, index + 1);
                group.type = type;
            };

        switch (groupIndex)
        {
        case 0:
            addStep(0, 0.0, 1, false, WaveType::CalmSingle, false);
            group.pauseAfterSec = 2.50;
            group.type = WaveType::CalmSingle;
            break;
        case 1:
            addStep(0, 0.0, 1, false, WaveType::CalmSingle, false);
            group.pauseAfterSec = 2.50;
            group.type = WaveType::CalmSingle;
            break;
        case 2:
            addStep(0, 0.0, 1, false, WaveType::BreathingGap, true);
            group.pauseAfterSec = 2.50;
            group.type = WaveType::BreathingGap;
            break;
        case 3:
            addStep(0, 0.0, 1, false, WaveType::SingleChain, false);
            addStep(1, 0.70, 1, false, WaveType::SingleChain, false);
            addStep(2, 0.70, 1, false, WaveType::SingleChain, false);
            addStep(3, 0.70, 1, false, WaveType::SingleChain, false);
            addStep(4, 0.70, 1, false, WaveType::SingleChain, false);
            addStep(5, 0.70, 1, false, WaveType::SingleChain, false);
            addStep(6, 0.70, 1, false, WaveType::SingleChain, false);
            group.pauseAfterSec = 2.50;
            group.type = WaveType::SingleChain;
            break;
        case 4:
            addStep(0, 0.0, 1, false, WaveType::MixedRush, true);
            addStep(1, 0.70, 1, false, WaveType::MixedRush, true);
            addStep(2, 0.72, 1, false, WaveType::MixedRush, false);
            addStep(3, 0.72, 1, false, WaveType::MixedRush, false);
            addStep(4, 0.72, 1, false, WaveType::MixedRush, false);
            group.pauseAfterSec = 2.50;
            group.type = WaveType::MixedRush;
            break;
        case 5:
            addStep(0, 0.0, 5, true, WaveType::VerticalCombo, false, useSameFruitVariant() && rareRoll(rng_) < 0.40);
            group.pauseAfterSec = 2.50;
            group.type = WaveType::VerticalCombo;
            break;
        case 6:
            addStep(0, 0.0, 4, true, WaveType::VerticalCombo, false, useSameFruitVariant() && rareRoll(rng_) < 0.30);
            group.pauseAfterSec = 2.50;
            group.type = WaveType::VerticalCombo;
            break;
        case 7:
            addStep(0, 0.0, 2, false, WaveType::MixedRush, false);
            addStep(1, 0.70, 1, false, WaveType::MixedRush, true);
            addStep(2, 0.72, 2, false, WaveType::MixedRush, false);
            addStep(3, 0.70, 1, false, WaveType::MixedRush, true);
            addStep(4, 0.72, 1, false, WaveType::MixedRush, false);
            group.pauseAfterSec = 2.50;
            group.type = WaveType::MixedRush;
            break;
        case 8:
            addStep(0, 0.0, 2, false, WaveType::VerticalCombo, false);
            addStep(1, 0.72, 2, false, WaveType::VerticalCombo, false);
            addStep(2, 0.70, 2, false, WaveType::VerticalCombo, false);
            addStep(3, 0.72, 2, false, WaveType::VerticalCombo, false);
            addStep(4, 0.70, 2, false, WaveType::VerticalCombo, false);
            group.pauseAfterSec = 2.50;
            group.type = WaveType::VerticalCombo;
            break;
        case 9:
            addStep(0, 0.0, 1, false, WaveType::CalmSingle, false);
            group.pauseAfterSec = 2.50;
            group.type = WaveType::CalmSingle;
            break;
        default:
            addStep(0, 0.0, 2, false, WaveType::VerticalCombo, false);
            addStep(1, 0.72, 2, false, WaveType::VerticalCombo, false);
            addStep(2, 0.70, 2, false, WaveType::VerticalCombo, false);
            addStep(3, 0.72, 2, false, WaveType::VerticalCombo, false);
            addStep(4, 0.70, 2, false, WaveType::VerticalCombo, false);
            group.pauseAfterSec = 2.50;
            group.type = WaveType::VerticalCombo;
            break;
        }

        return group;
    }

    void WaveController::advanceGroup()
    {
        if (groupIndex_ < 3)
        {
            groupIndex_ += 1;
        }
        else
        {
            static constexpr std::array<std::pair<std::size_t, int>, kCycleGroupCount> kWeightedGroups = { {
                {3, 22},
                {4, 15},
                {5, 9},
                {6, 8},
                {7, 17},
                {8, 21},
                {9, 8},
            } };

            std::uniform_int_distribution<int> weightDist(1, 100);
            const int roll = weightDist(rng_);
            int cumulative = 0;
            for (const auto& pair : kWeightedGroups)
            {
                const auto& index = pair.first;
                const auto& weight = pair.second;
                cumulative += weight;
                if (roll <= cumulative)
                {
                    groupIndex_ = index;
                    break;
                }
            }

            if (groupIndex_ < 3)
            {
                groupIndex_ = 9;
            }
        }

        currentGroup_ = buildGroup(groupIndex_);
        primeGroup();
    }

    void WaveController::primeGroup()
    {
        stepElapsed_ = 0.0;
        groupPauseElapsed_ = 0.0;
        hasPendingBatch_ = false;
        pendingBatch_ = {};
        inGroupPause_ = false;

        if (currentGroup_.stepCount == 0)
        {
            stepIndex_ = 0;
            return;
        }

        pendingBatch_ = currentGroup_.steps[0].batch;
        hasPendingBatch_ = true;
        stepIndex_ = 1;

        if (currentGroup_.stepCount == 1)
        {
            inGroupPause_ = true;
        }
    }

    void WaveController::update(double dt)
    {
        if (hasPendingBatch_)
        {
            return;
        }

        if (currentGroup_.stepCount == 0)
        {
            currentGroup_ = buildGroup(groupIndex_);
        }

        if (inGroupPause_)
        {
            groupPauseElapsed_ += dt;
            if (groupPauseElapsed_ >= currentGroup_.pauseAfterSec)
            {
                advanceGroup();
            }
            return;
        }

        stepElapsed_ += dt;
        while (!inGroupPause_ && stepIndex_ < currentGroup_.stepCount && stepElapsed_ >= currentGroup_.steps[stepIndex_].durationSec)
        {
            stepElapsed_ -= currentGroup_.steps[stepIndex_].durationSec;
            pendingBatch_ = currentGroup_.steps[stepIndex_].batch;
            hasPendingBatch_ = true;
            stepIndex_ += 1;

            if (stepIndex_ >= currentGroup_.stepCount)
            {
                inGroupPause_ = true;
                groupPauseElapsed_ = 0.0;
                break;
            }

            if (hasPendingBatch_)
            {
                break;
            }
        }
    }

    bool WaveController::tryConsumeBatch(SpawnBatch& batch)
    {
        if (!hasPendingBatch_)
        {
            return false;
        }

        batch = pendingBatch_;
        hasPendingBatch_ = false;
        pendingBatch_ = {};
        return true;
    }

    void WaveController::setDifficultyTier(int tier) noexcept
    {
        difficultyTier_ = std::max(0, std::min(tier, 3));
    }

    WaveType WaveController::currentWaveType() const
    {
        return currentGroup_.type;
    }
}
