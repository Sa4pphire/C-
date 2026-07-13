#include "taskhub/http_server.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace taskhub {
namespace {

constexpr std::size_t kMaxRequestBytes = 1024 * 1024;

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

const char* reason_phrase(int status) {
    switch (status) {
        case 200:
            return "OK";
        case 201:
            return "Created";
        case 204:
            return "No Content";
        case 400:
            return "Bad Request";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        default:
            return "Internal Server Error";
    }
}

void send_all(int client, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const auto result = send(client, data.data() + sent, data.size() - sent, 0);
        if (result <= 0) {
            return;
        }
        sent += static_cast<std::size_t>(result);
    }
}

HttpRequest parse_request(const std::string& raw_request) {
    const std::size_t headers_end = raw_request.find("\r\n\r\n");
    if (headers_end == std::string::npos) {
        throw std::runtime_error("Request headers are incomplete");
    }
    std::istringstream stream(raw_request.substr(0, headers_end));
    std::string request_line;
    if (!std::getline(stream, request_line)) {
        throw std::runtime_error("Request line is missing");
    }
    request_line = trim(request_line);
    std::istringstream request_line_stream(request_line);
    HttpRequest request;
    std::string target;
    std::string protocol;
    if (!(request_line_stream >> request.method >> target >> protocol) || protocol.rfind("HTTP/", 0) != 0) {
        throw std::runtime_error("Malformed request line");
    }

    const std::size_t query_start = target.find('?');
    request.path = target.substr(0, query_start);
    request.query = query_start == std::string::npos ? "" : target.substr(query_start + 1);

    std::string line;
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        const std::size_t separator = line.find(':');
        if (separator == std::string::npos) {
            throw std::runtime_error("Malformed request header");
        }
        request.headers.emplace(lowercase(trim(line.substr(0, separator))), trim(line.substr(separator + 1)));
    }
    request.body = raw_request.substr(headers_end + 4);
    return request;
}

std::size_t content_length(const std::string& headers) {
    const std::string lowered = lowercase(headers);
    const std::string needle = "content-length:";
    const std::size_t start = lowered.find(needle);
    if (start == std::string::npos) {
        return 0;
    }
    const std::size_t line_end = lowered.find("\r\n", start);
    const std::string value = trim(headers.substr(start + needle.size(), line_end - start - needle.size()));
    try {
        return static_cast<std::size_t>(std::stoull(value));
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid Content-Length");
    }
}

std::string make_response(const HttpResponse& response) {
    std::ostringstream stream;
    stream << "HTTP/1.1 " << response.status << ' ' << reason_phrase(response.status) << "\r\n";
    stream << "Content-Type: " << response.content_type << "\r\n";
    stream << "Content-Length: " << response.body.size() << "\r\n";
    stream << "Connection: close\r\n\r\n";
    stream << response.body;
    return stream.str();
}

void handle_client(int client, const HttpServer::Handler& handler) {
    try {
        std::string raw_request;
        std::array<char, 4096> buffer{};
        std::size_t expected_size = 0;
        while (raw_request.size() < kMaxRequestBytes) {
            const auto received = recv(client, buffer.data(), buffer.size(), 0);
            if (received <= 0) {
                break;
            }
            raw_request.append(buffer.data(), static_cast<std::size_t>(received));
            const std::size_t headers_end = raw_request.find("\r\n\r\n");
            if (headers_end != std::string::npos && expected_size == 0) {
                expected_size = headers_end + 4 + content_length(raw_request.substr(0, headers_end));
            }
            if (expected_size != 0 && raw_request.size() >= expected_size) {
                break;
            }
        }
        if (raw_request.size() >= kMaxRequestBytes) {
            throw std::runtime_error("Request is too large");
        }
        const HttpResponse response = handler(parse_request(raw_request));
        send_all(client, make_response(response));
    } catch (const std::exception&) {
        send_all(client, make_response(HttpResponse{400, R"({"error":{"message":"invalid HTTP request"}})"}));
    }
    close(client);
}

}  // namespace

HttpServer::HttpServer(Handler handler) : handler_(std::move(handler)) {}

void HttpServer::listen(unsigned short port) {
    const int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        throw std::runtime_error("Unable to create server socket");
    }
    const int enabled = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(server);
        throw std::runtime_error("Unable to bind server socket");
    }
    if (::listen(server, SOMAXCONN) < 0) {
        close(server);
        throw std::runtime_error("Unable to listen on server socket");
    }

    std::cout << "TaskHub is listening on http://127.0.0.1:" << port << std::endl;
    while (true) {
        sockaddr_in client_address{};
        socklen_t client_length = sizeof(client_address);
        const int client = accept(server, reinterpret_cast<sockaddr*>(&client_address), &client_length);
        if (client < 0) {
            continue;
        }
        std::thread(handle_client, client, std::cref(handler_)).detach();
    }
}

}  // namespace taskhub
