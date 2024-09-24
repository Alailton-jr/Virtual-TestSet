
#include "transient.hpp"

#include "raw_socket.hpp"
#include "signal_processing.hpp"

#include <fstream>
#include <sstream>

struct transient_plan{
    void (*_execute)(transient_plan* plan);
    void execute(){
        _execute(this);
    }


    std::vector<std::vector<int32_t>> buffer;
    uint8_t loop_flag;
    uint8_t interval_flag;

    double interval;
    uint8_t stop;
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

std::vector<int32_t> getTransientData(transient_config* conf){
    
    if (conf->fileName.empty()){
        conf->error = 1;
    }

    std::vector<std::vector<double>> data = getDataFromCsv(conf->fileName);
    data = resample(data, conf->file_data_fs, conf->sv_config.smpRate);

    std::vector<std::vector<int32_t>> res;

    for (int i = 0; i < conf->noChannels; i++){
        std::vector<int32_t> channel_data;
        for (int j = 0; j < data[i].size(); j++){
            channel_data.push_back(static_cast<int32_t>(data[i][j] * conf->scale[i]));
        }
        res.push_back(channel_data);
    }
}


transient_plan create_plan(transient_config* conf, std::vector<std::vector<int32_t>> data){
    
    transient_plan plan;
    plan.buffer = data;
    plan.loop_flag = conf->loop_flag;
    plan.interval_flag = conf->interval_flag;
    plan.interval = conf->interval;

    if (plan.loop_flag){
        plan._execute = loop_replay;
    } else if (plan.interval_flag){
        plan._execute = interval_replay;
    } else {
        plan._execute = simple_replay;
    }

    return plan;
}

void simple_replay(transient_plan* plan){
 
}

void loop_replay(transient_plan* plan){

}

void interval_replay(transient_plan* plan){

}



void* run_transient_test(void* arg){

    auto conf = reinterpret_cast<transient_config*> (arg);

    conf->running = 1;
    conf->stop = 0;


    auto data = getTransientData(conf);



    
    
    
    // Get Base Packet



    return nullptr;
}







