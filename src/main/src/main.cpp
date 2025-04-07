
#include "main.hpp"
#include "raw_socket.hpp"

#include "Protocols.hpp"
#include "sv_sender.hpp"

#include "tests.hpp"
#include <time.h>

// Real 

TCPServer::TCPServer(int port) : port(port), isRunning(false), serverSocket(-1) {}

TCPServer::~TCPServer() {
    stop();
}

void TCPServer::start() {
    isRunning = true;
    // std::thread serverThread(&TCPServer::run, this);
    // serverThread.detach();
    this->run();
}

void TCPServer::stop() {
    isRunning = false;
    if (serverSocket != -1) {
        close(serverSocket);
        serverSocket = -1;
    }
    for (auto& t : clientThreads) {
        if (t.joinable()) {
            t.join();
        }
    }
    clientThreads.clear();
}

void TCPServer::run() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Failed to create socket.\n";
        return;
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        std::cerr << "Setsockopt failed: " << strerror(errno) << "\n";
        close(serverSocket);
        return;
    }

    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Binding failed.\n";
        return;
    }

    if (listen(serverSocket, 5) == -1) {
        std::cerr << "Listening failed.\n";
        return;
    }

    std::cout << "Server listening on port " << port << std::endl;

    while (isRunning) {
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == -1) {
            if (isRunning) {
                std::cerr << "Accept failed.\n";
            }
            continue;
        }
        clientThreads.emplace_back(&TCPServer::handleClient, this, clientSocket);
    }
}

// Save any file sent by the client
std::string save_file2(std::string fileName, const char* buffer, int bytesReceived) {
    std::ofstream file("files"+fileName, std::ios::app);
    if (file.is_open()) {
        file.write(buffer, bytesReceived);
        file.close();
        std::cout << "All data received and saved to file: " << fileName << std::endl;
        return "0";
    } else {
        std::cerr << "Unable to open file for writing.\n";
        return "Unable to open file for writing.";
    }
}

int32_t save_file(int* clientSocket, char* buffer, int maxBufferSize) {
    char fileName[256];
    memset(fileName, 0, sizeof(fileName));
    send(*clientSocket, "OK", 2, 0);
    int bytesReceived = recv(*clientSocket, fileName, sizeof(fileName), 0);
    if (bytesReceived <= 0) {
        return -1;
    }
    send(*clientSocket, "OK", 2, 0);
    std::string fileNameStr(fileName);
    std::cout << "File name received: " << fileNameStr << std::endl;
    int bytesReceivedFileSize = recv(*clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceivedFileSize <= 0) {
        return -1;
    }
    send(*clientSocket, "OK", 2, 0);
    int fileSize = atoi(buffer);
    std::cout << "File size received: " << fileSize << std::endl;
    std::ofstream file("files/" + fileNameStr, std::ios::trunc);
    if (file.is_open()) {
        int totalBytesReceived = 0;
        while (totalBytesReceived < fileSize) {
            int bytesReceived = recv(*clientSocket, buffer, maxBufferSize, 0);
            if (bytesReceived <= 0) {
                std::cerr << "Error receiving file data.\n";
                file.close();
                return -1;
            }
            file.write(buffer, bytesReceived);
            totalBytesReceived += bytesReceived;
            send(*clientSocket, "OK", 2, 0);
        }
        file.close();
        std::cout << "All data received and saved to file: " << fileNameStr << std::endl;
        return 0;
    } else {
        std::cerr << "Unable to open file for writing.\n";
        return -1;
    }
}


int save_goose_input_configFile(int* clientSocket, char* buffer, int maxBufferSize) {
    send(*clientSocket, "OK", 2, 0);
    int bytesReceivedFileSize = recv(*clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceivedFileSize <= 0) {
        return -1;
    }
    send(*clientSocket, "OK", 2, 0);
    int fileSize = atoi(buffer);
    std::cout << "File size received: " << fileSize << std::endl;
    std::ofstream file("files/goose_input_config.json", std::ios::trunc);
    if (file.is_open()) {
        int totalBytesReceived = 0;
        while (totalBytesReceived < fileSize) {
            int bytesReceived = recv(*clientSocket, buffer, maxBufferSize, 0);
            if (bytesReceived <= 0) {
                std::cerr << "Error receiving file data.\n";
                file.close();
                return -1;
            }
            file.write(buffer, bytesReceived);
            totalBytesReceived += bytesReceived;
            send(*clientSocket, "OK", 2, 0);
        }
        file.close();
        std::cout << "All data received and saved to file: sniffer_config.json" << std::endl;
        return 0;
    } else {
        std::cerr << "Unable to open file for writing.\n";
        return -1;
    }
}

// Save the transient test configuration file
// std::string save_transient_test_configFile(const char* buffer, int bytesReceived) {
//     std::ofstream file("files/transient_test.json", std::ios::app);
//     if (file.is_open()) {
//         file.write(buffer, bytesReceived);
//         file.close();
//         std::cout << "All data received and saved to file: transient_test.json" << std::endl;
//         return "0";
//     } else {
//         std::cerr << "Unable to open file for writing.\n";
//         return "Unable to open file for writing.";
//     }
// }

int save_transient_test_configFile(int* clientSocket, char* buffer, int maxBufferSize) {
    send(*clientSocket, "OK", 2, 0);
    int bytesReceivedFileSize = recv(*clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceivedFileSize <= 0) {
        return -1;
    }
    send(*clientSocket, "OK", 2, 0);
    int fileSize = atoi(buffer);
    std::cout << "File size received: " << fileSize << std::endl;
    std::ofstream file("files/transient_test.json", std::ios::trunc);
    if (file.is_open()) {
        int totalBytesReceived = 0;
        while (totalBytesReceived < fileSize) {
            int bytesReceived = recv(*clientSocket, buffer, maxBufferSize, 0);
            if (bytesReceived <= 0) {
                std::cerr << "Error receiving file data.\n";
                file.close();
                return -1;
            }
            file.write(buffer, bytesReceived);
            totalBytesReceived += bytesReceived;
            send(*clientSocket, "OK", 2, 0);
        }
        file.close();
        std::cout << "All data received and saved to file: transient_test.json" << std::endl;
        return 0;
    } else {
        std::cerr << "Unable to open file for writing.\n";
        return -1;
    }
}

// Start the transient test
std::string start_transient_test(Tests_Class *testSet) {
    try{
        std::vector<transient_config> conf = get_transient_test_config("files/transient_test.json");
        for (auto& c: conf){
            if (c.fileloaded == 0){
                std::cerr << c.error_msg << std::endl;
                return c.error_msg;
            }
        }
        testSet->start_transient_test(conf);
        std::cout << "Transient test started" << std::endl;
        return "0";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return "Error: " + std::string(e.what());
    }
}

// Stop the transient test
std::string stop_transient_test(Tests_Class *testSet) {
    try{
        testSet->stop_transient_test();
        return "0";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return "Error: " + std::string(e.what());
    }
}

// Return transient test results
std::string get_transient_test_results(Tests_Class *testSet) {
    try{
        if (testSet->transient_tests.size() == 0){
            return "No transient test running";
        }
        std::string results;
        for (int i = 0; i < testSet->transient_tests.size(); i++){
            results += "Test " + std::to_string(i) + ": ";
            results += "File loaded: " + std::to_string(testSet->transient_tests[i].fileloaded) + "\n";
            results += "Running: " + std::to_string(testSet->transient_tests[i].running) + "\n";
            results += "Time started sec: " + std::to_string(testSet->transient_tests[i].time_started.tv_sec) + "\n";
            results += "Time started nsec: " + std::to_string(testSet->transient_tests[i].time_started.tv_nsec) + "\n";
            results += "Time ended sec: " + std::to_string(testSet->transient_tests[i].time_ended.tv_sec) + "\n";
            results += "Time ended nsec: " + std::to_string(testSet->transient_tests[i].time_ended.tv_nsec) + "\n";
            results += "Trip time: " + std::to_string(testSet->transient_tests[i].trip_time) + "\n";
        }
        return results;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return "Error: " + std::string(e.what());
    }
}

// Return if any test is running
std::string get_test_status(Tests_Class *testSet) {
    try{
        if (testSet->is_running() == 1){
            return "1";
        } else {
            return "0";
        }
        std::cout << "Test status checked" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return "Error: " + std::string(e.what());
    }
}

void TCPServer::handleClient(int clientSocket) {
    char buffer[4096];
    // For simplicity, we assume the command is sent as a single message.
    // For file transfers, you might need a loop that reads the command and then the file contents.
    memset(buffer, 0, sizeof(buffer));
    int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceived <= 0) {
        close(clientSocket);
        return;
    }
    
    // Parse command; assume tokens are separated by a space
    std::istringstream iss(std::string(buffer, bytesReceived));
    std::string command;
    iss >> command;

    std::string response = "Unknown command";
    if (command == "SAVE_FILE") {
        int saved = save_file(&clientSocket, buffer, sizeof(buffer));
        if (saved == 0) {
            response = "0";
        } else {
            response = "Failed to save file";
        }
    } else if (command == "SAVE_GOOSE_INPUT_CONFIG") {
        int saved = save_goose_input_configFile(&clientSocket, buffer, sizeof(buffer));
        if (saved == 0) {
            response = "0";
        } else {
            response = "Failed to save transient test config";
        }
    }else if (command == "SAVE_TRANSIENT_CONFIG") {
        int saved = save_transient_test_configFile(&clientSocket, buffer, sizeof(buffer));
        if (saved == 0) {
            response = "0";
        } else {
            response = "Failed to save transient test config";
        }
    } else if (command == "START_TRANSIENT_TEST") {
        response = start_transient_test(&this->testSet);
    } else if (command == "STOP_TRANSIENT_TEST") {
        response = stop_transient_test(&this->testSet);
    } else if (command == "GET_TRANSIENT_RESULTS") {
        response = get_transient_test_results(&this->testSet);
    } else if (command == "GET_STATUS") {
        response = get_test_status(&this->testSet);
    }
    // Send the response back to the client
    send(clientSocket, response.c_str(), response.size(), 0);
    close(clientSocket);
}

// Testing getDataFromCsv
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include <math.h>

void plotIt(std::vector<double> x, std::vector<double> y){

    std::ofstream dataFile("data.txt");

    for (size_t i = 0; i < x.size(); ++i) {
        dataFile << x[i] << " " << y[i] << std::endl;
    }


    dataFile.close();
    system("gnuplot -e 'set terminal pdf; set output \"plot.pdf\"; plot \"data.txt\" with lines'");
    remove("data.txt");
}

void test_sampledValue_Pkt(){

    std::vector<uint8_t> base_pkt;
    
    SampledValue_Config* sv_conf = new SampledValue_Config();

    sv_conf->appID = 0x4000;
    sv_conf->noAsdu = 2;
    sv_conf->svID = "Conprove_MU01";
    sv_conf->smpCnt = 566;
    sv_conf->confRev = 1;
    sv_conf->smpSynch = 0x01;

    // Ethernet
    Protocols::Ethernet eth(sv_conf->srcMac, sv_conf->dstMac);
    auto encoded_eth = eth.getEncoded();
    base_pkt.insert(base_pkt.end(), encoded_eth.begin(), encoded_eth.end());

    // Virtual LAN
    Protocols::Virtual_LAN vlan(sv_conf->vlanId, sv_conf->vlanPcp, sv_conf->vlanDei);
    auto encoded_vlan = vlan.getEncoded();
    base_pkt.insert(base_pkt.end(), encoded_vlan.begin(), encoded_vlan.end());

    // SampledValue
    Protocols::SampledValue sv(
        sv_conf->appID,
        sv_conf->noAsdu,
        sv_conf->svID,
        sv_conf->smpCnt,
        sv_conf->confRev,
        sv_conf->smpSynch,
        sv_conf->smpMod
    );

    auto encoded_sv = sv.getEncoded(8);
    int idx_SV_Start = base_pkt.size();
    base_pkt.insert(base_pkt.end(), encoded_sv.begin(), encoded_sv.end());

    // Send the packet
    RawSocket raw_socket;

    raw_socket.iov.iov_base = (void*)base_pkt.data();
    raw_socket.iov.iov_len = base_pkt.size();

    int smpCount = sv.getParamPos(1, "smpCnt") + idx_SV_Start;
    base_pkt[smpCount] = 0x00;
    base_pkt[smpCount + 1] = 0x4;

    for (int i = 0; i < 100; i++){
        sendmsg(raw_socket.socket_id, &raw_socket.msg_hdr, 0);
    }

}


void test_Sniffer(){

    // TestSet_Class testSet;
    
    Tests_Class testSet;

    // Transient
    transient_config tran_conf;

    tran_conf.channelConfig = {
        // {0,1}, {1,2}, {2,3}, {4,4}, {5,5}, {6,6}
        {0,6}, {1,5}, {2,4}, {4,3}, {5,2}, {6,1}
    };
    tran_conf.file_data_fs = 9600;
    // tran_conf.fileName = "files/noFault.csv";
    tran_conf.fileName = "files/Fault_main.csv";
    tran_conf.interval = 0;
    tran_conf.interval_flag = 0;
    tran_conf.loop_flag = 0;
    tran_conf.scale = {1,1,1,1,1,1,1,1}; 

    //SV Config 
    tran_conf.sv_config.appID = 0x4000;
    tran_conf.sv_config.confRev = 1;
    tran_conf.sv_config.dstMac = "01-0C-CD-04-00-01";
    tran_conf.sv_config.noAsdu = 1;
    tran_conf.sv_config.smpCnt = 0;
    tran_conf.sv_config.smpMod = 0;
    tran_conf.sv_config.smpRate = 4800;
    tran_conf.sv_config.smpSynch = 1;
    tran_conf.sv_config.svID = "SV_01";
    tran_conf.sv_config.vlanDei = 0;
    tran_conf.sv_config.vlanId = 100;
    tran_conf.sv_config.vlanPcp = 4;
    tran_conf.sv_config.noChannels = 8;

    std::vector<int> angs = {90};//{0, 45, 90}
    std::vector<int> ress = {30};//{0, 15, 30, 50};
    double *trip_time;
    trip_time = &tran_conf.trip_time;

    tran_conf.fileName = "files/Fault_main_0_0.csv";
    testSet.start_transient_test({tran_conf});
    sleep(1);
    while(testSet.transient_tests[0].running == 1){
        sleep(1);
    }

    return;

    for (const auto& ang : angs) {
        for (const auto& res : ress) {
        
            std::cout << "Ang: " << ang << std::endl;
            std::cout << "Resistence: " << res << std::endl;
            for (uint8_t j=4;j<5;j++){
                if (j == 2) continue;
                // Sniffer
                Goose_info goInfo = {
                    .goCbRef = "GCBR_01",
                    .mac_dst = {0x01, 0x0c, 0xcd, 0x01, 0x00, 0x01},
                    .input = {{0,j}, {1,1}}
                };
                if (j == 0) std::cout<< "PIOC" << std::endl;
                else if (j == 1) std::cout<< "PTOC" << std::endl;
                else if (j == 2) std::cout<< "PTOV" << std::endl;
                else if (j == 3) std::cout<< "PTUV" << std::endl;
                else if (j == 4) std::cout<< "PDIS" << std::endl;

                std::string protName = "";
                if (j == 0) protName = "PIOC";
                else if (j == 1) protName = "PTOC";
                else if (j == 2) protName = "PTOV";
                else if (j == 3) protName = "PTUV";
                else if (j == 4) protName = "PDIS";
                std::string fileName = protName + "_ang_" + std::to_string(ang)+ "_res_" + std::to_string(res) + ".txt";
                std::ofstream file = std::ofstream(fileName, std::ios::app);

                testSet.sniffer.startThread({goInfo});
                for (int i=0;i<50;i++){
                    tran_conf.fileName = "files/Fault_main_" + std::to_string(ang) + "_" + std::to_string(res) + ".csv";
                    testSet.start_transient_test({tran_conf});
                    sleep(1);
                    while(testSet.transient_tests[0].running == 1){
                        sleep(1);
                    }
                    std::cout<< *trip_time << std::endl;
                    file << *trip_time << std::endl;
                }
                testSet.sniffer.stopThread();
                file.close();
            }

        }
    }
    
}

void testProtection (){

    Tests_Class testSet;

    Goose_info goInfo = {
        .goCbRef = "GCBR_01",
        .mac_dst = {0x01, 0x0c, 0xcd, 0x01, 0x00, 0x01},
        .input = {{0,0}}
    };

    testSet.sniffer.startThread({goInfo});
    sleep(15);
    
    testSet.sniffer.stopThread();
}

void test_defined_time(){

    Tests_Class testSet;

    struct timespec t_now;
    clock_gettime(CLOCK_REALTIME, &t_now);
    std::cout << "Current time in nanoseconds: " << t_now.tv_sec * 1e9 + t_now.tv_nsec << std::endl;
    
    std::vector<transient_config> conf = get_transient_test_config("files/transient_test.json");
    for (auto& c: conf){
        if (c.fileloaded == 0){
            std::cerr << c.error_msg << std::endl;
            return;
        }
    }
    testSet.start_transient_test(conf);

    sleep(1);
    while(testSet.transient_tests[0].running == 1){
        sleep(1);
    }
    std::cout << "Test finished" << std::endl;
    std::cout << "Trip time: " << conf[0].trip_time << std::endl;
    std::cout << "Test finished" << std::endl;

}

void test_server(){

    TCPServer server(8080);
    server.start();

    // Simulate some work in the main thread
    std::this_thread::sleep_for(std::chrono::seconds(10));

    server.stop();

}

int main(){

    std::cout << "Hello World!" << std::endl;


    TCPServer server(8080);
    server.start();

    // get UTC time in nanoseconds now
    // test_Sniffer();
    // testTransient();
    // while (1){
    //     sleep(1);
    // }

    return 0;
}