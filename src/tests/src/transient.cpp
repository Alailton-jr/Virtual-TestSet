
#include "transient.hpp"

#include "raw_socket.hpp"

#include <fstream>
#include <sstream>

std::vector<std::vector<int32_t>> getDataFromCsv(const std::string path){

    std::vector<std::vector<int32_t>> data;

    std::ifstream file(path);

    if (!file.is_open()) {
        return {};
    }

    std::string line;




}

std::vector<uint32_t> getTransientData(transient_config* conf){
    
    if (conf->fileName.empty()){
        conf->error = 1;
        return {};
    }
    


} 


void* run_transient_test(void* arg){

    auto conf = reinterpret_cast<transient_config*> (arg);

    conf->running = 1;
    conf->stop = 0;

    RawSocket rawSocket;
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    
    
    
    // Get Base Packet



    return nullptr;
}







