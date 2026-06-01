/**
 ******************************************************************************
 * @file    bsp_esp8266.h
 * @brief   ESP8266 驱动 — 单缓冲稳定版
 ****************************************************************************** */

#ifndef __BSP_ESP8266_H
#define __BSP_ESP8266_H

#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* USART3 */
#define ESP8266_USART            USART3
#define ESP8266_USART_CLK        RCC_APB1Periph_USART3
#define ESP8266_USART_APBxCmd    RCC_APB1PeriphClockCmd
#define ESP8266_USART_BAUDRATE   115200
#define ESP8266_USART_IRQ        USART3_IRQn
#define ESP8266_USART_IRQHandler USART3_IRQHandler

/* GPIO — PB8-RST, PB9-EN, PB10-TX, PB11-RX */
#define ESP8266_GPIO_CLK         (RCC_APB2Periph_GPIOB)
#define ESP8266_GPIO_APBxCmd     RCC_APB2PeriphClockCmd
#define ESP8266_TX_PORT          GPIOB
#define ESP8266_TX_PIN           GPIO_Pin_10
#define ESP8266_RX_PORT          GPIOB
#define ESP8266_RX_PIN           GPIO_Pin_11
#define ESP8266_RST_PORT         GPIOB
#define ESP8266_RST_PIN          GPIO_Pin_8
#define ESP8266_EN_PORT          GPIOB
#define ESP8266_EN_PIN           GPIO_Pin_9

#define ESP8266_RX_BUF_SIZE      1024

/* ======= 接口 ======= */
void     ESP8266_Init(void);
void     ESP8266_ClearRxBuf(void);
void     ESP8266_SendCmd(const char *fmt, ...);
uint8_t  ESP8266_WaitResponse(const char *expect, uint32_t timeout_ms);
uint8_t  ESP8266_WaitOK(uint32_t timeout_ms);

uint8_t  ESP8266_StartAP(const char *ssid, const char *pwd);
uint8_t  ESP8266_StartTCPServer(uint16_t port);
uint8_t  ESP8266_SendHTTP(uint8_t link_id, const char *body);
uint8_t  ESP8266_ParseHTTP(char *path_buf, uint16_t *len, uint8_t *link_id);
void     ESP8266_CloseLink(uint8_t link_id);

#endif
