#ifndef FREQ_MEASURE_H_
#define FREQ_MEASURE_H_
/* Brief: This header file describe the frequency measurement using quick measure and precise measure */
void MX_TIM3_Init(void);
void MX_TIM1_Init(void);
void MX_TIM1_Capture(void);
void MX_TIM1_External(void);
void divider_input_signal_start(void);
void frequency_estimate_init(void);
#endif /* TIMER_DIVIDER_H_ */