#pragma once

#include "HttpRequest.h"

namespace muduo {

namespace net {
    class Buffer;
};

};

class HttpContext {

public:
    enum class HttpRequestParseState {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll
    };

    HttpContext();
    ~HttpContext();

    bool parseRequest(muduo::net::Buffer* buf);
    bool gotAll() const;
    void reset();
    const HttpRequest& request() const;
    HttpRequest& request();

private:
    bool processRequestLine(const char* begin, const char* end);

    HttpRequestParseState state_;
    HttpRequest request_;
};