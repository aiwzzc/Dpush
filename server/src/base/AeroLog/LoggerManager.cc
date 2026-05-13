#include "LoggerManager.h"
#include "AsyncWorker.h"
#include "LogSink.h"

namespace pulse::Logger {

LoggerManager::~LoggerManager() {
    if(this->worker_) {
        this->worker_->stop();
    }
}

LoggerManager& LoggerManager::Instance() {
    static LoggerManager instance;
    return instance;
}

Logger& LoggerManager::Default() {
    return *(Instance().default_logger_);
}

Logger& LoggerManager::GetLogger(const std::string& name) {
    auto it = Instance().loggers_.find(name);
    if(it == Instance().loggers_.end()) {
        auto new_logger = std::make_shared<Logger>(
            name, LogLevel::DEBUG, Instance().worker_.get()
        );
    }

    return *it->second;
}

void LoggerManager::addSink(LogSinkPtr sink) {
    if(!Instance().worker_) {
        Instance().worker_ = std::make_unique<AsyncWorker>();
    }

    Instance().worker_->addSink(sink);
}

void LoggerManager::start() {
    if(!Instance().worker_) {
        Instance().worker_ = std::make_unique<AsyncWorker>();
        Instance().worker_->addSink(std::make_shared<ConsoleSink>());
    }

    Instance().default_logger_ = std::make_shared<Logger>(
        "default", LogLevel::INFO, Instance().worker_.get()
    );

    Instance().worker_->start();
}

};