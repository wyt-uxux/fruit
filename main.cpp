#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <conio.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include <cwctype>

#include "Fruit.h"
#include "Effect.h"
#include "Knife.h"
#include "SlashEffects.h"
#include "ComboJudge.h"
#include "ComboEffects.h"
#include "MusicController.h"

namespace fs = std::filesystem;

namespace
{
    struct MissIconSlot
    {
        const IMAGE* normal = nullptr;
        const IMAGE* failed = nullptr;
        int startX = 0;
        int endX = 0;
        int y = 0;
        bool wasFailed = false;
        double animTimer = 1.0;
    };

    inline unsigned char getA(DWORD c)
    {
        return static_cast<unsigned char>((c >> 24) & 0xFF);
    }

    double clamp01(double v)
    {
        return std::max(0.0, std::min(1.0, v));
    }

    double easeOutBack(double t)
    {
        t = clamp01(t);
        const double c1 = 1.70158;
        const double c3 = c1 + 1.0;
        const double u = t - 1.0;
        return 1.0 + c3 * u * u * u + c1 * u * u;
    }

    double easeOutExpo(double t)
    {
        t = clamp01(t);
        if (t >= 1.0)
        {
            return 1.0;
        }
        return 1.0 - std::pow(2.0, -10.0 * t);
    }

    inline unsigned char getR(DWORD c)
    {
        return static_cast<unsigned char>((c >> 16) & 0xFF);
    }

    inline unsigned char getG(DWORD c)
    {
        return static_cast<unsigned char>((c >> 8) & 0xFF);
    }

    inline unsigned char getB(DWORD c)
    {
        return static_cast<unsigned char>(c & 0xFF);
    }

    inline DWORD makeARGB(unsigned char a, unsigned char r, unsigned char g, unsigned char b)
    {
        return (static_cast<DWORD>(a) << 24) |
            (static_cast<DWORD>(r) << 16) |
            (static_cast<DWORD>(g) << 8) |
            static_cast<DWORD>(b);
    }

    int colorDistanceSq(DWORD px, COLORREF ref)
    {
        const int dr = static_cast<int>(getR(px)) - static_cast<int>(GetRValue(ref));
        const int dg = static_cast<int>(getG(px)) - static_cast<int>(GetGValue(ref));
        const int db = static_cast<int>(getB(px)) - static_cast<int>(GetBValue(ref));
        return dr * dr + dg * dg + db * db;
    }

    bool isNearBlack(DWORD px)
    {
        return getR(px) < 18 && getG(px) < 18 && getB(px) < 18;
    }

    void removeBlackMatte(IMAGE& image)
    {
        const int width = image.getwidth();
        const int height = image.getheight();
        if (width <= 0 || height <= 0)
        {
            return;
        }

        DWORD* buffer = GetImageBuffer(&image);
        if (buffer == nullptr)
        {
            return;
        }

        for (int i = 0; i < width * height; ++i)
        {
            DWORD& px = buffer[i];
            const unsigned char a = getA(px);
            const unsigned char r = getR(px);
            const unsigned char g = getG(px);
            const unsigned char b = getB(px);

            if (a == 0 && r < 10 && g < 10 && b < 10)
            {
                px = makeARGB(0, 0, 0, 0);
                continue;
            }

            if (isNearBlack(px) || colorDistanceSq(px, RGB(0, 0, 0)) < 700)
            {
                px = makeARGB(0, 0, 0, 0);
                continue;
            }

            if (a < 255)
            {
                const int aa = std::max(1, static_cast<int>(a));
                const unsigned char rr = static_cast<unsigned char>(std::min(255, (static_cast<int>(r) * 255 + aa / 2) / aa));
                const unsigned char gg = static_cast<unsigned char>(std::min(255, (static_cast<int>(g) * 255 + aa / 2) / aa));
                const unsigned char bb = static_cast<unsigned char>(std::min(255, (static_cast<int>(b) * 255 + aa / 2) / aa));
                px = makeARGB(a, rr, gg, bb);
            }
        }
    }

    void cleanupAlphaMatte(IMAGE& image)
    {
        const int width = image.getwidth();
        const int height = image.getheight();
        if (width <= 0 || height <= 0)
        {
            return;
        }

        DWORD* buffer = GetImageBuffer(&image);
        if (buffer == nullptr)
        {
            return;
        }

        const int edgeThreshold = 16;
        const int matteThreshold = 1200;

        auto processPixel = [&](int index, bool forceTransparent)
            {
                DWORD& px = buffer[index];
                const unsigned char a = getA(px);
                const unsigned char r = getR(px);
                const unsigned char g = getG(px);
                const unsigned char b = getB(px);

                if (forceTransparent || (a == 0 && r < 10 && g < 10 && b < 10))
                {
                    px = makeARGB(0, 0, 0, 0);
                    return;
                }

                if (a < 255)
                {
                    const int aa = std::max(1, static_cast<int>(a));
                    const unsigned char rr = static_cast<unsigned char>(std::min(255, (static_cast<int>(r) * 255 + aa / 2) / aa));
                    const unsigned char gg = static_cast<unsigned char>(std::min(255, (static_cast<int>(g) * 255 + aa / 2) / aa));
                    const unsigned char bb = static_cast<unsigned char>(std::min(255, (static_cast<int>(b) * 255 + aa / 2) / aa));
                    px = makeARGB(a, rr, gg, bb);
                }
            };

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const int index = y * width + x;
                const bool onEdge = x < edgeThreshold || y < edgeThreshold || x >= width - edgeThreshold || y >= height - edgeThreshold;
                if (onEdge)
                {
                    const DWORD px = buffer[index];
                    if (getA(px) == 0 || colorDistanceSq(px, RGB(0, 0, 0)) < matteThreshold)
                    {
                        processPixel(index, true);
                        continue;
                    }
                }

                processPixel(index, false);
            }
        }
    }

    void sanitizeFlashSprite(IMAGE& image)
    {
        const int width = image.getwidth();
        const int height = image.getheight();
        if (width <= 0 || height <= 0)
        {
            return;
        }

        DWORD* buffer = GetImageBuffer(&image);
        if (buffer == nullptr)
        {
            return;
        }

        for (int i = 0; i < width * height; ++i)
        {
            DWORD& px = buffer[i];
            const int r = static_cast<int>(getR(px));
            const int g = static_cast<int>(getG(px));
            const int b = static_cast<int>(getB(px));

            const int brightness = (r + g + b) / 3;
            const int maxc = std::max(r, std::max(g, b));
            const int minc = std::min(r, std::min(g, b));
            const int chroma = maxc - minc;

            const bool dark = (r <= 24 && g <= 24 && b <= 24);
            const bool whiteCore = (brightness >= 170 && chroma <= 85);
            const bool hotCore = (brightness >= 210 && r >= g && g >= b && (r - b) <= 55);
            const bool keep = whiteCore || hotCore;

            if (dark || !keep)
            {
                px = makeARGB(0, 0, 0, 0);
                continue;
            }

            // Normalize the preserved core toward bright tones to suppress mesh texture.
            const int lift = std::min(255, brightness + 35);
            const unsigned char rr = static_cast<unsigned char>(std::min(255, std::max(lift, r)));
            const unsigned char gg = static_cast<unsigned char>(std::min(255, std::max(lift, g)));
            const unsigned char bb = static_cast<unsigned char>(std::min(255, std::max(lift, b)));
            px = makeARGB(255, rr, gg, bb);
        }
    }

    COLORREF juiceColorForFruit(const std::wstring& name)
    {
        if (name == L"apple")
        {
            return RGB(200, 233, 37);
        }
        if (name == L"banana")
        {
            return RGB(245, 218, 78);
        }
        if (name == L"peach")
        {
            return RGB(230, 199, 49);
        }
        if (name == L"sandia")
        {
            return RGB(204, 0, 0);
        }
        if (name == L"basaha")
        {
            return RGB(204, 0, 0);
        }
        return RGB(220, 220, 220);
    }

    FruitGame::CutSfxKind cutSfxForFruit(const std::wstring& name)
    {
        if (name == L"banana")
        {
            return FruitGame::CutSfxKind::Banana;
        }

        // Juicy fruits keep original heavy splatter.
        if (name == L"sandia")
        {
            return FruitGame::CutSfxKind::Splatter;
        }

        // Smaller fruits default to splatter2.
        return FruitGame::CutSfxKind::Splatter2;
    }

    void drawOutlinedText(int x, int y, const std::wstring& text, int size, const wchar_t* fontFace, COLORREF fillColor)
    {
        setbkmode(TRANSPARENT);
        settextstyle(size, 0, fontFace);
        settextcolor(RGB(20, 20, 20));
        outtextxy(x - 2, y + 2, text.c_str());
        outtextxy(x + 2, y + 2, text.c_str());
        outtextxy(x - 2, y - 1, text.c_str());
        outtextxy(x + 2, y - 1, text.c_str());
        settextcolor(fillColor);
        outtextxy(x, y, text.c_str());
    }

    int lerpInt(int from, int to, double t)
    {
        return static_cast<int>(std::lround(static_cast<double>(from) + (static_cast<double>(to - from) * clamp01(t))));
    }

    POINT closestPointOnSegment(const POINT& p, const POINT& a, const POINT& b)
    {
        const double abx = static_cast<double>(b.x - a.x);
        const double aby = static_cast<double>(b.y - a.y);
        const double apx = static_cast<double>(p.x - a.x);
        const double apy = static_cast<double>(p.y - a.y);
        const double ab2 = abx * abx + aby * aby;

        if (ab2 <= 1e-6)
        {
            return a;
        }

        const double t = clamp01((apx * abx + apy * aby) / ab2);
        POINT q{};
        q.x = static_cast<LONG>(std::lround(static_cast<double>(a.x) + abx * t));
        q.y = static_cast<LONG>(std::lround(static_cast<double>(a.y) + aby * t));
        return q;
    }

    double distanceSqPointToSegment(const POINT& p, const POINT& a, const POINT& b)
    {
        const double abx = static_cast<double>(b.x - a.x);
        const double aby = static_cast<double>(b.y - a.y);
        const double apx = static_cast<double>(p.x - a.x);
        const double apy = static_cast<double>(p.y - a.y);
        const double ab2 = abx * abx + aby * aby;

        if (ab2 <= 1e-6)
        {
            return apx * apx + apy * apy;
        }

        const double t = clamp01((apx * abx + apy * aby) / ab2);
        const double cx = static_cast<double>(a.x) + abx * t;
        const double cy = static_cast<double>(a.y) + aby * t;
        const double dx = static_cast<double>(p.x) - cx;
        const double dy = static_cast<double>(p.y) - cy;
        return dx * dx + dy * dy;
    }

    void drawImageAlphaScaled(const IMAGE* img, int dstX, int dstY, double scale, double alphaMul);

    void drawImageAlphaRotated(const IMAGE* img, int centerX, int centerY, double angleRad, double scale, double alphaMul = 1.0)
    {
        if (img == nullptr || img->getwidth() <= 0 || img->getheight() <= 0 || scale <= 1e-5)
        {
            return;
        }

        DWORD* dstBuf = GetImageBuffer();
        DWORD* srcBuf = GetImageBuffer(const_cast<IMAGE*>(img));
        if (dstBuf == nullptr || srcBuf == nullptr)
        {
            return;
        }

        const int dstW = getwidth();
        const int dstH = getheight();
        const int srcW = img->getwidth();
        const int srcH = img->getheight();

        const double c = std::cos(angleRad);
        const double s = std::sin(angleRad);
        const double absC = std::abs(c);
        const double absS = std::abs(s);
        const double halfW = 0.5 * static_cast<double>(srcW - 1);
        const double halfH = 0.5 * static_cast<double>(srcH - 1);

        const int outHalfW = std::max(1, static_cast<int>(std::ceil((absC * srcW + absS * srcH) * scale * 0.5)));
        const int outHalfH = std::max(1, static_cast<int>(std::ceil((absS * srcW + absC * srcH) * scale * 0.5)));

        const int minX = std::max(0, centerX - outHalfW);
        const int maxX = std::min(dstW - 1, centerX + outHalfW);
        const int minY = std::max(0, centerY - outHalfH);
        const int maxY = std::min(dstH - 1, centerY + outHalfH);

        const double invScale = 1.0 / scale;
        const double alpha = clamp01(alphaMul);

        for (int py = minY; py <= maxY; ++py)
        {
            const double dy = static_cast<double>(py - centerY);
            for (int px = minX; px <= maxX; ++px)
            {
                const double dx = static_cast<double>(px - centerX);

                const double u = (c * dx + s * dy) * invScale + halfW;
                const double v = (-s * dx + c * dy) * invScale + halfH;
                if (u < 0.0 || v < 0.0 || u >= static_cast<double>(srcW) || v >= static_cast<double>(srcH))
                {
                    continue;
                }

                const int sx = std::min(srcW - 1, std::max(0, static_cast<int>(std::lround(u))));
                const int sy = std::min(srcH - 1, std::max(0, static_cast<int>(std::lround(v))));
                const DWORD srcPx = srcBuf[sy * srcW + sx];
                int a = static_cast<int>(std::lround(static_cast<double>(getA(srcPx)) * alpha));
                if (a <= 0)
                {
                    continue;
                }

                DWORD& dstPx = dstBuf[py * dstW + px];
                if (a >= 255)
                {
                    dstPx = makeARGB(0xFF, getR(srcPx), getG(srcPx), getB(srcPx));
                    continue;
                }

                const int ia = 255 - a;
                const unsigned char rr = static_cast<unsigned char>((getR(srcPx) * a + getR(dstPx) * ia) / 255);
                const unsigned char gg = static_cast<unsigned char>((getG(srcPx) * a + getG(dstPx) * ia) / 255);
                const unsigned char bb = static_cast<unsigned char>((getB(srcPx) * a + getB(dstPx) * ia) / 255);
                dstPx = makeARGB(0xFF, rr, gg, bb);
            }
        }
    }

    void drawScoreHud(const IMAGE* scoreIcon, int score, double introProgress)
    {
        constexpr int kRefW = 640;
        constexpr int kRefH = 480;
        const double sx = static_cast<double>(FruitGame::kWindowWidth) / static_cast<double>(kRefW);
        const double sy = static_cast<double>(FruitGame::kWindowHeight) / static_cast<double>(kRefH);
        const double eased = easeOutExpo(introProgress);

        const int iconSx = static_cast<int>(std::lround(-94.0 * sx));
        const int iconEx = static_cast<int>(std::lround(6.0 * sx));
        const int iconY = static_cast<int>(std::lround(4.0 * sy));
        const int textSx = static_cast<int>(std::lround(-59.0 * sx));
        const int textEx = static_cast<int>(std::lround(41.0 * sx));
        const int textY = static_cast<int>(std::lround(18.0 * sy));

        const int iconX = lerpInt(iconSx, iconEx, eased);

        if (scoreIcon != nullptr && scoreIcon->getwidth() > 0 && scoreIcon->getheight() > 0)
        {
            drawImageAlphaScaled(scoreIcon, iconX, iconY, 1.12, 1.0);
        }

        const int scoreX = lerpInt(textSx, textEx, eased);
        const std::wstring scoreText = std::to_wstring(score);

        // Match all.js score style: warm orange-yellow number with heavy dark edge.
        setbkmode(TRANSPARENT);
        settextstyle(static_cast<int>(std::lround(30.0 * sy)), 0, L"Arial Black");
        settextcolor(RGB(68, 36, 10));
        outtextxy(scoreX - 2, textY + 2, scoreText.c_str());
        outtextxy(scoreX + 2, textY + 2, scoreText.c_str());
        outtextxy(scoreX - 2, textY - 2, scoreText.c_str());
        outtextxy(scoreX + 2, textY - 2, scoreText.c_str());

        settextcolor(RGB(252, 127, 12));
        outtextxy(scoreX, textY, scoreText.c_str());

        settextcolor(RGB(255, 236, 83));
        outtextxy(scoreX, textY - 1, scoreText.c_str());

        settextstyle(static_cast<int>(std::lround(14.0 * sy)), 0, L"Arial");
        settextcolor(RGB(175, 124, 5));
        outtextxy(lerpInt(static_cast<int>(std::lround(-93.0 * sx)), static_cast<int>(std::lround(7.0 * sx)), eased),
            static_cast<int>(std::lround(52.0 * sy)),
            L"BEST 999");
    }

    void drawImageAlphaScaled(const IMAGE* img, int dstX, int dstY, double scale, double alphaMul = 1.0)
    {
        if (img == nullptr || img->getwidth() <= 0 || img->getheight() <= 0)
        {
            return;
        }

        DWORD* dstBuf = GetImageBuffer();
        DWORD* srcBuf = GetImageBuffer(const_cast<IMAGE*>(img));
        if (dstBuf == nullptr || srcBuf == nullptr)
        {
            return;
        }

        const int dstW = getwidth();
        const int dstH = getheight();
        const int srcW = img->getwidth();
        const int srcH = img->getheight();
        const int w = std::max(1, static_cast<int>(std::round(srcW * scale)));
        const int h = std::max(1, static_cast<int>(std::round(srcH * scale)));

        for (int y = 0; y < h; ++y)
        {
            const int sy = std::min(srcH - 1, static_cast<int>(y / scale));
            const int py = dstY + y;
            if (py < 0 || py >= dstH)
            {
                continue;
            }

            for (int x = 0; x < w; ++x)
            {
                const int sx = std::min(srcW - 1, static_cast<int>(x / scale));
                const int px = dstX + x;
                if (px < 0 || px >= dstW)
                {
                    continue;
                }

                const DWORD srcPx = srcBuf[sy * srcW + sx];
                int a = static_cast<int>(std::round(static_cast<double>(getA(srcPx)) * clamp01(alphaMul)));
                if (a <= 0)
                {
                    continue;
                }

                DWORD& dstPx = dstBuf[py * dstW + px];
                if (a >= 255)
                {
                    dstPx = makeARGB(0xFF, getR(srcPx), getG(srcPx), getB(srcPx));
                    continue;
                }

                const int ia = 255 - a;
                const unsigned char rr = static_cast<unsigned char>((getR(srcPx) * a + getR(dstPx) * ia) / 255);
                const unsigned char gg = static_cast<unsigned char>((getG(srcPx) * a + getG(dstPx) * ia) / 255);
                const unsigned char bb = static_cast<unsigned char>((getB(srcPx) * a + getB(dstPx) * ia) / 255);
                dstPx = makeARGB(0xFF, rr, gg, bb);
            }
        }
    }

    void blendScreenToWhite(double amount)
    {
        const double t = clamp01(amount);
        if (t <= 0.0)
        {
            return;
        }

        DWORD* dstBuf = GetImageBuffer();
        if (dstBuf == nullptr)
        {
            return;
        }

        const int pixels = getwidth() * getheight();
        const int whiteR = 255;
        const int whiteG = 255;
        const int whiteB = 255;
        const int alpha = static_cast<int>(std::round(t * 255.0));
        const int invAlpha = 255 - alpha;

        for (int i = 0; i < pixels; ++i)
        {
            DWORD px = dstBuf[i];
            const unsigned char rr = static_cast<unsigned char>((getR(px) * invAlpha + whiteR * alpha) / 255);
            const unsigned char gg = static_cast<unsigned char>((getG(px) * invAlpha + whiteG * alpha) / 255);
            const unsigned char bb = static_cast<unsigned char>((getB(px) * invAlpha + whiteB * alpha) / 255);
            dstBuf[i] = makeARGB(0xFF, rr, gg, bb);
        }
    }

    void drawExplosionRay(POINT origin, double angle, int rayLength, COLORREF coreColor, COLORREF glowColor)
    {
        if (rayLength <= 1)
        {
            return;
        }

        const double dirX = std::cos(angle);
        const double dirY = std::sin(angle);
        const double normalX = -dirY;
        const double normalY = dirX;

        DWORD* dstBuf = GetImageBuffer();
        if (dstBuf == nullptr)
        {
            return;
        }

        const int scrWidth = getwidth();
        const int scrHeight = getheight();
        const double rayLength_d = static_cast<double>(rayLength);
        const int coreR = GetRValue(coreColor);
        const int coreG = GetGValue(coreColor);
        const int coreB = GetBValue(coreColor);
        const int glowR = GetRValue(glowColor);
        const int glowG = GetGValue(glowColor);
        const int glowB = GetBValue(glowColor);

        const double endX = origin.x + dirX * rayLength_d;
        const double endY = origin.y + dirY * rayLength_d;
        const double originX_d = static_cast<double>(origin.x);
        const double originY_d = static_cast<double>(origin.y);

        int minX = std::max(0, static_cast<int>(std::min(originX_d, endX)) - 16);
        int maxX = std::min(scrWidth - 1, static_cast<int>(std::max(originX_d, endX)) + 16);
        int minY = std::max(0, static_cast<int>(std::min(originY_d, endY)) - 16);
        int maxY = std::min(scrHeight - 1, static_cast<int>(std::max(originY_d, endY)) + 16);

        for (int py = minY; py <= maxY; ++py)
        {
            for (int px = minX; px <= maxX; ++px)
            {
                const double vx = static_cast<double>(px) - origin.x;
                const double vy = static_cast<double>(py) - origin.y;

                const double projLen = vx * dirX + vy * dirY;
                if (projLen < 0.0 || projLen > rayLength_d)
                {
                    continue;
                }

                const double perpDist = std::abs(vx * normalX + vy * normalY);

                const double localWidth = 1 + 44 * (projLen / rayLength_d);
                if (perpDist > localWidth + 1.0)
                {
                    continue;
                }

                double alpha = 1.0;
                if (perpDist > localWidth)
                {
                    alpha = 1.0 - (perpDist - localWidth);
                }

                const double distRatio = clamp01(projLen / rayLength_d);
                const double colorMix = 0.3 + 0.7 * distRatio;

                const int finalR = static_cast<int>(std::round(glowR + (coreR - glowR) * colorMix));
                const int finalG = static_cast<int>(std::round(glowG + (coreG - glowG) * colorMix));
                const int finalB = static_cast<int>(std::round(glowB + (coreB - glowB) * colorMix));

                const int pixelIdx = py * scrWidth + px;
                if (pixelIdx >= 0 && pixelIdx < scrWidth * scrHeight)
                {
                    DWORD oldPixel = dstBuf[pixelIdx];
                    const int oldR = static_cast<int>(getR(oldPixel));
                    const int oldG = static_cast<int>(getG(oldPixel));
                    const int oldB = static_cast<int>(getB(oldPixel));

                    const int alphaInt = static_cast<int>(std::round(alpha * 255.0));
                    const int invAlpha = 255 - alphaInt;

                    const int blendR = (oldR * invAlpha + finalR * alphaInt) / 255;
                    const int blendG = (oldG * invAlpha + finalG * alphaInt) / 255;
                    const int blendB = (oldB * invAlpha + finalB * alphaInt) / 255;

                    dstBuf[pixelIdx] = makeARGB(0xFF, blendR, blendG, blendB);
                }
            }
        }
    }

    void drawMissCounterHud(std::array<MissIconSlot, 3>& slots, int missedCount, double dt, double introProgress)
    {
        constexpr double kAnimDuration = 0.20;
        const double eased = easeOutExpo(introProgress);

        for (int i = 0; i < 3; ++i)
        {
            MissIconSlot& slot = slots[i];
            const int baseX = lerpInt(slot.startX, slot.endX, eased);
            const bool failed = missedCount >= (i + 1);
            if (failed && !slot.wasFailed)
            {
                slot.animTimer = 0.0;
            }
            slot.wasFailed = failed;

            if (!failed)
            {
                slot.animTimer = kAnimDuration;
                if (slot.normal != nullptr)
                {
                    drawImageAlphaScaled(slot.normal, baseX, slot.y, 1.0, 1.0);
                }
                continue;
            }

            slot.animTimer = std::min(kAnimDuration, slot.animTimer + dt);
            const double t = clamp01(slot.animTimer / kAnimDuration);
            const double scale = 0.0001 + 0.9999 * easeOutBack(t);
            const double alpha = 0.20 + 0.80 * t;

            const int baseW = (slot.normal != nullptr && slot.normal->getwidth() > 0)
                ? slot.normal->getwidth()
                : ((slot.failed != nullptr) ? slot.failed->getwidth() : 24);
            const int baseH = (slot.normal != nullptr && slot.normal->getheight() > 0)
                ? slot.normal->getheight()
                : ((slot.failed != nullptr) ? slot.failed->getheight() : 24);

            const int drawW = std::max(1, static_cast<int>(std::round(baseW * scale)));
            const int drawH = std::max(1, static_cast<int>(std::round(baseH * scale)));
            const int drawX = baseX + (baseW - drawW) / 2;
            const int drawY = slot.y + (baseH - drawH) / 2;
            drawImageAlphaScaled(slot.failed, drawX, drawY, scale, alpha);
        }
    }
}

int main()
{
    initgraph(FruitGame::kWindowWidth, FruitGame::kWindowHeight);
    BeginBatchDraw();

    const fs::path fruitDir = fs::absolute("images\\fruit");
    struct LoadedFruitSet
    {
        std::unique_ptr<IMAGE> whole;
        std::unique_ptr<IMAGE> half1;
        std::unique_ptr<IMAGE> half2;
        FruitGame::FruitSpriteSet view;
        std::wstring baseName;
    };

    std::unordered_map<std::wstring, LoadedFruitSet> loadedFruitMap;
    std::vector<LoadedFruitSet> loadedFruitSets;
    std::vector<const FruitGame::FruitSpriteSet*> fruitSpriteSets;

    if (std::filesystem::exists(fruitDir) && std::filesystem::is_directory(fruitDir))
    {
        for (const auto& entry : std::filesystem::directory_iterator(fruitDir))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            const std::filesystem::path filePath = entry.path();
            std::wstring stem = filePath.stem().wstring();
            std::transform(stem.begin(), stem.end(), stem.begin(), towlower);

            if (stem == L"boom")
            {
                continue;
            }

            std::wstring ext = filePath.extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), towlower);
            if (ext != L".png" && ext != L".jpg" && ext != L".jpeg" && ext != L".bmp")
            {
                continue;
            }

            bool isHalf1 = false;
            bool isHalf2 = false;
            std::wstring baseName = stem;
            if (stem.size() > 2 && stem.rfind(L"-1") == stem.size() - 2)
            {
                isHalf1 = true;
                baseName = stem.substr(0, stem.size() - 2);
            }
            else if (stem.size() > 2 && stem.rfind(L"-2") == stem.size() - 2)
            {
                isHalf2 = true;
                baseName = stem.substr(0, stem.size() - 2);
            }

            auto image = std::make_unique<IMAGE>();
            loadimage(image.get(), filePath.c_str());
            if (image->getwidth() <= 0 || image->getheight() <= 0)
            {
                continue;
            }

            removeBlackMatte(*image);
            cleanupAlphaMatte(*image);

            LoadedFruitSet& set = loadedFruitMap[baseName];
            if (isHalf1)
            {
                set.half1 = std::move(image);
            }
            else if (isHalf2)
            {
                set.half2 = std::move(image);
            }
            else
            {
                set.whole = std::move(image);
            }

            set.baseName = baseName;
        }
    }

    for (auto& it : loadedFruitMap)
    {
        LoadedFruitSet& set = it.second;
        if (!set.whole)
        {
            continue;
        }

        set.view.whole = set.whole.get();
        set.view.half1 = set.half1 ? set.half1.get() : nullptr;
        set.view.half2 = set.half2 ? set.half2.get() : nullptr;
        set.view.juiceColor = juiceColorForFruit(set.baseName);
        set.view.isBomb = false;
        set.view.cutSfx = cutSfxForFruit(set.baseName);
        loadedFruitSets.push_back(std::move(set));
    }

    for (auto& set : loadedFruitSets)
    {
        fruitSpriteSets.push_back(&set.view);
    }

    if (fruitSpriteSets.empty())
    {
        cleardevice();
        setbkmode(TRANSPARENT);
        settextcolor(WHITE);
        outtextxy(20, 20, _T("Failed to load complete fruits in images/fruit"));
        outtextxy(20, 50, _T("Need files like apple.png, banana.png and not *-1/*-2."));
        FlushBatchDraw();
        _getch();
        EndBatchDraw();
        closegraph();
        return 1;
    }

    IMAGE bombImage;
    const std::filesystem::path bombPath = std::filesystem::absolute("images\\fruit\\boom.png");
    loadimage(&bombImage, bombPath.c_str());
    if (bombImage.getwidth() > 0 && bombImage.getheight() > 0)
    {
        removeBlackMatte(bombImage);
        cleanupAlphaMatte(bombImage);
    }

    FruitGame::FruitSpriteSet bombSpriteSetView;
    if (bombImage.getwidth() > 0 && bombImage.getheight() > 0)
    {
        bombSpriteSetView.whole = &bombImage;
        bombSpriteSetView.half1 = nullptr;
        bombSpriteSetView.half2 = nullptr;
        bombSpriteSetView.juiceColor = RGB(255, 96, 64);
        bombSpriteSetView.isBomb = true;
        bombSpriteSetView.cutSfx = FruitGame::CutSfxKind::Splatter;
    }

    IMAGE backgroundImage;
    const std::filesystem::path backgroundPath = std::filesystem::absolute("images\\background.jpg");
    loadimage(&backgroundImage, backgroundPath.c_str(), FruitGame::kWindowWidth, FruitGame::kWindowHeight, true);

    IMAGE shadowImage;
    const std::filesystem::path shadowPath = std::filesystem::absolute("images\\shadow.png");
    loadimage(&shadowImage, shadowPath.c_str());

    IMAGE gameOverImage;
    const std::filesystem::path gameOverPath = std::filesystem::absolute("images\\game-over.png");
    loadimage(&gameOverImage, gameOverPath.c_str());

    IMAGE newGameImage;
    IMAGE quitImage;
    loadimage(&newGameImage, std::filesystem::absolute("images\\new-game.png").c_str());
    loadimage(&quitImage, std::filesystem::absolute("images\\quit.png").c_str());
    if (newGameImage.getwidth() > 0)
    {
        cleanupAlphaMatte(newGameImage);
    }
    if (quitImage.getwidth() > 0)
    {
        cleanupAlphaMatte(quitImage);
    }

    IMAGE flashImage;
    const std::filesystem::path flashPath = std::filesystem::absolute("images\\flash.png");
    loadimage(&flashImage, flashPath.c_str());
    if (flashImage.getwidth() > 0 && flashImage.getheight() > 0)
    {
        sanitizeFlashSprite(flashImage);
    }

    IMAGE scoreImage;
    const std::filesystem::path scorePath = std::filesystem::absolute("images\\score.png");
    loadimage(&scoreImage, scorePath.c_str());
    if (scoreImage.getwidth() > 0)
    {
        removeBlackMatte(scoreImage);
        cleanupAlphaMatte(scoreImage);
    }

    IMAGE missXImage;
    IMAGE missXFImage;
    IMAGE missXXImage;
    IMAGE missXXFImage;
    IMAGE missXXXImage;
    IMAGE missXXXFImage;
    loadimage(&missXImage, std::filesystem::absolute("images\\x.png").c_str());
    loadimage(&missXFImage, std::filesystem::absolute("images\\xf.png").c_str());
    loadimage(&missXXImage, std::filesystem::absolute("images\\xx.png").c_str());
    loadimage(&missXXFImage, std::filesystem::absolute("images\\xxf.png").c_str());
    loadimage(&missXXXImage, std::filesystem::absolute("images\\xxx.png").c_str());
    loadimage(&missXXXFImage, std::filesystem::absolute("images\\xxxf.png").c_str());

    if (missXImage.getwidth() > 0)
        cleanupAlphaMatte(missXImage);
    if (missXFImage.getwidth() > 0)
        cleanupAlphaMatte(missXFImage);
    if (missXXImage.getwidth() > 0)
        cleanupAlphaMatte(missXXImage);
    if (missXXFImage.getwidth() > 0)
        cleanupAlphaMatte(missXXFImage);
    if (missXXXImage.getwidth() > 0)
        cleanupAlphaMatte(missXXXImage);
    if (missXXXFImage.getwidth() > 0)
        cleanupAlphaMatte(missXXXFImage);

    std::array<MissIconSlot, 3> missSlots{};
    missSlots[0].normal = (missXImage.getwidth() > 0) ? &missXImage : nullptr;
    missSlots[0].failed = (missXFImage.getwidth() > 0) ? &missXFImage : missSlots[0].normal;
    missSlots[1].normal = (missXXImage.getwidth() > 0) ? &missXXImage : nullptr;
    missSlots[1].failed = (missXXFImage.getwidth() > 0) ? &missXXFImage : missSlots[1].normal;
    missSlots[2].normal = (missXXXImage.getwidth() > 0) ? &missXXXImage : nullptr;
    missSlots[2].failed = (missXXXFImage.getwidth() > 0) ? &missXXXFImage : missSlots[2].normal;

    // Follow original Fruit Ninja lose icon layout from all.js conf1/2/3 ex,y values.
    constexpr int kRefW = 640;
    constexpr int kRefH = 480;
    const double sx = static_cast<double>(FruitGame::kWindowWidth) / static_cast<double>(kRefW);
    const double sy = static_cast<double>(FruitGame::kWindowHeight) / static_cast<double>(kRefH);

    // Keep right-top style from all.js but tighten spacing for better visual grouping.
    const int sxPos[3] = { 650, 671, 697 };
    const int exPos[3] = { 575, 589, 607 };
    const int yPos[3] = { 5, 5, 6 };
    for (int i = 0; i < 3; ++i)
    {
        missSlots[i].startX = static_cast<int>(std::round(sxPos[i] * sx));
        missSlots[i].endX = static_cast<int>(std::round(exPos[i] * sx));
        missSlots[i].y = static_cast<int>(std::round(yPos[i] * sy));
    }

    FruitGame::FruitField game(
        fruitSpriteSets,
        bombSpriteSetView.whole != nullptr ? &bombSpriteSetView : nullptr,
        backgroundImage.getwidth() > 0 ? &backgroundImage : nullptr,
        shadowImage.getwidth() > 0 ? &shadowImage : nullptr);

    FruitGame::FruitKnife knife;
    FruitGame::ComboJudge comboJudge;
    FruitGame::MusicController music;
    FruitGame::EffectManager hitEffects;
    FruitGame::EffectManager comboTextEffects;
    int score = 0;

    const IMAGE* restartFruitIcon = nullptr;
    const FruitGame::FruitSpriteSet* restartFruitSetView = nullptr;
    for (const auto& set : loadedFruitSets)
    {
        if (set.baseName == L"sandia")
        {
            restartFruitIcon = set.view.whole;
            restartFruitSetView = &set.view;
            break;
        }
    }
    if (restartFruitIcon == nullptr && !loadedFruitSets.empty())
    {
        restartFruitIcon = loadedFruitSets.front().view.whole;
        restartFruitSetView = &loadedFruitSets.front().view;
    }

    const std::filesystem::path soundDir = std::filesystem::absolute("sound");
    music.registerSceneBgm(FruitGame::UIScene::MainMenu, soundDir / "menu.mp3", true);
    music.registerSceneBgm(FruitGame::UIScene::GameOver, soundDir / "over.mp3", false);
    music.registerSceneBgm(FruitGame::UIScene::KnifeSelect, soundDir / "menu.mp3", true);
    music.registerSfx(FruitGame::SfxId::Start, soundDir / "start.mp3", 2);
    music.registerSfx(FruitGame::SfxId::Throw, soundDir / "throw.mp3", 5);
    music.registerSfx(FruitGame::SfxId::Boom, soundDir / "boom.mp3", 2);
    music.registerSfx(FruitGame::SfxId::Splatter, soundDir / "splatter.mp3", 6);
    music.registerSfx(FruitGame::SfxId::Splatter2, soundDir / "splatter2.mp3", 6);
    music.registerSfx(FruitGame::SfxId::SplatterBanana, soundDir / "splatter_banana.mp3", 4);
    music.registerSfx(FruitGame::SfxId::Combo3, soundDir / "hits_3.mp3", 2);
    music.registerSfx(FruitGame::SfxId::Combo4, soundDir / "hits_4.mp3", 2);
    music.registerSfx(FruitGame::SfxId::Error, soundDir / "error.mp3", 2);
    music.registerSfx(FruitGame::SfxId::Knife1, soundDir / "knife1.mp3", 3);
    music.registerSfx(FruitGame::SfxId::Knife2, soundDir / "knife2.mp3", 3);
    music.registerSfx(FruitGame::SfxId::Knife3, soundDir / "knife3.mp3", 3);
    music.registerSfx(FruitGame::SfxId::Knife4, soundDir / "knife4.mp3", 3);

    auto requestSceneTransition = [&music](FruitGame::UIScene scene)
        {
            music.enterScene(scene);
        };

    double bombHintTimer = 0.0;
    POINT bombHintPoint{ FruitGame::kWindowWidth / 2, FruitGame::kWindowHeight / 3 };
    enum class BombTransitionPhase
    {
        None,
        Pause,
        Rays,
        RaysHold,
        Whiteout,
        WhiteHold
    };

    BombTransitionPhase bombTransitionPhase = BombTransitionPhase::None;
    bool bombSceneRequested = false;
    double bombTransitionTimer = 0.0;
    constexpr double kBombShakeDurationSec = 1;
    constexpr double kBombWhiteHoldSec = 0.80;
    constexpr double kBombBackdropBlendSec = 0.28;
    double bombShakeTimer = 0.0;
    POINT bombShakeOrigin{};
    POINT bombImpactPoint{ FruitGame::kWindowWidth / 2, FruitGame::kWindowHeight / 3 };
    int bombRayIndex = 0;
    double bombRayEmitTimer = 0.0;
    bool bombPostGameOverMode = false;
    bool bombBackdropBlendActive = false;
    double bombBackdropBlendTimer = 0.0;
    game.setBombHitCallback([&bombHintTimer, &bombHintPoint, &bombTransitionPhase, &bombSceneRequested, &bombTransitionTimer, &bombShakeTimer, &bombImpactPoint, &bombRayIndex, &bombRayEmitTimer, &music, kBombShakeDurationSec](POINT impact)
        {
            bombHintTimer = 0.90;
            bombHintPoint = impact;
            bombImpactPoint = impact;
            bombTransitionPhase = BombTransitionPhase::Pause;
            bombSceneRequested = false;
            bombTransitionTimer = 1.55;
            bombShakeTimer = kBombShakeDurationSec;
            bombRayIndex = 0;
            bombRayEmitTimer = 0.0;
            music.playSfx(FruitGame::SfxId::Boom);
            std::cout << "[Bomb] hit! starting transition" << std::endl; });

    enum class MissGameOverPhase
    {
        None,
        Delay,
        Reveal,
        Hold
    };
    MissGameOverPhase missGameOverPhase = MissGameOverPhase::None;
    double missGameOverTimer = 0.0;
    bool missGameOverSceneRequested = false;
    constexpr double kMissStopThrowDelaySec = 2.0;
    constexpr double kMissGameOverRevealSec = 0.50;
    constexpr double kRestartEnableDelaySec = 0.75;
    bool prevRestartClickDown = false;
    bool restartMenuSliceLatched = false;
    bool restartMenuVisibleLastFrame = false;
    double restartMenuAnimTimer = 0.0;
    enum class RestartMenuSliceAction
    {
        None,
        Restart,
        Quit
    };
    RestartMenuSliceAction restartMenuSliceAction = RestartMenuSliceAction::None;
    double restartMenuSliceTimer = 0.0;
    double restartMenuSliceAngle = -0.7;

    auto bindGameCallbacks = [&]()
        {
            game.setSpawnCallback([&music](int count)
                {
                    for (int i = 0; i < count; ++i)
                    {
                        music.playSfx(FruitGame::SfxId::Throw);
                    } });

                    game.setBombHitCallback([&bombHintTimer, &bombHintPoint, &bombTransitionPhase, &bombSceneRequested, &bombTransitionTimer, &bombShakeTimer, &bombImpactPoint, &bombRayIndex, &bombRayEmitTimer, &music, kBombShakeDurationSec](POINT impact)
                        {
                            bombHintTimer = 0.90;
                            bombHintPoint = impact;
                            bombImpactPoint = impact;
                            bombTransitionPhase = BombTransitionPhase::Pause;
                            bombSceneRequested = false;
                            bombTransitionTimer = 1.55;
                            bombShakeTimer = kBombShakeDurationSec;
                            bombRayIndex = 0;
                            bombRayEmitTimer = 0.0;
                            music.playSfx(FruitGame::SfxId::Boom);
                            std::cout << "[Bomb] hit! starting transition" << std::endl; });
        };
    bindGameCallbacks();

    music.enterScene(FruitGame::UIScene::Gameplay);
    music.playSfx(FruitGame::SfxId::Start);

    bool running = true;
    const HWND hwnd = GetHWnd();
    auto previousTime = std::chrono::steady_clock::now();
    double hudIntroTimer = 0.0;
    constexpr double kHudIntroDuration = 0.50;
    constexpr double kGameplaySpawnDelaySec = 2.35;

    // Knife SFX tuning block (adjust these values for feel).
    // 1) Speed gate: raise for fewer triggers, lower for more triggers.
    constexpr double kKnifeMinSpeedPxPerSec = 980.0;
    constexpr double kKnifeExitSpeedPxPerSec = 820.0;
    // 2) Replay gap range: this is the absolute interval between two knife SFX.
    // Tune here if needed.
    constexpr double kKnifeReplayGapMinSec = 0.36;
    constexpr double kKnifeReplayGapMaxSec = 0.48;
    // 4) Smoothing: larger value reacts faster to speed changes.
    constexpr double kKnifeSpeedSmoothHz = 14.0;
    // 5) If a slice SFX happened this frame, defer knife SFX by this amount.
    constexpr double kKnifeDeferredRetrySec = 0.024;

    // Slice SFX limiter tuning block (to avoid muddy sound in combo bursts).
    constexpr double kSliceSfxBurstWindowSec = 0.18;
    constexpr int kSliceSfxBurstStartHits = 3;
    constexpr double kSliceSfxGapNormalSec = 0.016;
    constexpr double kSliceSfxGapBurstSec = 0.055;
    constexpr double kSliceSfxKeepChanceBurst = 0.42;

    constexpr double kSmallFruitHeavySplatterChance = 0.33;
    double gameTimeSec = 0.0;
    int previousMissedCount = 0;
    double sliceSfxCooldownSec = 0.0;
    double sliceSfxBurstTimerSec = 0.0;
    int sliceSfxBurstHits = 0;
    double knifeSfxCooldownSec = 0.0;
    double knifeSpeedFiltered = 0.0;
    bool knifeInHighSpeedBand = false;
    bool knifeWasSlicingLastFrame = false;
    bool knifeSfxPlayedInStroke = false;
    bool hasPrevCursorPointForSfx = false;
    POINT prevCursorPointForSfx{};
    bool knifeSfxPending = false;
    FruitGame::SfxId pendingKnifeSfx = FruitGame::SfxId::Knife1;

    std::mt19937 sfxRng(std::random_device{}());
    std::uniform_int_distribution<int> knifePickDist(0, 3);
    std::uniform_real_distribution<double> knifeGapDist(kKnifeReplayGapMinSec, kKnifeReplayGapMaxSec);
    std::uniform_real_distribution<double> cutMixDist(0.0, 1.0);
    std::uniform_real_distribution<double> sliceBurstKeepDist(0.0, 1.0);

    auto playComboSfx = [&music](int comboCount)
        {
            if (comboCount >= 4)
            {
                music.playSfx(FruitGame::SfxId::Combo4);
            }
            else if (comboCount == 3)
            {
                music.playSfx(FruitGame::SfxId::Combo3);
            }
        };

    auto queueKnifeSfx = [&](void)
        {
            switch (knifePickDist(sfxRng))
            {
            case 0:
                pendingKnifeSfx = FruitGame::SfxId::Knife1;
                break;
            case 1:
                pendingKnifeSfx = FruitGame::SfxId::Knife2;
                break;
            case 2:
                pendingKnifeSfx = FruitGame::SfxId::Knife3;
                break;
            default:
                pendingKnifeSfx = FruitGame::SfxId::Knife4;
                break;
            }
            knifeSfxPending = true;
            knifeSfxCooldownSec = knifeGapDist(sfxRng);
            knifeSfxPlayedInStroke = true;
        };
    auto resetGameplayState = [&]()
        {
            game = FruitGame::FruitField(
                fruitSpriteSets,
                bombSpriteSetView.whole != nullptr ? &bombSpriteSetView : nullptr,
                backgroundImage.getwidth() > 0 ? &backgroundImage : nullptr,
                shadowImage.getwidth() > 0 ? &shadowImage : nullptr);
            bindGameCallbacks();

            knife = FruitGame::FruitKnife();
            comboJudge.reset();
            hitEffects.clear();
            comboTextEffects.clear();

            score = 0;
            gameTimeSec = 0.0;
            previousMissedCount = 0;
            sliceSfxCooldownSec = 0.0;
            sliceSfxBurstTimerSec = 0.0;
            sliceSfxBurstHits = 0;
            knifeSfxCooldownSec = 0.0;
            knifeSpeedFiltered = 0.0;
            knifeInHighSpeedBand = false;
            knifeWasSlicingLastFrame = false;
            knifeSfxPlayedInStroke = false;
            hasPrevCursorPointForSfx = false;
            prevCursorPointForSfx = POINT{};
            knifeSfxPending = false;
            pendingKnifeSfx = FruitGame::SfxId::Knife1;

            bombHintTimer = 0.0;
            bombHintPoint = POINT{ FruitGame::kWindowWidth / 2, FruitGame::kWindowHeight / 3 };
            bombTransitionPhase = BombTransitionPhase::None;
            bombSceneRequested = false;
            bombTransitionTimer = 0.0;
            bombShakeTimer = 0.0;
            bombShakeOrigin = POINT{};
            bombImpactPoint = POINT{ FruitGame::kWindowWidth / 2, FruitGame::kWindowHeight / 3 };
            bombRayIndex = 0;
            bombRayEmitTimer = 0.0;
            bombPostGameOverMode = false;
            bombBackdropBlendActive = false;
            bombBackdropBlendTimer = 0.0;

            missGameOverPhase = MissGameOverPhase::None;
            missGameOverTimer = 0.0;
            missGameOverSceneRequested = false;
            prevRestartClickDown = false;
            restartMenuSliceLatched = false;
            restartMenuVisibleLastFrame = false;
            restartMenuAnimTimer = 0.0;
            restartMenuSliceAction = RestartMenuSliceAction::None;
            restartMenuSliceTimer = 0.0;
            restartMenuSliceAngle = -0.7;

            hudIntroTimer = 0.0;
            for (auto& slot : missSlots)
            {
                slot.wasFailed = false;
                slot.animTimer = 1.0;
            }

            music.enterScene(FruitGame::UIScene::Gameplay);
            music.playSfx(FruitGame::SfxId::Start);
        };

    while (running && IsWindow(hwnd))
    {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
        {
            running = false;
        }

        bool hadSliceHitThisFrame = false;

        const auto currentTime = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(currentTime - previousTime).count();
        previousTime = currentTime;
        dt = std::clamp(dt, 0.0, 0.033);
        gameTimeSec += dt;
        sliceSfxCooldownSec = std::max(0.0, sliceSfxCooldownSec - dt);
        sliceSfxBurstTimerSec = std::max(0.0, sliceSfxBurstTimerSec - dt);
        if (sliceSfxBurstTimerSec <= 0.0)
        {
            sliceSfxBurstHits = 0;
        }
        bombHintTimer = std::max(0.0, bombHintTimer - dt);
        hudIntroTimer = std::min(kHudIntroDuration, hudIntroTimer + dt);
        const double hudIntroProgress = clamp01(hudIntroTimer / kHudIntroDuration);

        if (bombTransitionPhase != BombTransitionPhase::None)
        {
            bombTransitionTimer -= dt;
            if (bombShakeTimer > 0.0)
            {
                bombShakeTimer = std::max(0.0, bombShakeTimer - dt);
            }
            knifeSfxPending = false;
        }

        if (bombBackdropBlendActive)
        {
            bombBackdropBlendTimer = std::min(kBombBackdropBlendSec, bombBackdropBlendTimer + dt);
            if (bombBackdropBlendTimer >= kBombBackdropBlendSec)
            {
                bombBackdropBlendActive = false;
            }
        }

        if (missGameOverPhase != MissGameOverPhase::None)
        {
            missGameOverTimer = std::max(0.0, missGameOverTimer - dt);
            if (missGameOverPhase == MissGameOverPhase::Delay && missGameOverTimer <= 0.0)
            {
                missGameOverPhase = MissGameOverPhase::Reveal;
                missGameOverTimer = kMissGameOverRevealSec;
                if (!missGameOverSceneRequested)
                {
                    requestSceneTransition(FruitGame::UIScene::GameOver);
                    missGameOverSceneRequested = true;
                }
            }
            else if (missGameOverPhase == MissGameOverPhase::Reveal && missGameOverTimer <= 0.0)
            {
                missGameOverPhase = MissGameOverPhase::Hold;
                missGameOverTimer = kRestartEnableDelaySec;
            }
            knifeSfxPending = false;
        }

        const bool spawnUnlocked = (gameTimeSec >= kGameplaySpawnDelaySec);
        const bool restartReady = (missGameOverPhase == MissGameOverPhase::Hold) && (missGameOverTimer <= 0.0);
        if (bombTransitionPhase == BombTransitionPhase::None && !bombPostGameOverMode)
        {
            if (spawnUnlocked)
            {
                game.setDifficultyTier(score);
                game.update(dt);
            }
            knife.update(dt, hwnd);
            hitEffects.update(dt);
            comboTextEffects.update(dt);
        }
        else if (bombTransitionPhase == BombTransitionPhase::None && restartReady)
        {
            knife.update(dt, hwnd);
        }

        double cursorSpeedForSfx = 0.0;
        if (bombTransitionPhase == BombTransitionPhase::None && missGameOverPhase == MissGameOverPhase::None && knife.hasCursorPoint())
        {
            const POINT cursor = knife.cursorPoint();
            if (hasPrevCursorPointForSfx)
            {
                const double dx = static_cast<double>(cursor.x - prevCursorPointForSfx.x);
                const double dy = static_cast<double>(cursor.y - prevCursorPointForSfx.y);
                cursorSpeedForSfx = std::sqrt(dx * dx + dy * dy) / std::max(1e-4, dt);
            }
            prevCursorPointForSfx = cursor;
            hasPrevCursorPointForSfx = true;
        }
        else
        {
            hasPrevCursorPointForSfx = false;
        }

        POINT bladeFrom{};
        POINT bladeTo{};
        const bool hasActiveBlade = (bombTransitionPhase == BombTransitionPhase::None) &&
            (!bombPostGameOverMode) &&
            (missGameOverPhase == MissGameOverPhase::None) &&
            knife.isSlicing() && knife.hasBladeSegment();
        if (hasActiveBlade)
        {
            knife.bladeSegment(&bladeFrom, &bladeTo);
        }

        knifeSfxCooldownSec = std::max(0.0, knifeSfxCooldownSec - dt);
        if (bombTransitionPhase == BombTransitionPhase::None && !bombPostGameOverMode && missGameOverPhase == MissGameOverPhase::None && !knife.isSlicing())
        {
            knifeSfxCooldownSec = 0.0;
            knifeInHighSpeedBand = false;
        }

        if (bombTransitionPhase == BombTransitionPhase::None && !bombPostGameOverMode && missGameOverPhase == MissGameOverPhase::None && knife.isSlicing() && !knifeWasSlicingLastFrame)
        {
            knifeSfxPlayedInStroke = false;
        }

        if (hasActiveBlade)
        {
            const double speed = cursorSpeedForSfx;
            const double speedAlpha = 1.0 - std::exp(-kKnifeSpeedSmoothHz * dt);
            knifeSpeedFiltered += (speed - knifeSpeedFiltered) * speedAlpha;

            if (!knifeInHighSpeedBand)
            {
                if (knifeSpeedFiltered >= kKnifeMinSpeedPxPerSec)
                {
                    knifeInHighSpeedBand = true;
                    if (knifeSfxCooldownSec <= 0.0)
                    {
                        queueKnifeSfx();
                    }
                }
            }
            else if (knifeSpeedFiltered < kKnifeExitSpeedPxPerSec)
            {
                knifeInHighSpeedBand = false;
            }

            if (knifeInHighSpeedBand && knifeSfxCooldownSec <= 0.0)
            {
                queueKnifeSfx();
            }
        }

        if (bombTransitionPhase == BombTransitionPhase::None && !bombPostGameOverMode && missGameOverPhase == MissGameOverPhase::None && !knife.isSlicing() && knifeWasSlicingLastFrame)
        {
            if (!knifeSfxPlayedInStroke && knifeSpeedFiltered >= kKnifeMinSpeedPxPerSec && knifeSfxCooldownSec <= 0.0)
            {
                queueKnifeSfx();
            }
            knifeInHighSpeedBand = false;
            knifeSpeedFiltered = 0.0;
        }

        if (bombTransitionPhase == BombTransitionPhase::None && !bombPostGameOverMode && missGameOverPhase == MissGameOverPhase::None)
        {
            if (auto resolved = comboJudge.update(gameTimeSec, knife.isSlicing()))
            {
                score += resolved->comboCount;
                comboTextEffects.add(FruitGame::makeComboTextEffect(resolved->impactPoint, resolved->comboCount));
                playComboSfx(resolved->comboCount);
            }

            if (hasActiveBlade)
            {
                std::vector<FruitGame::SliceEvent> sliceEvents;
                const int slicedCount = game.trySliceAlongSegment(bladeFrom, bladeTo, knife.cutAngle(), &sliceEvents);
                if (slicedCount > 0)
                {
                    hadSliceHitThisFrame = true;
                    sliceSfxBurstTimerSec = kSliceSfxBurstWindowSec;
                    sliceSfxBurstHits += slicedCount;
                    score += slicedCount;
                    for (std::size_t i = 0; i < sliceEvents.size(); ++i)
                    {
                        const auto& event = sliceEvents[i];
                        if (auto resolved = comboJudge.registerHit(gameTimeSec, event.impactPoint, knife.isSlicing()))
                        {
                            score += resolved->comboCount;
                            comboTextEffects.add(FruitGame::makeComboTextEffect(resolved->impactPoint, resolved->comboCount));
                            playComboSfx(resolved->comboCount);
                        }

                        constexpr double kHitEffectTrailAnchorBlend = 0.82;
                        const POINT trailAnchor = closestPointOnSegment(event.impactPoint, bladeFrom, bladeTo);
                        POINT effectPoint{};
                        effectPoint.x = static_cast<LONG>(lerpInt(event.impactPoint.x, trailAnchor.x, kHitEffectTrailAnchorBlend));
                        effectPoint.y = static_cast<LONG>(lerpInt(event.impactPoint.y, trailAnchor.y, kHitEffectTrailAnchorBlend));

                        hitEffects.add(FruitGame::makeFlashEffect(flashImage.getwidth() > 0 ? &flashImage : nullptr, effectPoint, event.angle));
                        hitEffects.add(FruitGame::makeJuiceEffect(effectPoint, event.juiceColor));

                        FruitGame::SfxId cutSfxId = FruitGame::SfxId::Splatter2;
                        if (event.cutSfx == FruitGame::CutSfxKind::Banana)
                        {
                            cutSfxId = FruitGame::SfxId::SplatterBanana;
                        }
                        else if (event.cutSfx == FruitGame::CutSfxKind::Splatter)
                        {
                            cutSfxId = FruitGame::SfxId::Splatter;
                        }
                        else
                        {
                            cutSfxId = (cutMixDist(sfxRng) < kSmallFruitHeavySplatterChance)
                                ? FruitGame::SfxId::Splatter
                                : FruitGame::SfxId::Splatter2;
                        }

                        const bool burstMode = (sliceSfxBurstHits >= kSliceSfxBurstStartHits) || (sliceEvents.size() >= 2);
                        const bool firstHitInFrame = (i == 0);
                        const double keepChance = (cutSfxId == FruitGame::SfxId::SplatterBanana)
                            ? std::max(0.58, kSliceSfxKeepChanceBurst)
                            : kSliceSfxKeepChanceBurst;

                        const bool passBurstGate = !burstMode || firstHitInFrame || (sliceBurstKeepDist(sfxRng) < keepChance);
                        const bool passGapGate = firstHitInFrame || (sliceSfxCooldownSec <= 0.0);

                        if (passBurstGate && passGapGate)
                        {
                            music.playSfx(cutSfxId);
                            sliceSfxCooldownSec = burstMode ? kSliceSfxGapBurstSec : kSliceSfxGapNormalSec;
                        }
                    }
                }
            }
        }

        if (knifeSfxPending)
        {
            if (!hadSliceHitThisFrame)
            {
                music.playSfx(pendingKnifeSfx);
                knifeSfxPending = false;
            }
            else
            {
                knifeSfxCooldownSec = std::max(knifeSfxCooldownSec, kKnifeDeferredRetrySec);
                knifeSfxPending = false;
            }
        }

        knifeWasSlicingLastFrame = knife.isSlicing();

        const int missedNow = game.missedCount();
        if (missedNow > previousMissedCount)
        {
            if (missGameOverPhase == MissGameOverPhase::None)
            {
                for (int i = 0; i < missedNow - previousMissedCount; ++i)
                {
                    music.playSfx(FruitGame::SfxId::Error);
                }
            }
            previousMissedCount = missedNow;
        }

        if (bombTransitionPhase == BombTransitionPhase::None &&
            missGameOverPhase == MissGameOverPhase::None &&
            missedNow >= 3)
        {
            missGameOverPhase = MissGameOverPhase::Delay;
            missGameOverTimer = kMissStopThrowDelaySec;
            game.setSpawnEnabled(false);
            knifeSfxPending = false;
            knifeSfxCooldownSec = 0.0;
        }

        if (bombTransitionPhase == BombTransitionPhase::None &&
            bombPostGameOverMode &&
            !bombBackdropBlendActive &&
            missGameOverPhase == MissGameOverPhase::None)
        {
            missGameOverPhase = MissGameOverPhase::Reveal;
            missGameOverTimer = kMissGameOverRevealSec;
            if (!missGameOverSceneRequested)
            {
                requestSceneTransition(FruitGame::UIScene::GameOver);
                missGameOverSceneRequested = true;
            }
        }

        if (restartReady)
        {
            if (restartMenuSliceAction != RestartMenuSliceAction::None)
            {
                restartMenuSliceTimer += dt;
                if (restartMenuSliceTimer >= 0.34)
                {
                    if (restartMenuSliceAction == RestartMenuSliceAction::Restart)
                    {
                        resetGameplayState();
                    }
                    else
                    {
                        running = false;
                    }
                    restartMenuSliceAction = RestartMenuSliceAction::None;
                    restartMenuSliceTimer = 0.0;
                }
            }

            if (!restartMenuVisibleLastFrame)
            {
                restartMenuAnimTimer = 0.0;
                restartMenuSliceAction = RestartMenuSliceAction::None;
                restartMenuSliceTimer = 0.0;
            }
            restartMenuAnimTimer += dt;

            const double sxMenu = static_cast<double>(FruitGame::kWindowWidth) / 640.0;
            const double syMenu = static_cast<double>(FruitGame::kWindowHeight) / 480.0;
            const double entryT = clamp01(restartMenuAnimTimer / 0.50);
            const double entryPop = std::max(1e-4, easeOutBack(entryT));

            const int menuHalfGap = static_cast<int>(std::lround(111.0 * sxMenu));
            const int menuCenterX = FruitGame::kWindowWidth / 2;
            const int menuCenterY = static_cast<int>(std::lround((344.0 + std::sin(gameTimeSec * 2.2) * 3.0) * syMenu));
            const int restartX = menuCenterX - menuHalfGap;
            const int restartY = menuCenterY;
            const int quitX = menuCenterX + menuHalfGap;
            const int quitY = menuCenterY;

            const POINT restartCenter{ restartX, restartY - 10 };
            const POINT quitCenter{ quitX, quitY - 10 };
            const double kSliceHitRadius = std::max(24.0, 34.0 * entryPop);

            if (!knife.isSlicing())
            {
                restartMenuSliceLatched = false;
            }
            else if (!restartMenuSliceLatched && knife.hasBladeSegment())
            {
                POINT menuBladeFrom{};
                POINT menuBladeTo{};
                knife.bladeSegment(&menuBladeFrom, &menuBladeTo);

                const double restartD2 = distanceSqPointToSegment(restartCenter, menuBladeFrom, menuBladeTo);
                const double quitD2 = distanceSqPointToSegment(quitCenter, menuBladeFrom, menuBladeTo);
                const double hitR2 = kSliceHitRadius * kSliceHitRadius;
                const double bladeDx = static_cast<double>(menuBladeTo.x - menuBladeFrom.x);
                const double bladeDy = static_cast<double>(menuBladeTo.y - menuBladeFrom.y);
                const double bladeAngle = std::atan2(bladeDy, bladeDx);

                if (restartD2 <= hitR2)
                {
                    restartMenuSliceLatched = true;
                    restartMenuSliceAction = RestartMenuSliceAction::Restart;
                    restartMenuSliceTimer = 0.0;
                    restartMenuSliceAngle = bladeAngle;
                }
                else if (quitD2 <= hitR2)
                {
                    restartMenuSliceLatched = true;
                    restartMenuSliceAction = RestartMenuSliceAction::Quit;
                    restartMenuSliceTimer = 0.0;
                }
            }
        }
        else
        {
            restartMenuAnimTimer = 0.0;
            restartMenuSliceAction = RestartMenuSliceAction::None;
            restartMenuSliceTimer = 0.0;
        }
        restartMenuVisibleLastFrame = restartReady;

        const bool restartClickDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        prevRestartClickDown = restartClickDown;

        cleardevice();

        const bool bombInTransition = bombTransitionPhase != BombTransitionPhase::None;
        if (bombInTransition)
        {
            const double shakePhase = std::max(0.0, kBombShakeDurationSec - bombShakeTimer);
            const double shakeStrength = 7.5 * clamp01(bombShakeTimer / kBombShakeDurationSec);
            bombShakeOrigin.x = static_cast<LONG>(std::lround(std::sin(shakePhase * 62.0) * shakeStrength));
            bombShakeOrigin.y = static_cast<LONG>(std::lround(std::cos(shakePhase * 69.0) * shakeStrength));
            setorigin(bombShakeOrigin.x, bombShakeOrigin.y);

            game.render();
            knife.render();
            hitEffects.render();
            comboTextEffects.render();
            setorigin(0, 0);

            const int rayCount = 12;
            const double rayStepSec = 0.10;
            const int rayMaxLength = 1080;
            const COLORREF rayCoreColor = RGB(255, 255, 255);
            const COLORREF rayGlowColor = RGB(255, 200, 160);

            auto drawBombSprite = [&]()
                {
                    setorigin(bombShakeOrigin.x, bombShakeOrigin.y);
                    if (bombSpriteSetView.whole != nullptr && bombSpriteSetView.whole->getwidth() > 0)
                    {
                        const int bombW = bombSpriteSetView.whole->getwidth();
                        const int bombH = bombSpriteSetView.whole->getheight();
                        drawImageAlphaScaled(bombSpriteSetView.whole, bombImpactPoint.x - bombW / 2, bombImpactPoint.y - bombH / 2, 1.0, 1.0);
                    }
                    setorigin(0, 0);
                };

            if (bombTransitionPhase == BombTransitionPhase::Pause)
            {
                drawBombSprite();

                if (bombTransitionTimer <= 0.0)
                {
                    bombTransitionPhase = BombTransitionPhase::Rays;
                    bombTransitionTimer = rayCount * rayStepSec;
                    bombRayIndex = 0;
                    bombRayEmitTimer = 0.0;
                }
            }
            else if (bombTransitionPhase == BombTransitionPhase::Rays)
            {
                bombRayEmitTimer += dt;
                while (bombRayEmitTimer >= rayStepSec && bombRayIndex < rayCount)
                {
                    bombRayEmitTimer -= rayStepSec;
                    ++bombRayIndex;
                }

                const double rayClockwiseStep = 6.28318530717958647692 / static_cast<double>(rayCount);
                const double baseAngle = -1.35;
                for (int i = 0; i < bombRayIndex; ++i)
                {
                    const double ang = baseAngle - rayClockwiseStep * static_cast<double>(i);
                    drawExplosionRay(bombImpactPoint, ang, rayMaxLength, rayCoreColor, rayGlowColor);
                }

                drawBombSprite();

                if (bombRayIndex >= rayCount && bombTransitionTimer <= 0.0)
                {
                    bombTransitionPhase = BombTransitionPhase::RaysHold;
                    bombTransitionTimer = 1.00;
                }
            }
            else if (bombTransitionPhase == BombTransitionPhase::RaysHold)
            {
                const double rayClockwiseStep = 6.28318530717958647692 / static_cast<double>(rayCount);
                const double baseAngle = -1.35;
                for (int i = 0; i < rayCount; ++i)
                {
                    const double ang = baseAngle - rayClockwiseStep * static_cast<double>(i);
                    drawExplosionRay(bombImpactPoint, ang, rayMaxLength, rayCoreColor, rayGlowColor);
                }

                drawBombSprite();

                if (bombTransitionTimer <= 0.0)
                {
                    bombTransitionPhase = BombTransitionPhase::Whiteout;
                    bombTransitionTimer = 0.75;
                }
            }

            if (bombTransitionPhase == BombTransitionPhase::Whiteout)
            {
                const double rayClockwiseStep = 6.28318530717958647692 / static_cast<double>(rayCount);
                const double baseAngle = -1.35;
                for (int i = 0; i < rayCount; ++i)
                {
                    const double ang = baseAngle - rayClockwiseStep * static_cast<double>(i);
                    drawExplosionRay(bombImpactPoint, ang, rayMaxLength, rayCoreColor, rayGlowColor);
                }

                drawBombSprite();

                const double t = clamp01(1.0 - bombTransitionTimer / 0.75);
                blendScreenToWhite(0.42 + 0.58 * t);

                if (bombTransitionTimer <= 0.0)
                {
                    bombTransitionPhase = BombTransitionPhase::WhiteHold;
                    bombTransitionTimer = kBombWhiteHoldSec;
                }
            }
            else if (bombTransitionPhase == BombTransitionPhase::WhiteHold)
            {
                blendScreenToWhite(1.0);
                if (bombTransitionTimer <= 0.0)
                {
                    bombTransitionPhase = BombTransitionPhase::None;
                    bombPostGameOverMode = true;
                    bombBackdropBlendActive = true;
                    bombBackdropBlendTimer = 0.0;
                    game.setSpawnEnabled(false);
                    game.fruits().clear();
                    previousMissedCount = game.missedCount();
                }
            }

            drawScoreHud(scoreImage.getwidth() > 0 ? &scoreImage : nullptr, score, hudIntroProgress);
            drawMissCounterHud(missSlots, game.missedCount(), dt, hudIntroProgress);
        }
        else
        {
            if (!bombPostGameOverMode)
            {
                game.render();
                knife.render();
                hitEffects.render();
                comboTextEffects.render();
            }
            else
            {
                if (backgroundImage.getwidth() > 0 && backgroundImage.getheight() > 0)
                {
                    putimage(0, 0, &backgroundImage);
                }
                else
                {
                    setfillcolor(RGB(78, 171, 238));
                    solidrectangle(0, 0, FruitGame::kWindowWidth, FruitGame::kWindowHeight);
                    setfillcolor(RGB(44, 160, 77));
                    solidrectangle(0, FruitGame::kWindowHeight - 76, FruitGame::kWindowWidth, FruitGame::kWindowHeight);
                }

                if (restartReady)
                {
                    knife.render();
                }
            }

            drawScoreHud(scoreImage.getwidth() > 0 ? &scoreImage : nullptr, score, hudIntroProgress);
            drawMissCounterHud(missSlots, game.missedCount(), dt, hudIntroProgress);
        }

        if (!bombInTransition && bombPostGameOverMode && bombBackdropBlendActive)
        {
            const double t = clamp01(bombBackdropBlendTimer / kBombBackdropBlendSec);
            blendScreenToWhite(1.0 - t);
        }

        if (!bombInTransition && (missGameOverPhase == MissGameOverPhase::Reveal || missGameOverPhase == MissGameOverPhase::Hold) &&
            gameOverImage.getwidth() > 0 && gameOverImage.getheight() > 0)
        {
            const double t = (missGameOverPhase == MissGameOverPhase::Reveal)
                ? clamp01(1.0 - missGameOverTimer / kMissGameOverRevealSec)
                : 1.0;
            const double pop = std::max(1e-5, easeOutExpo(t));

            const double sxGameOver = static_cast<double>(FruitGame::kWindowWidth) / 640.0;
            const double syGameOver = static_cast<double>(FruitGame::kWindowHeight) / 480.0;
            const double targetW = 490.0 * sxGameOver;
            const double targetH = 85.0 * syGameOver;
            const double baseScale = std::min(targetW / static_cast<double>(gameOverImage.getwidth()),
                targetH / static_cast<double>(gameOverImage.getheight()));
            const double drawScale = std::max(1e-5, baseScale * pop);

            const int centerX = static_cast<int>(std::lround((75.0 + 245.0) * sxGameOver));
            const int centerY = static_cast<int>(std::lround((198.0 + 42.5) * syGameOver));
            const int drawW = std::max(1, static_cast<int>(std::lround(gameOverImage.getwidth() * drawScale)));
            const int drawH = std::max(1, static_cast<int>(std::lround(gameOverImage.getheight() * drawScale)));
            const int drawX = centerX - drawW / 2;
            const int drawY = centerY - drawH / 2;

            drawImageAlphaScaled(&gameOverImage, drawX, drawY, drawScale, 0.65 + 0.35 * t);
        }

        if (restartReady)
        {
            const double sxMenu = static_cast<double>(FruitGame::kWindowWidth) / 640.0;
            const double syMenu = static_cast<double>(FruitGame::kWindowHeight) / 480.0;
            const int menuHalfGap = static_cast<int>(std::lround(111.0 * sxMenu));
            const int menuCenterX = FruitGame::kWindowWidth / 2;
            const int menuCenterY = static_cast<int>(std::lround((344.0 + std::sin(gameTimeSec * 2.2) * 3.0) * syMenu));
            const int restartX = menuCenterX - menuHalfGap;
            const int restartY = menuCenterY;
            const int quitX = menuCenterX + menuHalfGap;
            const int quitY = menuCenterY;

            const double entryT = clamp01(restartMenuAnimTimer / 0.50);
            const double entryPop = std::max(1e-4, easeOutBack(entryT));
            const double ringSpin = gameTimeSec * (12.0 * 3.14159265358979323846 / 180.0);
            const double sliceT = clamp01(restartMenuSliceTimer / 0.34);
            const double sliceSpread = 20.0 + 46.0 * easeOutExpo(sliceT);
            const double sliceFall = 4.0 + 64.0 * sliceT * sliceT;
            const double restartScale = 0.84 * entryPop;
            const double restartFruitScale = 0.60 * entryPop;
            const double quitScale = 0.84 * entryPop;
            const double quitBombScale = 0.52 * entryPop;

            if (newGameImage.getwidth() > 0)
            {
                drawImageAlphaRotated(&newGameImage, restartX, restartY, ringSpin, restartScale, 0.98);
            }
            if (quitImage.getwidth() > 0)
            {
                drawImageAlphaRotated(&quitImage, quitX, quitY, -ringSpin, quitScale, 0.98);
            }

            if (restartFruitIcon != nullptr && restartFruitIcon->getwidth() > 0)
            {
                if (restartMenuSliceAction == RestartMenuSliceAction::None || restartFruitSetView == nullptr || restartFruitSetView->half1 == nullptr || restartFruitSetView->half2 == nullptr)
                {
                    drawImageAlphaRotated(restartFruitIcon, restartX, restartY - 10, ringSpin * 1.35, restartFruitScale, 0.98);
                }
                else
                {
                    const double cutPerp = restartMenuSliceAngle + 1.57079632679;
                    const int half1X = restartX + static_cast<int>(std::lround(std::cos(cutPerp) * sliceSpread));
                    const int half1Y = restartY + static_cast<int>(std::lround(std::sin(cutPerp) * sliceSpread + sliceFall));
                    const int half2X = restartX - static_cast<int>(std::lround(std::cos(cutPerp) * sliceSpread));
                    const int half2Y = restartY - static_cast<int>(std::lround(std::sin(cutPerp) * sliceSpread)) + static_cast<int>(std::lround(sliceFall));
                    const double half1Angle = restartMenuSliceAngle + 0.58 + 1.0 * sliceT;
                    const double half2Angle = restartMenuSliceAngle - 0.58 - 1.0 * sliceT;
                    const double halfScale = 0.70 * entryPop;

                    drawImageAlphaRotated(restartFruitSetView->half1, half1X, half1Y, half1Angle, halfScale, 0.98);
                    drawImageAlphaRotated(restartFruitSetView->half2, half2X, half2Y, half2Angle, halfScale, 0.98);
                }
            }
            if (bombSpriteSetView.whole != nullptr && bombSpriteSetView.whole->getwidth() > 0)
            {
                drawImageAlphaRotated(bombSpriteSetView.whole, quitX, quitY - 10, -ringSpin * 1.2, quitBombScale, 0.98);
            }
        }

        FlushBatchDraw();

        Sleep(16);
    }

    EndBatchDraw();
    music.stopAll();
    closegraph();
    return 0;
}