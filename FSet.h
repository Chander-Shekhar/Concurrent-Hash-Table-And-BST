#include<unordered_map>
#include<atomic>
using namespace std;

enum OPType{
	INS,
	REM
};

template<typename T,typename S>
struct FSetOP{
	OPType type;
	T key;
	S value;
	int resp; // we will add different types of responses 0 ==> error, 1 ==> new key and value added, 2==> new value added 
	FSetOP(OPType type,T key, S value){
		this->type=type;
		this->key=key;
		this->value=value;
	}
	bool getResponse() {
		return resp;
	}
};

template<typename T,typename S>
struct FSetNode
{
	unordered_map<T,S> *map;
	bool ok;
	FSetNode(){
		this->ok = true;
	}
	FSetNode(unordered_map<T,S> *m_map,bool ok ) {
        this->map = m_map;
        this->ok = ok;
    }
};

template<typename T,typename S>
class FSet{
private:
	atomic<FSetNode<T,S>*> node;
public:
	FSet(unordered_map<T,S> *m_map, bool ok){
		FSetNode<T,S> *p=new FSetNode<T,S>(m_map,ok);
		node.store(p,memory_order_seq_cst);
	}

	unordered_map<T,S>* freeze(){
		FSetNode<T,S>* o = node.load(memory_order_seq_cst);
		unordered_map<T,S> *new_map = new unordered_map<T,S>();
		while(o->ok){
			*new_map = *o->map;
			FSetNode<T,S> *n= new FSetNode<T,S>(new_map,false);
			if(node.compare_exchange_strong(o,n))
				break;
			o=node.load(memory_order_seq_cst);
		}
		return o->map;
	}

	bool invoke(FSetOP<T,S>* op){
		FSetNode<T,S>* o = node.load(memory_order_seq_cst);

		while(o->ok){
			unordered_map<T,S> *map = new unordered_map<T,S>();
			int resp;
			if(op->type==INS){
				if(o->map->find(op->key)==o->map->end()){
					*map=*(o->map);
					(*map)[op->key]=op->value;
					resp=1;
				}
				else{
					if(op->value==o->map->at(op->key))
						resp=0;
					else{
						*map=*(o->map);
						(*map)[op->key]=op->value;
						resp=2;
					}
				}
			}
			else if(op->type==REM){
				if(o->map->find(op->key)==o->map->end())
					resp=0;
				else{
					*map=*(o->map);
					map->erase(op->key);
					resp=1;
				}
			}
			FSetNode<T,S>* n=new FSetNode<T,S>(map,true);
			if(node.compare_exchange_strong(o,n)){
				op->resp=resp;
				return true;
			}
			o=node.load(memory_order_seq_cst);
		}
		return false;
	}

	bool hasMember(T k){
		FSetNode<T,S>* o = node.load(memory_order_seq_cst);
		return o->map->find(k)!=o->map->end();
	}

    FSetNode<T,S> *getHead() {
        return node.load();
    }

};
