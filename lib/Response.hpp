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

class Response {
    friend class DoricoRemoteImpl;
    public:
        virtual ~Response () = default;
        virtual const std::string_view type () const =0;
        const nlohmann::json& json () const {return json_;}

    protected:
        virtual bool assign (const nlohmann::json &json) {
            if (json.is_object() && json.contains("message") && json["message"] == std::string(type())) {
                json_ = json;
                return true;
            }
            return false;
        }

        static std::string getValue (const nlohmann::json &json, const std::string &key) {
            std::string value;
            if (json.is_object() && json.contains(key))
                value = json[key];
            return value;
        }

    private:
        nlohmann::json json_;
};

class NoResponse : public Response {
    friend class DoricoRemoteImpl;
    public:
        const std::string_view type () const override {return "";}

    protected:
        bool assign (const nlohmann::json &json) override {return false;}
};

class CodeResponse : public Response {
    public:
        const std::string_view type () const override {return "response";}
        const std::string& code () const {return code_;}
        const std::string& detail () const {return detail_;}

    protected:
        bool assign (const nlohmann::json &json) override {
            if (!Response::assign(json))
                return false;
            code_ = getValue(json, "code");
            detail_ = getValue(json, "detail");
            return !code_.empty();
        }

    private:
        std::string code_;
        std::string detail_;
};

class SessionTokenResponse : public Response {
    public:
        const std::string_view type () const override {return "sessiontoken";}
        const std::string& token () const {return token_;}

    protected:
        bool assign (const nlohmann::json &json) override {
            if (!Response::assign(json))
                return false;
            token_ = getValue(json, "sessionToken");
            return !token_.empty();
        }

    private:
        std::string token_;
};

class VersionResponse : public Response {
    public:
        const std::string_view type () const override {return "version";}
        const std::string& number () const {return number_;}
        const std::string& variant () const {return variant_;}

    protected:
        bool assign (const nlohmann::json &json) override {
            if (!Response::assign(json))
                return false;
            number_ = getValue(json, "number");
            variant_ = getValue(json, "variant");
            return !number_.empty() && !variant_.empty();
        }

    private:
        std::string number_;
        std::string variant_;
};

struct CommandInfo {
    explicit CommandInfo (std::string cmdName) : commandName(std::move(cmdName)) {}
    std::string commandName;
    std::vector<std::string> requiredParameters;
    std::vector<std::string> optionalParameters;
};

class CommandListResponse : public Response {
    public:
        const std::string_view type () const override {return "commandlist";}

        std::vector<CommandInfo> commandInfos () const {
            std::vector<CommandInfo> infos;
            if (json().is_object() && json().contains("commands")) {
                for (const auto &cmd : json()["commands"]) {
                    infos.emplace_back(cmd["name"]);
                    if (cmd.contains("requiredParameters")) {
                        for (const auto &param: cmd["requiredParameters"])
                            infos.back().requiredParameters.push_back(param);
                    }
                    if (cmd.contains("optionalParameters")) {
                        for (const auto &param: cmd["optionalParameters"])
                            infos.back().optionalParameters.push_back(param);
                    }
                }
            }
            return infos;
        }

    protected:
        bool assign (const nlohmann::json &json) override {
            return (Response::assign(json) && !json.contains("commands"));
        }
};