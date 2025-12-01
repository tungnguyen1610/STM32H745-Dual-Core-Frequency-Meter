#include "http_sever.h"
#include <stm32h7xx_hal.h>
#include "lwip/opt.h"
#include "lwip/api.h"
#include "lwip/apps/fs.h"
#include "lwip/apps/httpd.h"
#include "string.h"
#include "cmsis_os2.h"

// Define the tags and their corresponding index used in ssi_handler
char const* SSI_TAGS[] = {"x", "y", "z","w"};
char const** TAGS = SSI_TAGS;
#define NUM_SSI_TAGS 3
extern uint16_t freqChannels[4];
// Define status strings

double get_freq_ch0() {return freqChannels[0];}
double get_freq_ch1() {return freqChannels[1];}
double get_freq_ch2() {return freqChannels[2];}
double get_freq_ch3() {return freqChannels[3];}
 
uint16_t ssi_handler (int iIndex, char *pcInsert, int iInsertLen)
{
    double state=0.0;
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
    sprintf(pcInsert,"%.6f",state);
    return strlen(pcInsert);
}

void http_sever_init()
{

//	sys_thread_new("http_thread",(osThreadFunc_t)&http_thread, NULL, DEFAULT_THREAD_STACKSIZE, osPriorityNormal);
	httpd_init();
    http_set_ssi_handler(ssi_handler, (char const **)TAGS, 4);
}