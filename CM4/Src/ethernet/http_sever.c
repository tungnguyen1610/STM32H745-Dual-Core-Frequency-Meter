#include "http_sever.h"
#include <stm32h7xx_hal.h>
#include "lwip/opt.h"
#include "lwip/api.h"
#include "lwip/apps/fs.h"
#include "lwip/apps/httpd.h"
#include "string.h"
#include "cmsis_os2.h"

// Define the tags and their corresponding index used in ssi_handler
char const* SSI_TAGS[] = {"x", "y", "z"};
char const** TAGS = SSI_TAGS;
#define NUM_SSI_TAGS 3
// Define status strings
#define SSI_LED_ON  "ON"
#define SSI_LED_OFF "OFF"

int get_freq_ch0() {return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5);}
int get_freq_ch1() {return HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_1);}
int get_freq_ch2() {return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14);}

uint16_t ssi_handler (int iIndex, char *pcInsert, int iInsertLen)
{
    char *status;
    int state=0;
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
		default :
            return 0;
	}
    if (state){
        status = SSI_LED_ON;
    } else {
        status = SSI_LED_OFF;
    }
    snprintf(pcInsert,10,"%s",status);
    return strlen(pcInsert);
}

void http_sever_init()
{

//	sys_thread_new("http_thread",(osThreadFunc_t)&http_thread, NULL, DEFAULT_THREAD_STACKSIZE, osPriorityNormal);
	httpd_init();
    http_set_ssi_handler(ssi_handler, (char const **)TAGS, 3);
}