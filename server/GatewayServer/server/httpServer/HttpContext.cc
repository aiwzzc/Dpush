#include "HttpContext.h"
#include "muduo/net/Buffer.h"
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
bool HttpContext::parseRequest(muduo::net::Buffer* buf) {
    bool ok{true};
    bool hasMore{true};
    
    while(hasMore) {
        if(this->state_ == HttpContext::HttpRequestParseState::kExpectRequestLine) {
            const char* crlf = buf->findCRLF();
            if(crlf != NULL) {
                ok = processRequestLine(buf->peek(), crlf);

                if(ok) {
                    this->state_ = HttpContext::HttpRequestParseState::kExpectHeaders;
                    buf->retrieve(crlf - buf->peek() + 2);

                } else {
                    hasMore = false;
                }

            } else {
                hasMore = false;
            }

        } else if(this->state_ == HttpContext::HttpRequestParseState::kExpectHeaders) {
            const char* crlf = buf->findCRLF();
            if(crlf != NULL) {
                if(crlf == buf->peek()) {
                    this->state_ = HttpContext::HttpRequestParseState::kExpectBody;
                    buf->retrieve(2);

                } else {
                    const char* colon = buf->find(":", 1);
                    if(colon != nullptr && colon < crlf) {
                        this->request_.addHeader(buf->peek(), colon, crlf);
                        
                    } else {}

                    buf->retrieve(crlf - buf->peek() + 2);
                }

            } else {
                hasMore = false;
            }

        } else if(this->state_ == HttpContext::HttpRequestParseState::kExpectBody) {
            auto strlen_opt = this->request_.getHeader("Content-Length");

            if(!strlen_opt.has_value()) {
                this->state_ = HttpContext::HttpRequestParseState::kGotAll;

                return ok;
            }

            const std::string& strlen = *strlen_opt.value();

            std::size_t content_length = std::stoul(strlen);

            if(buf->readableBytes() >= content_length) {
                this->request_.setBody(buf->peek(), buf->peek() + content_length);
                this->state_ = HttpContext::HttpRequestParseState::kGotAll;
                
                buf->retrieve(content_length);

            } else {
                hasMore = false;
            }

        } else if(this->state_ == HttpContext::HttpRequestParseState::kGotAll) {
            hasMore = false;
        }
    }

    return ok;
}