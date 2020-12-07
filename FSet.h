#include<unordered_map>
#include<atomic>
using namespace std;

enum OPType{
	INS,
	REM
};

struct FSetOP{
	OPType type;
	int key;
	int value;
	int resp; // we will add different types of responses 0 ==> error, 1 ==> new key and value added, 2==> new value added 

	bool getResponse() {
		return resp;
	}
};

template<typename T>
class FSetNode
{
	unordered_map<T,T> map;
	bool ok;
	FSetNode(){
		this->ok = true;
	}
	FSetNode(unordered_map<T,T> m_map,bool ok ) {
        this->map = m_map;
        this->ok = ok;
    }
};

template<typename T>
class FSet{
private:
	atomic<FSetNode<T>*>node;
public:
	unordered_map<T,T> freeze(){
		FSetNode<T>* o = node.load(memory_order_seq_cst);
		while(o->ok){
			FSetNode<T> *n= new FSetNode<T>(o->map,false);
			if(node.compare_exchange_strong(o,n))
				break;
			o=node.load(memory_order_seq_cst);
		}
		return o->map;
	}

	bool invoke(FSetOP op){
		FSetNode<T>* o = node.load(memory_order_seq_cst);
		while(o->ok){
			unordered_map<T,T> map;
			int resp;
			if(op.type==INS){
				if(o->map.find(op.key)==o->map.end()){
					map=o->map;
					map[op.key]=op.value;
					resp=1;
				}
				else{
					if(op.value==o->map[op.key])
						resp=0;
					else{
						map=o->map;
						map[op.key]=op.value;
						resp=2;
					}
				}
			}
			else if(op.type==REM){
				if(o->map.find(op.key)==o->map.end())
					resp=0;
				else{
					o->map.erase(op.key);
					resp=1;
				}
			}
			FSetNode<T>* n=new FSetNode<T>(map,true);
			if(node.compare_exchange_strong(o,n)){
				op.resp=resp;
				return true;
			}
			o=node.load(memory_order_seq_cst);
		}
		return false;
	}

	bool hasMember(T k){
		FSetNode<T>* o = node.load(memory_order_seq_cst);
		return o->map.find(k)!=o->map.end();
	}

    FSetNode<T> *getHead() {
        return node.load();
    }

};
