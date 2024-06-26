#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include <stdio.h>

#define I2C_BAUD_RATE 1000 * 1000
#define I2C_PER_BYTE_TIMEOUT_US 1000

#define AS5600_ADDRESS 0x36
#define AS5600_RAW_ANGLE_REGISTER 0x0c
#define AS5600_AGC_REGISTER 0x1a

#define LED_GPIO 22

bool i2c_read(i2c_inst_t *i2c, uint8_t addr, uint8_t *dst, size_t len,
              bool nostop) {
  int result = i2c_read_timeout_per_char_us(i2c, addr, dst, len, nostop,
                                            I2C_PER_BYTE_TIMEOUT_US);
  if (result == PICO_ERROR_TIMEOUT) {
    printf("Timed out while reading data over I2C.\n");
    return false;
  }
  if (result == PICO_ERROR_GENERIC) {
    printf("Error reading data over I2C.\n");
    return false;
  }
  return true;
}

bool i2c_write(i2c_inst_t *i2c, uint8_t addr, const uint8_t *src, size_t len,
               bool nostop) {
  int result = i2c_write_timeout_per_char_us(i2c, addr, src, len, nostop,
                                             I2C_PER_BYTE_TIMEOUT_US);
  if (result == PICO_ERROR_TIMEOUT) {
    printf("Timed out while writing data over I2C.\n");
    return false;
  }
  if (result == PICO_ERROR_GENERIC) {
    printf("Error writing data over I2C.\n");
    return false;
  }
  return true;
}

int main() {
  uint8_t raw_angle_register = AS5600_RAW_ANGLE_REGISTER;
  uint8_t agc_register = AS5600_AGC_REGISTER;

  stdio_init_all();

  i2c_init(i2c_default, I2C_BAUD_RATE);
  gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
  gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

  uint slice_num = pwm_gpio_to_slice_num(LED_GPIO);
  uint channel = pwm_gpio_to_channel(LED_GPIO);
  gpio_set_function(LED_GPIO, GPIO_FUNC_PWM);
  pwm_set_wrap(slice_num, 64);
  pwm_set_enabled(slice_num, true);

  printf("Booted.\n");

  while (true) {
    uint8_t gain = 0;
    if (!i2c_write(i2c_default, AS5600_ADDRESS, &agc_register, 1, true)) {
      continue;
    }
    if (!i2c_read(i2c_default, AS5600_ADDRESS, &gain, 1, false)) {
      continue;
    }

    uint8_t angle_bytes[2] = {0, 0};
    if (!i2c_write(i2c_default, AS5600_ADDRESS, &raw_angle_register, 1, true)) {
      continue;
    }
    if (!i2c_read(i2c_default, AS5600_ADDRESS, angle_bytes, 2, false)) {
      continue;
    }
    uint16_t angle = (angle_bytes[0] << 8 | angle_bytes[1]) & ((1 << 12) - 1);

    int progress = angle / 64;
    int remaining = 64 - progress;
    pwm_set_chan_level(slice_num, channel, progress);

    printf("[");
    for (int i = 0; i < progress; ++i) {
      printf("#");
    }
    for (int i = 0; i < remaining; ++i) {
      printf("-");
    }
    printf("] %10f°  Gain: %d\n", 360.0f * angle / 4096.0f, gain);
  }
}