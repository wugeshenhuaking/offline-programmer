#include "bsp_usart.h"

#define RS485_TX_EN gpio_bits_set(RS485_EN2_GPIO_PORT,RS485_EN2_PIN)
#define RS485_RX_EN gpio_bits_reset(RS485_EN2_GPIO_PORT,RS485_EN2_PIN)

typedef struct
{
  usart_type *usart;
  uint32_t baud_rate;
} bsp_usart_config_t;

#define MODBUS_FRAME_MAX_LENGTH 256

uint8_t ctx_send_buf[MODBUS_FRAME_MAX_LENGTH];
uint8_t ctx_read_buf[MODBUS_FRAME_MAX_LENGTH];

void bsp_usart_init(void)
{

}

int rs485_send(uint8_t *buf, int len)
{
  RS485_TX_EN();
  
  RS485_RX_EN();
  return len;
}

int rs485_receive(uint8_t *buf, int bufsz, int timeout, int bytes_timeout)
{
  int len = 0;

  while(1)
  {
    

  }
}

void UART4_IRQHandler(void)
{
  /* add user code begin UART4_IRQ 0 */

  /* add user code end UART4_IRQ 0 */

  __IO uint16_t val_tx;

  if(usart_interrupt_flag_get(UART4, USART_TDBE_FLAG) != RESET)
  {
    /* add user code begin UART4_USART_TDBE_FLAG */
    /* handle data transmit and clear flag */
    val_tx = 0xAA;
    usart_data_transmit(UART4, val_tx);
    /* add user code end UART4_USART_TDBE_FLAG */ 
  }

  if(usart_interrupt_flag_get(UART4, USART_TDC_FLAG) != RESET)
  {
    /* add user code begin UART4_USART_TDC_FLAG */
    /* clear flag */
    usart_flag_clear(UART4, USART_TDC_FLAG);
    /* add user code end UART4_USART_TDC_FLAG */
  }

  if(usart_interrupt_flag_get(UART4, USART_IDLEF_FLAG) != RESET)
  {
    /* add user code begin UART4_USART_IDLEF_FLAG */
    /* clear flag */
    usart_flag_clear(UART4, USART_IDLEF_FLAG);
    /* add user code end UART4_USART_IDLEF_FLAG */ 
  }

  /* add user code begin UART4_IRQ 1 */

  /* add user code end UART4_IRQ 1 */
}

void DMA1_Channel1_IRQHandler(void)
{
  /* add user code begin DMA1_Channel1_IRQ 0 */

  /* add user code end DMA1_Channel1_IRQ 0 */

  if(dma_interrupt_flag_get(DMA1_FDT1_FLAG) != RESET)
  {   
    /* add user code begin DMA1_FDT1_FLAG */
    /* handle full data transfer and clear flag */
    dma_flag_clear(DMA1_FDT1_FLAG);
    /* add user code end DMA1_FDT1_FLAG */ 
  }

  if(dma_interrupt_flag_get(DMA1_HDT1_FLAG) != RESET)
  {   
    /* add user code begin DMA1_HDT1_FLAG */
    /* handle half data transfer and clear flag */
    dma_flag_clear(DMA1_HDT1_FLAG);
    /* add user code end DMA1_HDT1_FLAG */ 
  }

  /* add user code begin DMA1_Channel1_IRQ 1 */

  /* add user code end DMA1_Channel1_IRQ 1 */
} 

