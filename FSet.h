#include<unordered_map>
#include<atomic>
using namespace std;
#include "/usr/include/llvm-12/llvm/ADT/SmallSet.h"
enum OPType{
	INS,
	REM
};

template<typename T>
struct FSetOP{
	OPType type;
	T key;
	int resp; // we will add different types of responses 0 ==> error, 1 ==> new key and value added, 2==> new value added 
	FSetOP(OPType type,T key){
		this->type=type;
		this->key=key;
	}
	bool getResponse() {
		return resp;
	}
};

template<typename T>
struct FSetNode
{
	llvm::SmallSet <T , 30>  *map;
	bool ok;
	FSetNode(){
		this->ok = true;
	}
	FSetNode(llvm::SmallSet <T , 30> *m_map,bool ok ) {
        this->map = m_map;
        this->ok = ok;
    }
};

template<typename T>
class FSet{
private:
	atomic<FSetNode<T>*> node;
public:
	FSet(llvm::SmallSet <T , 30> *m_map, bool ok){
		FSetNode<T> *p=new FSetNode<T>(m_map,ok);
		node.store(p,memory_order_seq_cst);
	}

	llvm::SmallSet <T , 30>* freeze(){
		FSetNode<T>* o = node.load(memory_order_seq_cst);
		llvm::SmallSet <T , 30> *new_map = new llvm::SmallSet <T , 30>();
		while(o->ok){
			*new_map = *o->map;
			FSetNode<T> *n= new FSetNode<T>(new_map,false);
			if(node.compare_exchange_strong(o,n))
				break;
			o=node.load(memory_order_seq_cst);
		}
		return o->map;
	}

	bool invoke(FSetOP<T>* op){
		FSetNode<T>* o = node.load(memory_order_seq_cst);

		while(o->ok){
			llvm::SmallSet <T , 30> *map = new llvm::SmallSet <T , 30>();
			int resp;
			if(op->type==INS){
				if(o->map->contains(op->key)){
					*map=*(o->map);
					(*map).insert(op->key);
					resp=1;
				}
				else{
						resp=0;
				}
			}
			else if(op->type==REM){
				if(o->map->contains(op->key))
					resp=0;
				else{
					*map=*(o->map);
					map->erase(op->key);
					resp=1;
				}
			}
			FSetNode<T>* n=new FSetNode<T>(map,true);
			if(node.compare_exchange_strong(o,n)){
				op->resp=resp;
				return true;
			}
			o=node.load(memory_order_seq_cst);
		}
		return false;
	}

	bool hasMember(T k){
		FSetNode<T>* o = node.load(memory_order_seq_cst);
		return !o->map->contains(k);
	}

    FSetNode<T> *getHead() {
        return node.load();
    }

};
