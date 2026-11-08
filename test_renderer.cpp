#include <cassert>
#include "../src/Renderer.h"

typedef unsigned long long u64; //vector<unsigned long long>{0x1010}  ==   vector<u64>{0x1010}

// run render() but catch what it prints, so we can check it
string capture(Renderer& r, const vector<BasicBlock>& b, const vector<Loop>& l, bool color = false) {
    ostringstream buf; //makes an empty text bucket
    streambuf* old = cout.rdbuf(buf.rdbuf());   // send cout into the bucket
    r.render(b, l, color);
    cout.rdbuf(old);                            // put cout back to the screen
    return buf.str();
}

bool has(const string& s, const string& part) {
    return s.find(part) != string::npos; // npos ->nothing found 
}

// which line of the picture is this text on? (0 = first line)
int lineOf(const string& s, const string& part) {
    size_t pos = s.find(part);
    if (pos == string::npos) return -1;
    int line = 0;
    for (size_t i = 0; i < pos; i++)
        if (s[i] == '\n') line++;
    return line;
}

void testStraightLine() {
    vector<BasicBlock> b;
    b.push_back(BasicBlock(0x1000, vector<u64>{0x1010}));
    b.push_back(BasicBlock(0x1010, vector<u64>{0x1020}));
    b.push_back(BasicBlock(0x1020));
    Renderer r;
    string out = capture(r, b, vector<Loop>());
    assert(has(out, "0x1000") && has(out, "0x1010") && has(out, "0x1020"));
    assert(has(out, "?"));
    assert(lineOf(out, "0x1000") < lineOf(out, "0x1010"));      // going down
    assert(lineOf(out, "0x1010") < lineOf(out, "0x1020"));
    assert(!has(out, "\033"));                                  // plain mode: no color codes
    cout << "straight line ......... ok" << endl;
}

void testDiamond() {
    vector<BasicBlock> b;
    b.push_back(BasicBlock(0x2000, vector<u64>{0x2010, 0x2020}));
    b.push_back(BasicBlock(0x2010, vector<u64>{0x2030}));
    b.push_back(BasicBlock(0x2020, vector<u64>{0x2030}));
    b.push_back(BasicBlock(0x2030));
    Renderer r;
    string out = capture(r, b, vector<Loop>());
    assert(lineOf(out, "0x2010") == lineOf(out, "0x2020"));     // side by side
    assert(lineOf(out, "0x2000") < lineOf(out, "0x2010"));
    assert(lineOf(out, "0x2010") < lineOf(out, "0x2030"));
    assert(has(out, "cache hits: 8, misses: 4"));               // 4 rooms = 4 misses, rest are hits
    cout << "diamond ............... ok" << endl;
}

void testLoopAndColors() {
    vector<BasicBlock> b;
    b.push_back(BasicBlock(0x4000, vector<u64>{0x4010}));
    b.push_back(BasicBlock(0x4010, vector<u64>{0x4000, 0x4020}));
    b.push_back(BasicBlock(0x4020));
    vector<Loop> l;
    l.push_back(Loop(0x4000, vector<u64>{0x4000, 0x4010}));
    Renderer r;
    string plain = capture(r, b, l, false);
    assert(has(plain, "<- 0x4000"));                            // back-jump note
    assert(lineOf(plain, "0x4020") > lineOf(plain, "0x4010"));  // BFS did not loop forever
    assert(!has(plain, "\033"));
    string colored = capture(r, b, l, true);
    assert(has(colored, "\033[31m"));                           // red   (header)
    assert(has(colored, "\033[33m"));                           // yellow (inside)
    assert(has(colored, "\033[36m"));                           // cyan  (outside)
    cout << "loop + colors ......... ok" << endl;
}

void testNoLoopsMeansOnlyCyan() {
    vector<BasicBlock> b;
    b.push_back(BasicBlock(0x1000, vector<u64>{0x1010}));
    b.push_back(BasicBlock(0x1010));
    Renderer r;
    string out = capture(r, b, vector<Loop>(), true);
    assert(has(out, "\033[36m"));
    assert(!has(out, "\033[31m") && !has(out, "\033[33m"));
    cout << "no loops = cyan only .. ok" << endl;
}

void testNestedLoops() {
    vector<BasicBlock> b;
    b.push_back(BasicBlock(0x5000, vector<u64>{0x5010}));
    b.push_back(BasicBlock(0x5010, vector<u64>{0x5020, 0x5050}));
    b.push_back(BasicBlock(0x5020, vector<u64>{0x5030}));
    b.push_back(BasicBlock(0x5030, vector<u64>{0x5020, 0x5040}));
    b.push_back(BasicBlock(0x5040, vector<u64>{0x5010}));
    b.push_back(BasicBlock(0x5050));
    vector<Loop> l;
    l.push_back(Loop(0x5010, vector<u64>{0x5010, 0x5020, 0x5030, 0x5040}));
    l.push_back(Loop(0x5020, vector<u64>{0x5020, 0x5030}));
    Renderer r;
    string plain = capture(r, b, l, false);              // notes are easy to find in plain mode
    assert(has(plain, "<- 0x5020") && has(plain, "<- 0x5010"));
    string colored = capture(r, b, l, true);             // in color mode every letter has its own color code
    assert(has(colored, "\033[31m") && has(colored, "\033[33m") && has(colored, "\033[36m"));
    cout << "nested loops .......... ok" << endl;
}

void testOddInputs() {
    Renderer r;
    assert(capture(r, vector<BasicBlock>(), vector<Loop>()) == "");   // empty graph: draws nothing

    vector<BasicBlock> b;
    b.push_back(BasicBlock(0x100, vector<u64>{0x999999, 0x110}));      // 0x999999 does not exist
    b.push_back(BasicBlock(0x110));
    b.push_back(BasicBlock(0x777));                                    // nobody can reach it
    string out = capture(r, b, vector<Loop>());
    assert(has(out, "0x100") && has(out, "0x110"));
    assert(!has(out, "0x777"));

    vector<BasicBlock> c;                                              // real-looking long addresses
    c.push_back(BasicBlock(0x401000, vector<u64>{0x401010}));
    c.push_back(BasicBlock(0x401010, vector<u64>{0x401000}));
    string first = capture(r, c, vector<Loop>());
    assert(has(first, "<- 0x401000"));
    assert(first == capture(r, c, vector<Loop>()));                    // same input, same picture
    cout << "odd inputs ............ ok" << endl;
}

void testBigGraph() {
    vector<BasicBlock> b;                                              // 300 rooms in a chain
    for (int i = 0; i < 300; i++) {
        BasicBlock x(0x1000 + i * 16);
        if (i + 1 < 300) x.successor.push_back(0x1000 + (i + 1) * 16);
        b.push_back(x);
    }
    Renderer r(8);                                                     // tiny cache on purpose
    string out = capture(r, b, vector<Loop>());
    assert(has(out, "0x22b0"));                                        // the last room (0x1000 + 299*16)
    assert(has(out, "cache hits: 298, misses: 600"));
    cout << "300-block chain ....... ok" << endl;
}

int main() {
    testStraightLine();
    testDiamond();
    testLoopAndColors();
    testNoLoopsMeansOnlyCyan();
    testNestedLoops();
    testOddInputs();
    testBigGraph();
    cout << endl << "ALL RENDERER TESTS PASSED" << endl;
    return 0;
}