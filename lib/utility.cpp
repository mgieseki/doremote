#include <nlohmann/json.hpp>
#include "utility.hpp"

/** Replaces all occurrences of a substring with another one.
 *  @param[in] str string in which the substring is searched for
 *  @param[in] from substring to search
 *  @param[in] to replacement for the found substrings
 *  @return string with the replacements applied */
std::string util::replace_all (std::string str, const std::string &from, const std::string &to) {
    size_t pos = str.find(from);
    while (pos != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos = str.find(from, pos + from.length());
    }
    return str;
}

/** Converts a given string to a JSON string, i.e. special characters are escaped
 *  and the entire string is enclosed in quotes. E.g. ab"cd => "ab\"cd" */
std::string util::json_string (const std::string &str) {
    nlohmann::json json = str;
    return json.dump();
}