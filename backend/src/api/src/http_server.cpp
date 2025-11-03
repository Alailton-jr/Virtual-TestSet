#include "http_server.hpp"
#include "sv_publisher_manager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>

HTTPServer::HTTPServer(int port)
    : port_(port), running_(false) {
    server_ = std::make_unique<httplib::Server>();
    setupRoutes();
}

HTTPServer::~HTTPServer() {
    stop();
}

void HTTPServer::setupRoutes() {
    // CORS headers for development
    server_->set_post_routing_handler([](const httplib::Request& /*req*/, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, PATCH, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    });
    
    // OPTIONS handler for CORS preflight
    server_->Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, PATCH, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.status = 204;
    });
    
    // Health endpoint
    server_->Get("/api/v1/health", [this](const httplib::Request& req, httplib::Response& res) {
        handleHealth(req, res);
    });
    
    // Stream management endpoints (Module 13)
    server_->Get("/api/v1/streams", [this](const httplib::Request& req, httplib::Response& res) {
        handleGetStreams(req, res);
    });
    
    server_->Post("/api/v1/streams", [this](const httplib::Request& req, httplib::Response& res) {
        handleCreateStream(req, res);
    });
    
    server_->Patch("/api/v1/streams/:id", [this](const httplib::Request& req, httplib::Response& res) {
        handleUpdateStream(req, res);
    });
    
    server_->Delete("/api/v1/streams/:id", [this](const httplib::Request& req, httplib::Response& res) {
        handleDeleteStream(req, res);
    });
    
    server_->Post("/api/v1/streams/:id/start", [this](const httplib::Request& req, httplib::Response& res) {
        handleStartStream(req, res);
    });
    
    server_->Post("/api/v1/streams/:id/stop", [this](const httplib::Request& req, httplib::Response& res) {
        handleStopStream(req, res);
    });
    
    // Phasor endpoints (Module 2)
    server_->Post("/api/v1/phasors/:streamId", [this](const httplib::Request& req, httplib::Response& res) {
        handleUpdatePhasors(req, res);
    });
    
    server_->Post("/api/v1/phasors/:streamId/harmonics", [this](const httplib::Request& req, httplib::Response& res) {
        handleUpdateHarmonics(req, res);
    });
    
    // COMTRADE playback endpoint (Module 1)
    server_->Post("/api/v1/comtrade/playback", [this](const httplib::Request& req, httplib::Response& res) {
        handleComtradePlayback(req, res);
    });
    
    // Sequence endpoints (Module 3)
    server_->Post("/api/v1/sequences/run", [this](const httplib::Request& req, httplib::Response& res) {
        handleSequenceRun(req, res);
    });
    
    server_->Post("/api/v1/sequences/stop", [this](const httplib::Request& req, httplib::Response& res) {
        handleSequenceStop(req, res);
    });
    
    // GOOSE endpoints (Module 4)
    server_->Post("/api/v1/goose/scan", [this](const httplib::Request& req, httplib::Response& res) {
        handleGooseScan(req, res);
    });
    
    server_->Post("/api/v1/goose/config", [this](const httplib::Request& req, httplib::Response& res) {
        handleGooseConfig(req, res);
    });
    
    // Analyzer endpoint (Module 5)
    server_->Post("/api/v1/analyzer/select", [this](const httplib::Request& req, httplib::Response& res) {
        handleAnalyzerSelect(req, res);
    });
    
    // Impedance injection endpoint (Module 6)
    server_->Post("/api/v1/impedance/apply", [this](const httplib::Request& req, httplib::Response& res) {
        handleImpedanceApply(req, res);
    });
    
    // Ramping test endpoint (Module 7)
    server_->Post("/api/v1/ramp/run", [this](const httplib::Request& req, httplib::Response& res) {
        handleRampRun(req, res);
    });
    
    // Distance relay test endpoint (Module 9)
    server_->Post("/api/v1/distance/run", [this](const httplib::Request& req, httplib::Response& res) {
        handleDistanceRun(req, res);
    });
    
    // Overcurrent test endpoint (Module 10)
    server_->Post("/api/v1/overcurrent/run", [this](const httplib::Request& req, httplib::Response& res) {
        handleOvercurrentRun(req, res);
    });
    
    // Differential test endpoint (Module 11)
    server_->Post("/api/v1/differential/run", [this](const httplib::Request& req, httplib::Response& res) {
        handleDifferentialRun(req, res);
    });
}

void HTTPServer::start() {
    if (running_.load()) {
        std::cerr << "HTTP server already running" << std::endl;
        return;
    }
    
    running_.store(true);
    
    serverThread_ = std::thread([this]() {
        std::cout << "HTTP server starting on port " << port_ << std::endl;
        if (!server_->listen("0.0.0.0", port_)) {
            std::cerr << "Failed to start HTTP server on port " << port_ << std::endl;
            running_.store(false);
        }
    });
    
    std::cout << "HTTP server started on port " << port_ << std::endl;
}

void HTTPServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    server_->stop();
    
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    
    std::cout << "HTTP server stopped" << std::endl;
}

void HTTPServer::setSVPublisherManager(std::shared_ptr<SVPublisherManager> manager) {
    svManager_ = manager;
}

void HTTPServer::setSequenceEngine(std::shared_ptr<SequenceEngine> engine) {
    sequenceEngine_ = engine;
}

void HTTPServer::setGooseSubscriber(std::shared_ptr<GooseSubscriber> subscriber) {
    gooseSubscriber_ = subscriber;
}

void HTTPServer::setAnalyzerEngine(std::shared_ptr<AnalyzerEngine> analyzer) {
    analyzerEngine_ = analyzer;
}

// Health endpoint
void HTTPServer::handleHealth(const httplib::Request& /*req*/, httplib::Response& res) {
    json response = {
        {"status", "ok"},
        {"timestamp", std::time(nullptr)},
        {"version", "1.0.0"}
    };
    sendJsonResponse(res, 200, response);
}

// Stream management endpoints
void HTTPServer::handleGetStreams(const httplib::Request& /*req*/, httplib::Response& res) {
    if (!svManager_) {
        sendErrorResponse(res, 503, "Publisher manager not initialized");
        return;
    }
    
    try {
        json streams = svManager_->listStreams();
        sendJsonResponse(res, 200, streams);
    } catch (const std::exception& e) {
        sendErrorResponse(res, 500, std::string("Failed to list streams: ") + e.what());
    }
}

void HTTPServer::handleCreateStream(const httplib::Request& req, httplib::Response& res) {
    if (!svManager_) {
        sendErrorResponse(res, 503, "Publisher manager not initialized");
        return;
    }
    
    try {
        json body = json::parse(req.body);
        
        // TODO: Validate against stream-config.schema.json
        std::string streamId = svManager_->createStream(body);
        
        json response = {
            {"id", streamId},
            {"message", "Stream created successfully"}
        };
        sendJsonResponse(res, 201, response);
    } catch (const json::exception& e) {
        sendErrorResponse(res, 400, std::string("Invalid JSON: ") + e.what());
    } catch (const std::exception& e) {
        sendErrorResponse(res, 500, std::string("Failed to create stream: ") + e.what());
    }
}

void HTTPServer::handleUpdateStream(const httplib::Request& req, httplib::Response& res) {
    if (!svManager_) {
        sendErrorResponse(res, 503, "Publisher manager not initialized");
        return;
    }
    
    std::string streamId = req.path_params.at("id");
    
    try {
        json body = json::parse(req.body);
        svManager_->updateStream(streamId, body);
        
        json response = {
            {"id", streamId},
            {"message", "Stream updated successfully"}
        };
        sendJsonResponse(res, 200, response);
    } catch (const json::exception& e) {
        sendErrorResponse(res, 400, std::string("Invalid JSON: ") + e.what());
    } catch (const std::exception& e) {
        sendErrorResponse(res, 500, std::string("Failed to update stream: ") + e.what());
    }
}

void HTTPServer::handleDeleteStream(const httplib::Request& req, httplib::Response& res) {
    if (!svManager_) {
        sendErrorResponse(res, 503, "Publisher manager not initialized");
        return;
    }
    
    std::string streamId = req.path_params.at("id");
    
    try {
        svManager_->deleteStream(streamId);
        
        json response = {
            {"id", streamId},
            {"message", "Stream deleted successfully"}
        };
        sendJsonResponse(res, 200, response);
    } catch (const std::exception& e) {
        sendErrorResponse(res, 500, std::string("Failed to delete stream: ") + e.what());
    }
}

void HTTPServer::handleStartStream(const httplib::Request& req, httplib::Response& res) {
    if (!svManager_) {
        sendErrorResponse(res, 503, "Publisher manager not initialized");
        return;
    }
    
    std::string streamId = req.path_params.at("id");
    
    try {
        svManager_->startStream(streamId);
        
        json response = {
            {"id", streamId},
            {"message", "Stream started successfully"}
        };
        sendJsonResponse(res, 200, response);
    } catch (const std::exception& e) {
        sendErrorResponse(res, 500, std::string("Failed to start stream: ") + e.what());
    }
}

void HTTPServer::handleStopStream(const httplib::Request& req, httplib::Response& res) {
    if (!svManager_) {
        sendErrorResponse(res, 503, "Publisher manager not initialized");
        return;
    }
    
    std::string streamId = req.path_params.at("id");
    
    try {
        svManager_->stopStream(streamId);
        
        json response = {
            {"id", streamId},
            {"message", "Stream stopped successfully"}
        };
        sendJsonResponse(res, 200, response);
    } catch (const std::exception& e) {
        sendErrorResponse(res, 500, std::string("Failed to stop stream: ") + e.what());
    }
}

// Phasor endpoints
void HTTPServer::handleUpdatePhasors(const httplib::Request& req, httplib::Response& res) {
    if (!svManager_) {
        sendErrorResponse(res, 503, "Publisher manager not initialized");
        return;
    }
    
    std::string streamId = req.path_params.at("streamId");
    
    try {
        json body = json::parse(req.body);
        svManager_->updatePhasors(streamId, body);
        
        sendJsonResponse(res, 200, {{"message", "Phasors updated"}});
    } catch (const json::exception& e) {
        sendErrorResponse(res, 400, std::string("Invalid JSON: ") + e.what());
    } catch (const std::exception& e) {
        sendErrorResponse(res, 500, std::string("Failed to update phasors: ") + e.what());
    }
}

void HTTPServer::handleUpdateHarmonics(const httplib::Request& req, httplib::Response& res) {
    if (!svManager_) {
        sendErrorResponse(res, 503, "Publisher manager not initialized");
        return;
    }
    
    std::string streamId = req.path_params.at("streamId");
    
    try {
        json body = json::parse(req.body);
        svManager_->updateHarmonics(streamId, body);
        
        sendJsonResponse(res, 200, {{"message", "Harmonics updated"}});
    } catch (const json::exception& e) {
        sendErrorResponse(res, 400, std::string("Invalid JSON: ") + e.what());
    } catch (const std::exception& e) {
        sendErrorResponse(res, 500, std::string("Failed to update harmonics: ") + e.what());
    }
}

// COMTRADE playback endpoint
void HTTPServer::handleComtradePlayback(const httplib::Request& /*req*/, httplib::Response& res) {
    // TODO: Handle multipart/form-data file upload
    sendErrorResponse(res, 501, "COMTRADE playback not yet implemented");
}

// Sequence endpoints
void HTTPServer::handleSequenceRun(const httplib::Request& req, httplib::Response& res) {
    try {
        json body = json::parse(req.body);
        
        // TODO: Start sequence execution
        
        sendJsonResponse(res, 200, {{"message", "Sequence started"}});
    } catch (const json::exception& e) {
        sendErrorResponse(res, 400, std::string("Invalid JSON: ") + e.what());
    }
}

void HTTPServer::handleSequenceStop(const httplib::Request& /*req*/, httplib::Response& res) {
    // TODO: Stop sequence execution
    sendJsonResponse(res, 200, {{"message", "Sequence stopped"}});
}

// GOOSE endpoints
void HTTPServer::handleGooseScan(const httplib::Request& /*req*/, httplib::Response& res) {
    // TODO: Scan for GOOSE messages
    sendJsonResponse(res, 200, {{"entries", json::array()}});
}

void HTTPServer::handleGooseConfig(const httplib::Request& req, httplib::Response& res) {
    try {
        json body = json::parse(req.body);
        
        // TODO: Configure GOOSE subscription
        
        sendJsonResponse(res, 200, {{"message", "GOOSE configured"}});
    } catch (const json::exception& e) {
        sendErrorResponse(res, 400, std::string("Invalid JSON: ") + e.what());
    }
}

// Analyzer endpoint
void HTTPServer::handleAnalyzerSelect(const httplib::Request& req, httplib::Response& res) {
    try {
        json body = json::parse(req.body);
        
        // TODO: Select analyzer stream
        
        sendJsonResponse(res, 200, {{"message", "Analyzer stream selected"}});
    } catch (const json::exception& e) {
        sendErrorResponse(res, 400, std::string("Invalid JSON: ") + e.what());
    }
}

// Impedance injection endpoint
void HTTPServer::handleImpedanceApply(const httplib::Request& req, httplib::Response& res) {
    try {
        json body = json::parse(req.body);
        
        // TODO: Apply impedance injection
        
        sendJsonResponse(res, 200, {{"message", "Impedance applied"}});
    } catch (const json::exception& e) {
        sendErrorResponse(res, 400, std::string("Invalid JSON: ") + e.what());
    }
}

// Ramping test endpoint
void HTTPServer::handleRampRun(const httplib::Request& req, httplib::Response& res) {
    try {
        json body = json::parse(req.body);
        
        // TODO: Start ramping test
        
        sendJsonResponse(res, 200, {{"message", "Ramping test started"}});
    } catch (const json::exception& e) {
        sendErrorResponse(res, 400, std::string("Invalid JSON: ") + e.what());
    }
}

// Distance relay test endpoint
void HTTPServer::handleDistanceRun(const httplib::Request& req, httplib::Response& res) {
    try {
        json body = json::parse(req.body);
        
        // TODO: Start distance relay test
        
        sendJsonResponse(res, 200, {{"message", "Distance test started"}});
    } catch (const json::exception& e) {
        sendErrorResponse(res, 400, std::string("Invalid JSON: ") + e.what());
    }
}

// Overcurrent test endpoint
void HTTPServer::handleOvercurrentRun(const httplib::Request& req, httplib::Response& res) {
    try {
        json body = json::parse(req.body);
        
        // TODO: Start overcurrent test
        
        sendJsonResponse(res, 200, {{"message", "Overcurrent test started"}});
    } catch (const json::exception& e) {
        sendErrorResponse(res, 400, std::string("Invalid JSON: ") + e.what());
    }
}

// Differential test endpoint
void HTTPServer::handleDifferentialRun(const httplib::Request& req, httplib::Response& res) {
    try {
        json body = json::parse(req.body);
        
        // TODO: Start differential test
        
        sendJsonResponse(res, 200, {{"message", "Differential test started"}});
    } catch (const json::exception& e) {
        sendErrorResponse(res, 400, std::string("Invalid JSON: ") + e.what());
    }
}

// Utility functions
void HTTPServer::sendJsonResponse(httplib::Response& res, int status, const json& data) {
    res.status = status;
    res.set_content(data.dump(), "application/json");
}

void HTTPServer::sendErrorResponse(httplib::Response& res, int status, const std::string& message) {
    json error = {
        {"error", message},
        {"timestamp", std::time(nullptr)}
    };
    sendJsonResponse(res, status, error);
}

bool HTTPServer::validateJson(const json& /*data*/, const std::string& /*schemaName*/) {
    // TODO: Implement JSON schema validation
    return true;
}
