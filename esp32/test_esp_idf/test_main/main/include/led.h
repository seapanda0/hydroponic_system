// Prevent this header from being included more than once in one build unit.
#ifndef LED_H
#define LED_H

// Defines the bool type used by the LED API.
#include <stdbool.h>

// If C++ includes this header, keep the function names compatible with C.
#ifdef __cplusplus
extern "C" {
#endif

// Prepare the LED GPIO so it can be controlled.
void led_init(void);
// Explicitly turn the LED on or off.
void led_set(bool on);
// Flip the LED from on to off or from off to on.
void led_toggle(void);

// Close the C-compatible block for C++ compilers.
#ifdef __cplusplus
}
#endif

// End of the include guard started above.
#endif
