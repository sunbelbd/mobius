#include<stdio.h>
#include<string.h>
#include <iostream>
#include <fstream>
#include <queue>
#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include"logger.h"
#include"parser_dense.h"
#include"data.h"
#include"graph.h"
#include<stdlib.h>
#include<memory>
#include<vector>
#include<functional>

class StopW {
    std::chrono::steady_clock::time_point time_begin;
public:
    StopW() {
        time_begin = std::chrono::steady_clock::now();
    }

    double getElapsedTimeMicro() {
        std::chrono::steady_clock::time_point time_end = std::chrono::steady_clock::now();
        return (std::chrono::duration_cast<std::chrono::microseconds>(time_end - time_begin).count());
    }

    void reset() {
        time_begin = std::chrono::steady_clock::now();
    }

};

std::unique_ptr<Data> data;
std::unique_ptr<Data> data_original;
std::unique_ptr<GraphWrapper> graph; 
int topk = 0;
int display_topk = 1;
int build_idx_offset = 0;
int query_idx_offset = 0;

double build_graph_time = 0;
int count_query = 0;
StopW my_timer;


std::vector<std::pair<idx_t,std::vector<std::pair<int,value_t>>>> add_buffer;

void flush_add_buffer(){
    my_timer.reset();
    #pragma omp parallel for
    for(int i = 0;i < add_buffer.size();++i){
        auto& idx = add_buffer[i].first;
        auto& point = add_buffer[i].second;
        graph->add_vertex_lock(idx,point);
    }
    add_buffer.clear();
	build_graph_time += my_timer.getElapsedTimeMicro();
}


void build_callback_mobius(idx_t idx,std::vector<std::pair<int,value_t>> point){
	idx += build_idx_offset;
    
	//data->add(idx,point);
    data_original->add(idx,point);
    data->add_mobius(idx,point);
	if(idx < 1000){
		my_timer.reset();
    	graph->add_vertex(idx,point);
		build_graph_time += my_timer.getElapsedTimeMicro();
	}else{
        add_buffer.push_back(std::make_pair(idx,point));
	}
    if(add_buffer.size() >= 1000000)
        flush_add_buffer();
}
void build_callback(idx_t idx,std::vector<std::pair<int,value_t>> point){
	idx += build_idx_offset;
    
	data->add(idx,point);
	if(idx < 100){
		my_timer.reset();
    	graph->add_vertex(idx,point);
		build_graph_time += my_timer.getElapsedTimeMicro();
	}else{
        add_buffer.push_back(std::make_pair(idx,point));
	}
    if(add_buffer.size() >= 1000000)
        flush_add_buffer();
}

StopW stopw;
double total_time = 0;
int count_return = 0;

void query_callback(idx_t idx,std::vector<std::pair<int,value_t>> point){
	stopw.reset();
    std::vector<idx_t> result;
	graph->measures.distance_cnt = 0;
    graph->search_top_k(point,topk,result);
	double time_used = stopw.getElapsedTimeMicro();
	total_time += time_used;
	++count_query;
	count_return += graph->measures.distance_cnt;

    for(int i = 0;i < result.size() && i < display_topk;++i)
        printf("%zu ",result[i] + query_idx_offset);
    printf("\n");
}

void usage(char** argv){
    printf("Usage: %s <build/test> <build_data> <query_data> <search_top_k> <row> <dim> <return_top_k> [mobius_pow]\n",argv[0]);
}

int main(int argc,char** argv){
    if(argc != 8 && argc != 9){
        usage(argv);
        return 1;
    }

    std::string train_path = std::string(argv[2]);
    std::string query_path = std::string(argv[3]);
	topk = atoi(argv[4]);
	size_t row = atoll(argv[5]);
	int dim = atoi(argv[6]);
	display_topk = atoi(argv[7]);

	double mobius_pow = 2.0;
	if(argc >= 9){
		mobius_pow = atof(argv[8]);
	}

    std::string mode = std::string(argv[1]);
	std::string dist_type = "mobius";
	if(dist_type == "mobius")
		++row;
	data = std::unique_ptr<Data>(new Data(row,dim));
	if(dist_type == "mobius"){
		if(mode == "build"){
			graph = std::unique_ptr<GraphWrapper>(new FixedDegreeGraph<3>(data.get())); 
			if(argc == 7)
				((FixedDegreeGraph<3>*)graph.get())->get_data()->mobius_pow = atof(argv[6]);
			data_original = std::unique_ptr<Data>(new Data(row,dim));
		}else{
			graph = std::unique_ptr<GraphWrapper>(new FixedDegreeGraph<1>(data.get())); 
			//((FixedDegreeGraph<1>*)graph.get())->SEARCH_START_POINT = row - 1;
		}
	}else{
		usage(argv);
		return 1;
	}
    // topk = atoi(argv[4]);
    if(mode == "build"){
		std::vector<std::pair<int,value_t>> dummy_mobius_point;
		if(dist_type == "mobius"){
			for(int i = 0;i < dim;++i)
				dummy_mobius_point.push_back(std::make_pair(i,0));
		}
		if(dist_type == "mobius"){
        	std::unique_ptr<ParserDense> build_parser(new ParserDense(train_path.c_str(),build_callback_mobius));
		}else{
			usage(argv);
			return 1;
		}
        flush_add_buffer();
		printf("Graph construction time %f\n",build_graph_time);
		if(dist_type == "mobius"){
			graph->add_vertex(row - 1,dummy_mobius_point);
			data.swap(data_original);
		}
        //fprintf(stderr,"Building: explored %f (%lld/%d)\n",1.0 * graph->total_explore_cnt / graph->total_explore_times,graph->total_explore_cnt,graph->total_explore_times);
        fprintf(stderr,"Writing the graph and data...");    
        data->dump();
        fprintf(stderr,"...");    
        graph->dump();
        fprintf(stderr,"done\n");    
    }else if(mode == "test"){
        fprintf(stderr,"Loading the graph and data...");    
        data->load();
        fprintf(stderr,"...");    
        graph->load();
        fprintf(stderr,"done\n");    
		if(dist_type == "mobius"){
			((FixedDegreeGraph<1>*)graph.get())->search_start_point = row - 1;
			((FixedDegreeGraph<1>*)graph.get())->ignore_startpoint = false;
		//	query_idx_offset = -1;
		}
        std::unique_ptr<ParserDense> query_parser(new ParserDense(query_path.c_str(),query_callback));
		
		double time_us_per_query = total_time/ count_query;
		double queries_per_second = 1000000.0/time_us_per_query;
		
		double ave_count_return = count_return * 1. / count_query;
		double ave_return_percent = ave_count_return / row;
		fprintf(stderr,"queries per second %f, average return_percent %f%%\n",queries_per_second,ave_return_percent * 100.0);
    }else{
        usage(argv);
        return 1;
    }

    return 0;
}
