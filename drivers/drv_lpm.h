#ifndef __DRV_LPM_H__
#define __DRV_LPM_H__



#include "rtthread.h"



#define LPM_RCC_GPIOA			0
#define LPM_RCC_GPIOB			1
#define LPM_RCC_GPIOC			2
#define LPM_RCC_GPIOD			3
#define LPM_RCC_GPIOE			4
#define LPM_RCC_GPIOF			5
#define LPM_RCC_GPIOG			6
#define LPM_NUM_PERIPH_RCC		7



void lpm_rcc_en(rt_uint8_t rcc);
void lpm_rcc_dis(rt_uint8_t rcc);
void lpm_cpu_ref(rt_uint8_t dir);
void lpm_enter_stop(void);



#endif
