#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include <stdio.h>

// According to the datasheet, the AS5048A supports a maximum clock frequency
// of 100 MHz. However, the Raspberry Pi Pico can't go higher than 10 MHz.
#define SPI_BAUD_RATE 10 * 1000 * 1000

// From the AS5048A datasheet
#define AS5048A_ANGLE_COMMAND 0xffff
#define AS5048A_DIAGNOSTIC_AGC_COMMAND 0x7ffd
#define AS5048A_CLEAR_ERROR_COMMAND 0x4001

#define LED_GPIO 22

static inline void spi_select() {
  gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 0);

  // Assuming 133 MHz, wait 47 cycles to wait out the minimum 350 ns duration
  // between CSn falling edge and CLK rising edge (from the AS5048A datasheet).
  asm volatile(
      "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
      "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
      "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
      "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
      "nop \n nop \n nop \n nop \n nop \n nop \n nop");
}

static inline void spi_deselect() {
  // Assuming 133 MHz, wait 7 cycles to wait out the minimum 50 ns duration
  // between CLK falling edge and CSn rising edge (from the AS5048A datasheet).
  asm volatile("nop \n nop \n nop \n nop \n nop \n nop \n nop");

  gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);

  // Assuming 133 MHz, wait 47 cycles to wait out the minimum 350 ns high time
  // between two transmissions (from the AS5048A datasheet).
  asm volatile(
      "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
      "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
      "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
      "nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n "
      "nop \n nop \n nop \n nop \n nop \n nop \n nop");
}

static void spi_clear_error(spi_inst_t *spi);

// Write two bytes to the SPI bus, then read two bytes. Returns whether the
// operation succeeded.
static bool spi_request(spi_inst_t *spi, uint16_t command, uint16_t *result,
                        bool attempt_clear_error) {
  spi_select();
  int halfwords_written = spi_write16_blocking(spi_default, &command, 1);
  spi_deselect();

  if (halfwords_written != 1) {
    printf("Attempted to write 1 halfword, but %d were written.\n",
           halfwords_written);
    if (attempt_clear_error) {
      spi_clear_error(spi);
    }
    return false;
  }

  uint16_t response = 0;
  spi_select();
  int halfwords_read = spi_read16_blocking(spi_default, 0, &response, 1);
  spi_deselect();

  if (halfwords_read != 1) {
    printf("Attempted to read 1 halfword, but %d were read.\n", halfwords_read);
    if (attempt_clear_error) {
      spi_clear_error(spi);
    }
    return false;
  }

  if ((response >> 14) & 0x01) {
    printf("Transmission error.\n");
    if (attempt_clear_error) {
      spi_clear_error(spi);
    }
    return false;
  }

  *result = response & 0x3FFF;

  return true;
}

// Attempt to clear the error flag on the SPI device.
static void spi_clear_error(spi_inst_t *spi) {
  uint16_t clear_error_result = 0;
  if (spi_request(spi, AS5048A_CLEAR_ERROR_COMMAND, &clear_error_result,
                  false)) {
    printf("Framing error: %d\n", clear_error_result & 0x01);
    printf("Command invalid: %d\n", (clear_error_result >> 1) & 0x01);
    printf("Parity error: %d\n", (clear_error_result >> 2) & 0x01);
  }
}

// Let the fun begin!
int main() {
  // Initialize UART.
  stdio_init_all();

  // Intiialize SPI for the AS5048A.
  spi_init(spi_default, SPI_BAUD_RATE);
  spi_set_format(spi_default, 16, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);
  gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
  gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
  gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);
  gpio_init(PICO_DEFAULT_SPI_CSN_PIN);
  gpio_put(PICO_DEFAULT_SPI_CSN_PIN, 1);
  gpio_set_dir(PICO_DEFAULT_SPI_CSN_PIN, GPIO_OUT);

  // According to its datasheet, the AS5048A has a maximum startup time of
  // 10ms. Sleep for that long to give it time to boot.
  sleep_ms(10);

  // Initialize PWM for the LED.
  uint slice_num = pwm_gpio_to_slice_num(LED_GPIO);
  uint channel = pwm_gpio_to_channel(LED_GPIO);
  gpio_set_function(LED_GPIO, GPIO_FUNC_PWM);
  pwm_set_wrap(slice_num, 64);
  pwm_set_enabled(slice_num, true);

  // Print a message indicating that we're ready to start.
  printf("Booted.\n");

  // We'll use the AS5048A to implement a volume knob.
  uint16_t volume = 0; // [0, 16384)

  // The volume changes based on the change in angle, so we need to remember
  // the angle from the previous iteration.
  uint16_t previous_angle = 0; // [0, 16384)

  // The main program loop
  while (true) {
    // Read the diagnostics and gain data.
    uint16_t diagnostic_agc;
    if (!spi_request(spi_default, AS5048A_DIAGNOSTIC_AGC_COMMAND,
                     &diagnostic_agc, true)) {
      continue;
    }
    uint8_t gain = (uint8_t)diagnostic_agc; // [0, 256)
    if (!((diagnostic_agc >> 8) & 0x01)) {
      printf("Offset compensation not yet finished.\n");
      continue;
    }
    if ((diagnostic_agc >> 9) & 0x01) {
      printf("CORDIC overflow.\n");
      continue;
    }
    if ((diagnostic_agc >> 10) & 0x01) {
      printf("High magnetic field (gain: %d / 255).\n", gain);
      continue;
    }
    if ((diagnostic_agc >> 11) & 0x01) {
      printf("Weak magnetic field (gain %d / 255).\n", gain);
      continue;
    }

    // Read the angle.
    uint16_t angle; // [0, 16384)
    if (!spi_request(spi_default, AS5048A_ANGLE_COMMAND, &angle, true)) {
      continue;
    }

    // Adjust the volume based on the change in the angle.
    volume =
        MAX(0, MIN(16383,
                   volume + (24576 + (previous_angle - angle)) % 16384 - 8192));

    // Set the duty cycle of the LED based on the volume.
    int progress = volume / 256; // [0, 64)
    int remaining = 63 - progress;
    pwm_set_chan_level(slice_num, channel, progress);

    // Print the volume and gain.
    printf("[");
    for (int i = 0; i < progress; ++i) {
      printf("#");
    }
    for (int i = 0; i < remaining; ++i) {
      printf("-");
    }
    printf("] %10f%%  Magentic field strength: %d\n",
           100.0f * volume / 16383.0f, gain);

    // Remember the angle for the next iteration.
    previous_angle = angle;
  }
}