#include "http_sever.h"
#include <stm32h7xx_hal.h>
#include "lwip/opt.h"
#include "lwip/api.h"
#include "lwip/apps/fs.h"
#include "lwip/apps/httpd.h"
#include "lwip/netif.h" // Needed for netif_set_up
#include "string.h"
#include "cmsis_os2.h"

// Define the tags and their corresponding index used in ssi_handler
char const* SSI_TAGS[] = {"x", "y", "z","w"};
char const** TAGS = SSI_TAGS;
#define NUM_SSI_TAGS 4 // Corrected to 4 for x, y, z, w

extern uint16_t freqChannels[4]; // External variable declaration

double get_freq_ch0() {return freqChannels[0];}    

double get_freq_ch1() {return freqChannels[1];}
double get_freq_ch2() {return freqChannels[2];}
double get_freq_ch3() {return freqChannels[3];}
 
uint16_t ssi_handler (int iIndex, char *pcInsert, int iInsertLen)
{
    // Note: Using float/double here might still be relatively slow.
    // If performance is still an issue, consider sending scaled integers.
    double state = 0.0;
    switch (iIndex) {
        case 0:
            state=get_freq_ch0();
            break;
        case 1:
            state=get_freq_ch1();
            break;
        case 2:
            state=get_freq_ch2();
            break;
        case 3:
            state=get_freq_ch3();
            break;
        default :
            return 0;
    }
    // Limit output precision to fit within the buffer size (iInsertLen)
    int len = snprintf(pcInsert, iInsertLen, "%.6f", state);
    return (uint16_t)((len > 0) ? len : 0);
}

// The function that will be executed inside the LwIP thread when the stack is ready.
// This is where you safely initialize the HTTP server and bring the interface up.
void lwip_ready_callback(void *arg) {
    struct netif *intf = (struct netif *)arg;

    // 1. Initialize the HTTP Server
    httpd_init();
    
    // 2. Set up the SSI handler
    http_set_ssi_handler(ssi_handler, (char const **)TAGS, NUM_SSI_TAGS);

    // 3. Set the network interface as UP
    // This MUST be done after httpd_init() in a multi-threaded environment
    if (intf) {
        netif_set_up(intf);
    }
    
}

// This function is now just a shell, as the real init happens in the callback.
void http_sever_init()
{
    // Initialization is deferred to the lwip_ready_callback
}