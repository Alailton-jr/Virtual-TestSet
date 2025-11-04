#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <atomic>
#include <thread>

using json = nlohmann::json;

// Forward declarations
class SVPublisherManager;
class SequenceEngine;
class GooseSubscriber;
class AnalyzerEngine;

class HTTPServer {
public:
    HTTPServer(int port);
    ~HTTPServer();

    // Server control
    void start();
    void stop();
    bool isRunning() const { return running_.load(); }

    // Set component references
    void setSVPublisherManager(std::shared_ptr<SVPublisherManager> manager);
    void setSequenceEngine(std::shared_ptr<SequenceEngine> engine);
    void setGooseSubscriber(std::shared_ptr<GooseSubscriber> subscriber);
    void setAnalyzerEngine(std::shared_ptr<AnalyzerEngine> analyzer);
    void setWSServer(class WSServer* wsServer);

private:
    // Setup route handlers
    void setupRoutes();
    
    // Health endpoint
    void handleHealth(const httplib::Request& req, httplib::Response& res);
    
    // Stream management endpoints (Module 13)
    void handleGetStreams(const httplib::Request& req, httplib::Response& res);
    void handleCreateStream(const httplib::Request& req, httplib::Response& res);
    void handleUpdateStream(const httplib::Request& req, httplib::Response& res);
    void handleDeleteStream(const httplib::Request& req, httplib::Response& res);
    void handleStartStream(const httplib::Request& req, httplib::Response& res);
    void handleStopStream(const httplib::Request& req, httplib::Response& res);
    
    // Phasor endpoints (Module 2)
    void handleUpdatePhasors(const httplib::Request& req, httplib::Response& res);
    void handleUpdateHarmonics(const httplib::Request& req, httplib::Response& res);
    
    // COMTRADE playback endpoints (Module 1)
    void handleComtradePlayback(const httplib::Request& req, httplib::Response& res);
    
    // Sequence endpoints (Module 3)
    void handleSequenceRun(const httplib::Request& req, httplib::Response& res);
    void handleSequenceStop(const httplib::Request& req, httplib::Response& res);
    
    // GOOSE endpoints (Module 4)
    void handleGooseScan(const httplib::Request& req, httplib::Response& res);
    void handleGooseConfig(const httplib::Request& req, httplib::Response& res);
    
    // Analyzer endpoints (Module 5)
    void handleAnalyzerSelect(const httplib::Request& req, httplib::Response& res);
    
    // Impedance injection endpoints (Module 6)
    void handleImpedanceApply(const httplib::Request& req, httplib::Response& res);
    
    // Ramping test endpoints (Module 7)
    void handleRampRun(const httplib::Request& req, httplib::Response& res);
    
    // Distance relay test endpoints (Module 9)
    void handleDistanceRun(const httplib::Request& req, httplib::Response& res);
    
    // Overcurrent test endpoints (Module 10)
    void handleOvercurrentRun(const httplib::Request& req, httplib::Response& res);
    
    // Differential test endpoints (Module 11)
    void handleDifferentialRun(const httplib::Request& req, httplib::Response& res);
    
    // Utility functions
    void sendJsonResponse(httplib::Response& res, int status, const json& data);
    void sendErrorResponse(httplib::Response& res, int status, const std::string& message);
    bool validateJson(const json& data, const std::string& schemaName);

    int port_;
    std::atomic<bool> running_;
    std::unique_ptr<httplib::Server> server_;
    std::thread serverThread_;
    
    // Component references
    std::shared_ptr<SVPublisherManager> svManager_;
    std::shared_ptr<SequenceEngine> sequenceEngine_;
    std::shared_ptr<GooseSubscriber> gooseSubscriber_;
    std::shared_ptr<AnalyzerEngine> analyzerEngine_;
    class WSServer* wsServer_;
};

#endif // HTTP_SERVER_HPP
