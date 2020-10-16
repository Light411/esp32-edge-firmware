/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2020 Martin Jäger / Libre Solar
 */

#include "serial.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"

#include "driver/uart.h"

bool pub_serial_received = false;
bool resp_serial_received = false;

#if CONFIG_THINGSET_SERIAL

static char serial_json_buf[500];

static const int uart_num = UART_NUM_2;
static const int UART_RX_BUF_SIZE = 1024;

char *get_serial_json_data()
{
    return serial_json_buf;
}

void uart_setup(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(
        uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(
        uart_set_pin(uart_num, CONFIG_GPIO_UART_TX, CONFIG_GPIO_UART_RX,
            UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(
        uart_driver_install(uart_num, UART_RX_BUF_SIZE * 2, 0, 0, NULL, 0));
}

void uart_rx_task(void *arg)
{
    uint8_t data[UART_RX_BUF_SIZE];
    int pos = 0;        // stores next free position for incoming characters

    while (1) {
        const int len = uart_read_bytes(uart_num, &data[pos], 1, 20 / portTICK_RATE_MS);

        if (len != 1) {
            continue;
        }

        // \r\n and \n are markers for line end, i.e. command end
        // we accept this at any time, even if the buffer is 'full', since
        // there is always one last character left for the \0
        if (data[pos] == '\n') {
            if (pos > 0 && data[pos-1] == '\r') {
                data[pos-1] = '\0';
            }
            else {
                data[pos] = '\0';
            }
            // start processing
            ESP_LOGI("serial", "%s", data);
            if (data[0] == '#' && data[1] == ' ' && pos > 2) {
                //printf("Received pub msg with %d bytes: %s\n", pos, &data[2]);
                int len_json = strlen((char *)&data[2]);
                if (pub_serial_received == false && len_json < sizeof(serial_json_buf) - 1) {
                    // copy json string
                    strncpy(serial_json_buf, (char *)&data[2], len_json);
                    serial_json_buf[len_json] = '\0';
                    pub_serial_received = true;
                }
            }
            else if (data[0] == ':') {
            //else if (strstr((char *)data, ":85 Content. ") == data) {
                ESP_LOGI("serial", "Received response with %d bytes: %s\n", pos, &data[2]);
                int len_json = strlen((char *)&data[13]);
                if (resp_serial_received == false && len_json < sizeof(serial_json_buf) - 2) {
                    // copy json string
                    strncpy(serial_json_buf, (char *)&data[13], len_json);
                    serial_json_buf[len_json] = '\n';
                    serial_json_buf[len_json + 1] = '\0';
                    resp_serial_received = true;
                }
            }
            pos = 0;
        }
        // Fill the buffer up to all but 1 character (the last character is reserved for '\0')
        // Characters beyond the size of the buffer are dropped.
        else if (pos < (sizeof(data) - 1)) {
            pos++;
        }
    }
}

void uart_send(char *req)
{
    uart_write_bytes(uart_num, req, strlen(req));
}

#else /* not CONFIG_THINGSET_SERIAL */

char *get_serial_json_data()
{
    return "{}";
}

#endif
