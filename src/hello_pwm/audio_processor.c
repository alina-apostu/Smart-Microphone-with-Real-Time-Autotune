#include "audio_processor.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "audio_i2s.pio.h" // Generat automat din .pio
#include "buton_mute.h"
#include "buton_active.h"
#include "audio_effects.h"
#include "hardware/dma.h"

#define MIC_ADC_PIN 26
#define I2S_DATA_PIN 27 // 21
#define I2S_BCLK_PIN 21 // 19
#define I2S_WSEL_PIN 22 // 20
#define DAC_MUTE_PIN 12

// numarul de esantioane procesate o data in fiecare "cos" din buffer
#define SAMPLES_PER_BLOCK 64

static volatile bool is_muted = false;
static uint audio_sm;
static PIO audio_pio = pio0;

// cele doua buffere
uint16_t adc_buffer[2][SAMPLES_PER_BLOCK];  // ce vine de la microfon
uint32_t dac_buffer[2][SAMPLES_PER_BLOCK]; // ce merge la boxe

// indexul canalelor pentru ca Pico are 12 canale DMA
int dma_adc_chan; 
int dma_dac_chan;

// ofsset d eliniste MAX9814
// #define MIC_OFFSET 2048

void audio_processor_init()
{
    // config ADC
    adc_init();
    adc_gpio_init(MIC_ADC_PIN); // dezactivez functiile digitale ale pinului GP26
    adc_select_input(0);        // GP26 este ADC0, setează ADC ul sa citeasca de pe canalul 0

    // activam FIFO ca sa trimitem datele catre DMA
    adc_fifo_setup(true, true, 1, false, false);
    // setam ceasul intern al ADC-ului sa citeasca singur la exact 48.000Hz
    adc_set_clkdiv(48000000.0f / 48000.0f - 1);

    // 48MHz este frecventa de clock a ADC ului asa cum vine din fabrica

    // urechea umana capteaza pana la 20KHz, deci conform teoremei lui Nyquist, frecv de esan trebuie sa fie macar
    // dubla celei mai mari frecv pe care vrem sa o captam deci macar 40kHz, noi avem 48kHz

    // timp necesar pentru a umple sau goli un buffer intreg 64/48000 s= 1.33 ms
    // deci traseul sunetului este 1.33ms(umplere buffe intrare)+ procesare(instantanee)+ 1.33ms(golire buffer iesire) deci 2.66ms
    // care respecta limita noastra impusa in cerintele functionale de 20ms

    // config Mute hardware (DAC si Buton)
    gpio_init(DAC_MUTE_PIN);
    gpio_set_dir(DAC_MUTE_PIN, GPIO_OUT); // config pinul de mute al DAC ului ca out
    gpio_put(DAC_MUTE_PIN, 0);

    gpio_init(HW_MUTE_BUTTON);             
    gpio_set_dir(HW_MUTE_BUTTON, GPIO_IN); // setează pinul butonului ca intrare, deoarece Pico trebuie să citeasca dacă butonul este apasat
    gpio_pull_down(HW_MUTE_BUTTON);

    // config PIO I2S pentru DAC
    // PIO pio = pio0;
     // placa are 2 blocuri hardware pio fiecare cu 4 state machine
    audio_sm = pio_claim_unused_sm(audio_pio, true);  // cauta prima masina de stari care e libera
    uint offset = pio_add_program(audio_pio, &audio_i2s_program); // sadresa de memorie unde a fost pus codul de asamblare PIO

    float system_clock = clock_get_hz(clk_sys); // closck procesor
    // calcul divizor pentru aprox 48kHz
    float clk_div = system_clock / (48000 * 32 * 2); // 32 ptc trimit 16 biti pe canal stg si 16 pe drt, 2 de la cele doua cicluri pt PIO BLCK sus si jos
    audio_i2s_program_init(audio_pio, audio_sm, offset, I2S_DATA_PIN, I2S_BCLK_PIN, clk_div);

    // configurare DMA (gasire 2 canale disponibile din cele 12)
    dma_adc_chan = dma_claim_unused_channel(true);
    dma_dac_chan = dma_claim_unused_channel(true);

    // se ia de la adc si se pune secvential in adc_buffer
    dma_channel_config c_adc = dma_channel_get_default_config(dma_adc_chan);
    // cati biti de memorie sa copieze la o singura mutare
    channel_config_set_transfer_data_size(&c_adc, DMA_SIZE_16); // am ales de 16 pentru ca ADC ul este pe 12 biti si in acest buffer primul de la ADC
    channel_config_set_read_increment(&c_adc, false); // citeste din acelasi loc (de la microfon)
    channel_config_set_write_increment(&c_adc, true); // scrie avansand in memorie
    channel_config_set_dreq(&c_adc, DREQ_ADC); // DREQ = data request

    // luam secventa din dac_buffer si dam catre PIO(boxe)
    dma_channel_config c_dac = dma_channel_get_default_config(dma_dac_chan);
    channel_config_set_transfer_data_size(&c_dac, DMA_SIZE_32);
    channel_config_set_read_increment(&c_dac, true); // citeste avansand in memorie
    channel_config_set_write_increment(&c_dac, false); // scrie in acelasi loc (PIO)
    // actioneaza doar cand PIO transmite ca e gata sa ia un al cadru sonor
    channel_config_set_dreq(&c_dac, pio_get_dreq(audio_pio, audio_sm, true));

    dma_channel_set_config(dma_adc_chan, &c_adc, false);
    dma_channel_set_config(dma_dac_chan, &c_dac, false);

    // sursa este adresa registrului FIFO din modulul hardware ADC
    dma_channel_set_read_addr(dma_adc_chan, &adc_hw->fifo, false);
    // destinatia este adresa registrului TX(transmit) FIFO din perifericul PIO
    dma_channel_set_write_addr(dma_dac_chan, &audio_pio->txf[audio_sm], false);


    // initializare efecte audio
    audio_effects_init();
}

void audio_set_mute(bool mute)
{
    is_muted = mute; 
}

void audio_processor_loop()
{
    // variabila care tine pe care "cos" suntem 0 sau 1
    int active_idx = 0;
    // estimarea initiala pentru nivelul de liniste al microfonului ~1500
    // codul o va auto-ajusta in timp real pentru a mentine linistea la 0
    static float static_offset = 1500.0f;

    // umplem primul buffer inainte sa intram in blucla infinita
    dma_channel_set_write_addr(dma_adc_chan, adc_buffer[active_idx], false);
    dma_channel_set_trans_count(dma_adc_chan, SAMPLES_PER_BLOCK, true);
    adc_run(true); // dau drumul la microfon sa citeasca continuu

    // asteptam sa se umple un "cos"
    dma_channel_wait_for_finish_blocking(dma_adc_chan);

    while(true)
    {
        // aflam care este "cosul" pe care trebuie sa il folosim
        int next_idx = (active_idx +1)%2;

        // ii spunem dma ului sa stranga in urmatorul "cos", in fundal
        dma_channel_set_write_addr(dma_adc_chan, adc_buffer[next_idx], false);
        dma_channel_set_trans_count(dma_adc_chan, SAMPLES_PER_BLOCK, true); // true = start dma

        // procesam bufferul pe care l-am primit anterior
        if(is_muted || !g_active_state)
        {
            // setam hardware mut si stingem led-ul galben
            gpio_put(DAC_MUTE_PIN, 1);
            gpio_put(17, 0);
            // umplem buffer ul de iesire doar cu 0 (liniste)
            for(int i=0; i<SAMPLES_PER_BLOCK; i++)
                dac_buffer[active_idx][i] = 0;
        }
        else
        {
            gpio_put(DAC_MUTE_PIN, 0);

            for(int i=0; i< SAMPLES_PER_BLOCK; i++)
            {
                // filtru trece sus lent pentru eliminarea componentei continue:liniste
                uint16_t raw_val = adc_buffer[active_idx][i];

                // ajustam linistea treptat in functie de zgomotul de fond
                static_offset = (static_offset *0.999f) + ((float)raw_val * 0.001f);
                float centered = (float)raw_val - static_offset;

                // actualizam ledul galben doar la ultimul esantion din "cos"
                if(i == SAMPLES_PER_BLOCK -1)
                {
                    // centered poate varia in jurul lui 0 deci setam un prag de +/- 40 
                    // led-ul galben se aprinde doar cand detectam un sunet mai puternic
                    if(centered > 40.0f || centered < -40.0f)
                        gpio_put(17, 1); // aprindem led-ul
                    else
                        gpio_put(17, 0); // stingem led-ul
                }

                // intre -15 si 15 este liniste (0) ca atunci cand inmultim sa nu fie bazait de fundal desi ar trebui sa fie liniste
                if (centered < 15.0f && centered > -15.0f) 
                    centered = 0.0f;

                // amplificare
                int32_t amplified = (int32_t)(centered * 150.0f);
                // prevenim zgomotele neplacute (daca se striga în microfon)
                if (amplified > 32767)  // 2 la a 16 a=65636 de aceea mergem de la -32768 la 32767
                    amplified = 32767;
                if (amplified < -32768) 
                    amplified = -32768;

                // aplicam efectul
                int16_t sample = apply_audio_effect((int16_t)amplified, current_effect);

                // I2S asteapta 32 de biti (16 pt boxa stanga si 16 pt boxa dreapta) deci clonam sunetul si punem in buffer
                dac_buffer[active_idx][i] = ((uint32_t)((uint16_t)sample) << 16) | (uint16_t)sample;
            }
        }

        // blocam executia pana la finalizarea transferului pentru a preveni suprascrierea datelor care inca nu s au transmis
        dma_channel_wait_for_finish_blocking(dma_dac_chan);

        // setam dma-ul sa citeasca din bufferul pe care tocmai l am procesat
        dma_channel_set_read_addr(dma_dac_chan, dac_buffer[active_idx], false);
        // se porneste transferul iar datele pleaca spre boxe
        dma_channel_set_trans_count(dma_dac_chan, SAMPLES_PER_BLOCK, true);

        // asteptam ca microfonul sa adune date noi pentru a le procesa
        dma_channel_wait_for_finish_blocking(dma_adc_chan);

        // trecem la urmatorul "cos"
        active_idx = next_idx;
    }

}
