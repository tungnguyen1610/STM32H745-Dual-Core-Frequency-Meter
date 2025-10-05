/**
 *  CORTEX-M4 CORE
*/

#include <stdint.h>
#include <memory.h>

#include <stm32h7xx_hal.h>

#include <cmsis_os2.h>

#include "FreeRTOSConfig.h"

#include "ICC/icc.h"

#include "etherlib/dynmem.h"
#include "etherlib/etherlib.h"
#include "etherlib/prefab/conn_blocks/icmp_connblock.h"

#include "flexptp/task_ptp.h"
#include "flexptp/ptp_core.h"
#include "flexptp/profiles.h"
#include "flexptp/ptp_profile_presets.h"
#include "flexptp/settings_interface.h"
#include "flexptp/logging.h"

#include "cliutils/cli.h"

#include "cmds.h"

#include "ethernet/ethernet.h"

#include "ethernet/http_sever.h"
#define FLEXPTP_INITIAL_PROFILE ("gPTP")
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
void Error_Handler(){while(1) {}};
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);

void print_welcome_message() {
    MSG(ANSI_COLOR_BGREEN "Hi!" ANSI_COLOR_BYELLOW " This is a flexPTP demo for the STMicroelectronics NUCLEO-H745ZI-Q (STM32H745) board.\n\n"
                          "The application is built on FreeRTOS, flexPTP is currenty compiled against %s and uses the supplied example %s Network Stack Driver. "
                          "In this demo, the underlying Ethernet stack can be either lwip or EtherLib, the 'ETH_STACK' CMake variable (in the main CMakeLists.txt file) determines which one will be used. "
                          "The STM32H7xx PTP hardware module driver is also picked from the bundled ones. This flexPTP instance features a full CLI control interface, the help can be listed by typing '?' once the flexPTP has loaded. "
                          "The initial PTP preset that loads upon flexPTP initialization is the 'gPTP' (802.1AS) profile. It's a nowadays common profile, but we encourage "
                          "you to also try out the 'default' (plain IEEE 1588) profile and fiddle around with other options as well. The application will try to acquire an IP-address with DHCP. "
                          "Once the IP-address is secured, you might start the flexPTP module by typing 'flexptp'. 'Have a great time! :)'\n\n" ANSI_COLOR_RESET,
        ETH_STACK, ETH_STACK);

    MSG(ANSI_COLOR_BRED "By default, the MCU clock is sourced by the onboard (STLink) board controller on this devboard. According to our observations, this clock signal is loaded "
                        "with heavy noise rendering the clock synchronization unable to settle precisely. We highly recommend to solder a 8 or 25 MHz oscillator onto "
                        "the designated X3 pads to achieve the best results!\n\n" ANSI_COLOR_RESET);

    // MSG("Freq: %u\n", SystemCoreClock);
}

void task_startup(void *arg) {
    // open ICC pipe
    icc_open_pipe();

    osDelay(2000);

    MSG("Booting up the M4 core!\n");
    print_welcome_message();

    // initialize CLI
    cli_init();

    // initialize Ethernet stack
    init_ethernet();

    // initialize additional commands
    cmd_init();

    // -----------------

   __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitTypeDef init;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pin = GPIO_PIN_1;
    init.Pull = GPIO_NOPULL;
    init.Alternate = 0;
    init.Speed = GPIO_SPEED_LOW;
    HAL_GPIO_Init(GPIOE, &init);

    /* Loop forever */
    for (;;){

    }
}
void pwm_task(void *arg)
{
    // init timer
    MX_TIM3_Init();
    //TIM3->CCR1 = 50; // 50% duty cycle
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    for (;;){
    HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_1);
    osDelay(500);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 50);
    vTaskDelay(pdMS_TO_TICKS(1000));

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 75);
    vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
// ---------------
// THIS CORE BOOTS ONLY AFTER M7 HAS FINISHED CLOCK INITIALIZATION
// ---------------
int main(void)
{
    // initialize FPU and several system blocks
    SystemInit();

    // initialize HAL library
    HAL_Init();

    icc_wait_for_M7_bootup();

    // update clock values
    SystemCoreClockUpdate();

    // -------------

    // initialize the FreeRTOS kernel
    osKernelInitialize();

    // create startup thread
    osThreadAttr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.stack_size = 2048;
    attr.name = "init";
    osThreadNew(task_startup, NULL, &attr);
    osThreadNew(pwm_task, NULL, &attr);
    // start the FreeRTOS!
    osKernelStart();

	for(;;) {}
}

void flexptp_user_event_cb(PtpUserEventCode uev) {
    switch (uev) {
    case PTP_UEV_INIT_DONE:
        ptp_load_profile(ptp_profile_preset_get(FLEXPTP_INITIAL_PROFILE));
        ptp_print_profile();

        ptp_log_enable(PTP_LOG_DEF, true);
        ptp_log_enable(PTP_LOG_BMCA, true);
        break;
    default:
        break;
    }
}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 190-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 100-1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  //HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 190-1;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 100-1;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim4, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}
// ------------------------HAL_TICK_FREQ_1KHZ

uint8_t ucHeap[configTOTAL_HEAP_SIZE] __attribute__((section(".FreeRTOSHeapSection")));

// ------------------------

void vApplicationTickHook(void) {
    HAL_IncTick();
}

void vApplicationIdleHook(void) {
    return;
}