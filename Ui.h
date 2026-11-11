// Ui.h - Terminal look & feel:
//   LARPOS_ASCII=1    force plain ASCII drawing        LARPOS_UNICODE=1  force Unicode drawing
//   LARPOS_COLOR=1    force colors (even when piped)   NO_COLOR=1        no colors
//   LARPOS_FAST=1     no animation

#ifndef UI_H
#define UI_H

#include <sys/ioctl.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace ui {

enum Tone { GOOD, WARN, BAD, INFO, DIM, ACCENT, TEXT };

struct Theme { bool color = false, color256 = false, unicode = false, animate = false; int width = 72; };
inline Theme& theme() { static Theme t; return t; }

inline bool utf8Locale() {
    const char* names[] = {"LC_ALL", "LC_CTYPE", "LANG"};
    for (int i = 0; i < 3; i++) {
        const char* v = getenv(names[i]);
        if (v && *v) {
            std::string s = v;
            for (size_t k = 0; k < s.size(); k++) s[k] = (char)tolower((unsigned char)s[k]);
            return s.find("utf-8") != std::string::npos || s.find("utf8") != std::string::npos;
        }
    }
    return false;
}

inline void init() {
    Theme& t = theme();
    bool tty = isatty(STDOUT_FILENO) != 0;
    const char* term = getenv("TERM");
    bool dumb = term && strcmp(term, "dumb") == 0;
    t.color = getenv("LARPOS_COLOR") ? true : (tty && !dumb && !getenv("NO_COLOR"));
    t.color256 = t.color && ((term && strstr(term, "256")) || getenv("COLORTERM") || getenv("LARPOS_COLOR"));
    t.unicode = getenv("LARPOS_UNICODE") ? true : (!getenv("LARPOS_ASCII") && utf8Locale());
    t.animate = tty && !getenv("LARPOS_FAST");
    struct winsize ws;
    if (tty && ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        t.width = std::max(60, std::min(100, (int)ws.ws_col - 2));
}

// ------------------------------------------------------------ glyphs
struct Glyphs {
    std::string tl, tr, bl, br, h, v;                 // panel frame
    std::string full, light;                          // gauge cells
    std::string other, clean, susp, bad;              // disk map
    std::string arrow, bullet, tri, triOff;           // markers
    std::string ltee, lend, vline, blank;             // tree
};
inline const Glyphs& G() {
    static const Glyphs U = {"╭", "╮", "╰", "╯", "─", "│", "█", "░", "·", "■", "▲", "✖",
                             "▸", "•", "▲", "△", "├─ ", "└─ ", "│  ", "   "};
    static const Glyphs A = {"+", "+", "+", "+", "-", "|", "#", ".", ".", "o", "!", "X",
                             ">", "*", "^", "-", "|- ", "`- ", "|  ", "   "};
    return theme().unicode ? U : A;
}

// ------------------------------------------------------------ colors
inline std::string sgr(Tone t, bool bold = false) {
    if (!theme().color) return "";
    std::string b = bold ? "1;" : "";
    if (theme().color256) {
        static const int c[] = {46, 214, 196, 45, 244, 213, 255};   // GOOD WARN BAD INFO DIM ACCENT TEXT
        return "\033[" + b + "38;5;" + std::to_string(c[t]) + "m";
    }
    static const char* c8[] = {"32", "33", "31", "36", "2", "35", "37"};
    return "\033[" + b + c8[t] + "m";
}
inline std::string reset() { return theme().color ? "\033[0m" : ""; }
inline std::string paint(Tone t, const std::string& s, bool bold = false) {
    return theme().color ? sgr(t, bold) + s + "\033[0m" : s;
}
// green -> yellow -> red ramp, t in 0..1
inline std::string heatSgr(double t) {
    if (!theme().color) return "";
    if (theme().color256) {
        static const int ramp[] = {46, 82, 118, 154, 190, 226, 220, 214, 208, 202, 196};
        int i = std::max(0, std::min(10, (int)(t * 10 + 0.5)));
        return "\033[38;5;" + std::to_string(ramp[i]) + "m";
    }
    return t < 0.4 ? "\033[32m" : (t < 0.7 ? "\033[33m" : "\033[31m");
}

// ------------------------------------------------------------ text measuring (ignores color codes, counts UTF-8 as 1)
inline size_t visLen(const std::string& s) {
    size_t n = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0x1b) { while (i < s.size() && s[i] != 'm') i++; continue; }
        if ((c & 0xC0) != 0x80) n++;
    }
    return n;
}
inline std::string repeat(const std::string& s, int n) {
    std::string o;
    for (int i = 0; i < n; i++) o += s;
    return o;
}
inline std::string pad(const std::string& s, int w) { int n = (int)visLen(s); return n >= w ? s : s + std::string(w - n, ' '); }
inline std::string center(const std::string& s, int w) {
    int n = (int)visLen(s);
    if (n >= w) return s;
    int l = (w - n) / 2;
    return std::string(l, ' ') + s + std::string(w - n - l, ' ');
}
inline std::string clip(const std::string& s, size_t n) {
    if (visLen(s) <= n) return s;
    std::string o;
    size_t cnt = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0x1b) { while (i < s.size() && s[i] != 'm') o += s[i++]; if (i < s.size()) o += s[i++]; continue; }
        size_t len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
        if (cnt + 1 >= n) break;
        o += s.substr(i, len);
        i += len;
        cnt++;
    }
    return o + (theme().unicode ? "…" : "~") + reset();
}
inline std::vector<std::string> wrap(const std::string& text, int width, const std::string& indent = "") {
    std::vector<std::string> out;
    std::string line, w;
    std::istringstream in(text);
    while (in >> w) {
        if (line.empty()) line = (out.empty() ? "" : indent) + w;
        else if ((int)(visLen(line) + 1 + visLen(w)) <= width) line += " " + w;
        else { out.push_back(line); line = indent + w; }
    }
    if (!line.empty()) out.push_back(line);
    return out;
}
inline std::string commas(unsigned long long n) {
    std::string s = std::to_string(n), o;
    for (size_t i = 0; i < s.size(); i++) {
        if (i && (s.size() - i) % 3 == 0) o += ',';
        o += s[i];
    }
    return o;
}

// ------------------------------------------------------------ animation helpers
inline void tick(int ms) { if (theme().animate) usleep((useconds_t)ms * 1000); }
inline const char* spinFrame(size_t i) {
    static const char* U[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    static const char* A[] = {"|", "/", "-", "\\"};
    return theme().unicode ? U[i % 10] : A[i % 4];
}
inline void live(const std::string& s) { if (theme().animate) std::cout << "\r" << s << "\033[K" << std::flush; }
inline void liveEnd() { if (theme().animate) std::cout << "\r\033[K" << std::flush; }

// ------------------------------------------------------------ building blocks
// coloured bar: with gradient=true the colour runs green->red along the bar
inline std::string gauge(double v, double mx, int width, bool gradient = true, Tone solid = GOOD) {
    const Glyphs& g = G();
    int filled = mx > 0 ? (int)(v / mx * width + 0.5) : 0;
    filled = std::max(0, std::min(width, filled));
    std::string o;
    for (int i = 0; i < width; i++) {
        if (i < filled) o += (gradient ? heatSgr(width > 1 ? (double)i / (width - 1) : 0) : sgr(solid)) + g.full;
        else            o += sgr(DIM) + g.light;
    }
    return o + reset();
}

inline std::string spark(const std::vector<double>& v, Tone t = INFO) {
    static const char* U[] = {"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
    static const char* A = "._-=+*#@";
    double mx = 0;
    for (size_t i = 0; i < v.size(); i++) mx = std::max(mx, v[i]);
    std::string o;
    for (size_t i = 0; i < v.size(); i++) {
        int l = mx > 0 ? (int)(v[i] / mx * 7 + 0.5) : 0;
        l = std::max(0, std::min(7, l));
        o += theme().unicode ? std::string(U[l]) : std::string(1, A[l]);
    }
    return paint(t, o);
}

// a row of 14-wide stat tiles (4 lines tall)
inline std::vector<std::string> tiles(const std::vector<std::pair<std::string, std::string>>& items,
                                      const std::vector<Tone>& tones) {
    const int W = 14;
    const Glyphs& g = G();
    std::vector<std::string> L(4);
    for (size_t i = 0; i < items.size(); i++) {
        Tone t = i < tones.size() ? tones[i] : INFO;
        std::string gap = i ? "  " : "";
        L[0] += gap + paint(t, g.tl + repeat(g.h, W - 2) + g.tr);
        L[1] += gap + paint(t, g.v) + center(paint(t, items[i].first, true), W - 2) + paint(t, g.v);
        L[2] += gap + paint(t, g.v) + center(paint(DIM, items[i].second), W - 2) + paint(t, g.v);
        L[3] += gap + paint(t, g.bl + repeat(g.h, W - 2) + g.br);
    }
    return L;
}

// framed panel
inline void panel(const std::string& title, const std::vector<std::string>& lines, Tone tone = INFO) {
    const Glyphs& g = G();
    int inner = theme().width - 2;
    int fill = std::max(1, inner - 3 - (int)visLen(title));
    std::cout << paint(tone, g.tl + g.h + " ") << paint(tone, title, true)
              << paint(tone, " " + repeat(g.h, fill) + g.tr) << "\n";
    tick(5);
    for (size_t i = 0; i < lines.size(); i++) {
        std::cout << paint(tone, g.v) << " " << pad(clip(lines[i], inner - 2), inner - 2) << reset() << " "
                  << paint(tone, g.v) << "\n";
        tick(5);
    }
    std::cout << paint(tone, g.bl + repeat(g.h, inner) + g.br) << "\n";
}

// section divider:  ▌ TITLE ▐━━━━━━━━━━━━
inline void rule(const std::string& title, Tone tone = ACCENT) {
    const Glyphs& g = G();
    int fill = std::max(2, theme().width - (int)visLen(title) - 6);
    std::cout << "\n" << paint(tone, theme().unicode ? "▌ " : "[ ") << paint(tone, title, true)
              << paint(tone, theme().unicode ? " ▐" : " ]") << paint(DIM, repeat(g.h, fill)) << "\n";
}

// one-line status message:  ▸ text
inline void note(Tone tone, const std::string& text) {
    std::cout << "  " << paint(tone, G().arrow, true) << " " << text << "\n";
}

// ------------------------------------------------------------ big lettering (5 rows)
inline std::vector<std::string> bigText(const std::string& word) {
    static const std::map<char, std::vector<std::string>> F = {
        {'A', {" ### ", "#   #", "#####", "#   #", "#   #"}}, {'C', {" ####", "#    ", "#    ", "#    ", " ####"}},
        {'D', {"#### ", "#   #", "#   #", "#   #", "#### "}}, {'E', {"#####", "#    ", "#### ", "#    ", "#####"}},
        {'G', {" ####", "#    ", "# ###", "#   #", " ### "}}, {'I', {"#####", "  #  ", "  #  ", "  #  ", "#####"}},
        {'L', {"#    ", "#    ", "#    ", "#    ", "#####"}}, {'N', {"#   #", "##  #", "# # #", "#  ##", "#   #"}},
        {'O', {" ### ", "#   #", "#   #", "#   #", " ### "}}, {'R', {"#### ", "#   #", "#### ", "#  # ", "#   #"}},
        {'S', {" ####", "#    ", " ### ", "    #", "#### "}}, {'T', {"#####", "  #  ", "  #  ", "  #  ", "  #  "}},
        {'U', {"#   #", "#   #", "#   #", "#   #", " ### "}}, {' ', {"  ", "  ", "  ", "  ", "  "}}};
    std::vector<std::string> rows(5);
    for (size_t i = 0; i < word.size(); i++) {
        std::map<char, std::vector<std::string>>::const_iterator it = F.find(word[i]);
        if (it == F.end()) continue;
        for (int r = 0; r < 5; r++) rows[r] += it->second[r] + " ";
    }
    for (int r = 0; r < 5; r++) {
        std::string o;
        for (size_t k = 0; k < rows[r].size(); k++) o += rows[r][k] == '#' ? G().full : std::string(1, rows[r][k]);
        rows[r] = o;
    }
    return rows;
}

// ------------------------------------------------------------ folder tree
struct TreeNode { std::map<std::string, TreeNode> kids; int score = -1; };

inline void treeAdd(TreeNode& root, const std::string& rel, int score) {
    TreeNode* n = &root;
    std::istringstream in(rel);
    std::string part;
    while (std::getline(in, part, '/')) {
        if (part.empty()) continue;
        n = &n->kids[part];
    }
    n->score = score;
}

inline void treeLines(const TreeNode& n, const std::string& prefix, std::vector<std::string>& out,
                      const std::function<std::string(int)>& tag) {
    const Glyphs& g = G();
    size_t i = 0;
    for (std::map<std::string, TreeNode>::const_iterator it = n.kids.begin(); it != n.kids.end(); ++it, ++i) {
        bool last = (i + 1 == n.kids.size());
        bool isFile = it->second.score >= 0 && it->second.kids.empty();
        out.push_back(paint(DIM, prefix + (last ? g.lend : g.ltee)) +
                      (isFile ? it->first + "  " + tag(it->second.score) : paint(INFO, it->first + "/")));
        treeLines(it->second, prefix + (last ? g.blank : g.vline), out, tag);
    }
}

// ------------------------------------------------------------ disk map: one glyph per file (status 0 other, 1 clean, 2 suspicious, 3 dangerous)
inline std::vector<std::string> diskMap(const std::vector<char>& st, int& perCell) {
    const Glyphs& g = G();
    int perRow = std::max(8, (theme().width - 4) / 2);
    const size_t MAXCELLS = (size_t)perRow * 8;
    perCell = (int)((st.size() + MAXCELLS - 1) / MAXCELLS);
    if (perCell < 1) perCell = 1;
    std::vector<std::string> lines;
    std::string cur;
    int inRow = 0;
    for (size_t i = 0; i < st.size(); i += perCell) {
        char worst = 0;
        for (size_t k = i; k < st.size() && k < i + perCell; k++) worst = std::max(worst, st[k]);
        std::string cell;
        if (worst == 3)      cell = paint(BAD, g.bad, true);
        else if (worst == 2) cell = paint(WARN, g.susp, true);
        else if (worst == 1) cell = paint(GOOD, g.clean);
        else                 cell = paint(DIM, g.other);
        cur += cell + " ";
        if (++inRow == perRow) { lines.push_back(cur); cur.clear(); inRow = 0; }
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

} // namespace ui

#endif