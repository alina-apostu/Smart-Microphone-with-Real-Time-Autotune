#ifndef BUTON_MUTE_H
#define BUTON_MUTE_H

#include "pico/stdlib.h"

// Pini butoane
#define HW_MUTE_BUTTON 11   // GP11
#define HW_ACTIVE_BUTTON 10 // GP10

// Pini LED-uri
#define LED_RED 15   // GP15(LED ROSU - mute)
#define LED_GREEN 14 // GP14(LED VERDE - active)

// starrea globala de mute, actualiz atat de buton, cat si de interfata web
extern volatile bool g_mute_state;

// initializeaza pinul pentru butonul de mute ca intrare cu pull-down
void buton_mute_init();

// verifica starea butonului fizic de mute la fiecare iteratie
// comuta starea de mute daca detecteaza o apasare noua a butonului
void buton_mute_loop();

#endif