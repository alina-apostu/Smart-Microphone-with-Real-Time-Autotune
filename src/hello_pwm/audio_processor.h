// audio_processor.h -
// header folosit pt modulul de procesare audio, care va rula pe Core 1 al Pico, se ocupa de citirea datelor de la microfon, procesarea semnalului si trimiterea catre DAC prin I2S folosind PIO

#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include "pico/stdlib.h"

// initializ tot hardware ul audio (ADC, PIO, DMA, pinul fizic de mute)
void audio_processor_init();

// funct principala care va rula pe Core 1
// citeste datele de la microfon, filtreaza, amplifica si trimite sunetul la DAC
void audio_processor_loop();

// functie pentru controlul MUTE din exterior (server web)
void audio_set_mute(bool mute);

#endif