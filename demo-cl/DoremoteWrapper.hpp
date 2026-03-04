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
#include <doremote.h>

class DoremoteWrapper {
    using Commands = std::map<std::string,std::string>;
    public:
        ~DoremoteWrapper();
        int connect ();
        void disconnect ();
        bool connected () const {return !token_.empty();};
        const std::string& sessionToken () const {return token_;}
        Commands getCommands () const;
        std::string sendCommand (const std::string &cmd) const;
        DoricoAppInfo getDoricoAppInfo () const;
        void setConfirmationCallback (void (*callback)()) {confirmationCallback_ = callback;}
        void setTerminated (bool terminated) {terminated_ = terminated;}
        bool terminated () const {return terminated_;}

    protected:
        static void saveSessionToken (const std::string &token);
        static std::string readSessionToken ();

    private:
        doremote_handle handle_ = DOREMOTE_NULL_HANDLE;
        std::string token_;
        void (*confirmationCallback_)() = nullptr;
        bool terminated_ = false;
};
