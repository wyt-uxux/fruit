#define NOMINMAX
#include "ComboEffects.h"

#include "ComboConfig.h"
#include "Fruit.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace FruitGame
{
    namespace
    {
        inline double clamp01(double value)
        {
            return std::max(0.0, std::min(1.0, value));
        }

        inline double easeOutCubic(double t)
        {
            t = clamp01(t);
            const double u = 1.0 - t;
            return 1.0 - u * u * u;
        }

        inline double easeOutQuad(double t)
        {
            t = clamp01(t);
            return 1.0 - (1.0 - t) * (1.0 - t);
        }

        double comboPopScale(double timerSec)
        {
            if (timerSec <= ComboConfig::kPopGrowEndSec)
            {
                const double t = timerSec / ComboConfig::kPopGrowEndSec;
                return ComboConfig::kScaleStart +
                    (ComboConfig::kScalePeak - ComboConfig::kScaleStart) * easeOutCubic(t);
            }

            if (timerSec <= ComboConfig::kPopSettleEndSec)
            {
                const double t = (timerSec - ComboConfig::kPopGrowEndSec) /
                    std::max(0.001, ComboConfig::kPopSettleEndSec - ComboConfig::kPopGrowEndSec);
                return ComboConfig::kScalePeak +
                    (ComboConfig::kScaleSettle - ComboConfig::kScalePeak) * easeOutQuad(t);
            }

            const double tail = (timerSec - ComboConfig::kPopSettleEndSec) /
                std::max(0.001, ComboConfig::kTextDurationSec - ComboConfig::kPopSettleEndSec);
            return ComboConfig::kScaleSettle + (1.0 - ComboConfig::kScaleSettle) * easeOutQuad(tail);
        }

        int clampInt(int value, int low, int high)
        {
            return std::max(low, std::min(value, high));
        }

        int textWidthSpaced(const std::wstring& text, int fontSize, int letterSpacing)
        {
            if (text.empty())
            {
                return 0;
            }

            settextstyle(std::max(8, fontSize), 0, ComboConfig::kFontFace);
            int width = 0;
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const wchar_t ch[2] = { text[i], 0 };
                width += textwidth(ch);
                if (i + 1 < text.size())
                {
                    width += letterSpacing;
                }
            }
            return width;
        }

        void drawSpacedTextColor(int x, int y, const std::wstring& text, int fontSize, int letterSpacing, COLORREF color)
        {
            settextstyle(std::max(8, fontSize), 0, ComboConfig::kFontFace);
            settextcolor(color);

            int penX = x;
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const wchar_t ch[2] = { text[i], 0 };
                outtextxy(penX, y, ch);
                penX += textwidth(ch);
                if (i + 1 < text.size())
                {
                    penX += letterSpacing;
                }
            }
        }

        void drawGoldOutlinedText(int x, int y, const std::wstring& text, int fontSize, int letterSpacing, COLORREF mainColor, COLORREF lightColor)
        {
            // Hard outer stroke for crisp corner definition.
            drawSpacedTextColor(x - 2, y, text, fontSize, letterSpacing, RGB(36, 24, 8));
            drawSpacedTextColor(x + 2, y, text, fontSize, letterSpacing, RGB(36, 24, 8));
            drawSpacedTextColor(x, y - 2, text, fontSize, letterSpacing, RGB(36, 24, 8));
            drawSpacedTextColor(x, y + 2, text, fontSize, letterSpacing, RGB(36, 24, 8));

            // Inner stroke.
            drawSpacedTextColor(x - 1, y - 1, text, fontSize, letterSpacing, RGB(92, 68, 22));
            drawSpacedTextColor(x + 1, y - 1, text, fontSize, letterSpacing, RGB(92, 68, 22));
            drawSpacedTextColor(x - 1, y + 1, text, fontSize, letterSpacing, RGB(92, 68, 22));
            drawSpacedTextColor(x + 1, y + 1, text, fontSize, letterSpacing, RGB(92, 68, 22));

            // Main fill
            drawSpacedTextColor(x, y, text, fontSize, letterSpacing, mainColor);

            // Top highlight for metallic look, but keep edges hard.
            drawSpacedTextColor(x, y - 2, text, fontSize, letterSpacing, lightColor);
        }
    }

    class ComboTextEffect final : public Effect
    {
    public:
        ComboTextEffect(POINT impactPoint, int comboCount)
            : impactPoint_(impactPoint), comboCount_(std::max(ComboConfig::kMinComboCount, std::min(comboCount, ComboConfig::kMaxComboCount)))
        {
        }

        void update(double dt) override
        {
            timer_ += dt;
        }

        void render() const override
        {
            if (!isAlive())
            {
                return;
            }

            const double t = clamp01(timer_ / ComboConfig::kTextDurationSec);
            const double scale = comboPopScale(timer_);
            const int rise = static_cast<int>(std::lround(static_cast<double>(ComboConfig::kTextRisePixels) * t));
            const int anchorY = clampInt(impactPoint_.y - ComboConfig::kAnchorOffsetY - rise, 20, kWindowHeight - 40);
            const int anchorX = clampInt(impactPoint_.x, 80, kWindowWidth - 80);

            const std::wstring line1 = std::to_wstring(comboCount_) + L" FRUIT";
            const std::wstring line2 = L"COMBO";
            const std::wstring line3 = L"+" + std::to_wstring(comboCount_);

            const int fade = static_cast<int>(std::lround(45.0 * t));
            const COLORREF mainGold = RGB(245, std::max(158, 196 - fade), 42);
            const COLORREF highGold = RGB(255, std::max(205, 236 - fade), 140);

            setbkmode(TRANSPARENT);

            const int line1Size = std::max(8, static_cast<int>(std::lround(ComboConfig::kFruitLineFontSize * scale)));
            const int line2Size = std::max(8, static_cast<int>(std::lround(ComboConfig::kComboLineFontSize * scale)));
            const int line3Size = std::max(8, static_cast<int>(std::lround(ComboConfig::kPlusLineFontSize * scale)));
            const int lineGap = std::max(10, static_cast<int>(std::lround(ComboConfig::kLineGap * scale)));
            const int letterSpacing = std::max(1, static_cast<int>(std::lround(ComboConfig::kLetterSpacing * scale)));

            int line1X = anchorX - textWidthSpaced(line1, line1Size, letterSpacing) / 2;
            int line1Y = anchorY - lineGap;
            line1X = clampInt(line1X, 4, kWindowWidth - 280);

            int line2X = anchorX - textWidthSpaced(line2, line2Size, letterSpacing) / 2;
            int line2Y = anchorY;
            line2X = clampInt(line2X, 4, kWindowWidth - 220);

            int line3X = anchorX - textWidthSpaced(line3, line3Size, letterSpacing) / 2;
            int line3Y = anchorY + lineGap;
            line3X = clampInt(line3X, 4, kWindowWidth - 180);

            drawGoldOutlinedText(line1X, line1Y, line1, line1Size, letterSpacing, mainGold, highGold);
            drawGoldOutlinedText(line2X, line2Y, line2, line2Size, letterSpacing, mainGold, highGold);
            drawGoldOutlinedText(line3X, line3Y, line3, line3Size, letterSpacing, mainGold, highGold);
        }

        bool isAlive() const override
        {
            return timer_ < ComboConfig::kTextDurationSec;
        }

    private:
        POINT impactPoint_{};
        int comboCount_ = ComboConfig::kMinComboCount;
        double timer_ = 0.0;
    };

    std::unique_ptr<Effect> makeComboTextEffect(POINT impactPoint, int comboCount)
    {
        return std::make_unique<ComboTextEffect>(impactPoint, comboCount);
    }
}
