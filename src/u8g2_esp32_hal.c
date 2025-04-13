#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "u8g2_esp32_hal.h"

static const char* TAG = "u8g2_hal";

static spi_device_handle_t handle_spi;   // SPI handle.
static u8g2_esp32_hal_t u8g2_esp32_hal;  // HAL state data.

#define HOST    SPI2_HOST

#undef ESP_ERROR_CHECK
#define ESP_ERROR_CHECK(x)                   \
  do {                                       \
    esp_err_t rc = (x);                      \
    if (rc != ESP_OK) {                      \
      ESP_LOGE("err", "esp_err_t = %d", rc); \
      assert(0 && #x);                       \
    }                                        \
  } while (0);

/*
 * Initialze the ESP32 HAL.
 */
void u8g2_esp32_hal_init(u8g2_esp32_hal_t u8g2_esp32_hal_param) {
  u8g2_esp32_hal = u8g2_esp32_hal_param;
}  // u8g2_esp32_hal_init

/*
 * HAL callback function as prescribed by the U8G2 library.  This callback is
 * invoked to handle SPI communications.
 */
uint8_t u8g2_esp32_spi_byte_cb(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
  ESP_LOGD(TAG, "spi_byte_cb: Received a msg: %d, arg_int: %d, arg_ptr: %p", msg, arg_int, arg_ptr);
  switch (msg) {
    case U8X8_MSG_BYTE_SET_DC:
      if (u8g2_esp32_hal.dc != U8G2_ESP32_HAL_UNDEFINED) {
        gpio_set_level(u8g2_esp32_hal.dc, arg_int);
      }
      break;

    case U8X8_MSG_BYTE_INIT: {
      if (u8g2_esp32_hal.clk == U8G2_ESP32_HAL_UNDEFINED ||
          u8g2_esp32_hal.mosi == U8G2_ESP32_HAL_UNDEFINED ||
          u8g2_esp32_hal.cs == U8G2_ESP32_HAL_UNDEFINED) {
        break;
      }

      spi_bus_config_t bus_config = {
        .sclk_io_num = u8g2_esp32_hal.clk,
        .mosi_io_num = u8g2_esp32_hal.mosi,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC
      };

      // ESP_LOGI(TAG, "... Initializing bus.");
      ESP_ERROR_CHECK(spi_bus_initialize(HOST, &bus_config, SPI_DMA_CH_AUTO));

      spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000, // 10 MHz
        .mode = 0,                          // SPI mode 0
        .queue_size = 1,
    };
      // ESP_LOGI(TAG, "... Adding device bus.");
      ESP_ERROR_CHECK(spi_bus_add_device(HOST, &devcfg, &handle_spi));

      break;
    }

    case U8X8_MSG_BYTE_SEND: {
      spi_transaction_t trans_desc = {
        .addr = 0,
        .cmd = 0,
        .flags = 0,
        .length = 8 * arg_int,  // Number of bits NOT number of bytes.
        .rxlength = 0,
        .tx_buffer = arg_ptr,
        .rx_buffer = NULL
      };

      // ESP_LOGI(TAG, "... Transmitting %d bytes.", arg_int);
      ESP_ERROR_CHECK(spi_device_transmit(handle_spi, &trans_desc));
      break;
    }
  }
  return 0;
}  // u8g2_esp32_spi_byte_cb

/*
 * HAL callback function as prescribed by the U8G2 library.  This callback is
 * invoked to handle callbacks for GPIO and delay functions.
 */
uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t* u8x8,
                                     uint8_t msg,
                                     uint8_t arg_int,
                                     void* arg_ptr) {
  ESP_LOGD(TAG,
           "gpio_and_delay_cb: Received a msg: %d, arg_int: %d, arg_ptr: %p",
           msg, arg_int, arg_ptr);

  switch (msg) {
      // Initialize the GPIO and DELAY HAL functions.  If the pins for DC and
      // RESET have been specified then we define those pins as GPIO outputs.
    case U8X8_MSG_GPIO_AND_DELAY_INIT: {
      uint64_t bitmask = 0;
      if (u8g2_esp32_hal.dc != U8G2_ESP32_HAL_UNDEFINED) {
        bitmask = bitmask | (1ull << u8g2_esp32_hal.dc);
      }
      if (u8g2_esp32_hal.reset != U8G2_ESP32_HAL_UNDEFINED) {
        bitmask = bitmask | (1ull << u8g2_esp32_hal.reset);
      }
      if (u8g2_esp32_hal.cs != U8G2_ESP32_HAL_UNDEFINED) {
        bitmask = bitmask | (1ull << u8g2_esp32_hal.cs);
      }

      if (bitmask == 0) {
        break;
      }
      gpio_config_t gpioConfig;
      gpioConfig.pin_bit_mask = bitmask;
      gpioConfig.mode = GPIO_MODE_OUTPUT;
      gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
      gpioConfig.pull_down_en = GPIO_PULLDOWN_ENABLE;
      gpioConfig.intr_type = GPIO_INTR_DISABLE;
      gpio_config(&gpioConfig);
      break;
    }

      // Set the GPIO reset pin to the value passed in through arg_int.
    case U8X8_MSG_GPIO_RESET:
      if (u8g2_esp32_hal.reset != U8G2_ESP32_HAL_UNDEFINED) {
        gpio_set_level(u8g2_esp32_hal.reset, arg_int);
      }
      break;
      // Set the GPIO client select pin to the value passed in through arg_int.
    case U8X8_MSG_GPIO_CS:
      if (u8g2_esp32_hal.cs != U8G2_ESP32_HAL_UNDEFINED) {
        gpio_set_level(u8g2_esp32_hal.cs, arg_int);
      }
      break;
      // Delay for the number of milliseconds passed in through arg_int.
    case U8X8_MSG_DELAY_MILLI:
      vTaskDelay(arg_int / portTICK_PERIOD_MS);
      break;
  }
  return 0;
}  // u8g2_esp32_gpio_and_delay_cb
