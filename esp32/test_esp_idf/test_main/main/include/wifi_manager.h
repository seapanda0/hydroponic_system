// Prevent this header from being included more than once in one build unit.
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

// Defines the bool type used by the Wi-Fi API.
#include <stdbool.h>

// If C++ includes this header, keep the function names compatible with C.
#ifdef __cplusplus
extern "C" {
#endif

// Initialize the Wi-Fi station module and start connecting.
void wifi_manager_init(void);
// Return true only when the station is connected and has an IP address.
bool wifi_manager_is_connected(void);

// Close the C-compatible block for C++ compilers.
#ifdef __cplusplus
}
#endif

// End of the include guard started above.
#endif
