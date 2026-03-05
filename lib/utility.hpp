#pragma once

#include <string>

namespace util {
    std::string replace_all (std::string str, const std::string &from, const std::string &to);
    std::string json_string (const std::string &str);
}