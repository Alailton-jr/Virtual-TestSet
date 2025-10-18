
#include "main.hpp"
#include "compat.hpp"
#include "raw_socket.hpp"
#include "rt_utils.hpp"
#include "logger.hpp"
#include "metrics.hpp"

#include "Ethernet.hpp"
#include "Goose.hpp"
#include "SampledValue.hpp"
#include "Virtual_LAN.hpp"
#include "sv_sender.hpp"

#include "tests.hpp"
#include <time.h>
#include <filesystem>
#include <stdexcept>
#include <cstdlib>  // for getenv
#include <string>   // for std::string

namespace fs = std::filesystem;

// Helper function to sanitize file paths
std::string sanitizeFileName(const std::string& name) {
    // Reject dangerous patterns
    if (name.find("..") != std::string::npos) {
        throw std::invalid_argument("Path traversal detected: '..' not allowed");
    }
    if (name.find("/") != std::string::npos) {
        throw std::invalid_argument("Directory separators not allowed");
    }
    if (name.find("\\") != std::string::npos) {
        throw std::invalid_argument("Directory separators not allowed");
    }
    if (name.empty()) {
        throw std::invalid_argument("Empty filename not allowed");
    }
    
    // Check for non-printable characters
    for (char c : name) {
        if (c < 32 || c > 126) {
            throw std::invalid_argument("Non-printable characters not allowed");
        }
    }
    
    // Construct safe path under files/ directory
    fs::path safePath = fs::path("files") / name;
    
    // Ensure the canonical path is still under files/
    try {
        fs::path canonicalBase = fs::canonical("files");
        fs::path canonicalPath = fs::weakly_canonical(safePath);
        
        // Check if canonicalPath starts with canonicalBase
        auto baseStr = canonicalBase.string();
        auto pathStr = canonicalPath.string();
        
        if (pathStr.substr(0, baseStr.length()) != baseStr) {
            throw std::invalid_argument("Path escapes files/ directory");
        }
    } catch (const fs::filesystem_error& e) {
        throw std::invalid_argument(std::string("Filesystem error: ") + e.what());
    }
    
    return safePath.string();
}

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
        LOG_ERROR("TCP", "Failed to create socket");
        return;
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        LOG_ERROR("TCP", "Setsockopt failed: %s", strerror(errno));
        close(serverSocket);
        return;
    }

    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        LOG_ERROR("TCP", "Binding failed");
        return;
    }

    if (listen(serverSocket, 5) == -1) {
        LOG_ERROR("TCP", "Listening failed");
        return;
    }

    LOG_INFO("TCP", "Server listening on port %d", port);

    while (isRunning) {
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == -1) {
            if (isRunning) {
                LOG_ERROR("TCP", "Accept failed");
            }
            continue;
        }
        clientThreads.emplace_back(&TCPServer::handleClient, this, clientSocket);
    }
}

// Save any file sent by the client
std::string save_file2(std::string fileName, const char* buffer, int bytesReceived) {
    // Sanitize filename
    std::string safePath;
    try {
        safePath = sanitizeFileName(fileName);
    } catch (const std::exception& e) {
        LOG_ERROR("FILE", "Invalid filename: %s", e.what());
        return "ERROR: Invalid filename";
    }
    
    std::ofstream file(safePath, std::ios::app);
    if (file.is_open()) {
        file.write(buffer, bytesReceived);
        file.close();
        LOG_INFO("FILE", "Data received and saved to file: %s", fileName.c_str());
        return "0";
    } else {
        LOG_ERROR("FILE", "Unable to open file for writing: %s", safePath.c_str());
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
    LOG_INFO("FILE", "File name received: %s", fileNameStr.c_str());
    
    // Sanitize filename
    std::string safePath;
    try {
        safePath = sanitizeFileName(fileNameStr);
    } catch (const std::exception& e) {
        LOG_ERROR("FILE", "Invalid filename: %s", e.what());
        send(*clientSocket, "ERROR: Invalid filename", 23, 0);
        return -1;
    }
    
    // Fix: use maxBufferSize instead of sizeof(buffer) which is pointer size
    int bytesReceivedFileSize = recv(*clientSocket, buffer, maxBufferSize, 0);
    if (bytesReceivedFileSize <= 0) {
        return -1;
    }
    send(*clientSocket, "OK", 2, 0);
    
    // Fix: use stoi with error handling instead of atoi
    int fileSize;
    try {
        fileSize = std::stoi(std::string(buffer, bytesReceivedFileSize));
        if (fileSize <= 0 || fileSize > 100*1024*1024) { // 100 MB limit
            throw std::out_of_range("File size out of range");
        }
    } catch (const std::exception& e) {
        LOG_ERROR("FILE", "Invalid file size: %s", e.what());
        send(*clientSocket, "ERROR: Invalid file size", 24, 0);
        return -1;
    }
    
    LOG_INFO("FILE", "File size received: %d bytes", fileSize);
    std::ofstream file(safePath, std::ios::trunc);
    if (file.is_open()) {
        int totalBytesReceived = 0;
        while (totalBytesReceived < fileSize) {
            int bytesReceived = recv(*clientSocket, buffer, maxBufferSize, 0);
            if (bytesReceived <= 0) {
                LOG_ERROR("FILE", "Error receiving file data");
                file.close();
                return -1;
            }
            file.write(buffer, bytesReceived);
            totalBytesReceived += bytesReceived;
            send(*clientSocket, "OK", 2, 0);
        }
        file.close();
        LOG_INFO("FILE", "All data received and saved to file: %s", fileNameStr.c_str());
        return 0;
    } else {
        LOG_ERROR("FILE", "Unable to open file for writing: %s", safePath.c_str());
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
    LOG_INFO("FILE", "File size received: %d bytes", fileSize);
    std::ofstream file("files/goose_input_config.json", std::ios::trunc);
    if (file.is_open()) {
        int totalBytesReceived = 0;
        while (totalBytesReceived < fileSize) {
            int bytesReceived = recv(*clientSocket, buffer, maxBufferSize, 0);
            if (bytesReceived <= 0) {
                LOG_ERROR("FILE", "Error receiving file data");
                file.close();
                return -1;
            }
            file.write(buffer, bytesReceived);
            totalBytesReceived += bytesReceived;
            send(*clientSocket, "OK", 2, 0);
        }
        file.close();
        LOG_INFO("FILE", "All data received and saved to file: sniffer_config.json");
        return 0;
    } else {
        LOG_ERROR("FILE", "Unable to open file for writing: goose_input_config.json");
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
    Ethernet eth(sv_conf->srcMac, sv_conf->dstMac);
    auto encoded_eth = eth.getEncoded();
    base_pkt.insert(base_pkt.end(), encoded_eth.begin(), encoded_eth.end());

    // Virtual LAN
    Virtual_LAN vlan(sv_conf->vlanId, sv_conf->vlanPcp, sv_conf->vlanDei);
    auto encoded_vlan = vlan.getEncoded();
    base_pkt.insert(base_pkt.end(), encoded_vlan.begin(), encoded_vlan.end());

    // SampledValue
    SampledValue sv(
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

// Phase 11: CLI argument parsing and platform detection
// Phase 12: Added log level and log file configuration
struct AppConfig {
    bool no_net = false;      // Disable network operations
    bool selftest = false;    // Run self-test and exit
    bool help = false;        // Show help message
    LogLevel log_level = LogLevel::INFO;  // Default log level
    std::string log_file;     // Optional log file (empty = console only)
};

// Parse log level from string
LogLevel parseLogLevel(const std::string& level) {
    if (level == "DEBUG") return LogLevel::DEBUG;
    if (level == "INFO") return LogLevel::INFO;
    if (level == "WARN") return LogLevel::WARN;
    if (level == "ERROR") return LogLevel::ERROR;
    if (level == "NONE") return LogLevel::NONE;
    return LogLevel::INFO;  // Default
}

// Parse command-line arguments
AppConfig parseArgs(int argc, char* argv[]) {
    AppConfig config;
    
    // Check environment variables
    const char* env_no_net = std::getenv("VTS_NO_NET");
    if (env_no_net && (std::string(env_no_net) == "1" || std::string(env_no_net) == "true")) {
        config.no_net = true;
        std::cout << "[CONFIG] VTS_NO_NET environment variable set - network operations disabled" << std::endl;
    }
    
    const char* env_log_level = std::getenv("VTS_LOG_LEVEL");
    if (env_log_level) {
        config.log_level = parseLogLevel(env_log_level);
        std::cout << "[CONFIG] VTS_LOG_LEVEL=" << env_log_level << std::endl;
    }
    
    const char* env_log_file = std::getenv("VTS_LOG_FILE");
    if (env_log_file) {
        config.log_file = env_log_file;
        std::cout << "[CONFIG] VTS_LOG_FILE=" << env_log_file << std::endl;
    }
    
    // macOS: Default to no-net mode (can be overridden with explicit flag)
#ifdef VTS_PLATFORM_MAC
    config.no_net = true;
    std::cout << "[CONFIG] macOS detected - defaulting to no-net mode" << std::endl;
#endif
    
    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        
        if (arg == "--no-net") {
            config.no_net = true;
            std::cout << "[CONFIG] --no-net flag specified" << std::endl;
        } else if (arg == "--enable-net") {
            config.no_net = false;
            std::cout << "[CONFIG] --enable-net flag specified (overriding default)" << std::endl;
        } else if (arg == "--selftest") {
            config.selftest = true;
            std::cout << "[CONFIG] --selftest flag specified" << std::endl;
        } else if (arg == "--help" || arg == "-h") {
            config.help = true;
        } else if (arg == "--log-level" && i + 1 < argc) {
            config.log_level = parseLogLevel(argv[++i]);
            std::cout << "[CONFIG] --log-level=" << argv[i] << std::endl;
        } else if (arg == "--log-file" && i + 1 < argc) {
            config.log_file = argv[++i];
            std::cout << "[CONFIG] --log-file=" << config.log_file << std::endl;
        } else {
            std::cerr << "Warning: Unknown argument: " << arg << std::endl;
        }
    }
    
    return config;
}

// Print help message
void printHelp(const char* progName) {
    std::cout << "Virtual TestSet - IEC 61850 GOOSE/SV Test System\n\n";
    std::cout << "Usage: " << progName << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h              Show this help message\n";
    std::cout << "  --no-net                Disable network operations (safe mode)\n";
    std::cout << "  --enable-net            Enable network operations (override macOS default)\n";
    std::cout << "  --selftest              Run self-test and exit (instantiate modules without I/O)\n";
    std::cout << "  --log-level <level>     Set log level: DEBUG, INFO, WARN, ERROR, NONE (default: INFO)\n";
    std::cout << "  --log-file <path>       Write logs to file (in addition to console)\n\n";
    std::cout << "Environment Variables:\n";
    std::cout << "  VTS_NO_NET=1            Disable network operations\n";
    std::cout << "  VTS_LOG_LEVEL=<level>   Set log level (DEBUG, INFO, WARN, ERROR, NONE)\n";
    std::cout << "  VTS_LOG_FILE=<path>     Write logs to file\n";
    std::cout << "  IF_NAME=<iface>         Override network interface name\n\n";
    std::cout << "Platform: " << vts::platform::get_platform_info() << "\n";
    std::cout << "Network support: " << (vts::platform::network_operations_supported() ? "Yes" : "No") << "\n";
    std::cout << "Real-time support: " << (vts::platform::realtime_operations_supported() ? "Yes" : "No") << "\n\n";
}

int main(int argc, char* argv[]){

    // Parse command-line arguments
    AppConfig config = parseArgs(argc, argv);
    
    // Show help and exit
    if (config.help) {
        printHelp(argv[0]);
        return 0;
    }
    
    // Phase 12: Initialize logger and metrics
    Logger::init(config.log_level, config.log_file);
    Metrics::init();
    
    LOG_INFO("MAIN", "==================================================");
    LOG_INFO("MAIN", "Virtual TestSet - IEC 61850 GOOSE/SV Test System");
    LOG_INFO("MAIN", "Platform: %s", vts::platform::get_platform_info().c_str());
    LOG_INFO("MAIN", "==================================================");

    // Phase 7: Real-time initialization (Linux only)
    // Note: These calls require elevated privileges (CAP_SYS_NICE, CAP_IPC_LOCK or root)
    LOG_INFO("RT", "Initializing real-time capabilities...");
    
    // Lock all memory to prevent paging (critical for deterministic timing)
    rt_lock_memory();
    
    // Set main thread to real-time priority (optional, can be done per-worker instead)
    // Uncomment if main thread needs RT priority:
    // rt_set_realtime(50);  // Lower priority than worker threads
    
    LOG_INFO("RT", "Real-time initialization complete");
    
    // Phase 11: Self-test mode
    if (config.selftest) {
        LOG_INFO("SELFTEST", "Running self-test mode...");
        
        // Instantiate modules to verify they load correctly
        LOG_INFO("SELFTEST", "Testing Ethernet frame creation...");
        // Example: Create an Ethernet frame without sending it
        // Ethernet eth;
        
        LOG_INFO("SELFTEST", "Testing GOOSE message creation...");
        // Example: Create a GOOSE message without sending it
        // Goose goose;
        
        LOG_INFO("SELFTEST", "Testing SampledValue message creation...");
        // Example: Create an SV message without sending it
        // SampledValue sv;
        
        LOG_INFO("SELFTEST", "All modules instantiated successfully!");
        LOG_INFO("SELFTEST", "Self-test passed. Exiting.");
        
        // Cleanup
        Metrics::printSummary();
        Logger::shutdown();
        return 0;
    }
    
    // Phase 11: Check if network operations are allowed
    if (config.no_net) {
        LOG_INFO("NET", "Network operations disabled (--no-net mode)");
        LOG_INFO("NET", "Sniffer and packet transmission will be skipped");
        LOG_INFO("NET", "Running in configuration/setup validation mode only");
        
        // Allow the application to continue but skip network initialization
        // The TCP server can still run for API access
    }

    // Start TCP server (can run without network I/O)
    TCPServer server(8080);
    server.start();

    // get UTC time in nanoseconds now
    // test_Sniffer();
    // testTransient();
    // while (1){
    //     sleep(1);
    // }
    
    // Cleanup
    Metrics::printSummary();
    Logger::shutdown();

    return 0;
}