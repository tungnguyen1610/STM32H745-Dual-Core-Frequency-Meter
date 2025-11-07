#include "icc.h"

#include <stm32h7xx_hal.h>

volatile ICC_SharedData sharedData __attribute__((section(".icc_section")));

__weak void icc_recv_cb() {
    return;
}
__weak void icc_recv_variable_cb(){
    return;
}

//HSEM_Common_TypeDef * hsem = HSEM_COMMON;

void icc_wake_up_M4() {
    // enable HSEM clock
    __HAL_RCC_HSEM_CLK_ENABLE();

    // take the semaphore
    HAL_HSEM_FastTake(ICC_WAKEUP_SEMID);

    // release the semaphore
    HAL_HSEM_Release(ICC_WAKEUP_SEMID, 0);
}

void icc_init() {
    iccq_create(&(sharedData.fourBound), sharedData.pFourBound, ICC_QUEUE_LENGTH, 1);
    iccq_create(&(sharedData.sevenBound), sharedData.pSevenBound, ICC_QUEUE_LENGTH, 1);
    iccq_create(&(sharedData.fourBoundVariable),(uint8_t*) &sharedData.pFourBoundVariable, 2,4);
    iccq_create(&(sharedData.sevenBoundVariable),(uint8_t*) &sharedData.pSevenBoundVariable, 2,4);
}

void icc_open_pipe() {
    // uint32_t a = hsem->IER;
    HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(ICC_SEVENBOUND_SEMID));
    HAL_NVIC_SetPriority(HSEM1_IRQn, 0x7, 0);
    HAL_NVIC_EnableIRQ(HSEM1_IRQn);
}

void HSEM1_IRQHandler() {
    HAL_HSEM_IRQHandler();
}

void HAL_HSEM_FreeCallback(uint32_t SemMask) {
    icc_recv_cb();
    icc_recv_variable_cb();
    HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(ICC_SEVENBOUND_SEMID));
    return;
}

void icc_notify() {
    HAL_HSEM_FastTake(ICC_FOURBOUND_SEMID);
    HAL_HSEM_Release(ICC_FOURBOUND_SEMID, 0);
}


// ------

ICCQueue * icc_get_outbound_pipe() {
    return &(sharedData.fourBound);
}

ICCQueue * icc_get_inbound_pipe() {
    return &(sharedData.sevenBound);
}
ICCQueue * icc_get_outbound_variable_pipe() {
    return &(sharedData.fourBoundVariable);
}
ICCQueue * icc_get_inbound_variable_pipe() {
    return &(sharedData.sevenBoundVariable);
}