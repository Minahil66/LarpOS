
#include <cassert>
#include <random>
#include "../src/LRU.h"

void testDeletion() {
    LRUCache c(2);
    Coord r;
    c.put(1, Coord(10, 10));
    c.put(2, Coord(20, 20));
    assert(c.get(1, r) && r.x == 10);      // 1 is now most recent
    c.put(3, Coord(30, 30));               // full :delete 2
    assert(!c.get(2, r));                  // 2 is deleted
    assert(c.get(1, r) && r.x == 10);
    assert(c.get(3, r) && r.x == 30);
    cout << "eviction order " << endl;
}

void testUpdate() {
    LRUCache c(2);
    Coord r;
    c.put(1, Coord(1, 1));
    c.put(1, Coord(9, 9));                 // same key: update, not a new entry
    assert(c.size() == 1);
    assert(c.get(1, r) && r.x == 9);
    cout << "update existing key " << endl;
}

void testCapacityOne() {
    LRUCache c(1);
    Coord r;
    c.put(1, Coord(1, 1));
    c.put(2, Coord(2, 2));
    assert(!c.get(1, r));
    assert(c.get(2, r) && r.x == 2);
    cout << "capacity 1 " << endl;
}

void testCounters() {
    LRUCache c(2);
    Coord r;
    c.put(1, Coord());
    c.get(1, r);       // hit
    c.get(5, r);       // miss
    assert(c.getHits() == 1 && c.getMisses() == 1);
    c.clear();
    assert(c.size() == 0 && c.getHits() == 0);
    assert(!c.get(1, r));
    cout << "hit/miss + clear " << endl;
}

void testRandomAgainstSimpleModel() {
    const int CAP = 8;
    LRUCache c(CAP);
    vector<unsigned long long> order;
    mt19937_64 rng(7);
    for (int step = 0; step < 100000; step++) {
        unsigned long long key = rng() % 20;
        Coord r;
        if (rng() % 2 == 0) {
            c.put(key, Coord((int)key, 0));
            for (int i = 0; i < (int)order.size(); i++)
                if (order[i] == key) { order.erase(order.begin() + i); break; }
            order.push_back(key);
            if ((int)order.size() > CAP) order.erase(order.begin());
        } else {
            bool found = c.get(key, r);
            bool modelFound = false;
            for (int i = 0; i < (int)order.size(); i++)
                if (order[i] == key) {
                    modelFound = true;
                    order.erase(order.begin() + i);
                    order.push_back(key);
                    break;
                }
            assert(found == modelFound);
            if (found) assert(r.x == (int)key);
        }
        assert(c.size() == (int)order.size());
    }
    cout << "random vs simple model " << endl;
}

int main() {
    testDeletion();
    testUpdate();
    testCapacityOne();
    testCounters();
    testRandomAgainstSimpleModel();
    cout << endl << "ALL LRU TESTS PASSED" << endl;
    return 0;
}