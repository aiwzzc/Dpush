#include "HttpResponse.h"

HttpResponse::HttpResponse(bool close) : statusCode_(HttpResponse::HttpStatusCode::kUnknown), closeConnection_(close) {}
HttpResponse::~HttpResponse() = default;

void HttpResponse::setStatusCode(HttpStatusCode code) 
{ this->statusCode_ = code; }

void HttpResponse::setStatusMessage(const std::string& statusMessage) 
{ this->statusMessage_.assign(statusMessage); }

void HttpResponse::setCloseConnection(bool on) { this->closeConnection_ = on; }

void HttpResponse::setContentType(const std::string& contentType) 
{ this->addHeader("Content-Type", contentType); }

void HttpResponse::setCookie(const std::string& cookie) 
{ this->cookie_.assign(cookie); }

bool HttpResponse::closeConnection() const 
{ return this->closeConnection_; }

void HttpResponse::addHeader(const std::string& key, const std::string& value) 
{ this->headers_[key] = value; }

void HttpResponse::setBody(const std::string& body) 
{ this->body_.assign(body); }

void HttpResponse::appendToBuffer(std::string& output) const {
    output.append("HTTP/1.1 ");
    output.append(std::to_string(static_cast<int>(this->statusCode_)));
    output.append(" ");
    output.append(this->statusMessage_);
    output.append("\r\n");

    if(this->closeConnection_) output.append("Connection: close\r\n");
    else output.append("Connection: Keep-alive\r\n");

    if(!this->cookie_.empty()) {
        output.append("set-cookie: sid=" + this->cookie_ + "; ");
        output.append("Path=/; ");  // 添加路径设置
        output.append("HttpOnly; Max-Age=86400; SameSite=Strict\r\n");
    }

    output.append("Content-Length: ");
    output.append(std::to_string(this->body_.size()));
    output.append("\r\n");

    for(const auto& [key, value] : this->headers_) {
        output.append(key);
        output.append(": ");
        output.append(value);
        output.append("\r\n");
    }

    // output.append("Content-Type:application/json;charset=utf-8\r\n");
    output.append("\r\n");
    output.append(this->body_);
}