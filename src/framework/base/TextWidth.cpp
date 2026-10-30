#include "framework/base/TextWidth.h"

namespace {

    struct Range {
        unsigned int lo;
        unsigned int hi;
    };

    template <auto N>
    constexpr bool InRanges(unsigned int cp, const Range(&ranges)[N]) noexcept {
        for (decltype(N) i = 0; i < N; ++i) {
            if (cp >= ranges[i].lo && cp <= ranges[i].hi) {
                return true;
            }
        }
        return false;
    }

    constexpr Range kWideRanges[] = {
        {0x1100,  0x115F},
        {0x2329,  0x232A},
        {0x2E80,  0x303E},
        {0x3041,  0x33FF},
        {0x3400,  0x4DBF},
        {0x4E00,  0x9FFF},
        {0xA000,  0xA4CF},
        {0xA960,  0xA97F},
        {0xAC00,  0xD7A3},
        {0xF900,  0xFAFF},
        {0xFE10,  0xFE19},
        {0xFE30,  0xFE6F},
        {0xFF00,  0xFF60},
        {0xFFE0,  0xFFE6},
        {0x16FE0, 0x16FE4},
        {0x17000, 0x187F7},
        {0x18800, 0x18CD5},
        {0x1B000, 0x1B2FF},
        {0x1F200, 0x1F2FF},
        {0x1F300, 0x1F64F},
        {0x1F900, 0x1F9FF},
        {0x1FA00, 0x1FAFF},
        {0x20000, 0x2FFFD},
        {0x30000, 0x3FFFD},
    };

    constexpr Range kZeroRanges[] = {
        {0x0300,  0x036F},
        {0x0483,  0x0489},
        {0x0591,  0x05BD},
        {0x05BF,  0x05BF},
        {0x05C1,  0x05C2},
        {0x05C4,  0x05C5},
        {0x05C7,  0x05C7},
        {0x0610,  0x061A},
        {0x064B,  0x065F},
        {0x0670,  0x0670},
        {0x06D6,  0x06DC},
        {0x06DF,  0x06E4},
        {0x06E7,  0x06E8},
        {0x06EA,  0x06ED},
        {0x0711,  0x0711},
        {0x0730,  0x074A},
        {0x07A6,  0x07B0},
        {0x07EB,  0x07F3},
        {0x0816,  0x0819},
        {0x081B,  0x0823},
        {0x0825,  0x0827},
        {0x0829,  0x082D},
        {0x0859,  0x085B},
        {0x08E3,  0x0903},
        {0x093A,  0x093A},
        {0x093C,  0x093C},
        {0x0941,  0x0948},
        {0x094D,  0x094D},
        {0x0951,  0x0957},
        {0x0962,  0x0963},
        {0x0981,  0x0981},
        {0x09BC,  0x09BC},
        {0x09C1,  0x09C4},
        {0x09CD,  0x09CD},
        {0x09E2,  0x09E3},
        {0x0A01,  0x0A02},
        {0x0A3C,  0x0A3C},
        {0x0A41,  0x0A42},
        {0x0A47,  0x0A48},
        {0x0A4B,  0x0A4D},
        {0x0A70,  0x0A71},
        {0x0B01,  0x0B01},
        {0x0B3C,  0x0B3C},
        {0x0B3F,  0x0B3F},
        {0x0B41,  0x0B44},
        {0x0B4D,  0x0B4D},
        {0x0C3E,  0x0C40},
        {0x0C46,  0x0C48},
        {0x0C4A,  0x0C4D},
        {0x0D41,  0x0D44},
        {0x0D4D,  0x0D4D},
        {0x0E31,  0x0E31},
        {0x0E34,  0x0E3A},
        {0x0E47,  0x0E4E},
        {0x0EB1,  0x0EB1},
        {0x0EB4,  0x0EBC},
        {0x0EC8,  0x0ECD},
        {0x0F18,  0x0F19},
        {0x0F35,  0x0F35},
        {0x0F37,  0x0F37},
        {0x0F39,  0x0F39},
        {0x0F71,  0x0F7E},
        {0x0F80,  0x0F84},
        {0x0F86,  0x0F87},
        {0x0F8D,  0x0F97},
        {0x0F99,  0x0FBC},
        {0x0FC6,  0x0FC6},
        {0x135D,  0x135F},
        {0x1AB0,  0x1AFF},
        {0x1DC0,  0x1DFF},
        {0x20D0,  0x20FF},
        {0x2CEF,  0x2CF1},
        {0x2D7F,  0x2D7F},
        {0x302A,  0x302F},
        {0x3099,  0x309A},
        {0xA66F,  0xA672},
        {0xA674,  0xA67D},
        {0xA69E,  0xA69F},
        {0xA6F0,  0xA6F1},
        {0xA806,  0xA806},
        {0xA8C4,  0xA8C5},
        {0xA8E0,  0xA8F1},
        {0xA926,  0xA92D},
        {0xA947,  0xA951},
        {0xA980,  0xA982},
        {0xA9B3,  0xA9B3},
        {0xA9B6,  0xA9B9},
        {0xA9BC,  0xA9BC},
        {0xA9E5,  0xA9E5},
        {0xAA29,  0xAA2E},
        {0xAA31,  0xAA32},
        {0xAA35,  0xAA36},
        {0xAA43,  0xAA43},
        {0xAA4C,  0xAA4C},
        {0xAA7C,  0xAA7C},
        {0xAAB0,  0xAAB0},
        {0xAAB2,  0xAAB4},
        {0xAAB7,  0xAAB8},
        {0xAABE,  0xAABF},
        {0xAAC1,  0xAAC1},
        {0xAAEC,  0xAAED},
        {0xAAF6,  0xAAF6},
        {0xABE5,  0xABE5},
        {0xABE8,  0xABE8},
        {0xABED,  0xABED},
        {0xFB1E,  0xFB1E},
        {0xFE00,  0xFE0F},
        {0xFE20,  0xFE2F},
        {0x101FD, 0x101FD},
        {0x102E0, 0x102E0},
        {0x10376, 0x1037A},
        {0x10A01, 0x10A03},
        {0x10A05, 0x10A06},
        {0x10A0C, 0x10A0F},
        {0x10A38, 0x10A3A},
        {0x10A3F, 0x10A3F},
        {0x11001, 0x11001},
        {0x11038, 0x11046},
        {0x1107F, 0x11081},
        {0x110B3, 0x110B6},
        {0x110B9, 0x110BA},
        {0x11100, 0x11102},
        {0x11127, 0x1112B},
        {0x1112D, 0x11134},
        {0x11173, 0x11173},
        {0x11180, 0x11181},
        {0x111B6, 0x111BE},
        {0x1122F, 0x11231},
        {0x11234, 0x11234},
        {0x11236, 0x11237},
        {0x112DF, 0x112DF},
        {0x112E3, 0x112EA},
        {0x11300, 0x11301},
        {0x1133C, 0x1133C},
        {0x11340, 0x11340},
        {0x11366, 0x1136C},
        {0x11370, 0x11374},
        {0x114B3, 0x114B8},
        {0x114BA, 0x114BA},
        {0x114BF, 0x114C0},
        {0x114C2, 0x114C3},
        {0x115B2, 0x115B5},
        {0x115BC, 0x115BD},
        {0x115BF, 0x115C0},
        {0x11633, 0x1163A},
        {0x1163D, 0x1163D},
        {0x1163F, 0x11640},
        {0x116AB, 0x116AB},
        {0x116AD, 0x116AD},
        {0x116B0, 0x116B5},
        {0x116B7, 0x116B7},
        {0x1171D, 0x1171F},
        {0x11722, 0x11725},
        {0x11727, 0x1172B},
        {0x16AF0, 0x16AF4},
        {0x16B30, 0x16B36},
        {0x1BC9D, 0x1BC9E},
        {0x1D167, 0x1D169},
        {0x1D17B, 0x1D182},
        {0x1D185, 0x1D18B},
        {0x1D1AA, 0x1D1AD},
        {0x1D242, 0x1D244},
        {0x1DA00, 0x1DA36},
        {0x1DA3B, 0x1DA6C},
        {0x1DA75, 0x1DA75},
        {0x1DA84, 0x1DA84},
        {0x1DA9B, 0x1DA9F},
        {0x1DAA1, 0x1DAAF},
        {0x1E8D0, 0x1E8D6},
        {0xE0100, 0xE01EF},
        {0x200B,  0x200F},
        {0x202A,  0x202E},
        {0x2060,  0x2064},
        {0xFEFF,  0xFEFF},
    };

    constexpr int CharWidth(unsigned int cp) noexcept {
        if (cp < 0x20u || cp == 0x7Fu) return 0;
        if (cp < 0x80u) return 1;
        if (InRanges(cp, kZeroRanges)) return 0;
        if (InRanges(cp, kWideRanges)) return 2;
        return 1;
    }

    constexpr bool IsCont(unsigned char c) noexcept {
        return (c & 0xC0u) == 0x80u;
    }

    int Utf8Decode(const char*& p, unsigned int& cp) noexcept {
        const auto* s = reinterpret_cast<const unsigned char*>(p);
        const unsigned char c0 = s[0];

        if (c0 < 0x80u) {
            cp = c0;
            p += 1;
            return 1;
        }
        if ((c0 & 0xE0u) == 0xC0u) {
            const unsigned char c1 = s[1];
            if (!IsCont(c1)) return 0;
            const unsigned int v = ((c0 & 0x1Fu) << 6) | (c1 & 0x3Fu);
            if (v < 0x80u) return 0;
            cp = v;
            p += 2;
            return 2;
        }
        if ((c0 & 0xF0u) == 0xE0u) {
            const unsigned char c1 = s[1];
            if (!IsCont(c1)) return 0;
            const unsigned char c2 = s[2];
            if (!IsCont(c2)) return 0;
            const unsigned int v = ((c0 & 0x0Fu) << 12) |
                ((c1 & 0x3Fu) << 6) |
                (c2 & 0x3Fu);
            if (v < 0x800u) return 0;
            if (v >= 0xD800u && v <= 0xDFFFu) return 0;
            cp = v;
            p += 3;
            return 3;
        }
        if ((c0 & 0xF8u) == 0xF0u) {
            const unsigned char c1 = s[1];
            if (!IsCont(c1)) return 0;
            const unsigned char c2 = s[2];
            if (!IsCont(c2)) return 0;
            const unsigned char c3 = s[3];
            if (!IsCont(c3)) return 0;
            const unsigned int v = ((c0 & 0x07u) << 18) |
                ((c1 & 0x3Fu) << 12) |
                ((c2 & 0x3Fu) << 6) |
                (c3 & 0x3Fu);
            if (v < 0x10000u || v > 0x10FFFFu) return 0;
            cp = v;
            p += 4;
            return 4;
        }
        return 0;
    }

    constexpr char ToLowerAscii(char c) noexcept {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
    }

    constexpr bool StrEqNoCase(const char* a, const char* b) noexcept {
        while (*a && *b) {
            if (ToLowerAscii(*a) != ToLowerAscii(*b)) return false;
            ++a;
            ++b;
        }
        return *a == *b;
    }

    constexpr bool IsUtf8Mode(const char* mode) noexcept {
        if (mode == nullptr) return true;
        return StrEqNoCase(mode, "utf-8") || StrEqNoCase(mode, "utf8");
    }

}

int CalcTextWidth(const char* text, const char* mode) {
    if (text == nullptr) return 0;

    int width = 0;

    if (IsUtf8Mode(mode)) {
        const char* p = text;
        while (*p) {
            unsigned int cp = 0;
            const char* before = p;
            if (Utf8Decode(p, cp) > 0) {
                width += CharWidth(cp);
            }
            else {
                p = before + 1;
                width += 1;
            }
        }
    }
    else {
        for (const char* p = text; *p; ++p) {
            const auto c = static_cast<unsigned char>(*p);
            if (c >= 0x20u && c != 0x7Fu) {
                width += 1;
            }
        }
    }

    return width;
}
