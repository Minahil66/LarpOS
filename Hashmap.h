
#ifndef HASHMAP_H
#define HASHMAP_H

#include <iostream>
#include <vector>
using namespace std;
template <class V> //holder
class HashMap {
private:
    enum { EMPTY, FULL, DELETED }; 

    vector<unsigned long long> keys;
    vector<V> values;
    vector<int> state;
    int capacity;     // number of slots 
    int count;        // FULL slots
    int usedSlots;    // FULL + DELETED slots

    bool isPrime(int n) {
        if (n < 2) return false;
        for (int i = 2; i * i <= n; i++)
            if (n % i == 0) return false;
        return true;
    }
    int nextPrime(int n) {
        while (!isPrime(n)) n++;
        return n;
    }

    int hashFunc(unsigned long long key) {
        return (int)(key % (unsigned long long)capacity);
    }
    int findIndex(unsigned long long key) {
        int idx = hashFunc(key);
        for (int i = 0; i < capacity; i++) {
            int pos = (idx + i) % capacity;         
            if (state[pos] == EMPTY) return -1;     
            if (state[pos] == FULL && keys[pos] == key) return pos;

        }
        return -1;
    }
    void place(unsigned long long key, const V& value) {
        int idx = hashFunc(key);
        int i = 0;
        while (state[(idx + i) % capacity] == FULL) i++;
        int p = (idx + i) % capacity;               
        if (state[p] == EMPTY) usedSlots++;          
        keys[p] = key;
        values[p] = value;
        state[p] = FULL;
        count++;
    }
    void rehash() {
        vector<unsigned long long> oldKeys = keys;
        vector<V> oldValues = values;
        vector<int> oldState = state;
        int oldCapacity = capacity;

        if (count * 2 > capacity)
            capacity = nextPrime(capacity * 2);

        keys.assign(capacity, 0);
        values.assign(capacity, V());
        state.assign(capacity, EMPTY);
        count = 0;
        usedSlots = 0;

        for (int i = 0; i < oldCapacity; i++)
            if (oldState[i] == FULL)
                place(oldKeys[i], oldValues[i]);     
    }

public:
    HashMap(int size = 11) {
        capacity = nextPrime(size);
        count = 0;
        usedSlots = 0;
        keys.assign(capacity, 0);
        values.assign(capacity, V());
        state.assign(capacity, EMPTY);
    }

    void insert(unsigned long long key, const V& value) {
        int pos = findIndex(key);
        if (pos != -1) {                             
            values[pos] = value;
            return;
        }
        if ((usedSlots + 1) * 10 > capacity * 7)     // more than 70% used
            rehash();
        place(key, value);
    }
    bool get(unsigned long long key, V& result) {
        int pos = findIndex(key);
        if (pos == -1) return false;
        result = values[pos];
        return true;
    }

    bool contains(unsigned long long key) {
        return findIndex(key) != -1;
    }

    bool remove(unsigned long long key) {
        int pos = findIndex(key);
        if (pos == -1) return false;
        state[pos] = DELETED;
        values[pos] = V();
        count--;
        return true;
    }

    int size()        { return count; }
    bool isEmpty()    { return count == 0; }
    int getCapacity() { return capacity; }

    void display() {
        cout << "slot : key (value)" << endl;
        for (int i = 0; i < capacity; i++) {
            cout << "  " << i << " : ";
            if (state[i] == FULL)         cout << keys[i] << " (" << values[i] << ")";
            else if (state[i] == DELETED) cout << "DELETED";
            else                          cout << "-";
            cout << endl;
        }
        cout << "count = " << count << ", capacity = " << capacity << endl;
    }
};

#endif