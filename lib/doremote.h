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

#ifndef DOREMOTE_H
#define DOREMOTE_H

#include "doremote-export.h"

#define DOREMOTE_INVALID_PARAMETERS (-1)
#define DOREMOTE_CONNECTED 0
#define DOREMOTE_CONNECTION_DENIED 1
#define DOREMOTE_OFFLINE 2

#define DOREMOTE_LOGLEVEL_TRACE 1
#define DOREMOTE_LOGLEVEL_INFO  2
#define DOREMOTE_LOGLEVEL_WARN  3
#define DOREMOTE_LOGLEVEL_ERROR 4
#define DOREMOTE_LOGLEVEL_OFF   5

#define DOREMOTE_NULL_HANDLE 0

typedef struct DoricoRemoteHandle *doremote_handle;

typedef struct {
    char key[64];
    char value[256];
} KeyValuePair;

typedef struct {
    char number[16];
    char variant[16];
} DoricoAppInfo;

typedef void (*StatusCallback)(KeyValuePair *status, int size, void *userdata);
typedef void (*TerminationCallback)(void *userdata);

#ifdef __cplusplus
extern "C" {
# endif

DOREMOTE_EXPORT doremote_handle doremote_create_instance ();
DOREMOTE_EXPORT void doremote_destroy_instance (doremote_handle handle);
DOREMOTE_EXPORT void doremote_set_loglevel (doremote_handle handle, int level);
DOREMOTE_EXPORT int doremote_connect (doremote_handle handle, const char *name, const char *host, const char *port);
DOREMOTE_EXPORT int doremote_reconnect (doremote_handle handle, const char *name, const char *host, const char *port, const char *token);
DOREMOTE_EXPORT void doremote_disconnect (doremote_handle handle);
DOREMOTE_EXPORT int doremote_is_connected (doremote_handle handle);
DOREMOTE_EXPORT const char* doremote_session_token (doremote_handle handle);
DOREMOTE_EXPORT void doremote_get_app_info (doremote_handle handle, DoricoAppInfo *info);
DOREMOTE_EXPORT int doremote_send_command (doremote_handle handle, const char *command);
DOREMOTE_EXPORT void doremote_update_status (doremote_handle handle);
DOREMOTE_EXPORT int doremote_get_commands (doremote_handle handle, KeyValuePair *result, int *size);
DOREMOTE_EXPORT const char* doremote_get_response (doremote_handle handle, char *json, int size);
DOREMOTE_EXPORT void doremote_set_status_callback (doremote_handle handle, StatusCallback callback, void *userdata);
DOREMOTE_EXPORT void doremote_set_termination_callback (doremote_handle handle, TerminationCallback callback, void *userdata);

#ifdef __cplusplus
}
#endif

#endif