// demo_renderer: 4 mock graphs
#include "../src/Renderer.h"

typedef unsigned long long u64;

//  straight line //A ? B ? C
void straightLine(vector<BasicBlock>& b, vector<Loop>& l) {
    b.push_back(BasicBlock(0x1000, vector<u64>{0x1010}));
    b.push_back(BasicBlock(0x1010, vector<u64>{0x1020}));
    b.push_back(BasicBlock(0x1020));
    (void)l;
}

// diamond (if / else) //A splits into B and C, which join at D	
void diamond(vector<BasicBlock>& b, vector<Loop>& l) {
    b.push_back(BasicBlock(0x2000, vector<u64>{0x2010, 0x2020}));
    b.push_back(BasicBlock(0x2010, vector<u64>{0x2030}));
    b.push_back(BasicBlock(0x2020, vector<u64>{0x2030}));
    b.push_back(BasicBlock(0x2030));
    (void)l;
}

//  loop: 0x4010 jumps back to 0x4000
void simpleLoop(vector<BasicBlock>& b, vector<Loop>& l) {
    b.push_back(BasicBlock(0x4000, vector<u64>{0x4010}));
    b.push_back(BasicBlock(0x4010, vector<u64>{0x4000, 0x4020}));
    b.push_back(BasicBlock(0x4020));
    l.push_back(Loop(0x4000, vector<u64>{0x4000, 0x4010}));
}

// nested loops: inner loop 0x5020 <-> 0x5030, outer loop 0x5010 ... 0x5040
void nestedLoops(vector<BasicBlock>& b, vector<Loop>& l) {
    b.push_back(BasicBlock(0x5000, vector<u64>{0x5010}));
    b.push_back(BasicBlock(0x5010, vector<u64>{0x5020, 0x5050}));
    b.push_back(BasicBlock(0x5020, vector<u64>{0x5030}));
    b.push_back(BasicBlock(0x5030, vector<u64>{0x5020, 0x5040}));
    b.push_back(BasicBlock(0x5040, vector<u64>{0x5010}));
    b.push_back(BasicBlock(0x5050));
    l.push_back(Loop(0x5010, vector<u64>{0x5010, 0x5020, 0x5030, 0x5040}));
    l.push_back(Loop(0x5020, vector<u64>{0x5020, 0x5030}));
}

void show(const char* title, void (*make)(vector<BasicBlock>&, vector<Loop>&)) {
    vector<BasicBlock> blocks;
    vector<Loop> loops;
    make(blocks, loops);
    cout << "-----------" << title << " ----------" << endl;
    Renderer r;
    r.render(blocks, loops);          // colored; use render(blocks, loops, false) for plain
    cout << endl;
}

int main() {
    show("1. straight line", straightLine);
    show("2. diamond (if/else)", diamond);
    show("3. simple loop", simpleLoop);
    show("4. nested loops", nestedLoops);
    return 0;
}