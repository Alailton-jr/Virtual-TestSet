
#include "transient.hpp"

#include "raw_socket.hpp"
#include "signal_processing.hpp"
#include "timers.hpp"
#include "tests.hpp"
#include <time.h>

#include <fstream>
#include <sstream>

std::vector<uint8_t>* digital_input;


struct transient_plan{
    void (*_execute)(transient_plan* plan);
    void execute(){
        _execute(this);
    }

    std::vector<std::vector<int32_t>>* buffer;
    Sv_packet* sv_info;
    RawSocket* socket;

    uint8_t loop_flag;
    uint8_t interval_flag;

    double interval;
    int* stop;

    double time_started;
    double time_ended;
};



std::vector<std::vector<double>> getDataFromCsv(const std::string path){

    std::vector<std::vector<double>> data;

    std::ifstream file(path);

    if (!file.is_open()) {
        return {};
    }

    std::string line;
    std::getline(file, line);
    while (std::getline(file, line)){
        std::vector<double> row;
        std::stringstream ss(line);
        std::string cell;
        while (std::getline(ss, cell, ',')){
            row.push_back(std::stod(cell));
        }
        data.push_back(row);
    }

    std::vector<std::vector<double>> transposed_data(data[0].size(), std::vector<double>(data.size()));
    for (size_t i = 0; i < data.size(); ++i) {
        for (size_t j = 0; j < data[i].size(); ++j) {
            transposed_data[j][i] = data[i][j];
        }
    }
    return transposed_data;
}

std::vector<std::vector<int32_t>> getTransientData(transient_config* conf){
    
    if (conf->fileName.empty()){
        conf->error = 1;
    }

    std::vector<std::vector<double>> data = getDataFromCsv(conf->fileName);
    if (data.size() < 1) {
        std::cout << "File Not Found" << std::endl;
        return{};
    }

    data = resample(data, conf->file_data_fs, conf->sv_config.smpRate);

    std::vector<std::vector<int32_t>> res;
    res.resize(conf->sv_config.noChannels);

    for (auto& pos : conf->channelConfig){
        // pos[0] -> Channel
        // pos[1] -> csv data column
        int n_channel = pos[0];
        int n_data = pos[1];

        std::vector<int32_t> channel_data;
        for (int j = 0; j < data[n_data].size(); j++){
            channel_data.push_back(static_cast<int32_t>(data[n_data][j] * conf->scale[n_channel]));
        }
        res[n_channel] = channel_data;
    }

    return res;
}

int updatePkt(std::vector<std::vector<int32_t>>* buffer, Sv_packet* pkt_info, int& idx, int& smpCount){

    int restartbuffer = 0;
    for (int num = 0; num < pkt_info->noAsdu; num++){

        pkt_info->base_pkt[pkt_info->smpCnt_pos[num]] = (smpCount >> 8) & 0xFF;
        pkt_info->base_pkt[pkt_info->smpCnt_pos[num]+1] = (smpCount) & 0xFF;

        for (int cn = 0; cn < pkt_info->noChannels; cn++){
            if ((*buffer)[cn].empty()) continue;
            pkt_info->base_pkt[pkt_info->data_pos[num]     + cn*8] = ((*buffer)[cn][idx] >> 24) & 0xFF;
            pkt_info->base_pkt[pkt_info->data_pos[num] + 1 + cn*8] = ((*buffer)[cn][idx] >> 16) & 0xFF;
            pkt_info->base_pkt[pkt_info->data_pos[num] + 2 + cn*8] = ((*buffer)[cn][idx] >> 8) & 0xFF;
            pkt_info->base_pkt[pkt_info->data_pos[num] + 3 + cn*8] = ((*buffer)[cn][idx]) & 0xFF;
            restartbuffer = -cn;
        }
        idx = idx + 1;
        smpCount = smpCount + 1;

        if (smpCount >= pkt_info->smpRate){
            smpCount = 0;
        }
        if (idx >= (*buffer)[-restartbuffer].size()){
            idx = 0;
            restartbuffer = 1;
        }
    }
    return restartbuffer > 0;
}

void simple_replay(transient_plan* plan){

    Timer timer;
    struct timespec t_ini, t_end, t0, t1;

    long waitPeriod = (long)(1e9/plan->sv_info->smpRate);

    clock_gettime(CLOCK_REALTIME, &t_end);
    clock_gettime(CLOCK_MONOTONIC, &t_ini);

    if (t_end.tv_nsec > 5e8){
        t_ini.tv_sec += 2;
    }else{
        t_ini.tv_sec += 1;
    }
    t_ini.tv_nsec = (t_ini.tv_nsec - t_end.tv_nsec);
    if (t_ini.tv_nsec < 0) t_ini.tv_nsec = 1e9 - t_ini.tv_nsec;

    int buffer_idx = 0;
    int smpCount = 0;
    ssize_t sizeSented = 0;
    long nPkts = 0;
    

    updatePkt(plan->buffer, plan->sv_info, buffer_idx, smpCount);
    timer.start_period(t_ini);
    timer.wait_period(waitPeriod);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while ((!*plan->stop) && (!(*digital_input)[0]) || nPkts < 4800){
        sizeSented = sendmsg(plan->socket->socket_id, &plan->socket->msg_hdr, 0);
        if (updatePkt(plan->buffer, plan->sv_info, buffer_idx, smpCount)){
            break;
        }
        nPkts++;
        timer.wait_period(waitPeriod);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    plan->time_started = t0.tv_sec + t0.tv_nsec * 1e-9;
    plan->time_ended = t1.tv_sec + t1.tv_nsec * 1e-9;

    return;
}

void loop_replay(transient_plan* plan){

    Timer timer;
    struct timespec t_ini, t_end, t0, t1;

    long waitPeriod = (long)(1e9/plan->sv_info->smpRate);

    clock_gettime(CLOCK_REALTIME, &t_end);
    clock_gettime(CLOCK_MONOTONIC, &t_ini);

    if (t_end.tv_nsec > 5e8){
        t_ini.tv_sec += 2;
    }else{
        t_ini.tv_sec += 1;
    }
    t_ini.tv_nsec = (t_ini.tv_nsec - t_end.tv_nsec);
    if (t_ini.tv_nsec < 0) t_ini.tv_nsec = 1e9 - t_ini.tv_nsec;

    int buffer_idx = 0;
    int smpCount = 0;
    ssize_t sizeSented = 0;

    int n_stop = 0;

    updatePkt(plan->buffer, plan->sv_info, buffer_idx, smpCount);
    timer.start_period(t_ini);
    timer.wait_period(waitPeriod);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while ((!*plan->stop) && (!(*digital_input)[0])){
        sizeSented = sendmsg(plan->socket->socket_id, &plan->socket->msg_hdr, 0);
        updatePkt(plan->buffer, plan->sv_info, buffer_idx, smpCount);
        timer.wait_period(waitPeriod);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    plan->time_started = t0.tv_sec + t0.tv_nsec * 1e-9;
    plan->time_ended = t1.tv_sec + t1.tv_nsec * 1e-9;

    return;
}

void interval_replay(transient_plan* plan){

}


transient_plan create_plan(transient_config* conf, std::vector<std::vector<int32_t>>* data, Sv_packet* sv_info, RawSocket *socket){
    
    transient_plan plan;
    plan.buffer = data;
    plan.loop_flag = conf->loop_flag;
    plan.interval_flag = conf->interval_flag;
    plan.interval = conf->interval;
    plan.stop = &conf->stop;
    plan.sv_info = sv_info;
    plan.socket = socket;

    if (plan.loop_flag){
        plan._execute = &loop_replay;
    } else if (plan.interval_flag){
        plan._execute = &interval_replay;
    } else {
        plan._execute = &simple_replay;
    }

    return plan;
}


void* run_transient_test(void* arg){

    auto conf = reinterpret_cast<transient_config*> (arg);

    conf->running = 1;
    conf->stop = 0;

    //Only for test
    digital_input = conf->digital_input;
    (*digital_input)[0] = 0;

    std::vector<std::vector<int32_t>> buffer = getTransientData(conf);
    if (buffer.empty()){
        conf->running = 0;
        return nullptr;
    }
    Sv_packet sv_info = get_sampledValue_pkt_info(conf->sv_config);
    transient_plan plan = create_plan(conf, &buffer, &sv_info, conf->socket);

    plan.socket->iov.iov_base = (void*)sv_info.base_pkt.data();
    plan.socket->iov.iov_len = sv_info.base_pkt.size();

    try{
        plan.execute(); 
    }catch(...){}

    // std::cout << "" << plan.time_ended - plan.time_started << std::endl;

    if(conf->trip_time){
        *conf->trip_time = plan.time_ended - plan.time_started;
    }

    conf->running = 0;
    return nullptr;
}







