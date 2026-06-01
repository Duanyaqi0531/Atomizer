/**
 ******************************************************************************
 * @file    bsp_at24c02.h
 * @brief   AT24C02 256-byte I2C EEPROM 驱动
 *
 *  I2C1: PB6-SCL, PB7-SDA
 *  Device address: 0xA0
 ******************************************************************************
 */

#ifndef __BSP_AT24C02_H
#define __BSP_AT24C02_H

#include "stm32f10x.h"

void AT24C02_Init(void);
void AT24C02_WriteByte(uint8_t addr, uint8_t data);
uint8_t AT24C02_ReadByte(uint8_t addr);

#endif
