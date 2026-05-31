#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// setari fundamentale
#define NO_SYS 1      // fara sistem de operare
#define LWIP_SOCKET 0 // dezactivare API socket
#define LWIP_NETCONN 0

// MEMP_NUM_TCP_SEG >= TCP_SND_QUEUELEN
#define MEMP_NUM_TCP_SEG 32
#define TCP_SND_QUEUELEN 16 // lungimea cozii de transmisie
#define MEMP_NUM_TCP_PCB 4  // nr maxim de conexiuni TCP simultane
//

#define MEM_LIBC_MALLOC 0
#define MEM_ALIGNMENT 4 // alinierea memoriei la 4 octeti
#define MEM_SIZE 8000

#define LWIP_ARP 1 // activeza ARP (leg dintre IP si MAC)
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_RAW 1

#define TCP_MSS 1460
#define TCP_WND (8 * TCP_MSS)     // fereastra de receptie
#define TCP_SND_BUF (8 * TCP_MSS) // buffer de trimitere

#define LWIP_NETIF_STATUS_CALLBACK 1
#define LWIP_NETIF_LINK_CALLBACK 1
#define LWIP_NETIF_HOSTNAME 1
#define LWIP_DHCP 1
#define LWIP_IPV4 1
#define LWIP_TCP 1
#define LWIP_UDP 1
#define LWIP_STATS 0
#define LWIP_CHKSUM_ALGORITHM 3

#endif