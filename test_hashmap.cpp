//test  for Hashmap.h
#include <cassert>
#include <random>
#include <unordered_map>  
#include "../src/Hashmap.h"

void testBasic() {
    HashMap<int> m;
    assert(m.isEmpty());
    m.insert(10, 100);
    m.insert(18, 180);
    m.insert(26, 260);
    int r;
    assert(m.size() == 3);
    assert(m.get(10, r) && r == 100);
    assert(m.get(18, r) && r == 180);
    assert(m.get(26, r) && r == 260);
    assert(!m.get(99, r));
    assert(m.contains(18) && !m.contains(99));
    cout << "basic insert/get ..... ok" << endl;
}

void testKeyZeroAndBigKeys() {
    HashMap<int> m;
    int r;
    m.insert(0, 5);
    m.insert(0xFFFFFFFFFFFFFFFFULL, 6);
    assert(m.get(0, r) && r == 5);
    assert(m.get(0xFFFFFFFFFFFFFFFFULL, r) && r == 6);
    cout << "key 0 and huge keys .. ok" << endl;
}

void testUpdate() {
    HashMap<int> m;
    int r;
    m.insert(5, 1);
    m.insert(5, 2);
    assert(m.size() == 1);
    assert(m.get(5, r) && r == 2);
    cout << "update existing key .. ok" << endl;
}

void testRemoveKeepsChains() {
    HashMap<int> m(7);
    int r;
    for (unsigned long long k = 0; k < 200; k++) m.insert(k, (int)k * 2);
    for (unsigned long long k = 0; k < 200; k += 2) assert(m.remove(k));
    for (unsigned long long k = 0; k < 200; k++) {
        if (k % 2 == 0) assert(!m.get(k, r));
        else            assert(m.get(k, r) && r == (int)k * 2);
    }
    assert(m.size() == 100);
    assert(!m.remove(0));
    cout << "remove + tombstones .. ok" << endl;
}

void testReinsertAfterRemove() {
    HashMap<int> m;
    int r;
    m.insert(42, 1);
    m.remove(42);
    assert(!m.get(42, r));
    m.insert(42, 2);
    assert(m.get(42, r) && r == 2 && m.size() == 1);
    cout << "reinsert after remove  ok" << endl;
}

void testRehashAlignedAddresses() {
    HashMap<int> m;
    int start = m.getCapacity();
    int r;
    for (unsigned long long a = 0x401000, i = 0; i < 5000; a += 16, i++)
        m.insert(a, (int)i);
    assert(m.size() == 5000);
    assert(m.getCapacity() > start);
    for (unsigned long long a = 0x401000, i = 0; i < 5000; a += 16, i++)
        assert(m.get(a, r) && r == (int)i);
    assert(m.size() * 10 <= m.getCapacity() * 7);   // load never above 70%
    cout << "rehash + aligned keys  ok (capacity " << m.getCapacity() << ")" << endl;
}

void testTombstoneChurn() {
    HashMap<int> m(11);
    int r;
    for (unsigned long long k = 0; k < 10000; k++) {
        m.insert(k, 1);
        m.remove(k);
    }
    assert(m.isEmpty());
    assert(m.getCapacity() < 100);                  // did not blow up
    m.insert(7, 7);
    assert(m.get(7, r) && r == 7);
    cout << "tombstone churn ...... ok (capacity " << m.getCapacity() << ")" << endl;
}

struct Node { int x; };

void testPointerValues() {
    Node a, b;
    a.x = 1; b.x = 2;
    HashMap<Node*> m;
    Node* p;
    m.insert(1, &a);
    m.insert(2, &b);
    assert(m.get(1, p) && p->x == 1);
    assert(m.get(2, p) && p->x == 2);
    m.remove(1);
    assert(!m.get(1, p));
    cout << "pointer values ....... ok" << endl;
}

void testRandomAgainstStd() {
    mt19937_64 rng(12345);
    HashMap<unsigned long long> mine;
    unordered_map<unsigned long long, unsigned long long> truth;

    for (int step = 0; step < 200000; step++) {
        unsigned long long key = rng() % 3000;
        int op = (int)(rng() % 3);
        if (op == 0) {
            unsigned long long v = rng();
            mine.insert(key, v);
            truth[key] = v;
        } else if (op == 1) {
            bool a = mine.remove(key);
            bool b = truth.erase(key) > 0;
            assert(a == b);
        } else {
            unsigned long long got = 0;
            bool found = mine.get(key, got);
            auto it = truth.find(key);
            if (it == truth.end()) assert(!found);
            else                   assert(found && got == it->second);
        }
        assert(mine.size() == (int)truth.size());
    }
    cout << "random vs std map .... ok" << endl;
}

int main() {
    testBasic();
    testKeyZeroAndBigKeys();
    testUpdate();
    testRemoveKeepsChains();
    testReinsertAfterRemove();
    testRehashAlignedAddresses();
    testTombstoneChurn();
    testPointerValues();
    testRandomAgainstStd();
    cout << endl << "ALL HASHMAP TESTS PASSED" << endl;
    return 0;
}
