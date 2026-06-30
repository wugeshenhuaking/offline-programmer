#ifndef __BSP_USART_H__
#define __BSP_USART_H__

#include <bsp.h>

void bsp_usart_init(void);
void bsp_usart_blocking_tx_init(uint32_t baudrate);
int rs485_send(uint8_t *buf, int len);
int rs485_send_blocking_raw(const uint8_t *buf, int len);
int rs485_receive(uint8_t *buf, int bufsz, uint32_t timeout_ms);
void rs485_receive_rearm(void);

extern volatile uint32_t g_rs485_dbg_stage;
extern volatile uint32_t g_rs485_dbg_wait_flag;
extern volatile uint32_t g_rs485_dbg_uart4_sts;
extern volatile uint32_t g_rs485_dbg_uart4_ctrl1;
extern volatile uint32_t g_rs485_dbg_len;
extern volatile uint32_t g_rs485_dbg_index;


#endif // __BSP_USART_H__

