#ifndef __BSP_PWM_H
#define __BSP_PWM_H

#include "stm32f10x.h"

/* ======== PWM 输出引脚：PA2 (TIM2_CH3) ======== */
#define PWM_GPIO_PORT           GPIOA
#define PWM_GPIO_CLK            RCC_APB2Periph_GPIOA
#define PWM_GPIO_PIN            GPIO_Pin_2

/* ======== 定时器：TIM2 ======== */
#define PWM_TIM                 TIM2
#define PWM_TIM_CLK             RCC_APB1Periph_TIM2

/* ======== PWM 参数 ======== */
#define PWM_PERIOD              100     /* ARR = 100, 占空比 0~100 直接对应百分比  */

void PWM_Init(void);
void PWM_SetDuty(uint8_t percent);      /* 0 ~ 100 */

#endif /* __BSP_PWM_H */
