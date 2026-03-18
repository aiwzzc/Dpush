#include "HttpContext.h"
#include <algorithm>

HttpContext::HttpContext() : state_(HttpContext::HttpRequestParseState::kExpectRequestLine) {}
HttpContext::~HttpContext() = default;

bool HttpContext::gotAll() const { return this->state_ == HttpContext::HttpRequestParseState::kGotAll; }
void HttpContext::reset() {
    this->state_ = HttpContext::HttpRequestParseState::kExpectRequestLine;
    HttpRequest request{};
    this->request_.swap(request);
}
const HttpRequest& HttpContext::request() const { return this->request_; }
HttpRequest& HttpContext::request() { return this->request_; }

bool HttpContext::processRequestLine(const char* begin, const char* end) {
    bool succeed{false};
    const char* start = begin;
    const char* space = std::find(start, end, ' ');

    if(space != end && this->request_.setMethod(start, space)) {
        start = space + 1;
        space = std::find(start, end, ' ');

        if(space != end) {
            const char* question = std::find(start, space, '?');
            if(question != space) {
                this->request_.setPath(start, question);
                this->request_.setQuery(question + 1, space);

            } else {
                this->request_.setPath(start, space);
            }
        }

        start = space + 1;
        succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1.");

        if(succeed) {
            if(*(end - 1) == '1') this->request_.setVersion(HttpRequest::Version::kHttp11);
            else if(*(end - 1) == '0') this->request_.setVersion(HttpRequest::Version::kHttp10);
            else succeed = false;
        }
    }

    return succeed;
}
#include <iostream>
bool HttpContext::parseRequest(std::string& buf) {
    bool ok{true};
    bool hasMore{true};
    
    while(hasMore) {
        if(this->state_ == HttpContext::HttpRequestParseState::kExpectRequestLine) {
            std::size_t crlf = buf.find("\r\n");
            if(crlf != std::string::npos) {
                ok = processRequestLine(buf.c_str(), buf.c_str() + crlf);

                if(ok) {
                    this->state_ = HttpContext::HttpRequestParseState::kExpectHeaders;
                    buf.erase(0, crlf + 2);

                } else {
                    hasMore = false;
                }

            } else {
                hasMore = false;
            }

        } else if(this->state_ == HttpContext::HttpRequestParseState::kExpectHeaders) {
            std::size_t crlf = buf.find("\r\n");
            if(crlf != std::string::npos) {
                if(crlf == 0) {
                    this->state_ = HttpContext::HttpRequestParseState::kExpectBody;
                    buf.erase(0, 2);

                } else {
                    std::size_t colon = buf.find(":");
                    if(colon != std::string::npos && colon < crlf) {
                        this->request_.addHeader(buf.c_str(), buf.c_str() + colon, buf.c_str() + crlf);
                        
                    } else {}

                    buf.erase(0, crlf + 2);
                }

            } else {
                hasMore = false;
            }

        } else if(this->state_ == HttpContext::HttpRequestParseState::kExpectBody) {
            std::string strlen = this->request_.getHeader("Content-Length");

            if(strlen.empty()) {
                this->state_ = HttpContext::HttpRequestParseState::kGotAll;

                return ok;
            }

            std::size_t content_length = std::stoul(strlen);

            if(buf.size() >= content_length) {
                this->request_.setBody(buf.c_str(), buf.c_str() + content_length);
                this->state_ = HttpContext::HttpRequestParseState::kGotAll;
                buf.erase(0, content_length);

            } else {
                hasMore = false;
            }

        } else if(this->state_ == HttpContext::HttpRequestParseState::kGotAll) {
            hasMore = false;
        }
    }

    return ok;
}