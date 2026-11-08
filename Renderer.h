
// Renderer : draws blocks and loops as boxes and arrows
// DSA: BFS (layout), HashMap (lookups), LRU cache (positions)
#ifndef RENDERER_H
#define RENDERER_H
 
#include <queue>
#include <sstream> //used to write 0x4010
#include "Hashmap.h"
#include "LRU.h"
#include "GraphData.h"
#include "Canvas.h"
 
class Renderer {
private:
    static constexpr int BOX_W = 16, BOX_H = 4, COL_STEP = 21, ROW_STEP = 7;
 
    HashMap<int> indexOf;     // address -> block number
    HashMap<int> loopKind;    // address -> 1 = loop header, 2 = inside a loop
    vector<int> layerOf;      // row of each block (-1 = not placed)
    vector<int> colOf;        // column of each block
    LRUCache cache;           // address -> screen position
 
    string hex(unsigned long long addr) {
        ostringstream o;
        o << "0x" << std::hex << addr;
        return o.str();
    }
 
    void markLoops(const vector<Loop>& loops) {
        loopKind = HashMap<int>();
        for (int i = 0; i < (int)loops.size(); i++)
            for (int j = 0; j < (int)loops[i].body.size(); j++)
                loopKind.insert(loops[i].body[j], 2);
        for (int i = 0; i < (int)loops.size(); i++)
            loopKind.insert(loops[i].header, 1);          // header wins over body
    }
 
    int colorOf(unsigned long long addr) {
        int kind;
        if (loopKind.get(addr, kind))
            return (kind == 1) ? COLOR_RED : COLOR_YELLOW;
        return COLOR_CYAN;
    }
 
    // BFS: block 0 is layer 0, what it jumps to is layer 1, ...
    void computeLayout(const vector<BasicBlock>& blocks) {
        int n = (int)blocks.size();
        indexOf = HashMap<int>();
        for (int i = 0; i < n; i++) indexOf.insert(blocks[i].addr, i);
 
        layerOf.assign(n, -1);
        colOf.assign(n, 0);
        vector<int> nextCol;                 // next free column in each layer
        queue<int> q;
 
        layerOf[0] = 0;
        colOf[0] = 0;
        nextCol.push_back(1);
        q.push(0);
 
        while (!q.empty()) {
            int u = q.front();
            q.pop();
            for (int k = 0; k < (int)blocks[u].successor.size(); k++) {
                int v;
                if (!indexOf.get(blocks[u].successor[k], v)) continue;   // jump outside the code
                if (layerOf[v] != -1) continue;                      // already placed
                int layer = layerOf[u] + 1;
                layerOf[v] = layer;
                if ((int)nextCol.size() <= layer) nextCol.push_back(0);
                colOf[v] = nextCol[layer];
                nextCol[layer]++;
                q.push(v);
            }
        }
    }
 
    // (layer, column) -> screen (x, y); ask the cache first
    Coord position(unsigned long long addr) {
        Coord c;
        if (cache.get(addr, c)) return c;                    // HIT
        int idx;
        indexOf.get(addr, idx);                              // MISS: calculate
        c = Coord(1 + colOf[idx] * COL_STEP, 1 + layerOf[idx] * ROW_STEP);
        cache.put(addr, c);                                  
        return c;
    }
 
    void drawEdge(Canvas& canvas, unsigned long long from, unsigned long long to) {
        Coord a = position(from);
        Coord b = position(to);
        int ax = a.x + BOX_W / 2;          // middle of source box
        int bx = b.x + BOX_W / 2;          // middle of target box
        int startY = a.y + BOX_H;          // first row under the source box
        int midY = startY + 1;             // row where the line turns sideways
        int endY = b.y - 1;                // row for the arrow head
 
        if (ax == bx) {
            canvas.vline(ax, startY, endY - 1);
        } else {
            canvas.vline(ax, startY, midY - 1);
            canvas.put(ax, midY, bx > ax ? "+" : "+");
            if (bx > ax) canvas.hline(midY, ax + 1, bx - 1);
            else         canvas.hline(midY, bx + 1, ax - 1);
            canvas.put(bx, midY, bx > ax ? "+" : "+");
            canvas.vline(bx, midY + 1, endY - 1);
        }
        canvas.put(bx, endY, "?");
    }
 
public:
    Renderer(int cacheSize = 64) : cache(cacheSize) {}
 
    // blocks[0] must be the entry block
    void render(const vector<BasicBlock>& blocks, const vector<Loop>& loops, bool useColor = true) {
        if (blocks.empty()) return;
        cache.clear();
        computeLayout(blocks);
        markLoops(loops);
        int n = (int)blocks.size();
 
        int maxLayer = 0, maxCol = 0;
        for (int i = 0; i < n; i++) {
            if (layerOf[i] > maxLayer) maxLayer = layerOf[i];
            if (colOf[i] > maxCol) maxCol = colOf[i];
        }
        Canvas canvas(1 + maxCol * COL_STEP + BOX_W + 1, 1 + maxLayer * ROW_STEP + BOX_H + 1);
 
        // arrows first, so boxes sit on top of them
        for (int u = 0; u < n; u++) {
            if (layerOf[u] == -1) continue;                  // unreachable block
            for (int k = 0; k < (int)blocks[u].successor.size(); k++) {
                int v;
                if (!indexOf.get(blocks[u].successor[k], v)) continue;
                if (layerOf[v] > layerOf[u]) drawEdge(canvas, blocks[u].addr, blocks[v].addr);
            }
        }
 
        // boxes, colors, and "<- target" for a jump back up
        for (int u = 0; u < n; u++) {
            if (layerOf[u] == -1) continue;
            Coord p = position(blocks[u].addr);
            int c = colorOf(blocks[u].addr);
            canvas.drawBox(p.x, p.y, BOX_W, BOX_H, hex(blocks[u].addr), c);
            for (int k = 0; k < (int)blocks[u].successor.size(); k++) {
                int v;
                if (!indexOf.get(blocks[u].successor[k], v)) continue;
                if (layerOf[v] != -1 && layerOf[v] <= layerOf[u]) {
                    canvas.text(p.x + 2, p.y + 2, ("<- " + hex(blocks[u].successor[k])).substr(0, BOX_W - 4), c);
                    break;                                   // only the first back-edge
                }
            }
        }
 
        canvas.print(useColor);
        cout << "\nLegend: red = loop header, yellow = inside a loop, cyan = normal block" << endl;
        cout << "cache hits: " << cache.getHits() << ", misses: " << cache.getMisses() << endl;
    }
};
 
#endif
 
