// Copyright (C) 2026 Martin Gieseking
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

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
