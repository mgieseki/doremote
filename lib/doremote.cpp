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

#include "doremote.h"
#include "DoricoRemote.hpp"

struct DoricoRemoteHandle {
    DoricoRemote instance;
};

extern "C" {
    doremote_handle doremote_create_instance () {
        return new DoricoRemoteHandle(DoricoRemote());
    }

    void doremote_destroy_instance (doremote_handle handle) {
        delete handle;
    }

    void doremote_set_loglevel (doremote_handle handle, int level) {
        if (handle) {
            DoricoRemote::LogLevel loglevel = DoricoRemote::LL_OFF;
            if (level < DOREMOTE_LOGLEVEL_TRACE)
                level = DOREMOTE_LOGLEVEL_TRACE;
            else {
                switch (level) {
                    case DOREMOTE_LOGLEVEL_TRACE: loglevel = DoricoRemote::LL_TRACE; break;
                    case DOREMOTE_LOGLEVEL_INFO:  loglevel = DoricoRemote::LL_INFO; break;
                    case DOREMOTE_LOGLEVEL_WARN:  loglevel = DoricoRemote::LL_WARN; break;
                    case DOREMOTE_LOGLEVEL_ERROR: loglevel = DoricoRemote::LL_ERROR; break;
                    default: loglevel = DoricoRemote::LL_OFF; break;
                }
            }
            handle->instance.setLogLevel(loglevel);
        }
    }

    int doremote_connect (doremote_handle handle, const char *name, const char *host, const char *port) {
        if (!handle || !name)
            return DOREMOTE_INVALID_PARAMETERS;
        if (!host)
            host = "127.0.0.1";
        if (!port)
            port = "4560";  // default port set in Dorico
        try {
            bool success = handle->instance.connect(name, host, port);
            return success ? DOREMOTE_CONNECTED : DOREMOTE_CONNECTION_DENIED;
        }
        catch (...) {
            return DOREMOTE_OFFLINE;
        }
    }

    int doremote_reconnect (doremote_handle handle, const char *name, const char *host, const char *port, const char *token) {
        if (!handle || !name || !token)
            return DOREMOTE_INVALID_PARAMETERS;
        if (!host)
            host = "127.0.0.1";
        if (!port)
            port = "4560";  // default port set in Dorico
        try {
            bool success = handle->instance.connect(name, host, port, token);
            return success ? DOREMOTE_CONNECTED : DOREMOTE_CONNECTION_DENIED;
        }
        catch (...) {
            return DOREMOTE_OFFLINE;
        }
    }

    void doremote_disconnect (doremote_handle handle) {
        if (handle)
            handle->instance.disconnect();
    }

    const char* doremote_session_token (doremote_handle handle) {
        if (handle) {
            const std::string &token = handle->instance.sessionToken();
            if (!token.empty())
                return token.c_str();
        }
        return nullptr;
    }

    void doremote_get_app_info (doremote_handle handle, DoricoAppInfo *info) {
        if (!info)
            return;
        if (!handle)
            *info->number = *info->variant = '\0';
        else {
            VersionResponse response = handle->instance.send(GetAppInfoRequest());
            strcpy(info->number, response.number().c_str());
            strcpy(info->variant, response.variant().c_str());
        }
    }

    int doremote_send_command (doremote_handle handle, const char *command) {
        if (!handle || !command)
            return 0;
        CodeResponse response = handle->instance.send(CommandRequest(command));
        return response.code() == "kOK";
    }

    const char* doremote_get_response (doremote_handle handle, char *json, int size) {
        if (!handle || !json)
            return nullptr;
        const nlohmann::json msg = handle->instance.getMessage("response");
        strncpy(json, msg.dump(4).c_str(), size);
        return json;
    }

    void doremote_update_status (doremote_handle handle) {
        if (handle)
            handle->instance.send(GetStatusRequest());
    }

    int doremote_get_commands (doremote_handle handle, KeyValuePair *result, int *size) {
        if (!handle)
            return 0;
        CommandListResponse response = handle->instance.send(GetCommandsRequest());
        const auto &commandInfos = response.commandInfos();
        int count = static_cast<int>(commandInfos.size());
        if (!result || !size)
            return count;
        if (count > *size)
            return 0;

        *size = count;
        for (const CommandInfo &info : commandInfos) {
            std::strncpy(result[*size-count].key, info.commandName.c_str(), 63);
            std::string paraminfo;
            for (const auto &param : info.requiredParameters)
                paraminfo += param + ",";
            for (const auto &param : info.optionalParameters)
                paraminfo += "[" + param + "],";
            if (!paraminfo.empty())
                paraminfo.pop_back();
            std::strncpy(result[*size-count].value, paraminfo.c_str(), 255);
            count--;
        }
        return *size;
    }

    void doremote_set_status_callback (doremote_handle handle, StatusCallback callback, void *userdata) {
        if (!handle || !callback)
            return;
        handle->instance.setStatusCallback([callback, userdata](const nlohmann::json &status_msg) {
            std::vector<KeyValuePair> status_kv;
            status_kv.reserve(status_msg.size());
            for (auto &[key, value] : status_msg.items()) {
                KeyValuePair kv;
                strncpy(kv.key, key.c_str(), 63);
                strncpy(kv.value, value.dump().c_str(), 255);
                status_kv.emplace_back(kv);
            }
            callback(status_kv.data(), static_cast<int>(status_kv.size()), userdata);
        });
    }

    void doremote_set_termination_callback (doremote_handle handle, TerminationCallback callback, void *userdata) {
        if (handle && callback) {
            handle->instance.setTerminationCallback([callback, userdata]() {
                callback(userdata);
            });
        }
    }
}