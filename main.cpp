#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>
#include <poll.h>

#include "Renderer.h"   // also pulls in Hashmap.h, LRU.h, GraphData.h, Canvas.h
#include "Dominator.h"
#include "Heap.h"
#include "disasm.h"
#include "graph.h"
#include "Scanner.h"
#include "Usb.h"
#include "Ui.h"

using namespace std;
using namespace ui;
typedef uint64_t Addr;

// ================================================================ small helpers
string plural(size_t n, const string& w) { return to_string(n) + " " + w + (n == 1 ? "" : "s"); }
string baseName(const string& p) { size_t i = p.find_last_of('/'); return i == string::npos ? p : p.substr(i + 1); }
string dot() { return string(" ") + G().bullet + " "; }

string sizeText(unsigned long long mb) {
    ostringstream o;
    if (mb >= 1024) o << fixed << setprecision(1) << (mb / 1024.0) << " GB";
    else o << mb << " MB";
    return o.str();
}

string addrHex(Addr a) { ostringstream o; o << "0x" << std::hex << a; return o.str(); }

// returns false at end of input
bool ask(const string& prompt, string& out) {
    if (!prompt.empty()) cout << prompt << "\n";
    cout << paint(ACCENT, G().arrow, true) << " " << flush;
    if (!getline(cin, out)) return false;
    size_t a = out.find_first_not_of(" \t\r"), b = out.find_last_not_of(" \t\r");
    out = (a == string::npos) ? "" : out.substr(a, b - a + 1);
    return true;
}

void logo() {
    static const char* art[] = {
        R"X( _                      ___  ____ )X",
        R"X(| |    __ _ _ __ _ __  / _ \/ ___|)X",
        R"X(| |   / _` | '__| '_ \| | | \___ \)X",
        R"X(| |__| (_| | |  | |_) | |_| |___) |)X",
        R"X(|_____\__,_|_|  | .__/ \___/|____/ )X",
        R"X(                |_|               )X"};
    static const int ramp[] = {51, 45, 39, 75, 111, 141};
    cout << "\n";
    for (int i = 0; i < 6; i++) {
        string c = !theme().color ? "" : (theme().color256 ? "\033[38;5;" + to_string(ramp[i]) + "m" : "\033[36m");
        cout << "  " << c << art[i] << reset() << "\n";
        tick(40);
    }
    cout << "  " << paint(DIM, "Minahil") << paint(ACCENT, dot()) << paint(DIM, "Rubaisha")
         << paint(ACCENT, dot()) << paint(DIM, "Hasan") << "\n";
}

// ================================================================ CFG helpers
struct Graph {
    vector<vector<int>> adj;
    vector<Addr> addr;
    int entry = 0;
    vector<int> instrs;
    size_t insns = 0;
};

bool buildGraph(const string& path, Graph& g) {
    vector<cs_insn> insns = Disasm::get_instruct(path);
    if (insns.empty()) return false;
    
    CFG cfg;
    cfg.build_from_instruct(insns);
    g = Graph();
    g.adj = cfg.translator(g.addr);
    if (g.adj.empty()) return false;

    // 1. Fill instruction counts properly
    g.instrs.assign(g.adj.size(), 1);
    for (size_t i = 0; i < g.adj.size(); i++) {
        Basic_Block bb;
        if (cfg.blocks.get(g.addr[i], bb)) g.instrs[i] = (int)bb.instruction.size();
    }
    g.insns = insns.size();

    // 2. Brute-force best entry point (Try first 200 blocks)
    int best_entry = 0;
    int max_loops = 0;
    
    for (int i = 0; i < 200 && i < g.adj.size(); i++) {
        red::DominatorTree test_dom = red::computeDominators(g.adj, i);
        std::vector<red::LoopInfo> test_loops = red::findLoops(g.adj, test_dom, g.instrs);
        if (test_loops.size() > max_loops) {
            max_loops = test_loops.size();
            best_entry = i;
        }
    }
    
    g.entry = best_entry; 
    return true;
}

// room sizes ordered by address (optionally only some rooms), squeezed to at most maxBuckets values
vector<double> roomSizes(const Graph& g, const vector<int>* subset, size_t maxBuckets) {
    vector<pair<Addr, int>> v;
    if (subset) for (size_t i = 0; i < subset->size(); i++) v.push_back({g.addr[(*subset)[i]], g.instrs[(*subset)[i]]});
    else        for (size_t i = 0; i < g.addr.size(); i++)  v.push_back({g.addr[i], g.instrs[i]});
    sort(v.begin(), v.end());
    vector<double> out;
    size_t per = max<size_t>(1, (v.size() + maxBuckets - 1) / maxBuckets);
    for (size_t i = 0; i < v.size(); i += per) {
        double m = 0;
        for (size_t k = i; k < v.size() && k < i + per; k++) m = max(m, (double)v[k].second);
        out.push_back(m);
    }
    return out;
}

// ================================================================ X-ray
struct Report {
    string name;
    size_t blocks = 0, loops = 0, deepest = 0;
    string verdict;
    int meter = 0;
};

// loop inside loop, drawn as nested frames
vector<string> onion(size_t depth) {
    const Glyphs& g = G();
    int D = (int)min<size_t>(depth, 4), W = 44;
    vector<string> out;
    auto col = [&](int k, const string& s) {
        return theme().color ? heatSgr(D > 1 ? (double)k / (D - 1) * 0.8 : 0) + s + "\033[0m" : s;
    };
    auto sides = [&](int d, string& l, string& r) {
        l.clear(); r.clear();
        for (int k = 0; k < d; k++) { l += col(k, g.v + " "); r = col(k, " " + g.v) + r; }
    };
    string l, r;
    for (int d = 0; d < D; d++) {
        sides(d, l, r);
        string title = "loop level " + to_string(d + 1) + " ";
        int fill = max(1, W - 4 * d - 4 - (int)title.size());
        out.push_back(l + col(d, g.tl + g.h + " " + title + repeat(g.h, fill) + g.tr) + r);
    }
    sides(D - 1, l, r);
    out.push_back(l + col(D - 1, g.v) + center(paint(DIM, "...runs again and again"), W - 4 * (D - 1) - 2) + col(D - 1, g.v) + r);
    for (int d = D - 1; d >= 0; d--) {
        sides(d, l, r);
        out.push_back(l + col(d, g.bl + repeat(g.h, W - 4 * d - 2) + g.br) + r);
    }
    if (depth > (size_t)D) out.push_back(paint(DIM, "(+" + to_string(depth - D) + " more levels inside)"));
    return out;
}

void heat(double score, string& word, Tone& tone, int& stars) {
    if (score < 200)      { word = "LIGHT";  tone = GOOD; stars = 1; }
    else if (score < 500) { word = "MEDIUM"; tone = WARN; stars = 2; }
    else                  { word = "HEAVY";  tone = BAD;  stars = 3; }
}

// draws the busiest loop (at most 6 rooms so it fits on screen)
void drawLoop(const red::LoopInfo& l, const vector<vector<int>>& adj, const vector<Addr>& addr) {
    const size_t LIMIT = 6;
    vector<bool> inBody(adj.size(), false), seen(adj.size(), false), shown(adj.size(), false);
    for (size_t i = 0; i < l.body.size(); i++) inBody[l.body[i]] = true;

    vector<int> chosen;
    queue<int> q;
    q.push(l.header);
    seen[l.header] = true;
    while (!q.empty() && chosen.size() < LIMIT) {
        int u = q.front();
        q.pop();
        chosen.push_back(u);
        shown[u] = true;
        for (size_t k = 0; k < adj[u].size(); k++) {
            int v = adj[u][k];
            if (inBody[v] && !seen[v]) { seen[v] = true; q.push(v); }
        }
    }

    vector<BasicBlock> blocks;
    vector<string> labels;
    Loop lp(addr[l.header]);
    for (size_t k = 0; k < chosen.size(); k++) {
        BasicBlock b(addr[chosen[k]]);
        for (size_t s = 0; s < adj[chosen[k]].size(); s++)
            if (shown[adj[chosen[k]][s]]) b.successor.push_back(addr[adj[chosen[k]][s]]);
        blocks.push_back(b);
        labels.push_back((k == 0 && theme().unicode ? string("◉ ") : string("")) + "Room " + to_string(k + 1));
        lp.body.push_back(addr[chosen[k]]);
    }
    Renderer r;
    cout << "\n";
    r.render(blocks, vector<Loop>(1, lp), theme().color, labels, false, theme().unicode);

    string back = theme().unicode ? "↻" : "<-";
    cout << "\n  " << paint(BAD, "double frame", true) << " = door into the loop    "
         << paint(WARN, "round frame") << " = other rooms in it    " << paint(INFO, back) << " = jump back, go again\n";
    if (l.body.size() > chosen.size())
        cout << paint(DIM, "  (showing " + to_string(chosen.size()) + " of " + to_string(l.body.size()) + " rooms in this loop)") << "\n";
}

Report analyzeGraph(const string& name, const Graph& g) {
    Report rep;
    rep.name = name;
    rep.blocks = g.adj.size();

    red::DominatorTree dom = red::computeDominators(g.adj, g.entry);
    vector<red::LoopInfo> loops = red::findLoops(g.adj, dom, g.instrs);
    vector<red::LoopInfo> ranked = red::LoopPriorityQueue::rank(loops);

    size_t reachable = dom.dfs.rpo.size(), nested = 0, deepest = 0;
    for (size_t i = 0; i < loops.size(); i++) {
        if (loops[i].nesting_depth > 1) nested++;
        deepest = max(deepest, (size_t)loops[i].nesting_depth);
    }
    rep.loops = loops.size();
    rep.deepest = deepest;
    int inner = theme().width - 4;

    rule("X-RAY  " + name);

    // ---- anatomy
    vector<string> a = tiles({{commas(g.insns), "STEPS"}, {commas(g.adj.size()), "ROOMS"},
                              {commas(loops.size()), "LOOPS"}, {to_string(deepest), "DEEPEST"}},
                             {INFO, INFO, WARN, deepest >= 3 ? BAD : WARN});
    a.push_back("");
    for (const string& s : wrap("A program is a list of tiny steps. Steps that run one after another form a room; "
                                "a jump is a door to another room; rooms that repeat form a loop.", inner))
        a.push_back(paint(DIM, s));
    a.push_back("");
    a.push_back("room sizes  " + spark(roomSizes(g, nullptr, 44)) + paint(DIM, "  start -> end"));
    int pct = g.adj.empty() ? 0 : (int)(reachable * 100 / g.adj.size());
    a.push_back("reached     " + gauge((double)reachable, (double)g.adj.size(), 30, false, INFO) + " " + to_string(pct) + "%");
    a.push_back(paint(DIM, "            reached by plain jumps; the rest are entered by function calls"));
    panel("ANATOMY", a, INFO);

    // ---- loops
    if (loops.empty()) {
        panel("REPEATING PARTS", {paint(GOOD, "No loops found.") + " This program runs straight through."}, GOOD);
    } else {
        vector<string> l = wrap("A loop is like a washing-machine cycle: the same rooms run again and again. "
                                "Programs spend most of their time inside loops.", inner);
        for (size_t i = 0; i < l.size(); i++) l[i] = paint(DIM, l[i]);
        if (deepest >= 2) {
            l.push_back("");
            vector<string> o = onion(deepest);
            l.insert(l.end(), o.begin(), o.end());
            l.push_back("");
            l.push_back(paint(WARN, "Loop inside loop multiplies work:") + " 100 x 100 = 10,000 runs.");
        }
        panel("REPEATING PARTS", l, WARN);
    }

    // ---- hot spots
    if (!ranked.empty()) {
        vector<string> h;
        double top = ranked[0].score;
        const Glyphs& g2 = G();
        for (size_t i = 0; i < ranked.size() && i < 3; i++) {
            string word; Tone tone; int stars;
            heat(ranked[i].score, word, tone, stars);
            string st;
            for (int k = 0; k < 3; k++) st += k < stars ? g2.tri : g2.triOff;
            h.push_back(paint(ACCENT, "#" + to_string(i + 1), true) + "  " + paint(tone, st + " " + pad(word, 6), true) +
                        "  " + gauge(ranked[i].score, top, 26));
            h.push_back("    " + plural(ranked[i].block_count, "room") + dot() + "about " + plural(ranked[i].instruction_count, "step") +
                        dot() + (ranked[i].nesting_depth == 1 ? string("plain loop")
                                                              : "inside " + plural(ranked[i].nesting_depth - 1, "other loop")));
            h.push_back("    weight " + spark(roomSizes(g, &ranked[i].body, 20), tone) +
                        paint(DIM, "   door at " + addrHex(g.addr[ranked[i].header])));
            if (i + 1 < ranked.size() && i + 1 < 3) h.push_back("");
        }
        panel("HOT SPOTS  (bigger score = busier)", h, ACCENT);

        rule("PICTURE OF HOT SPOT #1");
        drawLoop(ranked[0], g.adj, g.addr);
    }

    // ---- verdict
    rep.meter = (int)min<size_t>(100, loops.size() + 5 * deepest);
    Tone tone;
    string text;
    if (rep.meter < 5)       { rep.verdict = "SIMPLE";   tone = GOOD; text = "Easy to follow: few or no repeating parts."; }
    else if (rep.meter < 40) { rep.verdict = "MODERATE"; tone = WARN; text = "A fair amount of repetition. Start with the hot spots."; }
    else                     { rep.verdict = "COMPLEX";  tone = BAD;  text = "Lots of repetition and nesting. Focus on the top hot spots."; }
    rule("VERDICT");
    panel("COMPLEXITY", {"meter  " + gauge(rep.meter, 100, 40) + "  " + paint(tone, rep.verdict, true), text,
                         paint(DIM, "This describes how the code is shaped. It is not a virus verdict.")}, tone);
    return rep;
}

bool xrayFile(const string& path, Report& out) {
    note(INFO, "Reading " + paint(TEXT, path, true) + " ...");
    Graph g;
    if (!buildGraph(path, g)) {
        panel("COULD NOT READ", {"That does not look like a Linux program file (ELF).",
                                 "Try one like " + paint(INFO, "/bin/ls") + " and check the path."}, BAD);
        return false;
    }
    out = analyzeGraph(baseName(path), g);
    return true;
}

Report xrayDemo() {
    // 0->1->2->3->4->5, inner loop 3->2, outer loop 4->1, self loop 5->5, 6 is dead code
    Graph g;
    g.adj = {{1}, {2, 5}, {3}, {2, 4}, {1, 5}, {5}, {5}};
    g.instrs = {2, 4, 3, 5, 3, 2, 1};
    for (int i = 0; i < (int)g.adj.size(); i++) { g.addr.push_back(0x1000 + i * 16); g.insns += g.instrs[i]; }
    return analyzeGraph("built-in demo", g);
}

void scoreboard(const vector<Report>& reps) {
    vector<string> l;
    l.push_back(paint(DIM, pad("file", 20) + pad("rooms", 8) + pad("loops", 8) + "complexity"));
    for (size_t i = 0; i < reps.size(); i++) {
        Tone t = reps[i].verdict == "COMPLEX" ? BAD : (reps[i].verdict == "MODERATE" ? WARN : GOOD);
        l.push_back(pad(clip(reps[i].name, 19), 20) + pad(to_string(reps[i].blocks), 8) + pad(to_string(reps[i].loops), 8) +
                    gauge(reps[i].meter, 100, 14) + " " + paint(t, reps[i].verdict, true));
    }
    rule("SCOREBOARD");
    panel("ALL FILES SIDE BY SIDE", l, ACCENT);
}

void xrayMany(const string& line) {
    istringstream in(line);
    vector<Report> batch;
    string tok;
    while (in >> tok) {
        Report r;
        if (tok == "demo") batch.push_back(xrayDemo());
        else if (xrayFile(tok, r)) batch.push_back(r);
    }
    if (batch.size() >= 2) scoreboard(batch);
}

// ================================================================ virus scan
struct ScanSummary {
    size_t files = 0, elfs = 0, clean = 0, suspicious = 0, dangerous = 0;
    vector<scan::FileReport> flagged;   // suspicious or dangerous
    vector<char> map;                   // per file: 0 other, 1 clean program, 2 suspicious, 3 dangerous
};

string relPath(const string& root, const string& path) {
    string r = path.substr(min(root.size(), path.size()));
    while (!r.empty() && r[0] == '/') r.erase(0, 1);
    return r;
}

ScanSummary scanFolder(const string& root, scan::BlockList& bl) {
    ScanSummary s;
    scan::walk(root, 0, [&](const string& path) {
        s.files++;
        scan::FileReport r;
        char status = 0;
        if (scan::scanFile(path, bl, r)) {
            s.elfs++;
            scan::Level lv = scan::levelOf(r.score);
            if (lv == scan::CLEAN) { s.clean++; status = 1; }
            else {
                if (lv == scan::DANGEROUS) { s.dangerous++; status = 3; } else { s.suspicious++; status = 2; }
                s.flagged.push_back(r);
            }
        }
        s.map.push_back(status);
        if (s.files % 5 == 0)
            live("  " + paint(ACCENT, spinFrame(s.files / 5), true) + " scanning  " + pad(clip(relPath(root, path), 34), 34) +
                 paint(DIM, commas(s.files) + " files" + dot() + commas(s.elfs) + " programs" + dot() +
                                commas(s.flagged.size()) + " flagged"));
    });
    liveEnd();
    return s;
}

string shapeLine(const string& path, unsigned long long size) {
    if (size > 4ULL * 1024 * 1024) return "";
    Graph g;
    if (!buildGraph(path, g)) return "";
    red::DominatorTree dom = red::computeDominators(g.adj, g.entry);
    vector<red::LoopInfo> loops = red::findLoops(g.adj, dom, g.instrs);
    return "code shape: " + plural(g.adj.size(), "room") + ", " + plural(loops.size(), "loop");
}

void showScan(ScanSummary& s, const string& root) {
    scan::Level overall = s.dangerous ? scan::DANGEROUS : (s.suspicious ? scan::SUSPICIOUS : scan::CLEAN);
    Tone vt = overall == scan::DANGEROUS ? BAD : (overall == scan::SUSPICIOUS ? WARN : GOOD);
    string word = overall == scan::DANGEROUS ? "DANGER" : (overall == scan::SUSPICIOUS ? "CAUTION" : "CLEAR");
    string tag = overall == scan::DANGEROUS ? "Do NOT run or open the red files."
               : overall == scan::SUSPICIOUS ? "Some files look odd. Check where they came from."
                                              : "No warning signs found in this drive.";
    int inner = theme().width - 4;

    rule("SCAN RESULT  " + root);
    cout << "\n";
    vector<string> big = bigText(word);
    for (size_t i = 0; i < big.size(); i++) { cout << "   " << paint(vt, big[i], true) << "\n"; tick(40); }
    cout << "\n   " << paint(vt, tag, true) << "\n\n";

    panel("SUMMARY", tiles({{commas(s.files), "FILES"}, {commas(s.clean), "CLEAN"},
                            {commas(s.suspicious), "SUSPICIOUS"}, {commas(s.dangerous), "DANGEROUS"}},
                           {INFO, GOOD, WARN, BAD}), vt);

    int perCell = 1;
    vector<string> dm = diskMap(s.map, perCell);
    if (!dm.empty()) {
        const Glyphs& g = G();
        dm.push_back("");
        dm.push_back(paint(DIM, string(g.other) + " other file   ") + paint(GOOD, g.clean) + " clean program   " +
                     paint(WARN, g.susp) + " suspicious   " + paint(BAD, g.bad) + " dangerous");
        if (perCell > 1) dm.push_back(paint(DIM, "each cell = " + to_string(perCell) + " files (shows the worst one)"));
        panel("DISK MAP  (one mark per file)", dm, INFO);
    }

    sort(s.flagged.begin(), s.flagged.end(),
         [](const scan::FileReport& a, const scan::FileReport& b) { return a.score > b.score; });

    if (!s.flagged.empty()) {
        ui::TreeNode root_node;
        for (size_t i = 0; i < s.flagged.size(); i++) treeAdd(root_node, relPath(root, s.flagged[i].path), s.flagged[i].score);
        vector<string> tl;
        tl.push_back(paint(INFO, root + "/", true));
        treeLines(root_node, "", tl, [](int sc) {
            Tone t = scan::levelOf(sc) == scan::DANGEROUS ? BAD : WARN;
            return paint(t, scan::levelOf(sc) == scan::DANGEROUS ? G().bad : G().susp, true) + " " + paint(DIM, "score " + to_string(sc));
        });
        panel("WHERE THE TROUBLE IS", tl, vt);
    }

    const size_t SHOW = 12;
    for (size_t i = 0; i < s.flagged.size() && i < SHOW; i++) {
        const scan::FileReport& r = s.flagged[i];
        scan::Level lv = scan::levelOf(r.score);
        Tone t = lv == scan::DANGEROUS ? BAD : WARN;
        vector<string> c;
        c.push_back(paint(DIM, relPath(root, r.path)));
        c.push_back("risk  " + gauge(min(r.score, 100), 100, 30) + "  " + paint(t, to_string(r.score), true) +
                    (r.score > 100 ? "  (off the scale)" : ""));
        for (size_t k = 0; k < r.findings.size(); k++) {
            if (r.findings[k].points <= 0) continue;
            vector<string> w = wrap(r.findings[k].why + "  (+" + to_string(r.findings[k].points) + ")", inner - 3, "  ");
            for (size_t m = 0; m < w.size(); m++) c.push_back((m == 0 ? paint(t, G().arrow, true) + " " : "  ") + w[m]);
        }
        string shape = (i < 5) ? shapeLine(r.path, r.size) : "";
        if (!shape.empty()) c.push_back(paint(DIM, shape));
        if (!r.sha256.empty()) c.push_back(paint(DIM, "sha256 " + r.sha256.substr(0, 40) + "..."));
        panel("#" + to_string(i + 1) + "  " + scan::levelName(lv), c, t);
    }
    if (s.flagged.size() > SHOW) note(DIM, "... and " + to_string(s.flagged.size() - SHOW) + " more flagged files.");

    vector<string> adv;
    if (overall == scan::DANGEROUS) {
        adv.push_back(paint(BAD, "Do not run or open the dangerous files.", true));
        adv.push_back("Do not copy them to other computers. Keep the drive unplugged from important machines.");
    } else if (overall == scan::SUSPICIOUS) {
        adv.push_back(paint(WARN, "Be careful.", true) + " Flagged files may be harmless tools, but only run them");
        adv.push_back("if you know where they came from.");
    } else {
        adv.push_back(paint(GOOD, "Nothing alarming found.", true));
    }
    adv.push_back("");
    for (const string& w : wrap("I look for warning signs. I cannot prove a file is safe, and brand-new viruses can slip past.", inner))
        adv.push_back(paint(DIM, w));
    panel("WHAT TO DO", adv, vt);
}

// scan one mounted folder, then let the user X-ray flagged files
void scanAndReport(const string& root, scan::BlockList& bl) {
    note(INFO, "Scanning " + paint(TEXT, root, true));
    ScanSummary s = scanFolder(root, bl);
    showScan(s, root);
    while (!s.flagged.empty()) {
        string in;
        if (!ask("\nType a number to X-ray that flagged file, or press Enter to continue", in) || in.empty()) break;
        size_t idx = (size_t)atoi(in.c_str());
        if (idx >= 1 && idx <= s.flagged.size() && idx <= 12) { Report r; xrayFile(s.flagged[idx - 1].path, r); }
    }
}

// ================================================================ USB flow
void scanDrive(usb::Drive d, scan::BlockList& bl) {
    rule("USB DRIVE  " + d.label + "  (" + sizeText(d.sizeMB) + ")");
    for (size_t i = 0; i < d.parts.size(); i++) {
        usb::Partition& p = d.parts[i];
        if (p.mountPoint.empty()) {
            string in;
            ask("\n" + p.dev + " is not opened yet. Open it read-only (nothing on it can run or change)? [y/n]", in);
            if (in != "y" && in != "Y") continue;
            string err;
            if (!usb::mountReadOnly(p, err)) {
                panel("COULD NOT OPEN " + p.dev, {err, paint(DIM, "Needs root, the /dev node, and a supported file system (FAT, exFAT, ext4, NTFS).")}, BAD);
                continue;
            }
        }
        scanAndReport(p.mountPoint, bl);
        usb::unmount(p);
    }
}

void listDrives(const vector<usb::Drive>& drives) {
    for (size_t i = 0; i < drives.size(); i++) {
        vector<string> l;
        l.push_back(paint(TEXT, drives[i].label, true) + paint(DIM, dot() + sizeText(drives[i].sizeMB) + dot() + "/dev/" + drives[i].name));
        for (size_t k = 0; k < drives[i].parts.size(); k++) {
            const usb::Partition& p = drives[i].parts[k];
            l.push_back("  " + pad(p.dev, 14) + (p.mountPoint.empty() ? paint(DIM, "not opened") : paint(GOOD, "opened at " + p.mountPoint)));
        }
        panel("USB #" + to_string(i + 1), l, INFO);
    }
}

void usbFlow(scan::BlockList& bl) {
    note(INFO, "Looking for USB drives ...");
    vector<usb::Drive> drives = usb::detect();
    if (drives.empty()) {
        panel("NO USB DRIVE FOUND", {"Plug one in and try again, or use option 2 to wait for it.",
                                     paint(DIM, "If it is plugged in: check kernel USB-storage support and that /dev nodes exist (mdev/udev).")}, WARN);
        return;
    }
    note(GOOD, "Found " + plural(drives.size(), "USB drive") + ":");
    listDrives(drives);
    string in;
    if (!ask("\nWhich one should I check? (number, 'a' for all, Enter to cancel)", in) || in.empty()) return;
    for (size_t i = 0; i < drives.size(); i++)
        if (in == "a" || in == "A" || (size_t)atoi(in.c_str()) == i + 1) scanDrive(drives[i], bl);
}

void waitFlow(scan::BlockList& bl) {
    vector<string> known;
    vector<usb::Drive> now = usb::detect();
    for (size_t i = 0; i < now.size(); i++) known.push_back(now[i].name);

    note(INFO, "Plug in your USB drive now. I'm watching...  (press Enter to cancel)");
    for (int tick_s = 0; tick_s < 180; tick_s++) {
        live("  " + paint(ACCENT, spinFrame(tick_s), true) + " waiting for a new USB drive  " + paint(DIM, to_string(tick_s) + "s"));
        struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
        if (poll(&pfd, 1, 1000) > 0) { string tmp; getline(cin, tmp); liveEnd(); note(DIM, "Cancelled."); return; }
        now = usb::detect();
        for (size_t i = 0; i < now.size(); i++) {
            if (find(known.begin(), known.end(), now[i].name) != known.end()) continue;
            liveEnd();
            note(GOOD, "USB detected: " + paint(TEXT, now[i].label, true) + " (" + sizeText(now[i].sizeMB) + ")");
            usleep(2000000);                 // give the kernel a moment to publish the partitions
            vector<usb::Drive> fresh = usb::detect();
            for (size_t k = 0; k < fresh.size(); k++)
                if (fresh[k].name == now[i].name) scanDrive(fresh[k], bl);
            return;
        }
    }
    liveEnd();
    note(WARN, "Nothing was plugged in. Giving up for now.");
}
void manual() {
    note(INFO, "Opening LARP OS Manual...");

    vector<string> p1 = {
        paint(ACCENT, "LARP OS (Live Action Role Play OS)"),
        "A single-purpose, Linux distribution built to reverse-engineer binaries.",
        "",
        paint(ACCENT, "1. THE VIRUS SCANNER"),
        "The Scanner acts as the triage nurse. It reads the raw bytes of a file WITHOUT executing it.",
        "It checks for:",
        "  - Disguised files (e.g., a .pdf that is secretly a Linux program).",
        "  - Dangerous commands (e.g., 'wget', 'bash -i', '/etc/shadow').",
        "  - Packed/encrypted code using Shannon Entropy.",
        "  - Known malware using SHA-256 hash matching."
    };
    panel("PAGE 1: THE SCANNER", p1, INFO);

    vector<string> p2 = {
        paint(ACCENT, "2. THE GRAPH ENGINE (X-RAY)"),
        "The X-Ray tool acts as the surgeon. It translates raw machine code into assembly and builds a map.",
        "",
        paint(ACCENT, "Terminology:"),
        "  - STEPS: Total assembly instructions (mov, add, jmp).",
        "  - ROOMS: Basic Blocks (straight-line chunks of code).",
        "  - ROOM SIZE: Number of Instructions in a room.",
        "  - DOOR: It is a loop header.",
        "  - DOMINATOR: A 'boss' node. You MUST pass through it to reach the next room.",
        "  - BACK-EDGE: An arrow jumping backward to its dominator. This proves a loop exists.",
        "  - HOT SPOT: The most dangerous, complex loop found in the binary."
    };
    panel("PAGE 2: THE GRAPH ENGINE", p2, ACCENT);

    vector<string> p3 = {
        paint(ACCENT, "3. READING THE VISUAL GRAPH"),
        "When you look at the Hot Spot picture, here is what the shapes mean:",
        "",
        "  - DOUBLE FRAME (╔══╗): The Loop Header. This is the 'door' into the cycle.",
        "  - ROUND FRAME (╭──╮): Other blocks inside the loop.",
        "  - CIRCLE ARROW (↻): The exact jump instruction going backward.",
        "",
        "Note: The terminal can only fit 6 blocks on the screen at once.",
        "If a loop has 39 blocks, we show the 6 most active ones so it fits."
    };
    panel("PAGE 3: VISUALS", p3, WARN);
    
    vector<string> p4 = {
        paint(ACCENT, "4. WHY DOMINATOR TREES FIND MALWARE"),
        "Malware relies on structural patterns. By using Cooper's Dominator Algorithm, we find them:",
        "",
        "  - RANSOMWARE: Deeply nested loops (Depth 3+) for encryption algorithms.",
        "  - ANTI-DEBUGGING: Self-loops (Block A jumps to Block A) to freeze antivirus scanners.",
        "  - BOTNETS: Massive loops for scanning IP addresses.",
        "",
        paint(DIM, "Use the '2' key in the main menu to run an X-Ray on a suspicious file.")
    };
    panel("PAGE 4: MALWARE BEHAVIOR", p4, BAD);
}
void menu() {
    auto key = [](const string& k) { return paint(ACCENT, "[" + k + "]", true); };
    panel("MAIN MENU", {// (being fixed)key("1") + "  Find USB drives and check them for viruses",
                        //being fixed key("2") + "  Wait for a USB drive to be plugged in",
                        key("1") + "  Check a folder",
                        key("2") + "  Analyze a program file (explain how it is built)",
                        key("3") + "  Demo of the Analysis",
                        key("4") + "  User Manual",
                        key("q") + "  Quit"}, ACCENT);
}

int main() {
    ui::init();
    logo();

    scan::BlockList blocklist;
    const char* env = getenv("LARPOS_BLOCKLIST");
    string blPath = env ? env : "/etc/larpos/blocklist.txt";
    cout << "\n";
    if (blocklist.load(blPath)) note(GOOD, "Known-malware list loaded: " + commas(blocklist.size()) + " fingerprints");
    else note(WARN, "No known-malware list at " + blPath + paint(DIM, "  (hash matching off; other checks still work)"));

    while (true) {
        cout << "\n";
        menu();
        string choice;
        if (!ask("", choice)) break;

        if (choice == "q" || choice == "Q") break;
       // else if (choice == "1") usbFlow(blocklist);
        //else if (choice == "2") waitFlow(blocklist);
        else if (choice == "1") {
            string path;
            if (ask("Folder to check (e.g. /mnt/usb)", path) && !path.empty()) scanAndReport(path, blocklist);
        } else if (choice == "2") {
            string line;
            if (ask("Enter one or more program files (e.g. /bin/ls /bin/cat)", line) && !line.empty()) xrayMany(line);
        } else if (choice == "3") xrayDemo();
        else if (choice == "4") manual();
        else note(WARN, "Please pick 1, 2, 3, 4, or q.");
    }
    note(ACCENT, "Sayounara");
    return 0;
}