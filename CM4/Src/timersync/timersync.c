#include "timersync.h"
#include "stm32h7xx_hal.h"
#include "cliutils/cli.h"
#include "EthDrv/mac_drv.h"
#include <stm32h7xx_ll_tim.h>
#include "stdlib.h"
#include "flexptp/timeutils.h"
#include "standard_output/standard_output.h"
#include "freqmeasure.h"

#define MIN(a,b) ((a < b) ? (a) : (b))
#define MAX(a,b) ((a < b) ? (a) : (b))
extern TIM_HandleTypeDef htim1;
TimestampU preCapture ={0,0};
TimestampU currentCapture={0,0};
/*
Based on experiment, when TIM1 operate in external clock based on external signal
-> f(tim_freq) = f(ext_signal)/2;
therefore if we have 1kHz signal -> TIM1 frequency = 500Hz
-> Original frequency = 2 * divider * (Measured_Freq_TIM2_CHx)
 */
/*
* INPUTS
* TIM2
- CH1: PA0
- CH2: PB3
- CH3: PB10
- CH4: PA3
*/
void timersync_init_gpio()
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    //__HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    // CH1
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    // CH2
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    // CH3
    GPIO_InitStruct.Pin = GPIO_PIN_10;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    //CH 4
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}
#define TIMERSYNC_DEFAULT_ADDEND (((uint32_t)(200E+06)) - 1)

static CMD_FUNCTION(CB_params);
static CMD_FUNCTION(CB_start_stop);
static CMD_FUNCTION(CB_compare);


static void timer_sync_basic_timer_setup(TIM_HandleTypeDef* htim)
{
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_IC_InitTypeDef sConfigIC = {0};
    htim->Instance = TIM2;
    htim->Init.Prescaler=0;
    htim->Init.CounterMode=TIM_COUNTERMODE_UP;
    htim->Init.Period=TIMERSYNC_DEFAULT_ADDEND;
    htim->Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
    htim->Init.RepetitionCounter=0;
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_Base_Init(htim);
    HAL_TIM_ConfigClockSource(htim, &sClockSourceConfig);
    // Channels
    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = 0;
    HAL_TIM_IC_ConfigChannel(htim, &sConfigIC, TIM_CHANNEL_1);
    HAL_TIM_IC_ConfigChannel(htim, &sConfigIC, TIM_CHANNEL_2);
    HAL_TIM_IC_ConfigChannel(htim, &sConfigIC, TIM_CHANNEL_3);
    HAL_TIM_IC_ConfigChannel(htim, &sConfigIC, TIM_CHANNEL_4);
    // Enable Interrupts
    HAL_TIM_IC_Start_IT(htim, TIM_CHANNEL_1);
    HAL_TIM_IC_Start_IT(htim, TIM_CHANNEL_2);
    HAL_TIM_IC_Start_IT(htim, TIM_CHANNEL_3);
    HAL_TIM_IC_Start_IT(htim, TIM_CHANNEL_4);
}
/*
static void timersync_basic_timer_setup(TIM_TypeDef * pstatic void timersync_basic_timer_setup(TIM_TypeDef * pstatic void timersync_basic_timer_setup(TIM_TypeDef * pRCCTim) {
static void timersync_basic_timer_setup(TIM_TypeDef * pRCCTim) {
RCCTim) {
    // timer basics
    LL_TIM_SetPrescaler(pTim, 0);
    LL_TIM_SetCounterMode(pTim, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetAutoReload(pTim, TIMERSYNC_DEFAULT_ADDEND);
    LL_TIM_SetClockDivision(pTim, LL_TIM_CLOCKDIVISION_DIV1);
    LL_TIM_SetRepetitionCounter(pTim, 0);

    LL_TIM_SetClockSource(pTim, LL_TIM_CLOCKSOURCE_INTERNAL);

    // channels
    uint32_t chConf = LL_TIM_ACTIVEINPUT_DIRECTTI | LL_TIM_ICPSC_DIV1 |
            LL_TIM_IC_FILTER_FDIV1 | LL_TIM_IC_POLARITY_FALLING;
    LL_TIM_IC_Config(pTim, LL_TIM_CHANNEL_CH1, chConf);
    LL_TIM_IC_Config(pTim, LL_TIM_CHANNEL_CH2, chConf);
    LL_TIM_IC_Config(pTim, LL_TIM_CHANNEL_CH3, chConf);
    LL_TIM_IC_Config(pTim, LL_TIM_CHANNEL_CH4, chConf);

    // enable channels
    LL_TIM_CC_EnableChannel(pTim, LL_TIM_CHANNEL_CH1 | LL_TIM_CHANNEL_CH2 |
            LL_TIM_CHANNEL_CH3 | LL_TIM_CHANNEL_CH4);

    // enable interrupts
    LL_TIM_EnableIT_CC1(pTim);
    LL_TIM_EnableIT_CC2(pTim);
    LL_TIM_EnableIT_CC3(pTim);
    LL_TIM_EnableIT_CC4(pTim);
}
*/
void timersync_init_timers() {
    __HAL_RCC_TIM2_CLK_ENABLE();

    // enable Eth Aux timestamps
    ETHHW_ClearAuxTimestampFIFO(ETH);
    ETHHW_AuxTimestampCh(ETH, 0, true);

    // base timers and channels
    TIM_HandleTypeDef htim;
    timer_sync_basic_timer_setup(&htim);

    // CH1 OUTPUT COMPARE // TODO: use this code to produce PPS output with one of the channels
//    LL_TIM_OC_ConfigOutput(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_OCPOLARITY_LOW | LL_TIM_OCIDLESTATE_LOW);
//    LL_TIM_OC_SetMode(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_OCMODE_ACTIVE);
//    LL_TIM_OC_SetCompareCH1(TIM2, 1);
//    LL_TIM_OC_SetIdleState(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_OCIDLESTATE_LOW);

    // ---- TIM2 -----
    LL_TIM_SetTriggerInput(TIM2, LL_TIM_TS_ITR4); // TIM2 trigger: ETH PPS
    LL_TIM_SetTriggerOutput(TIM2, LL_TIM_TRGO_UPDATE); // TRGO: update event
    LL_TIM_EnableIT_UPDATE(TIM2); // Enable UPDATE event on TIM2
    LL_TIM_SetSlaveMode(TIM2, LL_TIM_SLAVEMODE_TRIGGER); // Start TIM2 timer on trigger (ETH PPS)

    //LL_TIM_SetTriggerOutput2(TIM2, LL_TIM_TRGO2_OC1);

}

void timersync_init() {
    timersync_init_gpio();


    HAL_NVIC_SetPriority(TIM2_IRQn, 15, 15);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    // --------------------

    cli_register_command("timsync params [Kp Kd] \t\t\tTIM2 servo", 2, 0, CB_params);
    cli_register_command("timsync operate {start|stop} \t\t\tTIM2 start/stop", 2, 1, CB_start_stop);
    //cli_register_command("comp [comp_val] \t\t\tTIM2 CH1 compare value", 1, 1, CB_compare); // TODO: uncomment if PPS output was also required
}

// --------------------------------------

static double Kp = 0.02;
static double Kd = 0.03;

static int CB_params(const CliToken_Type *ppArgs, uint8_t argc)
{
    // set if parameters passed after command
    if (argc >= 2)
    {
        Kp = atof(ppArgs[0]);
        Kd = atof(ppArgs[1]);
    }

    MSG("> TIM2 params: K_p = %.6f, K_d = %.6f\n", Kp, Kd);

    return 0;
}

static int CB_start_stop(const CliToken_Type *ppArgs, uint8_t argc)
{
    if (!strcmp(ppArgs[0], "start"))
    {
        timersync_start();
        MSG("Timersync start initiated!\n");
    } else if (!strcmp(ppArgs[0], "stop")) {
        timersync_stop();
        MSG("Timersync stopped!\n");
    }

    return 0;
}

static int CB_compare(const CliToken_Type *ppArgs, uint8_t argc)
{
    uint32_t comp = atoi(ppArgs[0]);
    LL_TIM_OC_SetCompareCH1(TIM2, comp);
    MSG("OC: %d\n", comp);
    return 0;
}

//static int skipCycles = 0;

void timersync_run_ctrl(ControllerState * ctrlState, const TimestampI *newTs) {
    LL_TIM_SetAutoReload(ctrlState->pTim, ctrlState->period);

    if (ctrlState->skipCycles > 0) {
        //MSG("SKIP: %d\n", skipCycles);
        ctrlState->skipCycles--;
        return;
    }

    ctrlState->ts[1] = ctrlState->ts[0];
    ctrlState->ts[0] = *newTs;

    TimestampI d, one_sec = { 1, 0 };
    subTime(&d, &(ctrlState->ts[0]), &(ctrlState->ts[1]));
    subTime(&d, &d, &one_sec);
    normTime(&d);

//    if (d.sec != 0) {
//        MSG("Error too large!\n");
//        return;
//    }

    //MSG("DIFF: %09i\n", d.nanosec);

    // --------- PD controller ----------

    ctrlState->err_ns[1] = ctrlState->err_ns[0];
    ctrlState->err_ns[0] = (int64_t) ctrlState->ts[0].nanosec - ((int64_t) ctrlState->ts[0].nanosec > (NANO_PREFIX / 2) ? NANO_PREFIX : 0);

    //MSG("ERR: %09li\n", err_ns[0]);


    if (llabs(ctrlState->err_ns[0]) > 2 * NANO_PREFIX) {
        MSG("Controller error too large!\n");
        return;
    } else if (llabs(ctrlState->err_ns[0]) > 2000) {
        uint32_t ptpAddend = ETHHW_GetPTPAddend(ETH);
        double K = (double) 0xFFFFFFFF / (double)ptpAddend;
        ctrlState->period = (TIMERSYNC_DEFAULT_ADDEND + 1) * K - 1;

        uint32_t jumpPeriod = ctrlState->period - (ctrlState->err_ns[0] / 5) * K;
        LL_TIM_SetAutoReload(ctrlState->pTim, jumpPeriod);
        //MSG("PERIOD: %09u %09u\n", jumpPeriod, period);

        ctrlState->skipCycles = 1;
        ctrlState->err_ns[0] = 0;

        return;
    }

    double diff = (ctrlState->err_ns[0] - ctrlState->err_ns[1]);
    double corr = Kp * (ctrlState->err_ns[0]) + Kd * diff;

    uint64_t newPeriod = LL_TIM_GetAutoReload(ctrlState->pTim);
    newPeriod += -(int32_t) corr;
    newPeriod = MIN(MAX(0, (int64_t)(newPeriod)), 0xFFFFFFFF);
    LL_TIM_SetAutoReload(ctrlState->pTim, newPeriod);

    ctrlState->period = newPeriod;

    //MSG("ARR: %u %.6f\n", period, corr);
}

void timersync_stop() {
    LL_TIM_SetSlaveMode(TIM2, LL_TIM_SLAVEMODE_DISABLED);
    LL_TIM_DisableCounter(TIM2);
}

static ControllerState sCtrlState[2];

void timersync_start() {
    memset(sCtrlState, 0, 2 * sizeof(ControllerState));
    sCtrlState[0].pTim = TIM2;
    sCtrlState[0].period = TIMERSYNC_DEFAULT_ADDEND;

    timersync_init_timers();
    MX_TIM1_External();
    //divider_input_signal_start();
    // Enable external clock mode in timer 1
    //MX_TIM1_Init();
    HAL_TIM_Base_Start_IT(&htim1);
    divider_input_signal_start();
    //LL_TIM_SetSlaveMode(TIM2, LL_TIM_SLAVEMODE_TRIGGER);
}

// --------------------------------------

//static bool sOCLevel = false;

void timersync_update() {

#define AUXILIARY_CHANNELS (1)

#define STORE_TIMESTAMP(n,tsarr,unprocarr,ch,s,ns) \
    if (ch & (1 << n)) {\
        tsarr[n].sec = s;\
        tsarr[n].nanosec = ns;\
        unprocarr[n] = true;\
        /*MSG("%d\n", n);*/\
    }

    // collect timestamps
    static TimestampI ts[AUXILIARY_CHANNELS] = { 0 };
    static bool unprocessed[AUXILIARY_CHANNELS] = { 0 };

    while (ETHHW_GetAuxTimestampCnt(ETH) > 0) {
        //MSG("%d\n", ETHHW_GetAuxTimestampCnt(ETH));

        uint32_t s, ns, ch;
        ETHHW_ReadLastAuxTimestamp(ETH, &s, &ns, &ch);

        for (uint8_t i = 0; i < AUXILIARY_CHANNELS; i++) {
            STORE_TIMESTAMP(i, ts, unprocessed, ch, s, ns);
        }
    }

    // use collected timestamps
    for (uint8_t i = 0; i < AUXILIARY_CHANNELS; i++) {
        if (unprocessed[i]) {
            timersync_run_ctrl(&sCtrlState[i], &ts[i]);
            unprocessed[i] = false;
        }
    }
}

/* ------------------Full Timestamp Capture ----------------------
Timer clock = PTP clock (for eg: 190 MHz )
PTP_INCREMENT_NSEC = 6
Subsecond part: k(captured)
Second part: k(captured)<k(PTP)? Tcap=Tptp : Tcap=Tptp-1 
*/
static void timersync_process_capture(uint8_t ch, uint32_t ns) {
    uint32_t ptp_s, ptp_ns;
    ETHHW_GetTime(ETH, &ptp_s, &ptp_ns);
    uint32_t s = (ptp_ns > ns) ? ptp_s : (ptp_s - 1);
    currentCapture.nanosec=ns;
    currentCapture.sec =s;
    MSG("CH%u %u.%09u\n", ch, s, ns);
    // Channel 0 used for timer synchronization (feeding PPS)
    //if (ch!=0)
    //{
    //}
}

#define CAP_TO_NS(cap,period) (uint32_t)((double)(cap) / (double)(period + 1) * 1E+09)
#define CAP_IT_HANDLER(TIM,CH,CHIDX) if (LL_TIM_IsActiveFlag_CC##CH(TIM)) {\
    LL_TIM_ClearFlag_CC##CH(TIM);\
    uint32_t period = LL_TIM_GetAutoReload(TIM);\
    uint32_t cap = LL_TIM_IC_GetCaptureCH##CH(TIM);\
    timersync_process_capture(CHIDX, CAP_TO_NS(cap, period));\
}

void TIM2_IRQHandler(void) {
    // run controller
    if (LL_TIM_IsActiveFlag_UPDATE(TIM2)) {
        LL_TIM_ClearFlag_UPDATE(TIM2);
        timersync_update();
    }

    // ---- CHANNEL CAPTURES -------
    CAP_IT_HANDLER(TIM2, 1, 0);
    CAP_IT_HANDLER(TIM2, 2, 1);
    CAP_IT_HANDLER(TIM2, 3, 2);
    CAP_IT_HANDLER(TIM2, 4, 3);

    // -- Frequency Measurement --

}