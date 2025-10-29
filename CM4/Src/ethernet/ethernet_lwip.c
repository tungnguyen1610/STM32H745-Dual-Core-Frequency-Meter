#include "ethernet.h"
#include "http_sever.h"
#include "EthDrv/eth_drv_lwip.h"
#include "cmsis_os2.h"
#include "lwip/dhcp.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "standard_output/standard_output.h"
#include <stdbool.h>
#include <stddef.h>

#define PRINT_IPv4(ip) MSG("%u.%u.%u.%u", (ip & 0xFF), ((ip >> 8) & 0xFF), ((ip >> 16) & 0xFF), ((ip >> 24) & 0xFF))

static ip4_addr_t ipaddr;
static ip4_addr_t netmask;
static ip4_addr_t router;
static struct netif intf;

err_t ethernetif_init(struct netif *netif);

static osTimerId_t checkDhcpTmr;

static bool prevDhcpOK = false;

void check_dhcp_state(void *param) {
    bool dhcpOK = dhcp_supplied_address(&intf);
    if (dhcpOK != prevDhcpOK) {
        if (dhcpOK) {
            MSG("IP-address: %s\n", ipaddr_ntoa(&(netif_default->ip_addr)));
        } else {
            MSG("DHCP configuration lost!\n");
        }
        prevDhcpOK = dhcpOK;
    }
}

void link_chg_cb(struct netif *netif) {
    bool ls = netif_is_link_up(netif);
    MSG("ETH LINK: %s%s", (ls ? (ANSI_COLOR_BGREEN "UP ") : (ANSI_COLOR_BRED "DOWN\n")), ANSI_COLOR_RESET);

    if (ls) {
        LinkState *linkState = (LinkState *)netif->state;
        MSG("(%u Mbps, %s duplex)\n", linkState->speed, linkState->duplex ? "FULL" : "HALF");
    }

    if (ls) {
        dhcp_start(netif);
    } else {
        dhcp_stop(netif);
    }
}

void interface_print_infor (const struct netif *intf)
{
    MSG("IP: " ANSI_COLOR_BYELLOW);
    PRINT_IPv4(intf->ip_addr.addr);
    MSG("\nNetmask: ");
    PRINT_IPv4(intf->netmask.addr);

    MSG("\n\n");

    MSG("\n");
}
void init_ethernet() {
    // initialize lwIP
    tcpip_init(NULL, NULL);
    // clear all associated addresses
    //ip_addr_set_zero_ip4(&ipaddr);
    //ip_addr_set_zero_ip4(&netmask);
    //ip_addr_set_zero_ip4(&router);
    IP4_ADDR(&ipaddr, 192, 168, 10, 11);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&router, 192, 168, 10, 10);
    // add network interface
    netif_add(&intf,
              &ipaddr,
              &netmask,
              &router,
              NULL,
              ethernetif_init,
              tcpip_input);

    // make it default
    netif_set_default(&intf);
    // register a link change callback
    netif_set_link_callback(&intf, link_chg_cb);
    // print interface info
    MSG("\n---- \n");
    interface_print_infor(&intf);
    MSG("---- \n\n");
    // init http sever
    http_sever_init();
    // initialize and start the DHCP-handling
   // checkDhcpTmr = osTimerNew(check_dhcp_state, osTimerPeriodic, NULL, NULL);
   // osTimerStart(checkDhcpTmr, 1000);
}

__attribute__((weak)) err_t hook_unknown_ethertype(struct pbuf *pbuf, struct netif *netif) {
    pbuf_free(pbuf);

    return ERR_OK;
}
