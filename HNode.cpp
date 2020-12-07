#include <iostream>
#include <unordered_map>
#include <atomic>
#include <FSet.h>
using namespace std;

template<typename T>
class HNode{
public:
    atomic<FSet<T>*> *buckets;
    int size, used;
    HNode<T> *pred;
    HNode(int capacity) {
        buckets = new atomic<FSet<T>*>[capacity];
        for(int i=0;i<capacity;i++)
            buckets[i].store(nullptr);
        size = capacity;
        pred = nullptr;
        used = 0;
    }
    HNode(atomic<FSet<T>*> *buckets, int capacity, HNode<T> *pred) {
        // throw error if buckets.size != capacity
        this->buckets = buckets;
        this->size = capacity;
        this->pred = pred;
        used = 0;
    }
};

template<typename T>
class HashTable {
private:
    atomic<HNode<T>*> head;

    bool apply(OPType type, T key, T value) {
        FSetOP *op = new FSetOP(type, key, value);
        while(true) {
            HNode<T> *t = head.load(memory_order_seq_cst);
            FSet<T> *curr_bucket = t->buckets[key % t->size].load(memory_order_seq_cst);
            if(!curr_bucket) {
                curr_bucket = initBucket(t, (key % t->size));
                if(type == INS)
                    t->used++;
            }
            else if(type == REM and curr_bucket->getHead()->map.size() == 1)
                t->used--;
            if(curr_bucket->invoke(op))
                return op->getResponse();
        }
    }

    void resize(bool grow) {
        HNode<T> *t = head.load(memory_order_seq_cst);
        if(t->size <= 1 and !grow)
            return ;
        for(int i=0;i<t->size;i++)
            initBucket(t, i);
        t->pred = nullptr;
        int size = grow ? t->size*2 :t->size/2;
        atomic<FSet<T>*> *buckets = new atomic<FSet<T>*>[size];
        HNode<T> *t_dash = new HNode<T>(buckets, size, t);
        // Confused if it should be in a loop
        head.compare_exchange_strong(t, t_dash);
    }

    FSet<T> *initBucket(HNode<T> *t, int i) {
        FSet<T> *b = t->buckets[i].load(memory_order_seq_cst);
        FSet<T> *m;
        unordered_map<T,T> new_set;
        HNode<T> *s = t->pred;
        if(!b and s) {
            if(t->size == s->size*2) {
                m = s->buckets[i % s->size].load(memory_order_seq_cst);
                unordered_map<T,T> set_1 = m->freeze();
                for(auto itr : set_1) {
                    if(itr.first % t->size == i)
                        new_set.insert(itr);
                }
            }
            else {
                m = s->buckets[i];
                FSet<T> *n = s->buckets[i+t->size];
                unordered_map<T,T> set_1 = m->freeze(), set_2 = n->freeze(); 
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
        head.store(new HNode<T>(new atomic<FSet<T>*>[1], 1, nullptr));
        head.load(memory_order_seq_cst)->buckets[0].store(new FSet<T>(new unordered_map<T,T>(), true));
    }

    bool insert(T key, T value) {
        bool resp = apply(INS, key, value);
        if(head.load()->used >= head.load()->size/2)
            resize(true);
        return resp;
    }

    bool remove(T key, T value) {
        bool resp = apply(REM, key, value);
        if(head.load()->used < head.load()->size/2)
            resize(false);
        return resp;
    }

    bool contains(T key) {
        HNode<T> *t = head.load(memory_order_seq_cst);
        FSet<T> *curr_bucket = t->buckets[key % t->size].load(memory_order_seq_cst);
        if(!curr_bucket) {
            HNode<T> *prev_node = t->pred;
            if(!prev_node)
                curr_bucket = prev_node->buckets[key % prev_node->size].load(memory_order_seq_cst);
            else
                curr_bucket = t->buckets[key % t->size].load(memory_order_seq_cst);
        }
        return curr_bucket->hasMember(key);
    }

};