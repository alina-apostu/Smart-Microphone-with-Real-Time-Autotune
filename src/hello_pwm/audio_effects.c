#include "audio_effects.h"

volatile AudioEffect current_effect = EFFECT_CLEAN;
volatile float effect_intensity = 0.5f;

// pentru echo
// definim dimensiunea memoriei alocate ecoului 
// 12000 de numere stocate reprezinta o intarziere de 250 milisecunde (un sfert de secunda)
#define ECHO_BUFFER_SIZE 12000 
static int16_t echo_buffer[ECHO_BUFFER_SIZE];
static int echo_idx = 0;

// pentru chipmunk si deep voice
#define PITCH_BUFFER_SIZE 1920
static int16_t pitch_buffer[PITCH_BUFFER_SIZE];
static int pitch_write_idx = 0;

static float pitch_read_chipmunk = 0.0f;
static float pitch_read_deep = 0.0f;

void audio_effects_init()
{
    for(int i = 0; i < ECHO_BUFFER_SIZE; i++)
        echo_buffer[i] = 0;

    for(int i = 0; i < PITCH_BUFFER_SIZE; i++)
        pitch_buffer[i] = 0;
}

int16_t apply_audio_effect(int16_t input_sample, AudioEffect effect)
{
    int32_t output = input_sample;

    pitch_buffer[pitch_write_idx] = input_sample;

    switch(effect)
    {
        case EFFECT_ECHO:
            // sunetul citit acum 250ms
            int32_t sunet_trecut = echo_buffer[echo_idx];
            
            // daca intensitatea efectului adica a ecoului este la 70% vocea va fi la 30%
            float atenuare_voce = 1.0f - effect_intensity;
            
            float volum_ecou = effect_intensity;

            output = (int32_t)((float)input_sample * atenuare_voce) + (int32_t)((float)sunet_trecut * volum_ecou);
            
            echo_buffer[echo_idx] = (input_sample/2) + (sunet_trecut/3);

            // bucla infinita
            echo_idx = (echo_idx + 1) % ECHO_BUFFER_SIZE;
            break;
        
            case EFFECT_ROBOT:
                static int robot_ct = 0;
                robot_ct++;

                float faza_negativa = 1.0f - (2.0f * effect_intensity);

                float modulator;

                // la fiecare 45 esantioane schimbam semnul (-)
                if((robot_ct/45) % 2 == 0)
                    modulator = 1.0f; // faza normala
                else
                    modulator = faza_negativa; // faza modificata de slider

                output = (int32_t)((float)input_sample * modulator);

                echo_buffer[echo_idx] = input_sample;
                echo_idx = (echo_idx + 1) % ECHO_BUFFER_SIZE;
                break;
            case EFFECT_DEEP_VOICE:
                if(effect_intensity <= 0.01f)
                    output = input_sample;
                else
                {
                    // citim sunetul mai lent decat il scriem
                    // unda sonora se "dilata" deci o sa avem frecvente mai joase
                    float deep_step = 1.0f - (0.2f * effect_intensity);
                    pitch_read_deep += deep_step;

                    if(pitch_read_deep >= PITCH_BUFFER_SIZE)
                        pitch_read_deep -= PITCH_BUFFER_SIZE;

                    output = pitch_buffer[(int)pitch_read_deep];
                }
                
                echo_buffer[echo_idx] = input_sample;
                echo_idx = (echo_idx + 1) % ECHO_BUFFER_SIZE;
                break;
            case EFFECT_CHIPMUNK:
                if(effect_intensity <= 0.01f)
                    output = input_sample;
                else
                {
                    // scriem sunetul la viteza normala, dar il citim si il trimitem la boxe la viteza rapida (+1.7f)
                    // "turtim" unda sonora deci frecventa o sa fie mai inalta
                    float chipmunk_step = 1.0f + (0.7f * effect_intensity);
                    pitch_read_chipmunk += chipmunk_step;
                    
                    // daca am citit pana la capatul memoriei, ne intoarcem la inceput
                    if(pitch_read_chipmunk >= PITCH_BUFFER_SIZE)
                        pitch_read_chipmunk -= PITCH_BUFFER_SIZE;
                    
                    output = pitch_buffer[(int)pitch_read_chipmunk];
                }

                // chiar daca nu suntem pe efectul ecou salvam vocea 
                // astfel daca utilizatorul comuta brusc pe ecou sa avem salvate ultimele 250 ms
                echo_buffer[echo_idx] = input_sample;
                echo_idx = (echo_idx + 1) % ECHO_BUFFER_SIZE;
                break;

            case EFFECT_CLEAN:
            default: 
                output = input_sample;
                // chiar daca nu suntem pe efectul ecou salvam vocea 
                // astfel daca utilizatorul comuta brusc pe ecou sa avem salvate ultimele 250 ms
                echo_buffer[echo_idx] = input_sample;
                echo_idx = (echo_idx + 1) % ECHO_BUFFER_SIZE;
                break;
    }

    pitch_write_idx = (pitch_write_idx + 1) % PITCH_BUFFER_SIZE;

    if(output > 32767) 
        output = 32767;
    if(output < -32768)
        output = -32768;

    return (int16_t)output;
}