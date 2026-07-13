#pragma once

#include <stdexcept>
#include <string>

namespace taskhub {

class ValidationError : public std::runtime_error {
public:
    explicit ValidationError(const std::string& message) : std::runtime_error(message) {}
};

class RepositoryError : public std::runtime_error {
public:
    explicit RepositoryError(const std::string& message) : std::runtime_error(message) {}
};

}  // namespace taskhub
