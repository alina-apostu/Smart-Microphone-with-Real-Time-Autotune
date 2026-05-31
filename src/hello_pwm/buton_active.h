#ifndef BUTON_ACTIVE_H
#define BUTON_ACTIVE_H

#include "pico/stdlib.h"

#define HW_ACTIVE_BUTTON 10 // GP10 - pinul butonului fizic de active
#define LED_GREEN 14        // GP14(LED VERDE - active)

// starea globala de active/inactive
extern volatile bool g_active_state;
extern volatile bool g_just_activated;

// initializeaza pinul pentru butonul de active ca intrare cu pull-down
void buton_active_init();
void buton_active_loop();

#endif