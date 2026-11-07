#ifndef GRAPHDATA_H
#define GRAPHDATA_H
#include <vector>
using namespace std;

class BasicBlock {
public:
    unsigned long long addr;              // starting address of the block
    vector<unsigned long long> successor;      // addresses it can jump or fall to

    BasicBlock(unsigned long long a = 0) { addr = a; }
    BasicBlock(unsigned long long a, const vector<unsigned long long>& s) {
        addr = a;
        successor = s;
    }
};

class Loop {
public:
    unsigned long long header;            // loop header address
    vector<unsigned long long> body;      // addresses of blocks inside the loop

    Loop(unsigned long long h = 0) { header = h; }
    Loop(unsigned long long h, const vector<unsigned long long>& b) {
        header = h;
        body = b;
    }
};

#endif