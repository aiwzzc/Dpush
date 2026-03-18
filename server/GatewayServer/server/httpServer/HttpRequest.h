#pragma once

#include <string>
#include <map>
#include <algorithm>
#include <unordered_map>

struct CaseInsensitiveLess {
    bool operator()(const std::string& a, const std::string& b) const {
        return std::lexicographical_compare(
            a.begin(), a.end(),
            b.begin(), b.end(),[](unsigned char c1, unsigned char c2) {
                return std::tolower(c1) < std::tolower(c2);
            }
        );
    }
};

class HttpRequest {

public:
    enum class Method {
        kInvalid, kGet, kPost, kHead, kPut, kDelete
    };

    enum class Version {
        kUnknown, kHttp10, kHttp11, kHttp20
    };

    HttpRequest();
    ~HttpRequest();

    HttpRequest(HttpRequest&&) noexcept;
    HttpRequest& operator=(HttpRequest&&) noexcept;

    HttpRequest(const HttpRequest&);
    HttpRequest& operator=(const HttpRequest&);

    bool setMethod(const char* start, const char* end);
    void setVersion(Version v);
    void setPath(const char* start, const char* end);
    void setQuery(const char* start, const char* end);
    void addHeader(const char* start, const char* colon, const char* end);
    void setBody(const char* start, const char* end);

    void setMethod(Method m);
    void setPath(const std::string& path);
    void addHeader(const std::string& key, const std::string& value);

    Method method() const;
    Version version() const;
    const std::string& path() const;
    const std::string& query() const;
    const std::string& body() const;
    const std::map<std::string, std::string, CaseInsensitiveLess>& headers() const;
    std::string getHeader(const std::string& field) const;
    std::string getQueryParam(const std::string& key) const;

    std::string methodString() const;
    void swap(HttpRequest& other);

    void clear();

private:
    Method method_;
    Version version_;
    std::string path_;
    std::string query_;
    std::string body_;
    std::map<std::string, std::string, CaseInsensitiveLess> headers_;
    // std::unordered_map<std::string, std::string> cookies_;
};