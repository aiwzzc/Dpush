#pragma once

#include <unordered_map>
#include <string>

#include "Logging.h"
#include "AsyncWorker.h"
#include "LogSink.h"
#include "LoggerConfig.h"

namespace pulse::Logger {

class LoggerManager {

public:
    ~LoggerManager();

    static LoggerManager& Instance();

    static Logger& Default();
    static Logger& GetLogger(const std::string& name);

    static void addSink(LogSinkPtr sink);
    static void start();
    static void start(const LoggerConfig& config);

private:
    AsyncWorkerPtr worker_;
    LoggerPtr default_logger_;

    std::unordered_map<std::string, LoggerPtr> loggers_;

};

};