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

struct SongContext{
    void* graph;
    void* data;
};

class StopW {
    std::chrono::steady_clock::time_point time_begin;
public:
    StopW() {
        time_begin = std::chrono::steady_clock::now();
    }

    float getElapsedTimeMicro() {
        std::chrono::steady_clock::time_point time_end = std::chrono::steady_clock::now();
        return (std::chrono::duration_cast<std::chrono::microseconds>(time_end - time_begin).count());
    }

    void reset() {
        time_begin = std::chrono::steady_clock::now();
    }

};
static inline std::vector<StringRef> split(std::string const &str, char delimiter = ' ') {
    std::vector<StringRef> result;

    enum State {
        inSpace, inToken
    };

    State state = inSpace;
    char const *pTokenBegin = 0;    // Init to satisfy compiler.
    for (const char &it : str) {
        State const newState = (it == delimiter ? inSpace : inToken);
        if (newState != state) {
            switch (newState) {
                case inSpace:
                    result.emplace_back(pTokenBegin, &it - pTokenBegin);
                    break;
                case inToken:
                    pTokenBegin = &it;
            }
        }
        state = newState;
    }
    if (state == inToken) {
        result.emplace_back(pTokenBegin, &*str.end() - pTokenBegin);
    }
    return result;
}

static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
        return !std::isspace(ch);
    }));
}

static inline std::string ltrim_copy(std::string s) {
    ltrim(s);
    return s;
}

int topk = 0;
int display_topk = 1;
int build_idx_offset = 0;
int query_idx_offset = 0;



double build_graph_time = 0;
StopW my_timer;


void flush_add_buffer(
        std::vector<std::pair<idx_t,std::vector<std::pair<int,value_t>>>>& add_buffer,
        GraphWrapper* graph){
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



extern "C"{
void build_mobius_index(double* dense_mat,int row,int dim,double mobius_pow = 2.0){
    std::unique_ptr<Data> data;
    std::unique_ptr<Data> data_original;
    std::unique_ptr<GraphWrapper> graph; 
    int topk = 0;
    int display_topk = 1;
    int build_idx_offset = 0;
    int query_idx_offset = 0;
		
    ++row;
	data = std::unique_ptr<Data>(new Data(row,dim));
    graph = std::unique_ptr<GraphWrapper>(new FixedDegreeGraph<3>(data.get())); 

    std::vector<std::pair<idx_t,std::vector<std::pair<int,value_t>>>> add_buffer;
    
    ((FixedDegreeGraph<3>*)graph.get())->get_data()->mobius_pow = mobius_pow;
    data_original = std::unique_ptr<Data>(new Data(row,dim));

    std::vector<std::pair<int,value_t>> dummy_mobius_point;
    for(int i = 0;i < dim;++i)
        dummy_mobius_point.push_back(std::make_pair(i,0));
	
    //idx += build_idx_offset;
    
    for(int i = 0;i < row - 1;++i){

        std::vector<std::pair<int,value_t>> point;
        point.reserve(dim);
        for(int j = 0;j < dim;++j)
            point.push_back(std::make_pair(j,dense_mat[i * dim + j]));

        data_original->add(i,point);
        data->add_mobius(i,point);
        if(i < 1000){
        //    my_timer.reset();
            graph->add_vertex(i,point);
        //    build_graph_time += my_timer.getElapsedTimeMicro();
        }else{
            add_buffer.push_back(std::make_pair(i,point));
        }
        if(add_buffer.size() >= 1000000)
            flush_add_buffer(add_buffer,graph.get());
    }
    flush_add_buffer(add_buffer,graph.get());
    //printf("Graph construction time %f\n",build_graph_time);
    graph->add_vertex(row - 1,dummy_mobius_point);
    data.swap(data_original);
    //fprintf(stderr,"Building: explored %f (%lld/%d)\n",1.0 * graph->total_explore_cnt / graph->total_explore_times,graph->total_explore_cnt,graph->total_explore_times);
    //fprintf(stderr,"Writing the graph and data...");    
    data->dump();
    //fprintf(stderr,"...");    
    graph->dump();
    //fprintf(stderr,"done\n");    
}

void init_ipwrap_index32(int row,int dim,double max_ip_norm,SongContext* song_context){
    Data* data;
    GraphWrapper* graph; 
    int topk = 0;
    int display_topk = 1;
    int build_idx_offset = 0;
    int query_idx_offset = 0;
		
	data = new Data(row,dim);
    graph = new FixedDegreeGraph<4>(data); 
	((FixedDegreeGraph<4>*)graph)->get_data()->max_ip_norm = max_ip_norm;
	((FixedDegreeGraph<4>*)graph)->get_data()->max_ip_norm2 = max_ip_norm * max_ip_norm;
    song_context->data = data;
    song_context->graph = graph;
}

void release_context(SongContext* song_context){
    delete (Data*)(song_context->data);
    delete (GraphWrapper*)(song_context->graph);
}

void set_construct_pq_size(SongContext* song_context,int size){
    GraphWrapper* graph = (GraphWrapper*)(song_context->graph);
    graph->set_construct_pq_size(size);
}

void insert_ipwrap_index32(float* dense_mat,int row,int dim,int offset,SongContext* song_context){
    int topk = 0;
    int display_topk = 1;
    int build_idx_offset = 0;
    int query_idx_offset = 0;
		
	Data* data = (Data*)(song_context->data);
    GraphWrapper* graph = (GraphWrapper*)(song_context->graph);

    std::vector<std::pair<idx_t,std::vector<std::pair<int,value_t>>>> add_buffer;
    for(int i = 0;i < row;++i){
        std::vector<std::pair<int,value_t>> point;
        point.reserve(dim);
        for(int j = 0;j < dim;++j)
            point.push_back(std::make_pair(j,dense_mat[i * dim + j]));
        data->add(i + offset,point);
        if(i + offset < 100){
        //    my_timer.reset();
            graph->add_vertex(i + offset,point);
        //    build_graph_time += my_timer.getElapsedTimeMicro();
        }else{
            add_buffer.push_back(std::make_pair(i + offset,point));
        }
        if(add_buffer.size() >= 1000000)
            flush_add_buffer(add_buffer,graph);
    }
    flush_add_buffer(add_buffer,graph);
}

void build_ipwrap_index32(float* dense_mat,int row,int dim,double max_ip_norm,int suffix){
    std::unique_ptr<Data> data;
    std::unique_ptr<GraphWrapper> graph; 
    int topk = 0;
    int display_topk = 1;
    int build_idx_offset = 0;
    int query_idx_offset = 0;
		
	data = std::unique_ptr<Data>(new Data(row,dim));
    graph = std::unique_ptr<GraphWrapper>(new FixedDegreeGraph<4>(data.get())); 
	((FixedDegreeGraph<4>*)graph.get())->get_data()->max_ip_norm = max_ip_norm;
	((FixedDegreeGraph<4>*)graph.get())->get_data()->max_ip_norm2 = max_ip_norm * max_ip_norm;

    std::vector<std::pair<idx_t,std::vector<std::pair<int,value_t>>>> add_buffer;
    
    for(int i = 0;i < row;++i){
        std::vector<std::pair<int,value_t>> point;
        point.reserve(dim);
        for(int j = 0;j < dim;++j)
            point.push_back(std::make_pair(j,dense_mat[i * dim + j]));
        data->add(i,point);
        if(i < 1000){
        //    my_timer.reset();
            graph->add_vertex(i,point);
        //    build_graph_time += my_timer.getElapsedTimeMicro();
        }else{
            add_buffer.push_back(std::make_pair(i,point));
        }
        if(add_buffer.size() >= 1000000)
            flush_add_buffer(add_buffer,graph.get());
    }
    flush_add_buffer(add_buffer,graph.get());
    //printf("Graph construction time %f\n",build_graph_time);
    //fprintf(stderr,"Building: explored %f (%lld/%d)\n",1.0 * graph->total_explore_cnt / graph->total_explore_times,graph->total_explore_cnt,graph->total_explore_times);
    //fprintf(stderr,"Writing the graph and data...");    
    data->dump("bfsg.data." + std::to_string(suffix));
    //fprintf(stderr,"...");    
    graph->dump("bfsg.graph." + std::to_string(suffix));
    //fprintf(stderr,"done\n");    
}

void save_ipwrap_index_prefix(SongContext* song_context,const char* prefix){
    std::string str = std::string(prefix);
	Data* data = (Data*)(song_context->data);
    GraphWrapper* graph = (GraphWrapper*)(song_context->graph); 

    data->dump(str + ".data");
    graph->dump(str + ".graph");
}


void load_mobius_index(int row,int dim,SongContext* song_context){
    int topk = 0;
    int display_topk = 1;
    int build_idx_offset = 0;
    int query_idx_offset = 0;
		
    ++row;
	Data* data = new Data(row,dim);
    GraphWrapper* graph = new FixedDegreeGraph<1>(data); 

    std::vector<std::pair<idx_t,std::vector<std::pair<int,value_t>>>> add_buffer;

	
    //idx += build_idx_offset;

    data->load();
    graph->load();
    ((FixedDegreeGraph<1>*)graph)->search_start_point = row - 1;
    ((FixedDegreeGraph<1>*)graph)->ignore_startpoint = true;

    song_context->graph = graph;
    song_context->data = data;
}

void load_ipwrap_index(int row,int dim,SongContext* song_context,int suffix){
    int topk = 0;
    int display_topk = 1;
    int build_idx_offset = 0;
    int query_idx_offset = 0;
		
	Data* data = new Data(row,dim);
    GraphWrapper* graph = new FixedDegreeGraph<1>(data); 

    std::vector<std::pair<idx_t,std::vector<std::pair<int,value_t>>>> add_buffer;

	
    //idx += build_idx_offset;

    data->load("bfsg.data." + std::to_string(suffix));
    graph->load("bfsg.graph." + std::to_string(suffix));

    song_context->graph = graph;
    song_context->data = data;
}

void load_ipwrap_index_prefix(int row,int dim,SongContext* song_context,const char* prefix){
    std::string str = std::string(prefix);
		
	Data* data = new Data(row,dim);
    GraphWrapper* graph = new FixedDegreeGraph<1>(data); 

    std::vector<std::pair<idx_t,std::vector<std::pair<int,value_t>>>> add_buffer;

	
    //idx += build_idx_offset;

    data->load(str + ".data");
    graph->load(str + ".graph");

    song_context->graph = graph;
    song_context->data = data;
}

void search_mobius_index(double* dense_vec,int dim,int search_budget,int return_k, SongContext* song_context,idx_t* ret_id,double* ret_score){
    int topk = 0;
    int display_topk = 1;
    int build_idx_offset = 0;
    int query_idx_offset = 0;
		
	Data* data = reinterpret_cast<Data*>(song_context->data);
    GraphWrapper* graph = reinterpret_cast<GraphWrapper*>(song_context->graph);


    std::vector<std::pair<int,value_t>> point;
    point.reserve(dim);
    for(int j = 0;j < dim;++j)
        point.push_back(std::make_pair(j,dense_vec[j]));
    std::vector<idx_t> topN;
    std::vector<double> score;
    graph->search_top_k_with_score(point,search_budget,topN,score);
    for(int i = 0;i < topN.size() && i < return_k;++i){
//        printf("%d: (%zu, %f)\n",i,topN[i],score[i]);
        ret_id[i] = topN[i];
        ret_score[i] = score[i];
    }
}

void search_ipwrap_index32(float* dense_vec,int dim,int search_budget,int return_k, SongContext* song_context,idx_t* ret_id,double* ret_score){
    int topk = 0;
    int display_topk = 1;
    int build_idx_offset = 0;
    int query_idx_offset = 0;
		
	Data* data = reinterpret_cast<Data*>(song_context->data);
    GraphWrapper* graph = reinterpret_cast<GraphWrapper*>(song_context->graph);


    std::vector<std::pair<int,value_t>> point;
    point.reserve(dim);
    for(int j = 0;j < dim;++j)
        point.push_back(std::make_pair(j,dense_vec[j]));
    std::vector<idx_t> topN;
    std::vector<double> score;
    graph->search_top_k_with_score(point,search_budget,topN,score);
    for(int i = 0;i < topN.size() && i < return_k;++i){
//        printf("%d: (%zu, %f)\n",i,topN[i],score[i]);
        ret_id[i] = topN[i];
        ret_score[i] = score[i];
    }
}

} // extern "C"

