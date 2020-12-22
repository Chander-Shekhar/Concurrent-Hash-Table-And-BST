#include <iostream>
#include<iostream>
#include<math.h>
#include<sys/time.h> 
#include<bits/stdc++.h>
#include<thread>
#include<chrono>
#include<unistd.h>
#include<random>
using namespace std;
enum StateType{
	ONGOING,
	SUCCESSFUL,
    FAILED
};
enum flag_type {
	NONE = 0,
	MARK,
	CHILDCAS,
	RELOCATE
};

enum find_result_type {
	ABORT = 0,
	NOTFOUND_L,
	NOTFOUND_R,
	FOUND
};

// enum operation_state {
// 	ONGOING = 0,
// 	SUCCESSFUL,
	
// };

void *SETFLAG(void *ptr, int state)
{
	ptr = (void *) ((uintptr_t)ptr | (uintptr_t)state);
	return ptr;
}

int GETFLAG(void *ptr)
{
	int flag = ((uintptr_t) ptr & (uintptr_t)0x3L);
	return flag;
}

void *UNFLAG(void *ptr)
{
	ptr = (void *)((uintptr_t)ptr & ~(uintptr_t)0x3L);
	return ptr;
}

void *SETNULL(void *ptr)
{
	ptr = (void *) ((uintptr_t)ptr | (uintptr_t)0x1L);
	return ptr;
}

bool ISNULL(void *ptr)
{
	int val = ((uintptr_t)ptr & (uintptr_t)0x1L);
	if(val == 1) {
		return true;
	}
	return false;
}

struct Operation {};

struct Node {
    int volatile key;
    Operation* volatile op;
    Node* volatile left;
    Node* volatile right;
    Node(int key){
        this->key=key;
        op=NULL;
        left=(Node*)SETNULL(NULL);
        right=(Node*)SETNULL(NULL);
    }
    Node(){        
        op=NULL;
        left=(Node*)SETNULL(NULL);
        right=(Node*)SETNULL(NULL);
    }
}root;

struct ChildCASOp : Operation {
    bool isLeft;
    Node* expected;
    Node* update;
    ChildCASOp(bool isLeft, Node* expected, Node* update){
        this->isLeft=isLeft;
        this->expected=expected;
        this->update=update;       
    }
};
struct RelocateOp : Operation {
    int volatile state = ONGOING;
    Node* dest;
    Operation* destOp;
    int removeKey;
    int replaceKey;
    RelocateOp(Node* dest, Operation* destOp, int removeKey, int replaceKey){
        this->dest=dest;
        this->destOp=destOp;
        this->removeKey=removeKey;   
        this->replaceKey=replaceKey;   
    }
};


void helpChildCAS(ChildCASOp* op, Node* dest) {
    Node* volatile* address = op->isLeft ? &dest->left : &dest->right;
    __sync_bool_compare_and_swap(address, op->expected, op->update);
    __sync_bool_compare_and_swap(&dest->op, (Operation*)SETFLAG(op, CHILDCAS), (Operation*)SETFLAG(op, NONE));
}

void helpMarked(Node* pred, Operation* predOp, Node* curr) {
    Node* newRef;
    if (ISNULL(curr->left)) {
        if (ISNULL(curr->right))
            newRef = (Node *)SETNULL(curr);
        else
            newRef = curr->right;
    }else
        newRef = curr->left;
    Operation* casOp = new ChildCASOp(curr == pred->left, curr, newRef);
    if (__sync_bool_compare_and_swap(&pred->op, predOp, (Operation*)SETFLAG(casOp, CHILDCAS)))
        helpChildCAS((ChildCASOp*)casOp, pred);
}

bool helpRelocate(RelocateOp* op, Node* pred, Operation* predOp, Node* curr) {
    int seenState = op->state;
    if (seenState == ONGOING) {
        Operation* seenOp = __sync_val_compare_and_swap(&op->dest->op, op->destOp, (Operation*)SETFLAG(op, RELOCATE));
        if ((seenOp == op->destOp) || (seenOp == (Operation*)SETFLAG(op, RELOCATE))) {
            __sync_bool_compare_and_swap(&op->state, ONGOING, SUCCESSFUL);
            seenState = SUCCESSFUL;
        } else {
            seenState = __sync_val_compare_and_swap(&op->state, ONGOING, FAILED);
        }
    }
    if (seenState == SUCCESSFUL) {
        __sync_bool_compare_and_swap(&op->dest->key, op->removeKey, op->replaceKey);
        __sync_bool_compare_and_swap(&op->dest->op, (Operation*)SETFLAG(op, RELOCATE), (Operation*)SETFLAG(op, NONE));
    }
    bool result = (seenState == SUCCESSFUL);
    if (op->dest == curr) return result;
        __sync_bool_compare_and_swap(&curr->op, (Operation*)SETFLAG(op, RELOCATE), (Operation*)SETFLAG(op, result ? MARK : NONE));
    if (result) {
        if (op->dest == pred) 
        predOp = (Operation*)SETFLAG(op, NONE);
        helpMarked(pred, predOp, curr);
    }
    return result;
}


void help(Node* pred, Operation* predOp, Node* curr, Operation* currOp) {
    if (GETFLAG(currOp) == CHILDCAS)
        helpChildCAS((ChildCASOp*)UNFLAG(currOp), curr);
    else if (GETFLAG(currOp) == RELOCATE)
        helpRelocate((RelocateOp*)UNFLAG(currOp), pred, predOp, curr);
    else if (GETFLAG(currOp) == MARK)
        helpMarked(pred, predOp, curr);
}

int find(int k, Node*& pred, Operation*& predOp, Node*& curr, Operation*& currOp, Node* auxRoot) {
    int result, currKey;
    Node* next, * lastRight;
    Operation* lastRightOp;
    retry:
    result = NOTFOUND_R;
    curr = auxRoot;
    currOp = curr->op;
    if (GETFLAG(currOp) != NONE) {
        if (auxRoot == &root) {
            helpChildCAS((ChildCASOp*)UNFLAG(currOp), curr);
            goto retry;
        } else return ABORT;
    }
    next = curr->right;
    lastRight = curr;
    lastRightOp = currOp;
    while (!ISNULL(next) ) {
        pred = curr;
        predOp = currOp;
        curr = next;
        currOp = curr->op;
        if (GETFLAG(currOp) != NONE) {
            help(pred, predOp, curr, currOp);
            goto retry;
        }
        currKey = curr->key;
        if (k < currKey) {
            result = NOTFOUND_L;
            next = curr->left;
        } else if (k > currKey) {
            result = NOTFOUND_R;
            next = curr->right;
            lastRight = curr;
            lastRightOp = currOp;
        } else {
            result = FOUND;
            break;
        }
    }
    if ((result != FOUND) && (lastRightOp != lastRight->op)) goto retry;
    if (curr->op != currOp) goto retry;
    return result;
}

bool contains(int k) {
    Node* pred, * curr;
    Operation* predOp, * currOp;
    return find(k, pred, predOp, curr, currOp, &root) == FOUND;
}

bool add(int k) {
    Node* pred, * curr, * newNode;
    Operation* predOp, * currOp, * casOp;
    int result;
    while (true) {
        result = find(k, pred, predOp, curr, currOp, &root);
        if (result == FOUND) return false;
        newNode = new Node(k);
        bool isLeft = (result == NOTFOUND_L);
        Node* old = isLeft ? curr->left : curr->right;
        casOp = new ChildCASOp(isLeft, old, newNode);
        if (__sync_bool_compare_and_swap(&curr->op, currOp, (Operation*)SETFLAG(casOp, CHILDCAS))) {
            helpChildCAS((ChildCASOp*)casOp, curr);
            return true;
        }
    }
}


bool remove(int k) {
    Node* pred, * curr, * replace;
    Operation* predOp, * currOp, * replaceOp, * relocOp;
    while (true) {
        if (find(k, pred, predOp, curr, currOp, &root) != FOUND) return false;
        if (ISNULL(curr->right) || ISNULL(curr->left)) {
            // Node has < 2 children
            if (__sync_bool_compare_and_swap(&curr->op, currOp, (Operation*)SETFLAG(currOp, MARK))) {
                helpMarked(pred, predOp, curr);
                return true;
            }
        } else {
        // Node has 2 children
            if ((find(k, pred, predOp, replace, replaceOp, curr) == ABORT) || (curr->op != currOp)) continue;
                relocOp = new RelocateOp(curr, currOp, k, replace->key);
            if (__sync_bool_compare_and_swap(&replace->op, replaceOp, (Operation*)SETFLAG(relocOp, RELOCATE))) {
                if (helpRelocate((RelocateOp *)relocOp, pred, predOp, replace)) return true;
            }
        }
    }
}



class runner{
public:
	vector<time_t> waiting_time;
	runner(int n,int m,int l1){
		N=n;
	    M=m;
	    L1=l1;
	    File_Filter = fopen("CLH-Lock.txt","w");
	    // File.open("Hash-table.txt");
	    // hash=new HashTable<int,int>();
		waiting_time=vector<time_t>(n);
		// cout<<N<<"\t"<<M<<endl;
	}
	~runner(){
		fclose(File_Filter);
		// File.close();
	}

	void proc(int id){

		default_random_engine generator1,generator2,generator3;
		exponential_distribution<float> dis1(1.0/L1);
        bernoulli_distribution dist2(0.9);
        bernoulli_distribution dist3(0.7); 
		for(int i=0;i<M;i++){
			auto start = chrono::system_clock::now();
			time_t reqEnterTime = chrono::system_clock::to_time_t(start);
			// fprintf(File_Filter,"%dth CS request to Hash at %s by thread %d\n",i+1,getTimeinhr(reqEnterTime).c_str(),id+1);
			// File << i+1 <<"th CS Entry Request at "<<getTimeinhr(reqEnterTime)<<" by thread "<<id+1<<" (mesg 1)"<<endl;

			if(dist2(generator2)){
				if(dist3(generator3))   
                    add(rand()%100);
				else
					remove(rand()%100);
			}
			else{
                contains(rand()%100);
			}
			auto end = chrono::system_clock::now();
			time_t actEnterTime=chrono::system_clock::to_time_t(end);
			auto elapsed=chrono::duration_cast<chrono::microseconds>(end - start);
			waiting_time[id] += elapsed.count();
		// max_waiting_writers=max(max_waiting_writers,elapsed.count());
		// fprintf(File_Filter,"%dth complete in hash at %s by thread %d\n",i+1,getTimeinhr(actEnterTime).c_str(),id+1);
		// File << i+1 <<"th CS Entry at "<<getTimeinhr(reqEnterTime)<<" by thread "<<id+1<<" (mesg 2)"<<endl;
		// usleep(dis1(generator1)*1000);
			// cout<<i<<endl;
		}
	}
	string getTimeinhr(time_t inp_time)
	{
		struct tm* format;
		format = localtime(&inp_time);
		char out_time[9];
		sprintf(out_time,"%.2d:%.2d:%.2d",format->tm_hour,format->tm_min,format->tm_sec);
		return out_time;
	}
private:
	// HashTable<int,int> *hash;
	FILE * File_Filter;
	ofstream File;
	int N,M,L1;
};
int main()
{
	// ifstream input;
	// input.open("inp-params.txt");//get inputs from file
	int NM=1000000;
	for( int N=1;N<=512;N=N*2)
	{	
		int M=NM/N,L1=5;
		// cout<<NM<<"\t"<<N<<"\t"<<M<<endl;
		// input>>N>>M>>L1;
		thread th[N];
		runner * runPtr= new runner(N,M,L1);
		auto start = chrono::system_clock::now();
		for(int i=0;i<N;i++)
		{
			th[i]=thread(&runner::proc, runPtr,i);
		}
		for(int i = 0; i < N; ++i)
		{
			th[i].join();
		}
		auto end = chrono::system_clock::now();
		time_t tot_time=0;
		for(int i=0;i<N;i++)
		{
			tot_time+=runPtr->waiting_time[i];
		}
		auto elapsed=chrono::duration_cast<chrono::microseconds>(end - start);
		// waiting_time[id] += elapsed.count();
		printf("Total wait time to enter for Hash-table with %d threads is %ld microseconds\n",N,tot_time);
		printf("elapsed is %ld microseconds\n",elapsed.count());
	}// printf("Average wait time to exit for MCS lock is %ld\n",runPtr->waiting_time_exit/(N*M));
	return 0;
}