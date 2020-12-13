#include<iostream>
#include<math.h>
#include<sys/time.h> 
#include<bits/stdc++.h>
#include<thread>
#include<chrono>
#include<unistd.h>
#include<random>
#include "HNode.h"

using namespace std;

class runner{
public:
	vector<time_t> waiting_time;
	runner(int n,int m,int l1){
		N=n;
	    M=m;
	    L1=l1;
	    File_Filter = fopen("CLH-Lock.txt","w");
	    // File.open("Hash-table.txt");
	    hash=new HashTable<int,int>();
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
        bernoulli_distribution dist2(0.1);
        bernoulli_distribution dist3(0.7); 
		for(int i=0;i<M;i++){
			auto start = chrono::system_clock::now();
			time_t reqEnterTime = chrono::system_clock::to_time_t(start);
			// fprintf(File_Filter,"%dth CS request to Hash at %s by thread %d\n",i+1,getTimeinhr(reqEnterTime).c_str(),id+1);
			// File << i+1 <<"th CS Entry Request at "<<getTimeinhr(reqEnterTime)<<" by thread "<<id+1<<" (mesg 1)"<<endl;

			if(dist2(generator2)){
				if(dist3(generator3))   
					hash->insert(rand()%100,rand()%100);
				else
					hash->remove(rand()%100,rand()%100);
			}
			else{
				hash->contains(rand()%100);
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
	HashTable<int,int> *hash;
	FILE * File_Filter;
	ofstream File;
	int N,M,L1;
};
int main()
{
	// ifstream input;
	// input.open("inp-params.txt");//get inputs from file
	int NM=1048576;
	for( int N=2;N<=512;N=N*2)
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