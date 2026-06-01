/**
 ******************************************************************************
 * @file    bsp_at24c02.c
 * @brief   AT24C02 I2C EEPROM 驱动 (软件模拟 I2C, GPIO)
 *
 *  野火指南者: PB6-SCL, PB7-SDA
 *  AT24C02 设备地址: 0xA0 (写), 0xA1 (读)
 ******************************************************************************
 */

#include "bsp_at24c02.h"

/* ---- 引脚定义 ---- */
#define AT24C02_SCL_PORT    GPIOB
#define AT24C02_SCL_PIN     GPIO_Pin_6
#define AT24C02_SDA_PORT    GPIOB
#define AT24C02_SDA_PIN     GPIO_Pin_7
#define AT24C02_GPIO_CLK    RCC_APB2Periph_GPIOB

#define AT24C02_ADDR_WR     0xA0
#define AT24C02_ADDR_RD     0xA1

static void delay_us(uint32_t us)
{
    while (us--) {
        for (volatile uint32_t i = 0; i < 8; i++);
    }
}

/* ---- SDA/SCL 操作 ---- */
#define SDA_H()   GPIO_SetBits(AT24C02_SDA_PORT, AT24C02_SDA_PIN)
#define SDA_L()   GPIO_ResetBits(AT24C02_SDA_PORT, AT24C02_SDA_PIN)
#define SCL_H()   GPIO_SetBits(AT24C02_SCL_PORT, AT24C02_SCL_PIN)
#define SCL_L()   GPIO_ResetBits(AT24C02_SCL_PORT, AT24C02_SCL_PIN)
#define SDA_IN()  GPIO_ReadInputDataBit(AT24C02_SDA_PORT, AT24C02_SDA_PIN)

static void SDA_OUT(void)
{
    GPIO_InitTypeDef g;
    g.GPIO_Pin   = AT24C02_SDA_PIN;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Mode  = GPIO_Mode_Out_OD;  /* 开漏输出 */
    GPIO_Init(AT24C02_SDA_PORT, &g);
}

static void SDA_IN_FLOAT(void)
{
    GPIO_InitTypeDef g;
    g.GPIO_Pin  = AT24C02_SDA_PIN;
    g.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(AT24C02_SDA_PORT, &g);
}

/* ---- I2C 起始信号 ---- */
static void I2C_Start(void)
{
    SDA_OUT();
    SDA_H();
    SCL_H();
    delay_us(5);
    SDA_L();
    delay_us(5);
    SCL_L();
}

/* ---- I2C 停止信号 ---- */
static void I2C_Stop(void)
{
    SDA_OUT();
    SDA_L();
    SCL_H();
    delay_us(5);
    SDA_H();
    delay_us(5);
}

/* ---- 等待 ACK ---- */
static uint8_t I2C_WaitAck(void)
{
    uint8_t ack;
    SDA_IN_FLOAT();
    SCL_H();
    delay_us(5);
    ack = SDA_IN();
    SCL_L();
    SDA_OUT();
    return (ack == 0) ? 1 : 0;
}

/* ---- 发送 ACK ---- */
static void I2C_Ack(void)
{
    SDA_OUT();
    SDA_L();
    SCL_H();
    delay_us(5);
    SCL_L();
}

/* ---- 发送 NACK ---- */
static void I2C_NAck(void)
{
    SDA_OUT();
    SDA_H();
    SCL_H();
    delay_us(5);
    SCL_L();
}

/* ---- 发送一个字节 ---- */
static void I2C_SendByte(uint8_t data)
{
    SDA_OUT();
    for (uint8_t i = 0; i < 8; i++) {
        if (data & 0x80)
            SDA_H();
        else
            SDA_L();
        data <<= 1;
        SCL_H();
        delay_us(5);
        SCL_L();
        delay_us(5);
    }
}

/* ---- 读取一个字节 ---- */
static uint8_t I2C_ReadByte(void)
{
    uint8_t data = 0;
    SDA_IN_FLOAT();
    for (uint8_t i = 0; i < 8; i++) {
        data <<= 1;
        SCL_H();
        delay_us(5);
        if (SDA_IN()) data |= 1;
        SCL_L();
        delay_us(5);
    }
    SDA_OUT();
    return data;
}

/* ================================================================
 *  公开接口
 * ================================================================ */

void AT24C02_Init(void)
{
    RCC_APB2PeriphClockCmd(AT24C02_GPIO_CLK, ENABLE);

    GPIO_InitTypeDef g;

    /* SCL - PB6 开漏输出 */
    g.GPIO_Pin   = AT24C02_SCL_PIN;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Mode  = GPIO_Mode_Out_OD;
    GPIO_Init(AT24C02_SCL_PORT, &g);

    /* SDA - PB7 初始开漏输出 */
    g.GPIO_Pin  = AT24C02_SDA_PIN;
    GPIO_Init(AT24C02_SDA_PORT, &g);

    /* 总线释放 */
    SDA_H();
    SCL_H();
}

void AT24C02_WriteByte(uint8_t addr, uint8_t data)
{
    I2C_Start();
    I2C_SendByte(AT24C02_ADDR_WR);
    I2C_WaitAck();
    I2C_SendByte(addr);        /* 片内地址 */
    I2C_WaitAck();
    I2C_SendByte(data);
    I2C_WaitAck();
    I2C_Stop();

    /* 等待写入完成 (AT24C02 最多 5ms) */
    for (volatile uint32_t i = 0; i < 50000; i++);
}

uint8_t AT24C02_ReadByte(uint8_t addr)
{
    uint8_t data;

    /* 假写：设置要读的地址 */
    I2C_Start();
    I2C_SendByte(AT24C02_ADDR_WR);
    I2C_WaitAck();
    I2C_SendByte(addr);
    I2C_WaitAck();

    /* 重新开始，读 */
    I2C_Start();
    I2C_SendByte(AT24C02_ADDR_RD);
    I2C_WaitAck();
    data = I2C_ReadByte();
    I2C_NAck();
    I2C_Stop();

    return data;
}
