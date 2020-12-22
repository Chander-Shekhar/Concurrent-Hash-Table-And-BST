#include <iostream>
#include <unordered_map>
#include <atomic>
#include "FSet.h"
using namespace std;

template<typename T>
class HNode{
public:
    atomic<FSet<T>*> *buckets;
    int size, used;
    HNode<T> *pred;
    HNode(int capacity, HNode<T> *pred) {
        buckets = new atomic<FSet<T>*>[capacity];
        for(int i=0;i<capacity;i++)
            buckets[i].store(nullptr);
        size = capacity;
        this->pred = pred;
        used = 0;
    }
    // HNode(atomic<FSet<T,S>*> *buckets, int capacity, HNode<T,S> *pred) {
    //     // throw error if buckets.size != capacity
    //     this->buckets = buckets;
    //     this->size = capacity;
    //     this->pred = pred;
    //     used = 0;
    // }
};

template<typename T>
class HashTable {
private:
    atomic<HNode<T>*> head;

    bool apply(OPType type, T key) {
        FSetOP<T> *op = new FSetOP<T>(type, key);
        while(true) {
            HNode<T> *t = head.load(memory_order_seq_cst);
            FSet<T> *curr_bucket = t->buckets[key % t->size].load(memory_order_seq_cst);
            if(!curr_bucket) {
                curr_bucket = initBucket(t, (key % t->size));
                if(type == INS){
                    t->used++;
                    if(curr_bucket->getHead()->map->size()>=20){
                        resize(true);
                    }
                }
            }
            else if(type == REM and curr_bucket->getHead()->map->size() == 1)
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
        // atomic<FSet<T,S>*> *buckets = new atomic<FSet<T,S>*>[size];
        HNode<T> *t_dash = new HNode<T>(size, t);
        // Confused if it should be in a loop
        head.compare_exchange_strong(t, t_dash);
    }

    FSet<T> *initBucket(HNode<T> *t, int i) {
        FSet<T> *b = t->buckets[i].load(memory_order_seq_cst);
        FSet<T> *m;
        HNode<T> *s = t->pred;
        if(!b and s) {
            llvm::SmallSet <T , 30> *new_set = new llvm::SmallSet <T , 30>();
            if(t->size == s->size*2) {
                m = s->buckets[i % s->size].load(memory_order_seq_cst);
                llvm::SmallSet <T , 30> set_1 = *m->freeze();
                for(auto itr : set_1) {
                    if(itr % t->size == i)
                        new_set->insert(itr);
                }
            }
            else {
                m = s->buckets[i];
                FSet<T> *n = s->buckets[i+t->size];
                llvm::SmallSet <T , 30>set_1 = *m->freeze(), set_2 = *n->freeze(); 
                for(auto itr : set_1)
                    new_set->insert(itr);
                for(auto itr : set_2)
                    new_set->insert(itr);
            }
            FSet<T> *b_dash = new FSet<T>(new_set, true);
            // Confused if it should be in a loop
            FSet<T> *nil = nullptr;
            t->buckets[i].compare_exchange_strong(nil, b_dash);
        }
        return t->buckets[i].load(memory_order_seq_cst);
    }
public:
    HashTable() {
        // atomic<FSet<T,S>*> init = new atomic<FSet<T,S>*>[1];
        head.store(new HNode<T>(1, nullptr));
        head.load(memory_order_seq_cst)->buckets[0].store(new FSet<T>(new llvm::SmallSet <T , 30>(), true));
    }

    bool insert(T key) {
        bool resp = apply(INS, key);
        
        // HNode<T,S> *t = head.load(memory_order_seq_cst);
        // if(t->used >= (3*t->size)/4)
        //     resize(true);
        return resp;
    }

    bool remove(T key) {
        bool resp = apply(REM, key);
        HNode<T> *t = head.load(memory_order_seq_cst);
        int size=t->size;
        if(size >= 3){
            int a=rand()%size;
            int b=(rand()%size+a)%size;
            
            if(t->buckets[a].load(memory_order_seq_cst)->getHead()->map->size() <=5 & t->buckets[b].load(memory_order_seq_cst)->getHead()->map->size()<=5)
                resize(false);
        }
        return resp;
    }

    bool contains(T key) {
        HNode<T> *t = head.load(memory_order_seq_cst);
        FSet<T> *curr_bucket = t->buckets[key % t->size].load(memory_order_seq_cst);
        if(!curr_bucket) {
            HNode<T> *prev_node = t->pred;
            if(prev_node)
                curr_bucket = prev_node->buckets[key % prev_node->size].load(memory_order_seq_cst);
            else
                curr_bucket = t->buckets[key % t->size].load(memory_order_seq_cst);
        }
        return curr_bucket->hasMember(key);
    }
};
