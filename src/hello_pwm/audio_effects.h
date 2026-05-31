#ifndef AUDIO_EFFECTS_H
#define AUDIO_EFFECTS_H

#include "pico/stdlib.h"
#include <stdint.h>

// lista efectelor
typedef enum {
    EFFECT_CLEAN = 0,
    EFFECT_ECHO,
    EFFECT_ROBOT,
    EFFECT_DEEP_VOICE,
    EFFECT_CHIPMUNK
} AudioEffect;


// variabila globala partajata 
// extern inseamna ca memoria ei este alocata altundeva intr-un fisier .c
// volatile forteaza procesorul sa o citeasca direct din RAM la fiecare esantion 
extern volatile AudioEffect current_effect;
extern volatile float effect_intensity;

void audio_effects_init();

int16_t apply_audio_effect(int16_t input_sample, AudioEffect effect);

#endif