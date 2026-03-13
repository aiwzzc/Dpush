#include "HttpRequest.h"
#include <assert.h>
#include <cctype>

HttpRequest::HttpRequest() : method_(HttpRequest::Method::kInvalid), version_(HttpRequest::Version::kUnknown) {}
HttpRequest::~HttpRequest() = default;

HttpRequest::HttpRequest(HttpRequest&&) noexcept = default;
HttpRequest& HttpRequest::operator=(HttpRequest&&) noexcept = default;

HttpRequest::HttpRequest(const HttpRequest&) = default;
HttpRequest& HttpRequest::operator=(const HttpRequest&) = default;

bool HttpRequest::setMethod(const char* start, const char* end) {
    assert(this->method_ == HttpRequest::Method::kInvalid);
    std::string m(start, end);

    if(m == "GET") this->method_ = HttpRequest::Method::kGet;
    else if(m == "POST") this->method_ = HttpRequest::Method::kPost;
    else if(m == "HEAD") this->method_ = HttpRequest::Method::kHead;
    else if(m == "PUT") this->method_ = HttpRequest::Method::kPut;
    else if(m == "DELETE") this->method_ = HttpRequest::Method::kDelete;
    else this->method_ = HttpRequest::Method::kInvalid;

    return this->method_ != HttpRequest::Method::kInvalid;
}

void HttpRequest::setVersion(Version v) { this->version_ = v; }
void HttpRequest::setPath(const char* start, const char* end) { this->path_.assign(start, end); }
void HttpRequest::setQuery(const char* start, const char* end) { this->query_.assign(start, end); }
void HttpRequest::addHeader(const char* start, const char* colon, const char* end) {
    std::string field(start, colon);
    ++colon;
    while(colon < end && isspace(static_cast<unsigned char>(*colon))) ++colon;

    std::string value(colon, end);
    while(!value.empty() && isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }

    this->headers_[field] = value;
}

void HttpRequest::setBody(const char* start, const char* end) { this->body_.assign(start, end); }

void HttpRequest::setMethod(Method m) { this->method_ = m; }
void HttpRequest::setPath(const std::string& path) { this->path_.assign(path); }
void HttpRequest::addHeader(const std::string& key, const std::string& value) { this->headers_[key] = value; }

HttpRequest::Method HttpRequest::method() const { return this->method_; }
HttpRequest::Version HttpRequest::version() const { return this->version_; }
const std::string& HttpRequest::path() const { return this->path_; }
const std::string& HttpRequest::query() const { return this->query_; }
const std::string& HttpRequest::body() const { return this->body_; }
const std::map<std::string, std::string, CaseInsensitiveLess>& HttpRequest::headers() const { return this->headers_; }
std::string HttpRequest::getHeader(const std::string& field) const {
    std::string result{};
    auto it = this->headers_.find(field);
    if(it != this->headers_.end()) result.assign(it->second);

    return result;
}
std::string HttpRequest::getQueryParam(const std::string& key) const {
    if(key.empty()) return "";

    std::string searchkey = key + "=";
    std::size_t pos{0};

    while((pos = this->query_.find(searchkey, pos)) != std::string::npos) {
        if(pos == 0 || this->query_[pos - 1] == '&') {
            std::size_t value_start = pos + searchkey.length();
            std::size_t value_end = this->query_.find('&', value_start);

            if(value_end == std::string::npos) return this->query_.substr(value_start);
            else return this->query_.substr(value_start, value_end - value_start);
        }

        pos += searchkey.length();
    }

    return "";
}

std::string HttpRequest::methodString() const {
    std::string res("UNKNOWN");

    switch(this->method_) {
        case HttpRequest::Method::kGet:
            res = "GET";
            break;

        case HttpRequest::Method::kPost:
            res = "POST";
            break;

        case HttpRequest::Method::kHead:
            res = "HEAD";
            break;

        case HttpRequest::Method::kPut:
            res = "PUT";
            break;

        case HttpRequest::Method::kDelete:
            res = "DELETE";
            break;

        default:
            break;
    }

    return res;
}
void HttpRequest::swap(HttpRequest& other) {
    std::swap(this->method_, other.method_);
    std::swap(this->version_, other.version_);
    std::swap(this->path_, other.path_);
    std::swap(this->query_, other.query_);
    std::swap(this->body_, other.body_);
    std::swap(this->headers_, other.headers_);
}

void HttpRequest::clear() {
    this->method_ = HttpRequest::Method::kInvalid;
    this->version_ = HttpRequest::Version::kUnknown;
    this->path_.clear();
    this->query_.clear();
    this->body_.clear();
    this->headers_.clear();
}