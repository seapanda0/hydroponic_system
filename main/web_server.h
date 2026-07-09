#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdbool.h>
#include <stddef.h>

// Connects to WiFi in station mode; the HTTP server starts automatically once an IP is obtained.
void wifi_init_sta(void);

// True once the station has an IP (safe to poll from any task).
bool wifi_is_connected(void);

// --- Implemented in hydroponic_board.c: status/control glue used by the HTTP handlers ---

// Fills buf with the system status JSON served at /api/status
void hydro_build_status_json(char *buf, size_t buf_len);

// Returns the EC history JSON served at /api/ec_history.
// Uses a static buffer; only call from the httpd task.
const char *hydro_build_ec_history_json(void);

// Executes a device command from /api/toggle. Pass a negative concentration when not provided.
void hydro_web_toggle(const char *device, float concentration);

#endif // WEB_SERVER_H
