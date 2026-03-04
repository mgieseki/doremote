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

#pragma once

#include <nlohmann/json.hpp>

using json = nlohmann::json;

class WSMessage {
    public:
        WSMessage () =default;
        WSMessage (const std::string &message) : json_(json::parse(message)) {}
        WSMessage (json message) : json_(std::move(message)) {}

        void setMessage (const std::string &message) {
            try {
                json_ = json::parse(message);
            }
            catch (json::exception&) {
                json_.clear();
            }
        }

        void setMessage (json message) {
            json_ = std::move(message);
        }

        size_t size () const {
            return json_.size();
        }

        bool hasKey (const std::string &key) const {
            return json_.contains(key);
        }

        void remove (const std::string &key) {
            try {
                json_.erase(key);
            }
            catch (json::exception&) {
            }
        }

        template <typename T>
        T get (const std::string &key) const {
            try {
                if (json_.is_object() && json_.contains(key))
                    return json_[key];
            }
            catch (json::exception&) {
            }
            return T{};
        }

        std::string getAsString (const std::string &key) const {
            try {
                if (json_.is_object() && json_.contains(key)) {
                    std::ostringstream oss;
                    oss << json_[key];
                    return oss.str();
                }
            }
            catch (json::exception&) {
            }
            return {};
        }

        std::string toString (int indent = -1) const {
            return json_.dump(indent);
        }

        const json& getJson () const {
            return json_;
        }

    private:
        json json_;
};
