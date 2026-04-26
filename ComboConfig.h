#pragma once

namespace FruitGame::ComboConfig
{
    // Timing and recognition rules.
    inline constexpr double kMaxHitGapSec = 0.15;
    inline constexpr int kMinComboCount = 3;
    inline constexpr int kMaxComboCount = 6;

    // Combo text effect tuning.
    inline constexpr double kTextDurationSec = 0.34;
    inline constexpr int kTextRisePixels = 0;
    inline constexpr int kAnchorOffsetY = 30;
    inline constexpr int kLineGap = 36;
    inline constexpr int kLetterSpacing = 3;

    // Typography tuned to match the provided official style sample.
    inline constexpr const wchar_t* kFontFace = L"Impact";
    inline constexpr int kFruitLineFontSize = 42;
    inline constexpr int kComboLineFontSize = 42;
    inline constexpr int kPlusLineFontSize = 56;

    // Pop animation: grow fast, then settle.
    inline constexpr double kPopGrowEndSec = 0.11;
    inline constexpr double kPopSettleEndSec = 0.22;
    inline constexpr double kScaleStart = 0.52;
    inline constexpr double kScalePeak = 1.28;
    inline constexpr double kScaleSettle = 0.96;
}

