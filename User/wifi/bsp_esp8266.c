/**
 ******************************************************************************
 * @file    bsp_esp8266.c
 * @brief   ESP8266 AT 驱动 — 单缓冲 + 手动等待，稳定优先
 ******************************************************************************
 */

#include "bsp_esp8266.h"
#include <stdarg.h>

volatile uint8_t  rx_buf[ESP8266_RX_BUF_SIZE];
volatile uint16_t rx_len  = 0;
volatile uint8_t  rx_flag = 0;

static void delay_ms(uint32_t ms)
{
    while (ms--) for (volatile uint32_t i = 0; i < 8000; i++);
}

/* ================================================================
 *  底层 UART 输出
 * ================================================================ */

static void u3_putc(uint8_t ch)
{
    USART_SendData(ESP8266_USART, ch);
    while (USART_GetFlagStatus(ESP8266_USART, USART_FLAG_TXE) == RESET);
}

static void u3_write(const char *s, uint16_t len)
{
    while (len--) u3_putc((uint8_t)*s++);
}

/* ================================================================
 *  SendCmd — 清缓冲，发 AT 指令
 * ================================================================ */

void ESP8266_SendCmd(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    rx_len  = 0; rx_flag = 0;
    memset((void *)rx_buf, 0, ESP8266_RX_BUF_SIZE);

    u3_write(buf, strlen(buf));
    u3_putc('\r');
    u3_putc('\n');
}

/* ================================================================
 *  等待指定字符串 — 在 rx_buf 中搜索
 * ================================================================ */

uint8_t ESP8266_WaitResponse(const char *expect, uint32_t timeout_ms)
{
    while (timeout_ms > 0) {
        if (rx_flag) {
            rx_flag = 0;
            if (strstr((const char *)rx_buf, expect) != NULL) return 1;
            if (strstr((const char *)rx_buf, "ERROR") != NULL)  return 0;
            if (strstr((const char *)rx_buf, "FAIL")  != NULL)  return 0;
            rx_len = 0;
            memset((void *)rx_buf, 0, ESP8266_RX_BUF_SIZE);
        }
        delay_ms(1);
        timeout_ms--;
    }
    return 0;
}

uint8_t ESP8266_WaitOK(uint32_t t) { return ESP8266_WaitResponse("OK", t); }

void ESP8266_ClearRxBuf(void)
{
    rx_len = 0; rx_flag = 0;
    memset((void *)rx_buf, 0, ESP8266_RX_BUF_SIZE);
}

/* ================================================================
 *  初始化
 * ================================================================ */

void ESP8266_Init(void)
{
    GPIO_InitTypeDef  g;
    USART_InitTypeDef u;
    NVIC_InitTypeDef  n;

    RCC_APB1PeriphClockCmd(ESP8266_USART_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(ESP8266_GPIO_CLK, ENABLE);

    g.GPIO_Pin = ESP8266_RST_PIN; g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(ESP8266_RST_PORT, &g);
    GPIO_SetBits(ESP8266_RST_PORT, ESP8266_RST_PIN);

    g.GPIO_Pin = ESP8266_EN_PIN;
    GPIO_Init(ESP8266_EN_PORT, &g);
    GPIO_SetBits(ESP8266_EN_PORT, ESP8266_EN_PIN);

    g.GPIO_Pin = ESP8266_TX_PIN; g.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(ESP8266_TX_PORT, &g);

    g.GPIO_Pin = ESP8266_RX_PIN; g.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(ESP8266_RX_PORT, &g);

    u.USART_BaudRate            = ESP8266_USART_BAUDRATE;
    u.USART_WordLength          = USART_WordLength_8b;
    u.USART_StopBits            = USART_StopBits_1;
    u.USART_Parity              = USART_Parity_No;
    u.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    u.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(ESP8266_USART, &u);
    USART_Cmd(ESP8266_USART, ENABLE);

    USART_ITConfig(ESP8266_USART, USART_IT_RXNE, ENABLE);
    n.NVIC_IRQChannel                   = ESP8266_USART_IRQ;
    n.NVIC_IRQChannelPreemptionPriority = 1;
    n.NVIC_IRQChannelSubPriority        = 1;
    n.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&n);

    ESP8266_ClearRxBuf();
}

/* ================================================================
 *  AP 热点 — 延长等待时间，确保稳定
 * ================================================================ */

uint8_t ESP8266_StartAP(const char *ssid, const char *pwd)
{
    /* 硬复位 — 拉低 RST 再拉高 */
    GPIO_ResetBits(ESP8266_RST_PORT, ESP8266_RST_PIN);
    delay_ms(100);
    GPIO_SetBits(ESP8266_RST_PORT, ESP8266_RST_PIN);
    delay_ms(3000);   /* 等 ready */
    ESP8266_ClearRxBuf();

    ESP8266_SendCmd("AT");
    if (!ESP8266_WaitOK(3000)) return 0;

    ESP8266_SendCmd("AT+CWMODE=2");
    if (!ESP8266_WaitOK(3000)) return 0;

    /* 通道 1, 加密 3=WPA2_PSK */
    ESP8266_SendCmd("AT+CWSAP=\"%s\",\"%s\",1,3", ssid, pwd);
    if (!ESP8266_WaitOK(5000)) return 0;

    ESP8266_SendCmd("AT+CIPMUX=1");
    if (!ESP8266_WaitOK(3000)) return 0;

    /* 服务器不超时断开 */
    ESP8266_SendCmd("AT+CIPSTO=0");
    ESP8266_WaitOK(3000);

    return 1;
}

/* ================================================================
 *  启动 TCP Server
 * ================================================================ */

uint8_t ESP8266_StartTCPServer(uint16_t port)
{
    ESP8266_SendCmd("AT+CIPSERVER=1,%u", port);
    return ESP8266_WaitOK(5000);
}

/* ================================================================
 *  SendHTTP — 发送完整 HTTP 响应
 *
 *  流程: AT+CIPSEND → 等 ">" → 发数据 → 等 "SEND OK"
 *  注意: 等 ">" 期间可能会收到其他数据（如新连接通知），
 *        所以用 busy loop 扫描 ">" 而不是依赖 WaitResponse
 * ================================================================ */

uint8_t ESP8266_SendHTTP(uint8_t link_id, const char *body)
{
    uint16_t blen = strlen(body);

    /* 构建 header + body */
    char pkt[2048];
    int total = snprintf(pkt, sizeof(pkt),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        (int)blen, body);

    /* AT+CIPSEND */
    ESP8266_ClearRxBuf();
    ESP8266_SendCmd("AT+CIPSEND=%u,%d", link_id, total);

    /* 等 ">" — 可能在 rx_buf 中，也可能是后续收到的 */
    {
        uint32_t t = 8000;
        while (t > 0) {
            if (rx_flag) {
                rx_flag = 0;
                if (strstr((const char *)rx_buf, ">") != NULL) break;
                if (strstr((const char *)rx_buf, "ERROR") != NULL) return 0;
                rx_len = 0;
                memset((void *)rx_buf, 0, ESP8266_RX_BUF_SIZE);
            }
            delay_ms(1); t--;
        }
        if (t == 0) return 0;
    }

    /* 发送响应数据 */
    u3_write(pkt, total);

    /* 等 SEND OK */
    {
        uint32_t t = 10000;
        while (t > 0) {
            if (rx_flag) {
                rx_flag = 0;
                if (strstr((const char *)rx_buf, "SEND OK") != NULL) return 1;
                if (strstr((const char *)rx_buf, "ERROR")   != NULL) return 0;
                rx_len = 0;
                memset((void *)rx_buf, 0, ESP8266_RX_BUF_SIZE);
            }
            delay_ms(1); t--;
        }
    }
    return 0;
}

/* ================================================================
 *  ParseHTTP — 解析 HTTP GET 请求路径
 *
 *  关键: 收到 rx_flag 后加短暂延迟让 HTTP 数据收全，
 *        然后在缓冲区搜索 +IPD,<id>,<len>:GET /path
 * ================================================================ */

uint8_t ESP8266_ParseHTTP(char *path_buf, uint16_t *len, uint8_t *link_id)
{
    if (!rx_flag) return 0;

    /* 延迟等数据收全 (~50ms) */
    {
        volatile uint32_t w;
        for (w = 0; w < 400000; w++);
    }

    rx_flag = 0;
    char *raw = (char *)rx_buf;

    /* 查找 +IPD, */
    char *p = strstr(raw, "+IPD,");
    if (!p) {
        ESP8266_ClearRxBuf();
        return 0;
    }

    p += 5;
    *link_id = (uint8_t)atoi(p);

    p = strchr(p, ',');
    if (!p) { ESP8266_ClearRxBuf(); return 0; }
    p++;

    p = strchr(p, ':');
    if (!p) { ESP8266_ClearRxBuf(); return 0; }
    p++;

    /* 查找 GET */
    char *g = strstr(p, "GET ");
    if (!g) { ESP8266_ClearRxBuf(); return 0; }
    g += 4;

    /* 提取路径 */
    char *end = g;
    while (*end && *end != ' ' && *end != '\r' && *end != '\n') end++;

    uint16_t pl = (uint16_t)(end - g);
    if (pl == 0) {
        path_buf[0] = '/'; path_buf[1] = '\0'; *len = 1;
    } else {
        if (pl > *len - 1) pl = *len - 1;
        memcpy(path_buf, g, pl);
        path_buf[pl] = '\0';
        *len = pl;
    }

    ESP8266_ClearRxBuf();
    return 1;
}

void ESP8266_CloseLink(uint8_t link_id)
{
    ESP8266_SendCmd("AT+CIPCLOSE=%u", link_id);
    ESP8266_WaitOK(3000);
}

/* ================================================================
 *  USART3 中断 — 最简单的接收
 * ================================================================ */

void USART3_IRQHandler(void)
{
    if (USART_GetITStatus(ESP8266_USART, USART_IT_RXNE) == RESET) return;

    uint8_t ch = USART_ReceiveData(ESP8266_USART);
    if (rx_len < ESP8266_RX_BUF_SIZE - 1) {
        rx_buf[rx_len++] = ch;
        rx_buf[rx_len]   = '\0';
    } else {
        rx_len = 0;
        memset((void *)rx_buf, 0, ESP8266_RX_BUF_SIZE);
    }
    if (ch == '\n') rx_flag = 1;
}
