#include <stdio.h>
#include "buton_active.h"
#include "buton_mute.h"
#include "audio_processor.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include "audio_effects.h"

#define LED_BLUE 16

static bool last_active_state = false;
volatile bool g_active_state = false;
// volatile bool g_just_activated = false;

void buton_active_init()
{
    gpio_init(HW_ACTIVE_BUTTON);
    gpio_set_dir(HW_ACTIVE_BUTTON, GPIO_IN);
    gpio_pull_down(HW_ACTIVE_BUTTON);
}

void buton_active_loop()
{
    bool active_apasat = gpio_get(HW_ACTIVE_BUTTON);

    if (active_apasat && !last_active_state)
    {
        g_active_state = !g_active_state;
        gpio_put(LED_GREEN, g_active_state ? 0 : 1);

        if (!g_active_state)
        {
            gpio_put(LED_RED, 0);
            gpio_put(LED_BLUE, 0);
            g_mute_state = false;
            audio_set_mute(false);
            current_effect = EFFECT_CLEAN; // resetam efectul
            printf("Buton Active: OFF - mute si efect resetate\n");
        }
        else
        {
            printf("Buton Active: ON\n");
        }
    }
    last_active_state = active_apasat;
}