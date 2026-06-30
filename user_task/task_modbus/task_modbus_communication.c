#include "task_modbus_communication.h"
#include "bsp_usart\bsp_usart.h"

#include <agile_modbus.h>
#include <agile_modbus_slave_util.h>

#define LOG_TAG "MODBUS_SLAVE"
#define LOG_MODULE_LVL LOG_LVL_INFO
#include "log.h"

#define RS485_BLOCKING_TX_EXPERIMENT  0
#define RS485_TEST_BAUDRATE           19200U
#define RS485_TEST_PERIOD_MS          1000U
#define MODBUS_LOG_FRAME_DUMP         0

#define MODBUS_SLAVE_ADDR   2
#define MODBUS_RX_TIMEOUT   1000

TaskHandle_t task_modbus_communication_handle;

uint8_t ctx_send_buf[AGILE_MODBUS_MAX_ADU_LENGTH];
uint8_t ctx_read_buf[AGILE_MODBUS_MAX_ADU_LENGTH];

static const uint8_t rs485_test_frame[] = {0x55, 0xAA, 0x5A, 0xA5, 0x0D, 0x0A};

static void log_frame_hex(const char *prefix, const uint8_t *buf, int len)
{
#if !MODBUS_LOG_FRAME_DUMP
    (void)prefix;
    (void)buf;
    (void)len;
    return;
#else
    char line[3 * 16 + 4];
    int dump_len = (len > 16) ? 16 : len;
    int pos = 0;
    int i;

    if (buf == NULL || len <= 0)
    {
        log_info("%s len=%d", prefix, len);
        return;
    }

    for (i = 0; i < dump_len && pos < (int)sizeof(line); ++i)
    {
        pos += snprintf(&line[pos],
                        sizeof(line) - (size_t)pos,
                        (i == 0) ? "%02X" : " %02X",
                        buf[i]);
    }

    if (len > dump_len && pos < (int)sizeof(line))
    {
        snprintf(&line[pos], sizeof(line) - (size_t)pos, " ...");
    }

    log_info("%s len=%d, [%s]", prefix, len, line);
#endif
}

#if !RS485_BLOCKING_TX_EXPERIMENT
extern const agile_modbus_slave_util_map_t bit_maps[1];
extern const agile_modbus_slave_util_map_t input_bit_maps[1];
extern const agile_modbus_slave_util_map_t register_maps[1];
extern const agile_modbus_slave_util_map_t input_register_maps[1];

static int addr_check(agile_modbus_t *ctx,
                      struct agile_modbus_slave_info *slave_info)
{
    int slave = slave_info->sft->slave;

    if ((slave != ctx->slave) &&
        (slave != AGILE_MODBUS_BROADCAST_ADDRESS) &&
        (slave != 0xFF))
    {
        return -AGILE_MODBUS_EXCEPTION_UNKNOW;
    }

    return 0;
}

static const agile_modbus_slave_util_t slave_util = {
    bit_maps,
    sizeof(bit_maps) / sizeof(bit_maps[0]),
    input_bit_maps,
    sizeof(input_bit_maps) / sizeof(input_bit_maps[0]),
    register_maps,
    sizeof(register_maps) / sizeof(register_maps[0]),
    input_register_maps,
    sizeof(input_register_maps) / sizeof(input_register_maps[0]),
    addr_check,
    NULL,
    NULL
};
#endif

void task_modbus_communication_func(void *pvParameters)
{
    (void)pvParameters;

#if RS485_BLOCKING_TX_EXPERIMENT
    bsp_usart_blocking_tx_init(RS485_TEST_BAUDRATE);
    log_info("RS485 blocking TX experiment start, baud=%lu",
             (unsigned long)RS485_TEST_BAUDRATE);

    while (1)
    {
        int rc = 0;

        log_frame_hex("tx-exp", rs485_test_frame, (int)sizeof(rs485_test_frame));
        rc = rs485_send_blocking_raw(rs485_test_frame, (int)sizeof(rs485_test_frame));
        log_info("tx-exp rc=%d", rc);
        vTaskDelay(pdMS_TO_TICKS(RS485_TEST_PERIOD_MS));
    }
#else
    agile_modbus_rtu_t ctx_rtu;
    agile_modbus_t *ctx = &ctx_rtu._ctx;

    bsp_usart_init();

    agile_modbus_rtu_init(&ctx_rtu,
                          ctx_send_buf, sizeof(ctx_send_buf),
                          ctx_read_buf, sizeof(ctx_read_buf));
    agile_modbus_set_slave(ctx, MODBUS_SLAVE_ADDR);

    log_info("Modbus slave running, addr=%d", MODBUS_SLAVE_ADDR);

    while (1)
    {
        g_rs485_dbg_stage = 0x01U;
        int read_len = rs485_receive(ctx->read_buf,
                                     ctx->read_bufsz,
                                     MODBUS_RX_TIMEOUT);
        if (read_len == 0)
        {
            continue;
        }

        g_rs485_dbg_stage = 0x02U;
        int send_len = agile_modbus_slave_handle(ctx, read_len, 0,
                           agile_modbus_slave_util_callback,
                           &slave_util, NULL);
        if (send_len <= 0)
        {
            g_rs485_dbg_stage = 0x03U;
            rs485_receive_rearm();
            log_warn("slave_handle failed or broadcast, send_len=%d", send_len);
            continue;
        }

        g_rs485_dbg_stage = 0x04U;
        log_frame_hex("req", ctx->read_buf, read_len);
        g_rs485_dbg_stage = 0x05U;
        log_frame_hex("resp", ctx->send_buf, send_len);
        g_rs485_dbg_stage = 0x06U;

        {
            int rc = rs485_send(ctx->send_buf, send_len);
            g_rs485_dbg_stage = 0x30U;
            rs485_receive_rearm();
            g_rs485_dbg_stage = 0x31U;

            if (rc != send_len)
            {
                log_warn("Send failed, rc=%d", rc);
            }
            else
            {
                log_info("Send OK, rc=%d", rc);
            }
        }
    }
#endif
}

void task_modbus_communication_create(void)
{
    BaseType_t ret = xTaskCreate(task_modbus_communication_func,
                "modbus_slave",
                2048,
                NULL,
                2,
                &task_modbus_communication_handle);
    if (ret != pdPASS)
    {
        log_error("xTaskCreate modbus_slave FAILED! heap not enough?");
    }
}
