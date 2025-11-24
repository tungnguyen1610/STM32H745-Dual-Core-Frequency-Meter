#include "stm32h7xx_hal.h"
#include "freqmeasure.h"
#include "cliutils/cli.h"
#include "standard_output/standard_output.h"
#define TIMCLOCK 200000000.0f
#define PRESCALAR 2000
TIM_HandleTypeDef htim3;
uint32_t captured_value =0;
uint32_t last_captured_value =0;
uint32_t difference =0;
uint32_t display_frequency =0;
volatile uint8_t overflow_count=0;
float frequency= 0.0f;
int is_first_capture=0;
int count=0;
static CMD_FUNCTION(CB_start_stop);
TIM_HandleTypeDef htim1;
/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  // Peripherial clock enable
   __HAL_RCC_TIM1_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
    // GPIO Init
    // PE9: TIm1 CH1
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    // PE11: TIM1 CH2
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* USER CODE END TIM1_Init 0 */
  //TIM_SlaveConfigTypeDef sSlaveConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = PRESCALAR-1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 0xffff-1;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {}
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {}
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {}
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK) 
  {}
  /* Config PWM signal */
  TIM_OC_InitTypeDef sConfigOC = {0};
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0xff;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {}
  /* USER CODE BEGIN TIM3_Init 2 */
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_ENABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
  }
  /* USER CODE BEGIN TIM1_Init 2 */
  HAL_NVIC_SetPriority(TIM1_CC_IRQn, 15, 15);
  HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);
  HAL_NVIC_SetPriority(TIM1_UP_IRQn, 14, 15);
  HAL_NVIC_EnableIRQ(TIM1_UP_IRQn);
  /* USER CODE END TIM1_Init 2 */
}
void MX_TIM1_External()
{
  //HAL_TIM_PWM_DeInit(&htim1); // Stop and deinit previous configuration
  HAL_TIM_Base_DeInit(&htim1);
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  // Peripherial clock enable
   __HAL_RCC_TIM1_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
    // GPIO Init
    // PE9: TIm1 CH1
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    // PE11: TIM1 CH2
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    TIM_SlaveConfigTypeDef sSlaveConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = 0;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = (uint32_t)frequency-1;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
    {
    }
    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
    {
    }
    sSlaveConfig.SlaveMode = TIM_SLAVEMODE_EXTERNAL1;
    sSlaveConfig.InputTrigger = TIM_TS_TI2FP2;
    sSlaveConfig.TriggerPolarity = TIM_TRIGGERPOLARITY_RISING;
    sSlaveConfig.TriggerFilter = 0;
    if(HAL_TIM_SlaveConfigSynchro(&htim1, &sSlaveConfig) != HAL_OK)
    {
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
    {
    }
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = (uint32_t)((htim1.Init.Period+1) / 2);
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
    {
    }
    sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
    sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime = 0;
    sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.BreakFilter = 0;
    sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
    sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
    sBreakDeadTimeConfig.Break2Filter = 0;
    sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_ENABLE;
    if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
    {
    }
    /* USER CODE BEGIN TIM1_Init 2 */
    HAL_NVIC_SetPriority(TIM1_UP_IRQn, 15, 15);
    HAL_NVIC_EnableIRQ(TIM1_UP_IRQn);
    HAL_NVIC_SetPriority(TIM1_CC_IRQn, 15, 15);
    HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);
}
void divider_input_signal_start()
{
    TIM1->ARR= (uint32_t)frequency-1;
    TIM1->CCR1= (uint32_t)((TIM1->ARR+1)/2);
    HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_1);
}
void MX_TIM3_Init()
{
  /* USER CODE BEGIN TIM3_Init 0 */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  // Pin init PC6
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  // Pin init PC7 (Timer 3 Channel 2)
  //GPIO_InitStruct.Pin = GPIO_PIN_7;
  //HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = PRESCALAR-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 100-1;  

  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {}
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {}
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {}
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) 
  {}
  /* Config PWM signal */
  
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 50;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {}
  /* USER CODE BEGIN TIM3_Init 2 */
}

void MX_TIM1_Capture(void)
{
    TIM_IC_InitTypeDef sConfigIC = {0};
    // Input direct capture
    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = 0;
    if (HAL_TIM_IC_ConfigChannel(&htim1, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
    {}
}

static void capture_start(void)
{
    MX_TIM1_Init();
    MX_TIM1_Capture();
    HAL_TIM_Base_Start_IT(&htim1);
    HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_2); 
}

static void capture_stop(void)
{
    HAL_TIM_IC_Stop_IT(&htim1, TIM_CHANNEL_2);
    HAL_TIM_Base_Stop_IT(&htim1);
    HAL_TIM_Base_DeInit(&htim1);
}

static int CB_start_stop(const CliToken_Type *ppArgs, uint8_t argc)
{
    if (!strcmp(ppArgs[0], "start"))
    {
        count=0;
        capture_start();
        MSG("Capture channel initiated!\n");
    } else if (!strcmp(ppArgs[0], "stop")) {
        capture_stop();
        MSG("Capture channel stopped!\n");
    }

    return 0;
}

void frequency_estimate_init(void)
{
    MX_TIM3_Init();
    //MX_TIM1_Init();
    //MX_TIM1_Capture();
    //HAL_TIM_Base_Start_IT(&htim1);
    HAL_NVIC_SetPriority(TIM3_IRQn, 15, 15);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
    HAL_TIM_PWM_Start_IT(&htim3, TIM_CHANNEL_1);
    // test_input_signal
    cli_register_command("frequency estimation {start|stop} \t\t\tTIM3 start/stop", 2, 1, CB_start_stop);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
        overflow_count++;
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
    {
        if (is_first_capture==0)
        {
            last_captured_value = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
            is_first_capture=1;
        }
        else
        {
            uint8_t this_overflow_count= overflow_count;
            captured_value = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
            if (this_overflow_count==0)
            {
                difference = captured_value - last_captured_value;
            }
            // edge case: delay between overflow_count and capture callback (there for error between -1/-2)
            else
            {
                difference = (htim->Init.Period - last_captured_value) + captured_value + (this_overflow_count-2)* (htim->Init.Period);  
            }          
            frequency = TIMCLOCK / ((float)(PRESCALAR) * (float)difference);
            display_frequency = (uint32_t)(frequency);
            MSGVariable(&display_frequency, 1);
            __HAL_TIM_SetCounter(htim, 0);
            is_first_capture=0;
            overflow_count=0;
        }
    }
}
void TIM3_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim3);
}
void TIM1_CC_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim1);
    
}
void TIM1_UP_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim1);
}
