/**
 ******************************************************************************
 * @file    bsp_pwm.c
 * @brief   TIM2_CH3 (PA2) PWM 输出驱动
 *
 *          PWM 频率 10kHz，占空比 0~100 直接对应屏幕百分比
 *
 *          野火 F103-指南者, PA2 空闲未用
 ******************************************************************************
 */

#include "./pwm/bsp_pwm.h"

/**
 * @brief  初始化 TIM2_CH3 PWM 输出 (PA2)
 * @param  无
 * @retval 无
 */
void PWM_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    /* ---- 开启时钟 ---- */
    RCC_APB2PeriphClockCmd(PWM_GPIO_CLK, ENABLE);
    RCC_APB1PeriphClockCmd(PWM_TIM_CLK, ENABLE);

    /* ---- PA2 配置为复用推挽输出 ---- */
    GPIO_InitStructure.GPIO_Pin   = PWM_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PWM_GPIO_PORT, &GPIO_InitStructure);

    /* ---- TIM2 时基：72MHz / 72 = 1MHz, ARR=99 → 10kHz ---- */
    TIM_TimeBaseStructure.TIM_Period        = PWM_PERIOD - 1;    /* 99 */
    TIM_TimeBaseStructure.TIM_Prescaler     = 71;                /* 72MHz / (71+1) = 1MHz */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(PWM_TIM, &TIM_TimeBaseStructure);

    /* ---- PWM 模式 1，默认占空比 50% ---- */
    TIM_OCInitStructure.TIM_OCMode        = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState   = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse         = 50;                  /* 默认 50% */
    TIM_OCInitStructure.TIM_OCPolarity    = TIM_OCPolarity_High;
    TIM_OC3Init(PWM_TIM, &TIM_OCInitStructure);

    TIM_OC3PreloadConfig(PWM_TIM, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(PWM_TIM, ENABLE);

    /* ---- 启动定时器 ---- */
    TIM_Cmd(PWM_TIM, ENABLE);
}

/**
 * @brief  设置 PWM 占空比
 * @param  percent: 0 ~ 100，对应 0% ~ 100%
 * @retval 无
 */
void PWM_SetDuty(uint8_t percent)
{
    if (percent > 100) percent = 100;
    TIM_SetCompare3(PWM_TIM, percent);
}

/*********************************************END OF FILE**********************/
