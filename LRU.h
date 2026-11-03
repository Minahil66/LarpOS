// lRU :  Least Recently Used cache
//DSA used: Doubly-LL , Hashmap
#ifndef LRU_H
#define LRU_H

#include "Hashmap.h"

class Coord{
public:
    int x;
    int y;
    Coord(int a = 0, int b = 0) { x = a; y = b; }
};

class LRUCache {
    class Node { //nested class
    public:
        unsigned long long key;
        Coord value;
        Node* prev;
        Node* next;
        Node(unsigned long long k, Coord v) {
            key = k;
            value = v;
            prev = NULL;
            next = NULL;
        }
    };

    int capacity, count, hits, misses;
    Node* head;               //most recently used side //filled with NULL=-1
    Node* tail;               // least recently used side //filled with NULL=-1
    HashMap<Node*> map;       // key & its node 

    void removeNode(Node* n) { //remove from DLL
        n->prev->next = n->next;
        n->next->prev = n->prev;
    }

    void addToFront(Node* n) {  //insert after head // MRU
        n->next = head->next;
        n->prev = head;
        head->next->prev = n;
        head->next = n;
    }

public:
    LRUCache(int cap = 50) {
        capacity = cap;
        count = 0;
        hits = 0;
        misses = 0;
        head = new Node(0, Coord());
        tail = new Node(0, Coord());
        head->next = tail;
        tail->prev = head;
    }

    ~LRUCache() {
        clear();
        delete head;
        delete tail;
    }

    // copying would make two caches share the same nodes -> forbid it
    LRUCache(const LRUCache&) = delete;
    LRUCache& operator=(const LRUCache&) = delete;

    bool get(unsigned long long key, Coord& result) {
        Node* n;
        if (!map.get(key, n)) { //check if key already exist in map
            misses++;
            return false;
        }
        hits++;
        removeNode(n);
        addToFront(n); //updates to MRU
        result = n->value;
        return true;
    }

    // Insert key
    void put(unsigned long long key, Coord value) {
        if (capacity <= 0) return;
        Node* n;
        if (map.get(key, n)) {            // key exists: update + move to front
            n->value = value;
            removeNode(n);
            addToFront(n);
            return;
        }
        if (count == capacity) {          // full: delete LRU :node before tail
            Node* last = tail->prev;
            removeNode(last);
            map.remove(last->key);
            delete last;
            count--;
        }
        n = new Node(key, value);
        addToFront(n);
        map.insert(key, n);
        count++;
    }

    void clear() {
        Node* n = head->next;
        while (n != tail) {
            Node* nx = n->next;
            delete n;
            n = nx;
        }
        head->next = tail;
        tail->prev = head;
        map = HashMap<Node*>();
        count = 0;
        hits = 0;
        misses = 0;
    }

    int size()      { return count; }
    int getHits()   { return hits; }
    int getMisses() { return misses; }
};

#endif