/**
 *  CORTEX-M7 CORE
 **/
 #include <memory.h>
 #include <stdint.h>
 #include "main.h"
 #include "gc9a01.h"
 #include <stm32h7xx_hal.h>
 
 #include <ICC/icc.h>
 
 #include "FreeRTOSConfig.h"
 
 #include "standard_output/serial_io.h"
 #include "standard_output/standard_output.h"
 
 #include <cmsis_os2.h>
 
 #define TARGET_SYSCLK_MHZ (configCPU_CLOCK_HZ / 1000000)
 
 /* Private variables ---------------------------------------------------------*/
 
 SPI_HandleTypeDef hspi5;
 DMA_HandleTypeDef hdma_spi5_tx;
 osMutexId_t lcdMutex;
 void Error_Handler(void)
   {
     /* USER CODE BEGIN Error_Handler_Debug */
     /* User can add his own implementation to report the HAL error return state */
     __disable_irq();
     while (1)
     {
     }
     /* USER CODE END Error_Handler_Debug */
   }
 static void MX_SPI5_Init(void);
 void init_osc_and_clk() {
     // setup clock
     RCC_OscInitTypeDef osc;
     RCC_ClkInitTypeDef clk;
 
     // clear the structures
     memset(&osc, 0, sizeof(RCC_OscInitTypeDef));
     memset(&clk, 0, sizeof(RCC_ClkInitTypeDef));
 
     // ---------------------
 
     // Configure the Main PLL.
 
     osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
     osc.HSEState = RCC_HSE_ON; // TODO: turn HSE bypass OFF if using X3 instead of the board controller's clock output
     osc.HSIState = RCC_HSI_OFF;
     osc.CSIState = RCC_CSI_OFF;
     osc.PLL.PLLState = RCC_PLL_ON;
     osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
 
 if (HSE_VALUE == 25000000) {
     osc.PLL.PLLM = 5;
     osc.PLL.PLLN = TARGET_SYSCLK_MHZ / 5 * 2;
     osc.PLL.PLLFRACN = 0;
 } else if (HSE_VALUE == 8000000) {
     osc.PLL.PLLM = 4;
     osc.PLL.PLLN = TARGET_SYSCLK_MHZ;
     osc.PLL.PLLFRACN = 0;
 }
     osc.PLL.PLLP = 2;
     osc.PLL.PLLR = 2;
     osc.PLL.PLLQ = 4;
 
     osc.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
     osc.PLL.PLLRGE = RCC_PLL1VCIRANGE_1;
     osc.PLL.PLLFRACN = 0;
 
 
     HAL_RCC_OscConfig(&osc);
 
     // initialize clock tree
     clk.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                      RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                      RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1);
     clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
     clk.SYSCLKDivider = RCC_SYSCLK_DIV1;
     clk.AHBCLKDivider = RCC_HCLK_DIV2;
     clk.APB1CLKDivider = RCC_APB1_DIV2;
     clk.APB2CLKDivider = RCC_APB2_DIV2;
     clk.APB3CLKDivider = RCC_APB3_DIV2;
     clk.APB4CLKDivider = RCC_APB4_DIV2;
 
     HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4); // set a FLASH latency supporting maximum speed
     
 
     // compute SYSCLK values (HSE_VALUE must be correct when invoking this)
     SystemCoreClockUpdate();
 
     // set tick frequency
     HAL_SetTickFreq(HAL_TICK_FREQ_1KHZ);
 
     HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);
 
     // configure internal voltage regulator
     __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
     while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
     }
 
     __HAL_RCC_D2SRAM3_CLK_ENABLE();
 
     // configure GPIOs for high-speed operation
     __HAL_RCC_SYSCFG_CLK_ENABLE();
     __HAL_RCC_CSI_ENABLE();
     HAL_EnableCompensationCell();
 
     __HAL_RCC_HSI48_ENABLE();
 
     // configure USB onto pll1_q of 48MHz
     RCC_PeriphCLKInitTypeDef perClk = {0};
     perClk.PeriphClockSelection = RCC_PERIPHCLK_USB;
     perClk.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
     HAL_RCCEx_PeriphCLKConfig(&perClk);
     
 }
 
 void icc_recv_cb() {
     ICCQueue *q = icc_get_inbound_pipe();
     uint16_t avail = iccq_avail(q);
     if (avail > 0) {
         // MSGraw("[" ANSI_COLOR_YELLOW "M4" ANSI_COLOR_RESET "] ");
         for (uint16_t i = 0; i < avail; i++) {
             char c;
             iccq_top(q, &c);
             iccq_pop(q);
             MSGchar(c);
         }
     }
 }
 
 void serial_io_rx_callback(const uint8_t *data, uint32_t size) {
     ICCQueue *q = icc_get_outbound_pipe();
     for (uint16_t i = 0; i < size; i++) {
         iccq_push(q, data + i);
     }
     icc_notify(); 
 
 }
 
 void task_startup(void *arg) {
 #ifdef DEBUG
     osDelay(1000);
 #endif
     // print greetings
     MSGraw("\033[2J\033[H");
     MSG("Booting up the M7 core!\n");
     // wake up M4 core
     icc_wake_up_M4();
     // open ICC pipe
     icc_open_pipe();
     // -------------
     __HAL_RCC_GPIOB_CLK_ENABLE();
     __HAL_RCC_GPIOA_CLK_ENABLE();
     __HAL_RCC_GPIOE_CLK_ENABLE();
 
     GPIO_InitTypeDef init;
     init.Mode = GPIO_MODE_OUTPUT_PP;
     init.Pin = GPIO_PIN_14;
     init.Pull = GPIO_NOPULL;
     init.Alternate = 0;
     init.Speed = GPIO_SPEED_HIGH;
     HAL_GPIO_Init(GPIOB, &init);
 
       /*Configure GPIO pins : RST_Pin DC_Pin CS_Pin */
     init.Pin = RST_Pin|DC_Pin|CS_Pin;
     init.Mode = GPIO_MODE_OUTPUT_PP;
     init.Pull = GPIO_NOPULL;
     init.Speed = GPIO_SPEED_HIGH;
     HAL_GPIO_Init(GPIOA, &init);
     //Init SPI
     // Init SPI hardware
     MX_SPI5_Init();
     // --- Init LCD ---
     osMutexAcquire(lcdMutex, osWaitForever);
     GC9A01_Init();
     GC9A01_FillRect(0, 0, 240, 240, 0x0000); // Clear screen black
     osMutexRelease(lcdMutex);
     for (;;) {

    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14); 

    // Draw simple test pattern on black background
    /**/
    osMutexAcquire(lcdMutex, osWaitForever); 
    GC9A01_FillRect(0, 0, 240, 240, 0x0000); // Clear to black
    GC9A01_FillRect(50, 50, 40, 40, 0xFFFF); // White square
    GC9A01_FillRect(100, 100, 40, 40, 0xF800); // Red square
    GC9A01_FillRect(150, 150, 40, 40, 0x07E0); // Green square
    osDelay(1000); // Longer delay to see the pattern
    osMutexRelease(lcdMutex); 
    osMutexAcquire(lcdMutex, osWaitForever); 
    GC9A01_FillRect(0, 0, 240, 240, 0x0000); // Clear to black
    osDelay(1000);
    osMutexRelease(lcdMutex); 
    osMutexAcquire(lcdMutex, osWaitForever); 
    GC9A01_FillRect(0, 0, 240, 240, 0xFFFF); // White square
    osDelay(1000);
    osMutexRelease(lcdMutex); 
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);
    osDelay(500);
    osMutexRelease(lcdMutex); 
     }
}

 /**
   * @brief SPI5 Initialization Function
   * @param None
   * @retval None
   */
 static void MX_SPI5_Init(void)
 {
 
   /* USER CODE BEGIN SPI5_Init 0 */
 
   /* USER CODE END SPI5_Init 0 */
 
   /* USER CODE BEGIN SPI5_Init 1 */
 
   /* USER CODE END SPI5_Init 1 */
   /* SPI5 parameter configuration*/
   hspi5.Instance = SPI5;
   hspi5.Init.Mode = SPI_MODE_MASTER;
   hspi5.Init.Direction = SPI_DIRECTION_2LINES;
   hspi5.Init.DataSize = SPI_DATASIZE_8BIT;
   hspi5.Init.CLKPolarity = SPI_POLARITY_LOW;
   hspi5.Init.CLKPhase = SPI_PHASE_1EDGE;
   hspi5.Init.NSS = SPI_NSS_SOFT;
   hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
   hspi5.Init.FirstBit = SPI_FIRSTBIT_MSB;
   hspi5.Init.TIMode = SPI_TIMODE_DISABLE;
   hspi5.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
   hspi5.Init.CRCPolynomial = 0x0;
   hspi5.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
   hspi5.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
   hspi5.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
   hspi5.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
   hspi5.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
   hspi5.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
   hspi5.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
   hspi5.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
   hspi5.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
   hspi5.Init.IOSwap = SPI_IO_SWAP_DISABLE;
   if (HAL_SPI_Init(&hspi5) != HAL_OK)
   {
     Error_Handler();
   }
   /* USER CODE BEGIN SPI6_Init 2 */
 
   /* USER CODE END SPI5_Init 2 */
 
 }
 
 int main(void) {
     // initialize FPU and several system blocks
     SystemInit();
     // initialize HAL library
     HAL_Init();
     // Initalize SPI
     //MX_SPI1_Init();
     // Init display driver
     // GC9A01_Init();
     // initialize oscillator and clocking
     init_osc_and_clk();
     // initialize serial I/O
     serial_io_init();
     // -------------
     // initialize ICC
     icc_init();
 
     // boot up the M4 core
     HAL_RCCEx_EnableBootCore(RCC_BOOT_C2);
 
     // -------------
 
     // initialize the FreeRTOS kernel
     osKernelInitialize();
     // Create mutex for LCD
     lcdMutex = osMutexNew(NULL);
 
     // create startup thread
     osThreadAttr_t attr;
     memset(&attr, 0, sizeof(attr));
     attr.stack_size = 2048;
     attr.name = "init";
     osThreadNew(task_startup, NULL, &attr);
     // start the FreeRTOS!
     osKernelStart();
     for (;;) {
     }
 }
 
 // void SysTick_Handler() {
 //     HAL_IncTick();
 // }
 
 // ------------------------
 
 uint8_t ucHeap[configTOTAL_HEAP_SIZE] __attribute__((section(".FreeRTOSHeapSection")));
 
 // ------------------------
 
 void vApplicationTickHook(void) 
 {
     HAL_IncTick();
 }
 
 void vApplicationIdleHook(void) {
     return;
 }