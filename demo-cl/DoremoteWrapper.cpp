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

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include "DoremoteWrapper.hpp"

DoremoteWrapper::~DoremoteWrapper () {
    doremote_destroy_instance(handle_);
}

bool DoremoteWrapper::connected () const {
    return handle_ && doremote_is_connected(handle_);
}

int DoremoteWrapper::connect () {
    if (handle_ == DOREMOTE_NULL_HANDLE)
        handle_ = doremote_create_instance();
    else if (doremote_session_token(handle_))  // already connected?
        return DOREMOTE_CONNECTED;
//    doremote_set_loglevel(handle_, DOREMOTE_LOGLEVEL_TRACE);
    int status = DOREMOTE_OFFLINE;
    token_ = readSessionToken();
    if (!token_.empty()) {
        // try to connect using the previous session token
        status = doremote_reconnect(handle_, "Doremote CL Test Client", "127.0.0.1", "4560", token_.c_str());
    }
    bool reconnected = (status == DOREMOTE_CONNECTED);
    if (!reconnected) {
        // try to establish a new connection (requires confirmation in Dorico)
        if (confirmationCallback_)
            confirmationCallback_();
        status = doremote_connect(handle_, "Doremote CL Test Client", "127.0.0.1", "4560");
    }
    if (status == DOREMOTE_CONNECTED) {
        if (!reconnected) {
            // save session token for future connections
            token_ = doremote_session_token(handle_);
            saveSessionToken(token_);
        }
    }
    else {
        token_.clear();
    }
    return status;
}

void DoremoteWrapper::saveSessionToken (const std::string &token) {
    std::ofstream ofs(".sessiontoken");
    ofs << token;
}

std::string DoremoteWrapper::readSessionToken () {
    std::string token;
    std::ifstream ifs(".sessiontoken");
    if (ifs)
        ifs >> token;
    return token;
}

void DoremoteWrapper::disconnect () {
    doremote_disconnect(handle_);
    token_.clear();
}

std::string DoremoteWrapper::sendCommand (const std::string &cmd) const {
    std::string json;
    if (connected()) {
        doremote_send_command(handle_, cmd.c_str());
        json.resize(1024);
        doremote_get_response(handle_, json.data(), static_cast<int>(json.size()));
    }
    return json;
}

DoricoAppInfo DoremoteWrapper::getDoricoAppInfo() const {
    DoricoAppInfo info;
    doremote_get_app_info(handle_, &info);
    return info;
}

DoremoteWrapper::Commands DoremoteWrapper::getCommands() const {
    Commands commands;
    if (int count = doremote_get_commands(handle_, nullptr, nullptr)) {
        std::vector<KeyValuePair> commandPairs(count);
        if (doremote_get_commands(handle_, commandPairs.data(), &count)) {
            for (const auto &[cmd, params] : commandPairs)
                commands.emplace(cmd, params);
        }
    }
    return commands;
}

static std::vector<std::string> extract_params (std::string const &paramstr) {
    std::vector<std::string> params;
    std::istringstream is(paramstr);
    std::string param;
    while (std::getline(is, param, ','))
        params.emplace_back(param);
    return params;
}

bool DoremoteWrapper::exportCommands (const std::string &fname) const {
    if (!connected())
        return false;
    std::ofstream ofs(fname, std::ios::binary);
    if (!ofs)
        return false;
    const DoricoAppInfo info = getDoricoAppInfo();
    const auto commands = getCommands();
    ofs << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        << "<commands dorico-version='" << info.number << "'>\n";
    for (const auto &[cmd, params] : commands) {
        ofs << "  <command name='" << cmd << (params.empty() ? "'/>\n" : "'>\n");
        for (auto param : extract_params(params)) {
            bool mandatory = true;
            if (param.front() == '[') {
                param = param.substr(1);
                param.pop_back();
                mandatory = false;
            }
            ofs << "    <parameter name='" << param << "' mandatory='" << (mandatory ? "yes" : "no") << "'/>\n";
        }
        if (!params.empty())
            ofs << "  </command>\n";
    }
    ofs << "</commands>\n";
    return true;
}
