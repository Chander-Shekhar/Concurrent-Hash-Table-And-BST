#include <iostream>
#include <unordered_map>
#include <atomic>

using namespace std;

enum oper{INS, REM};

template<typename T>
class HNode{
public:
    atomic<FSet<T>*> *buckets;
    int size;
    HNode *pred;
    HNode(int capacity) {
        buckets = new atomic<FSet<T>*>[capacity];
        for(int i=0;i<capacity;i++)
            buckets[i].store(nullptr);
        size = capacity;
        pred = nullptr;
    }
    HNode(atomic<FSet<T>*> *buckets, int capacity, HNode *pred) {
        // throw error if buckets.size != capacity
        this->buckets = buckets;
        this->size = capacity;
        this->pred = pred;
    }
};

template<typename T>
class HashTable {
private:
    atomic<HNode*> head;

    bool apply(oper type, T key, T value) {
        FSetOP *op = new FSetOP(type, key, value);
        while(true) {
            HNode *t = head.load(memory_order_seq_cst);
            FSet<T> *curr_bucket = t->buckets[key % t->size].load(memory_order_seq_cst);
            if(!curr_bucket)
                curr_bucket = initBucket(t, (key % t->size));
            if(invoke(curr_bucket, op))
                return getResponse(op);
        }
    }

    void resize(bool grow) {
        HNode *t = head.load(memory_order_seq_cst);
        if(t->size <= 1 and !grow)
            return ;
        for(int i=0;i<t->size;i++)
            initBucket(t, i);
        t->pred = nullptr;
        size = grow ? t->size*2 :t->size/2;
        atomic<FSet<T>*> *buckets = new atomic<FSet<T>*>[size];
        t_dash = new HNode(buckets, size, t);
        // Confused if it should be in a loop
        head.compare_exchange_strong(t, t_dash);
    }

    FSet<T> *initBucket(HNode *t, int i) {
        FSet<T> *b = t->buckets[i].load(memory_order_seq_cst);
        FSet<T> *m;
        unordered_map<atomic<int>, atomic<int>> new_set;
        HNode *s = t->pred;
        if(!b and s) {
            if(t->size == s->size*2) {
                m = s->buckets[i % s->size].load(memory_order_seq_cst);
                unordered_map<atomic<int>, atomic<int>> set_1 = freeze(m);
                for(auto itr : set_1) {
                    if(itr.first.load() % t->size == i)
                        new_set.insert(itr);
                }
            }
            else {
                m = s->buckets[i];
                FSet<T> *n = s->buckets[i+t->size];
                unordered_map<atomic<int>, atomic<int>> set_1 = freeze(m), set_2 = freeze(n); 
                for(auto itr : set_1)
                    new_set.insert(itr);
                for(auto itr : set_2)
                    new_set.insert(itr);
            }
            FSet<T> *b_dash = new FSet<T>(new_set, true);
            // Confused if it should be in a loop
            t->buckets[i].compare_exchange_strong(nullptr, b_dash);
        }
        return t->buckets[i];
    }
public:
    HashTable() {
        atomic<FSet<T>*> init = new atomic<FSet<T>*>[1];
        head.store(new HNode(new atomic<FSet<T>*>[1], 1, nullptr));
        head.load(memory_order_seq_cst)->buckets[0].store(new FSet<T>(new unordered_map<atomic<int>, atomic<int>>(), true));
    }

    bool insert(T key, T value) {
        bool resp = apply(INS, key, value);
        if(/*policy to extend the table*/)
            resize(true);
        return resp;
    }

    bool remove(T key, T value) {
        bool resp = apply(REM, key, value);
        if(/*policy to shrink the table*/)
            resize(false);
        return resp;
    }

    bool contains(T key) {
        HNode *t = head.load(memory_order_seq_cst);
        FSet<T> *curr_bucket = t->buckets[key % t->size].load(memory_order_seq_cst);
        if(!curr_bucket) {
            HNode *prev_node = t->pred;
            if(!prev_node)
                curr_bucket = prev_node->buckets[key % prev_node->size].load(memory_order_seq_cst);
            else
                curr_bucket = t->buckets[key % t->size].load(memory_order_seq_cst);
        }
        return hasMember(curr_bucket, key);
    }

};