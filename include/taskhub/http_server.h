#pragma once

#include <functional>
#include <map>
#include <string>

namespace taskhub {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status{200};
    std::string body;
    std::string content_type{"application/json; charset=utf-8"};
};

class HttpServer {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    explicit HttpServer(Handler handler);
    void listen(unsigned short port);

private:
    Handler handler_;
};

}  // namespace taskhub
