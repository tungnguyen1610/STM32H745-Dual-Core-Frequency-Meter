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
char const* SSI_TAGS[] = {"x", "y", "z","w","tslog"};
char const** TAGS = SSI_TAGS;
#define NUM_SSI_TAGS 5 // Corrected to 5 for x, y, z, w, tslog
#define NUM_TIMESTAMP_LOG 10
extern uint16_t freqChannels[4]; // External variable declaration
extern char timestampLog[10][32]; // External variable declaration
extern uint8_t tslog_head;

double get_freq_ch0() {return freqChannels[0];}    

double get_freq_ch1() {return freqChannels[1];}
double get_freq_ch2() {return freqChannels[2];}
double get_freq_ch3() {return freqChannels[3];}
 
uint16_t ssi_handler (int iIndex, char *pcInsert, int iInsertLen)
{
    // Note: Using float/double here might still be relatively slow.
    // If performance is still an issue, consider sending scaled integers.
    double state = 0.0;
    uint16_t len=0;
    switch (iIndex) {
        case 0:
            state=get_freq_ch0();
            len = snprintf(pcInsert, iInsertLen, "%.6f", state);
            break;
        case 1:
            state=get_freq_ch1();
            len = snprintf(pcInsert, iInsertLen, "%.6f", state);
            break;
        case 2:
            state=get_freq_ch2();
            len = snprintf(pcInsert, iInsertLen, "%.6f", state);
            break;
        case 3:
            state=get_freq_ch3();
            len = snprintf(pcInsert, iInsertLen, "%.6f", state);
            break;
        case 4:
            len=0;
            // for tslog, get timestamp data from timsync module
            for (int i = 0; i < 10; ++i) 
            { 
                int idx = (tslog_head + i) % 10; 
                len += snprintf(pcInsert + len, iInsertLen - len, "%s\n", timestampLog[idx]);
                if (len >= iInsertLen - 1) break;
            }
            break;
        default :
            len = 0;
            break;
        }
    // Limit output precision to fit within the buffer size (iInsertLen)
    return len;
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