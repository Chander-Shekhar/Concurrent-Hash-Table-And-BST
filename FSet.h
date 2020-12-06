#include<unordered_map>
#include<atomic>
using namespace std;
enum OPType{
	INS,
	REM
};

struct FSetOp{
	OPType type;
	int key;
	int value;
	int resp; // we will add different types of responses
};

struct FSetNode
{
	unordered_map<int,int> map;
	bool ok;
	FSetNode(){
		this->ok = true;
	}
	FSetNode(unordered_map<int,int> &m_map,bool ok ) {
        this->map = m_map;
        this->ok = ok;
    }
};

class FSet{
private:
	atomic<FSetNode*>node;
public:
	unordered_map<int,int> freeze(){
		FSetNode* o = node.load(memory_order_seq_cst);
		while(o->ok){
			FSetNode *n= new FSetNode(o->map,false);
			if(node.compare_exchange_strong(o,n))
				break;
			o=node.load(memory_order_seq_cst);
		}
		return o->map;
	}

	bool invoke(FSetOp op){
		FSetNode* o = node.load(memory_order_seq_cst);
		while(o->ok){
			unordered_map<int,int> map;
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
			FSetNode* n=new FSetNode(map,true);
			if(node.compare_exchange_strong(o,n)){
				op.resp=resp;
				return true;
			}
			o=node.load(memory_order_seq_cst);
		}
		return false;
	}

	bool hasMember(int k){
		FSetNode* o = node.load(memory_order_seq_cst);
		return o->map.find(k)!=o->map.end();
	}

};
