#pragma once

#include <array>
#include <cstddef>
#include <random>

namespace FruitGame
{
    enum class WaveType
    {
        CalmSingle,
        SingleChain,
        VerticalCombo,
        MixedRush,
        BreathingGap
    };

    struct SpawnBatch
    {
        int count = 0;
        bool coherentVertical = false;
        WaveType waveType = WaveType::CalmSingle;
        bool bomb = false;
        bool sameFruit = false;
    };

    class WaveController
    {
    public:
        WaveController();

        void update(double dt);
        void setDifficultyTier(int tier) noexcept;
        [[nodiscard]] bool tryConsumeBatch(SpawnBatch& batch);
        [[nodiscard]] WaveType currentWaveType() const;

    private:
        struct WaveStep
        {
            SpawnBatch batch;
            double durationSec;
        };

        static constexpr std::size_t kMaxStepsPerGroup = 10;

        struct WaveGroup
        {
            std::array<WaveStep, kMaxStepsPerGroup> steps{};
            std::size_t stepCount = 0;
            double pauseAfterSec = 2.5;
            WaveType type = WaveType::CalmSingle;
        };

        [[nodiscard]] WaveGroup buildGroup(std::size_t groupIndex);
        void primeGroup();
        void advanceGroup();

        static constexpr std::size_t kCycleGroupCount = 7;
        WaveGroup currentGroup_{};
        std::size_t groupIndex_ = 0;
        std::size_t nextGroupIndex_ = 3;
        std::size_t stepIndex_ = 0;
        double stepElapsed_ = 0.0;
        bool inGroupPause_ = false;
        double groupPauseElapsed_ = 0.0;
        bool hasPendingBatch_ = false;
        SpawnBatch pendingBatch_{};
        int difficultyTier_ = 0;
        std::mt19937 rng_;
    };
}
