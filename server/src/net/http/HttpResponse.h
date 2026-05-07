#pragma once

#include <map>
#include <string>

class HttpResponse {

public:
    enum class HttpStatusCode {
        kUnknown,
        k200Ok = 200,
        k301MovedPermanently = 301,
        k400BadRequest = 400,
        k403Forbidden = 403,
        k404NotFound = 404,
        K503ServiceUnavailable = 503,
        k101SwitchingProtocols = 101
    };

    explicit HttpResponse(bool close);
    ~HttpResponse();

    void setStatusCode(HttpStatusCode code);
    void setStatusMessage(const std::string& statusMessage);
    void setCloseConnection(bool on);
    void setContentType(const std::string& contentType);
    void setCookie(const std::string& cookie);
    bool closeConnection() const;
    void addHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);
    void appendToBuffer(std::string& output) const;
    void appendToHeadBuffer(std::string& output, std::size_t body_size) const;

private:
    std::map<std::string, std::string> headers_;
    HttpStatusCode statusCode_;
    std::string statusMessage_; // 状态行
    std::string content_type_;
    std::string body_;
    std::string cookie_;
    bool closeConnection_;
};