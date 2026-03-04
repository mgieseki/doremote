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

#include <memory>
#include <string>
#include "WSMessage.hpp"
#include "Request.hpp"

class DoricoRemoteImpl;

struct ConnectionException : std::exception {
    const char* what () const noexcept override {return "Dorico not running";}
};

class DoricoRemote {
    public:
        using StatusCallback = std::function<void (const WSMessage &)>;
        using TerminationCallback = std::function<void ()>;
        enum LogLevel {LL_TRACE, LL_INFO, LL_WARN, LL_ERROR, LL_OFF};

        DoricoRemote ();
        DoricoRemote (const DoricoRemote &) = delete;
        DoricoRemote (DoricoRemote &&) = default;
        ~DoricoRemote ();
        DoricoRemote & operator= (const DoricoRemote &) = delete;
        bool connect (const std::string &clientName, const std::string &host, const std::string &port) const;
        bool connect (const std::string &clientName, const std::string &host, const std::string &port, const std::string &token) const;
        void disconnect () const;
        const std::string& sessionToken () const;

        template <class Request>
        Request::Response send (const Request &request) const {
            typename Request::Response response;
            send(request, response);
            return response;
        }

        WSMessage getMessage (const std::string &type) const;
        std::string getMessageValue (const std::string &type, const std::string &key) const;
        void setStatusCallback (StatusCallback callback) const;
        void setTerminationCallback (TerminationCallback callback) const;
        void setLogLevel (LogLevel level) const;

    protected:
        void send (const Request &request, Response &response) const;

    private:
        std::unique_ptr<DoricoRemoteImpl> impl_;
};