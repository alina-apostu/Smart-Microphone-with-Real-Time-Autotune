#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "dhcpserver.h"
#include "audio_processor.h"
#include "pico/multicore.h"
#include "hardware/adc.h"
#include "buton_mute.h"
#include "buton_active.h"
#include "audio_effects.h"

// Definire pini
#define LED_GREEN 14
#define LED_RED 15
#define LED_BLUE 16
#define LED_YELLOW 17

// Flash
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE) // adresa de memorie unde începe ultimul sector din memoria flash
#define MAGIC_NUMBER 0xDEADBEEF

// strcutura organizare date
typedef struct
{
    char ssid[64];
    char password[64];
    uint32_t magic;
} wifi_config_t;

// Flash citire
bool load_wifi_config(wifi_config_t *cfg)
{
    const wifi_config_t *flash_cfg =
        (const wifi_config_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
    if (flash_cfg->magic == MAGIC_NUMBER)
    {
        memcpy(cfg, flash_cfg, sizeof(wifi_config_t));
        return true;
    }
    return false;
}

// Flash scriere
void save_wifi_config(const char *ssid, const char *password)
{
    wifi_config_t cfg = {0};
    strncpy(cfg.ssid, ssid, 63);
    strncpy(cfg.password, password, 63);
    cfg.magic = MAGIC_NUMBER;

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, (uint8_t *)&cfg,
                        sizeof(wifi_config_t));
    restore_interrupts(ints);
    printf("Config salvat in flash: ssid=%s\n", ssid);
}

//
//
//
static err_t http_callback(void *arg, struct tcp_pcb *tpcb,
                           struct pbuf *p, err_t err) // tpcb este un pointer catre conexiunea tcp activa, p contine cererea HTTP trimisa de browser
{
    if (p != NULL)
    {
        char *request = (char *)p->payload; // transformare in string
        printf("\n[Pico Server] Cerere HTTP primita!\n");

        if (strstr(request, "led=red"))
        {
            if (strstr(request, "state=on"))
            {
                if (g_active_state)
                {
                    // doar daca suntem in starea active, putem comuta mute
                    gpio_put(LED_RED, 1);
                    audio_set_mute(true);
                    g_mute_state = true; // actualizez starea mute globala pentru a o putea folosi si in buton_mute_loop()
                    printf(" >> Comanda: LED ROSU (Mute) -> ON\n");
                }
            }
            else
            {
                gpio_put(LED_RED, 0);
                audio_set_mute(false);
                g_mute_state = false; // actualizez starea mute globala pentru a o putea folosi si in buton_mute_loop()
                printf(" >> Comanda: LED ROSU (Mute) -> OFF\n");
            }
        }
        if (strstr(request, "led=blue"))
        {
            if (strstr(request, "state=on"))
            {
                if (g_active_state)
                {
                    gpio_put(LED_BLUE, 1);
                    // g_just_activated = false;                // resetez flagul dupa ce am folosit informatia ca tocmai am intrat in starea active
                    printf(" >> Comanda: LED BLUE -> ON\n"); // NOU
                }
            }
            else
                gpio_put(LED_BLUE, 0);
        }
        if (strstr(request, "led=green"))
        {
            if (strstr(request, "state=on"))
            {
                gpio_put(LED_GREEN, 1);
                g_active_state = true;                    // actualizez starea active globala pentru a o putea folosi si in buton_active_loop()
                printf(" >> Comanda: LED VERDE -> ON\n"); // NOU
            }
            else
            {
                gpio_put(LED_GREEN, 0);
                g_active_state = false;
                // actualizez starea active globala pentru a o putea folosi si in buton_active_loop()

                gpio_put(LED_RED, 0);
                gpio_put(LED_BLUE, 0);
                g_mute_state = false;
                audio_set_mute(false);
                current_effect = EFFECT_CLEAN; // resetam efectul
                printf(" >> Comanda: LED VERDE -> OFF - mute si efectul resetate\n");
            }
        }
        if (strstr(request, "led=yellow"))
        {
            if (strstr(request, "state=on"))
                gpio_put(LED_YELLOW, 1);
            else
                gpio_put(LED_YELLOW, 0);
        }

        // interceptare comenzi efecte
        if (strstr(request, "efect="))
        {
            if (strstr(request, "efect=none"))
            {
                current_effect = EFFECT_CLEAN;
                printf(" >> Efect aplicat: CLEAN\n");
            }
            else if (strstr(request, "efect=echo"))
            {
                current_effect = EFFECT_ECHO;
                printf(" >> Efect aplicat: ECHO\n");
            }
            else if (strstr(request, "efect=robot"))
            {
                current_effect = EFFECT_ROBOT;
                printf(" >> Efect aplicat: ROBOT\n");
            }
            else if (strstr(request, "efect=deep"))
            {
                current_effect = EFFECT_DEEP_VOICE;
                printf(" >> Efect aplicat: DEEP VOICE\n");
            }
            else if (strstr(request, "efect=chipmunk"))
            {
                current_effect = EFFECT_CHIPMUNK;
                printf(" >> Efect aplicat: CHIPMUNK\n");
            }
        }

        // intensitate
        if (strstr(request, "intensitate="))
        {
            char *ptr = strstr(request, "intensitate") + 12;
            int val = atoi(ptr); // transforma textul in nr intreg

            // limite de siguranta
            if (val < 0)
                val = 0;
            if (val > 100)
                val = 100;

            // transformam in procent cu virgula (50% devine 0.5f)
            effect_intensity = (float)val / 100.0f;
            printf(" >> Intensitate setata la: %d%%\n", val);
        }

        if (strstr(request, "GET /intra-in-config"))
        {
            printf("[ADMIN] Cerere de resetare Wi-Fi primita din interfata!\n");

            // stergem configul din flash ca sa il fortam sa intre în AP la boot
            uint32_t ints = save_and_disable_interrupts();
            flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE); // (sterge de la..., dimensiune)
            restore_interrupts(ints);

            // trimitem un raspuns rapid inapoi catre browser înainte de restart
            const char *ok_res = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\n\r\nOK";
            tcp_write(tpcb, ok_res, strlen(ok_res), TCP_WRITE_FLAG_COPY);

            // resetam Pico
            watchdog_enable(500, 1); // resetare dupa 500 ms
            while (1)
                ;
        }
        // adaug un endpoint pentru a returna starea curenta a ledurilor si a butoanelor, util pentru interfata web sa stie ce stare are pico in orice moment
        if (strstr(request, "GET /status"))
        {
            char status_buf[128];
            snprintf(status_buf, sizeof(status_buf),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: application/json\r\n"
                     "Access-Control-Allow-Origin: *\r\n\r\n"
                     "{\"active\":%s,\"muted\":%s,\"blue\":%s}",
                     g_active_state ? "true" : "false",
                     g_mute_state ? "true" : "false",
                     gpio_get(LED_BLUE) ? "true" : "false");
            tcp_write(tpcb, status_buf, strlen(status_buf), TCP_WRITE_FLAG_COPY);
            tcp_recved(tpcb, p->tot_len);
            pbuf_free(p);
            tcp_close(tpcb);
            return ERR_OK;
        }
        const char *response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\nOK";
        tcp_write(tpcb, response, strlen(response), TCP_WRITE_FLAG_COPY);
        tcp_recved(tpcb, p->tot_len);
        pbuf_free(p);
        tcp_close(tpcb);
    }
    return ERR_OK;
}

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) // intrerupere
{
    tcp_recv(newpcb, http_callback); // ori de cate ori primesc date de la noua conexiune newpcb, apelez http_callback
    return ERR_OK;
}

//
//  UDP BROADCAST,  Pico anunta în retea IPul său
//
static struct udp_pcb *udp_broadcast_pcb = NULL;

static void udp_recv_callback(void *arg, struct udp_pcb *pcb,
                              struct pbuf *p, const ip_addr_t *addr,
                              u16_t port)
{
    if (p != NULL)
    {
        printf("UDP raspuns primit de la server Python!\n");
        pbuf_free(p);
    }
}

void anunta_ip_in_retea()
{
    printf("Trimit broadcast UDP ca sa ma anunt in retea...\n");

    udp_broadcast_pcb = udp_new();
    udp_bind(udp_broadcast_pcb, IP_ADDR_ANY, 5679);
    udp_recv(udp_broadcast_pcb, udp_recv_callback, NULL);

    // trimite broadcast la toată reteaua
    ip_addr_t broadcast;
    IP4_ADDR(&broadcast, 255, 255, 255, 255); // definire adresa de broascast

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 9, PBUF_RAM); // aloca memorie ram pt mesaj
    memcpy(p->payload, "PICO_HERE", 9);

    udp_sendto(udp_broadcast_pcb, p, &broadcast, 5679);
    pbuf_free(p);

    printf("Broadcast trimis!\n");
}

void start_led_server()
{
    printf("Pornesc serverul de LED-uri pe portul 80...\n");
    struct tcp_pcb *pcb = tcp_new(); // creare nou socket
    tcp_bind(pcb, IP_ADDR_ANY, 80);
    pcb = tcp_listen(pcb);                // pico in mod pasiv, asculta
    tcp_accept(pcb, connection_callback); // daca se conecteaza cineva, apeleaza connection_callback
    uint32_t last_msg_time = 0;           // variabila pentru a masura timpu
    uint32_t last_broadcast_time = 0;
    while (1)
    {
        cyw43_arch_poll();   // interoghează cipul WiFi pentru pachete noi
        buton_mute_loop();   // verificare butoane mute
        buton_active_loop(); // verificare butoane active
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_msg_time > 5000)
        {
            printf("[Pico] Server activ. Astept comenzi de la browser...\n");
            printf("DEBUG Microfon - Valoare ADC: %d\n", adc_read());
            printf("g_mute=%d g_active=%d\n", g_mute_state, g_active_state);
            last_msg_time = now;
        }
        if (now - last_broadcast_time > 10000)
        {
            anunta_ip_in_retea();
            last_broadcast_time = now;
        }
        sleep_ms(1);
    }
}

//
//  MOD AP, formular WiFi
//

// Pagina HTML servită în mod AP
const char *config_page =
    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
    "<html><body>"
    "<h2>Configurare Wi-Fi Pico</h2>"
    "<form method='GET' action='/save'>"
    "SSID: <input name='ssid'><br>"
    "Parola: <input name='pass' type='password'><br>"
    "<input type='submit' value='Conecteaza'>"
    "</form></body></html>";

// ne ajuta la salvarea valoarii ssid ului(X) sau a parolei(Y) din url ex: /save?ssid=X&pass=Y
static void parse_param(const char *request, const char *key, char *out, int maxlen)
{
    const char *pos = strstr(request, key);
    if (!pos)
        return;
    pos += strlen(key);
    int i = 0;
    while (*pos && *pos != '&' && *pos != ' ' && i < maxlen - 1)
    {
        out[i++] = *pos++;
    }
    out[i] = '\0';
}

static err_t ap_http_callback(void *arg, struct tcp_pcb *tpcb,
                              struct pbuf *p, err_t err)
{
    if (p != NULL)
    {
        char *request = (char *)p->payload;
        printf("AP primit request: %.80s\n", request);

        if (strstr(request, "GET /save"))
        {
            // Extragem ssid și parola din URL
            char ssid[64] = {0};
            char pass[64] = {0};
            parse_param(request, "ssid=", ssid, sizeof(ssid));
            parse_param(request, "pass=", pass, sizeof(pass));

            printf("SSID primit: %s\n", ssid);
            printf("Parola primita: %s\n", pass);

            // Salvăm în flash
            save_wifi_config(ssid, pass);

            const char *ok_page =
                "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                "<html><body><h2>Salvat! Pico se reconecteaza...</h2>"
                "<p>Reconecteaza-te la reteaua ta normala.</p>"
                "</body></html>";
            tcp_write(tpcb, ok_page, strlen(ok_page), TCP_WRITE_FLAG_COPY); // TCP_WRITE_FLAG_COPY folosit pt spune librariei lwIP să facă o copie a datelor tale într-un buffer intern, le pastreaza undeva papa când laptopul confirmă primirea datelor
        }
        else
        {
            // Servim formularul
            tcp_write(tpcb, config_page, strlen(config_page), TCP_WRITE_FLAG_COPY);
        }

        tcp_recved(tpcb, p->tot_len); // am terminat de citit, stiva de retea poate sa elibereze spatiul si sa anunte ca mai pot primi alte date
        pbuf_free(p);
        tcp_close(tpcb);
    }
    return ERR_OK;
}

static err_t ap_connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, ap_http_callback); // intrerupere
    return ERR_OK;
}
void start_ap_config_mode()
{
    printf("Pornesc in mod AP: 'Pico-Setup'...\n");

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
    printf("Flash sters. Astept configurare noua...\n");

    cyw43_arch_enable_ap_mode("Pico-Setup", "12345678", CYW43_AUTH_WPA2_AES_PSK); // activeaza mod acces point

    sleep_ms(3000); // timp pentru init complet

    // cauta interfata AP explicit dupa numele ei ("ap0" pe Pico W)
    struct netif *ap_netif = NULL;
    NETIF_FOREACH(ap_netif)
    {
        printf("Interfata gasita: %c%c%d\n", ap_netif->name[0], ap_netif->name[1], ap_netif->num);
        if (ap_netif->name[0] == 'a' && ap_netif->name[1] == 'p')
            break;
    }

    if (ap_netif == NULL)
    {
        printf("EROARE: Interfata AP nu a fost gasita! Folosesc netif_default.\n");
        ap_netif = netif_default;
    }

    ip4_addr_t ip, mask, gw;
    IP4_ADDR(&ip, 192, 168, 4, 1);
    IP4_ADDR(&mask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 192, 168, 4, 1);
    netif_set_addr(ap_netif, &ip, &mask, &gw);

    static dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &ip, &mask); // pornim server dhcp pentru a imparti ip uri dispozitivelor conectate la reteaua AP a lui pico
    printf("DHCP server pornit.\n");

    printf("IP setat: %s\n", ip4addr_ntoa(&ip));
    printf("AP pornit! Conecteaza-te la 'Pico-Setup' si deschide 192.168.4.1\n");

    // potnire server web pico (cel cu ssid ul si parola)
    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, 80);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, ap_connection_callback);

    while (1)
    {
        cyw43_arch_poll(); // mentine wifi ul activ
        sleep_ms(1);
        buton_mute_loop();

        wifi_config_t cfg;
        if (load_wifi_config(&cfg)) // daca s a  apasat pe save, deci pico va salva ssid ul si parola si va reporni
        {
            printf("Credentiale salvate! Resetez Pico...\n");
            sleep_ms(1000);
            watchdog_enable(1, 1);
            while (1)
                ;
        }
    }
}

//
//  MAIN
//
int main()
{
    stdio_init_all();

    // initializam audio
    audio_processor_init();
    buton_mute_init();
    buton_active_init();
    // lansez bucla audio pe al doilea nucleu (Core 1)
    multicore_launch_core1(audio_processor_loop);

    // asteapta 4 secunde ca Windowsul sa deschida portul COM
    sleep_ms(4000);
    printf("Pico a pornit!\n");

    // initializ GPIO
    int leds[] = {LED_GREEN, LED_RED, LED_BLUE, LED_YELLOW};
    for (int i = 0; i < 4; i++)
    {
        gpio_init(leds[i]);
        gpio_set_dir(leds[i], GPIO_OUT);
        gpio_put(leds[i], 1);
        printf("LED %d aprins\n", leds[i]);
    }
    sleep_ms(4000);

    for (int i = 0; i < 4; i++)
    {

        gpio_put(leds[i], 0);
    }
    // g_active_state = false;
    // g_mute_state = false;

    // initializare Wi-Fi
    printf("Initializez cipul Wi-Fi...\n");
    if (cyw43_arch_init())
    {
        printf("EROARE: Cipul Wi-Fi nu s-a putut inițializa!\n");
        while (1)
            sleep_ms(1000);
    }

    wifi_config_t cfg;

    if (load_wifi_config(&cfg))
    {
        // S-au gasit credentiale salvate în flash
        printf("Config gasit! Conectare la: %s\n", cfg.ssid);
        cyw43_arch_enable_sta_mode(); // activeaza modul STA (Station), pico este ca un client

        if (cyw43_arch_wifi_connect_timeout_ms(
                cfg.ssid, cfg.password,
                CYW43_AUTH_WPA2_AES_PSK, 60000) == 0) // ii dam ruterului timp sa raspunda
        {
            // Conectat cu succes
            printf("Conectat! IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default))); // extrage in ascii(extrage adresa Ip a lui pico alocata(reteaua))

            // Anunțăm IP-ul în retea via UDP broadcast// //nu mai anunt aici, ci in loopul de la start_led_server(); deoarece cateodata nu ajungeau pachetele UDP asa ca le trimit mai des ptc UDP nu se adigura ca ajung toate pachetele
            // anunta_ip_in_retea();

            // Serverul de LED-uri porneste normal
            start_led_server();
        }
        else
        {
            // Credentialele sunt gresite sau reteaua nu e disponibilă
            printf("Conectare esuata! Intru in mod AP...\n");
            cyw43_arch_deinit(); // prim cipul complet
            sleep_ms(100);
            cyw43_arch_init(); // il repornim
            start_ap_config_mode();
        }
    }
    else
    {
        // Prima pornire, niciun config salvat → mod AP
        printf("Niciun config gasit. Intru in mod AP...\n");
        start_ap_config_mode();
    }
}