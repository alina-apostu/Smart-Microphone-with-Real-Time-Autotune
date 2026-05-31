// la fiecare apelare a buton_mute_loop(), citesc starea butonului
// daca detectez o apasare noua (front crescator), comut starea de mute
// comutarea se permite doar daca sistemul e in starea active (g_active_state=true)
// starea mute e sincronizata intre butonul fizic si interfata web prin g_mute_state#include <stdio.h>
#include "buton_mute.h"
#include "buton_active.h"
#include "audio_processor.h"
#include "hardware/gpio.h"
#include "pico/time.h"

// se retine starea anterioara a butonului pt detectarea frontului
static bool last_mute_state = false;
static bool last_active_state = false;

// variabila globala, accesibila din hello_pwm.c si audio_processor.c
volatile bool g_mute_state = false;

void buton_mute_init()
{
    // butonul de mute e initializat deja in audio_processor_init()
    // initalizez butonul de active GP10 ca intrare cu pull-down
    gpio_init(HW_ACTIVE_BUTTON);
    gpio_set_dir(HW_ACTIVE_BUTTON, GPIO_IN);
    gpio_pull_down(HW_ACTIVE_BUTTON);
}

void buton_mute_loop()
{
    // citesc starea curenta a butonului de mute
    // butonul e activ HIGH, 0=neapasat, 1=apasat
    bool mute_apasat = gpio_get(HW_MUTE_BUTTON);
    bool active_apasat = !gpio_get(HW_ACTIVE_BUTTON);

    // detectez front crescator pe butonul de mute
    // conditia: butonul e apasat acum si nu era apasat la iteratia anterioara
    if (mute_apasat && !last_mute_state)
    {
        if (g_active_state) // doar daca suntem in starea active, putem comuta mute
        {
            g_mute_state = !g_mute_state;            // comut pe starea mute
            gpio_put(LED_RED, g_mute_state ? 1 : 0); // led rosu aprins; mute ON
            audio_set_mute(g_mute_state);            // transmit starea catre core 1 (audio_processor) pentru a activa/dezactiva mute pe DAC
        }
    }
    // retin starea curenta pentru iteratia urmatoare
    last_mute_state = mute_apasat;

    if (active_apasat && !last_active_state)
    {
        bool stare_curenta = gpio_get(LED_GREEN);
        gpio_put(LED_GREEN, !stare_curenta);
    }
    last_active_state = active_apasat;
}