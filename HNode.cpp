#include <iostream>
#include <atomic>

using namespace std;

template<typename T>
class HNode{
private:
    FSet<T> *buckets;
    int size;
    HNode *pred;
    int used;
public:
    int num_resizes;
    
    HNode(int capacity) {
        buckets = new FSet<T>[size];
        size = capacity;
        pred = nullptr;
        used = num_resizes = 0;
    }

    bool insert(T &key) {
        bool resp = apply(0, key);
        if(used >= size/2)
            resize(true);
        return resp;
    }

    bool remove(T &key) {
        bool resp = apply(1, key);
        if(used < size/2)
            resize(false);
        return resp;
    }

    bool contains(T &key) {
        FSet<T> curr_bucket = buckets[key % size];
        // Check the following if condition for correctness.
        if(!curr_bucket.getHead()->is_mutable) {
            HNode *prev_node = pred;
            if(!prev_node) {
                curr_bucket = prev_node->buckets[key % prev_node->size];
            }
        }
        return curr_bucket.hasMember(key);
    }
};