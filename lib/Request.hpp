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

#include <map>
#include <string>
#include "Response.hpp"
#include "utility.hpp"

class Request {
    public:
        virtual ~Request () =default;
        virtual std::string info () const =0;
        virtual std::string toString () const =0;

    protected:
        static std::string msg (const std::string &name) {
            return R"({"message":")" + name + "\"}";
        }

        static std::string msg (const std::string &name, const std::map<std::string, std::string> &entries) {
            std::string msg = R"({"message":")" + name + "\",";
            for (const auto &[key, value] : entries) {
                if (!value.empty()) {
                    msg += "\"" + key + "\":";
                    if (value[0] == '"')  // keep quotes created by json_string() unchanged
                        msg += value;
                    else
                        msg += "\"" + value + "\"";  // quote unquoted strings
                    msg += ',';
                }
            }
            msg.pop_back();
            msg += "}";
            return msg;
        }
};

template <class ExpectedResponse>
struct SendableRequest : Request {
    using Response = ExpectedResponse;
};

class ConnectRequest : public SendableRequest<SessionTokenResponse> {
    public:
        ConnectRequest (std::string clientName)
            : clientName_(std::move(clientName)) {}

        std::string info () const override {
            return "connect '"+clientName_+"'";
        }

        std::string toString () const override {
            return msg("connect", {
                {"handshakeVersion", "1.0"},
                {"clientName", util::json_string(clientName_)}
            });
        }

    private:
        const std::string clientName_;
};

class ReconnectRequest : public SendableRequest<CodeResponse> {
    public:
        ReconnectRequest (std::string clientName, std::string token)
            : clientName_(std::move(clientName)), sessionToken_(std::move(token)) {}

        std::string info () const override {
            return "connect '"+clientName_+"' using session token " + sessionToken_;
        }

        std::string toString () const override {
            return msg("connect", {
                {"handshakeVersion", "1.0"},
                {"clientName", util::json_string(clientName_)},
                {"sessionToken", util::json_string(sessionToken_)}
            });
        }

    private:
        const std::string clientName_;
        const std::string sessionToken_;
};

class AcceptSessionTokenRequest : public SendableRequest<CodeResponse> {
    public:
        AcceptSessionTokenRequest (const std::string &token) : sessionToken_(token) {}

        std::string info () const override {return "acceptSessionToken "+sessionToken_;}

        std::string toString () const override {
            return msg("acceptsessiontoken", {
                {"sessionToken", util::json_string(sessionToken_)}
            });
        }

    private:
        const std::string sessionToken_;
};

class DisconnectRequest : public SendableRequest<NoResponse> {
    public:
        std::string info () const override {return "disconnect";}
        std::string toString () const override {return msg("disconnect");}
};

class GetStatusRequest : public SendableRequest<NoResponse> {
    public:
        std::string info () const override {return "status";}
        std::string toString () const override {return msg("getstatus");}
};

class GetCommandsRequest : public SendableRequest<CommandListResponse> {
    public:
        std::string info () const override {return "getCommands";}
        std::string toString () const override {return msg("getcommands");}
};

class CommandRequest : public SendableRequest<CodeResponse> {
    public:
        CommandRequest (std::string command) : command_(std::move(command)) {}
        CommandRequest (std::string command, std::map<std::string, std::string> params)
            : command_(std::move(command)), params_(std::move(params)) {}

        std::string info () const override {return "command " + command();}
        std::string toString () const override {return msg("command", {{"command", command()}});}

    protected:
        std::string command () const {
            std::string msg = command_+"?";
            for (const auto &[key, value] : params_)
                msg += key + "=" + value + ",";
            msg.pop_back();  // remove trailing ',' or '?'
            return util::json_string(msg);
        }

    private:
        const std::string command_;
        const std::map<std::string,std::string> params_;  // key/value pairs
};

class GetAppInfoRequest : public SendableRequest<VersionResponse> {
    public:
        std::string info () const override {return "getAppInfo";}
        std::string toString () const override {return msg("getappinfo", {{"info", "version"}});}
};