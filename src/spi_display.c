#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <u8g2.h>
#include "spi_display.h"
#include "gps.h"
#include "common.h"

// GPIO pin configuration
#define GPIO_DC  3    // Data/Command pin
#define GPIO_RST 2     // Reset pin

// SPI device
#define SPI_DEVICE "/dev/spidev0.1"
#define SPI_SPEED  4000000  // 4 MHz

static u8g2_t u8g2;
static int spi_fd = -1;

// GPIO helper functions
static int gpio_export(int pin) {
    char buffer[64];
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) return -1;

    snprintf(buffer, sizeof(buffer), "%d", pin);
    write(fd, buffer, strlen(buffer));
    close(fd);

    // Wait for sysfs to create the files
    usleep(100000);
    return 0;
}

static int gpio_set_direction(int pin, const char *direction) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);

    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;

    write(fd, direction, strlen(direction));
    close(fd);
    return 0;
}

static int gpio_write(int pin, int value) {
    char path[64];
    char val = value ? '1' : '0';

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);

    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;

    write(fd, &val, 1);
    close(fd);
    return 0;
}

static int gpio_init(int pin) {
    // Try to export (may fail if already exported)
    gpio_export(pin);

    // Set as output
    return gpio_set_direction(pin, "out");
}

// Custom GPIO and delay callback for U8g2
uint8_t u8x8_gpio_and_delay_omega2(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    (void)u8x8;
    (void)arg_ptr;

    switch(msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            // Initialize GPIO pins
            gpio_init(GPIO_DC);
            gpio_init(GPIO_RST);
            break;

        case U8X8_MSG_GPIO_DC:
            // Set DC (Data/Command) pin
            gpio_write(GPIO_DC, arg_int);
            break;

        case U8X8_MSG_GPIO_RESET:
            // Set RST (Reset) pin
            gpio_write(GPIO_RST, arg_int);
            break;

        case U8X8_MSG_DELAY_NANO:
            // Nanosecond delay
            usleep(1);
            break;

        case U8X8_MSG_DELAY_MILLI:
            // Millisecond delay
            usleep(arg_int * 1000);
            break;

        case U8X8_MSG_DELAY_10MICRO:
            // 10 microsecond delay
            usleep(arg_int * 10);
            break;

        case U8X8_MSG_DELAY_100NANO:
            // 100 nanosecond delay
            usleep(1);
            break;

        default:
            return 0;
    }

    return 1;
}

// Custom SPI byte callback for U8g2
uint8_t u8x8_byte_omega2_hw_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    uint8_t *data;

    switch(msg) {
        case U8X8_MSG_BYTE_SEND:
            data = (uint8_t *)arg_ptr;
            if (spi_fd >= 0) {
                write(spi_fd, data, arg_int);
            }
            break;

        case U8X8_MSG_BYTE_INIT:
            // Initialize SPI device
            spi_fd = open(SPI_DEVICE, O_RDWR);
            if (spi_fd < 0) {
                fprintf(stderr, "Failed to open SPI device %s\n", SPI_DEVICE);
                return 0;
            }

            // Configure SPI mode
            uint8_t mode = SPI_MODE_0;
            ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);

            // Configure bits per word
            uint8_t bits = 8;
            ioctl(spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);

            // Configure max speed
            uint32_t speed = SPI_SPEED;
            ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
            break;

        case U8X8_MSG_BYTE_SET_DC:
            // DC is handled by GPIO callback
            u8x8_gpio_SetDC(u8x8, arg_int);
            break;

        case U8X8_MSG_BYTE_START_TRANSFER:
            // Chip select is handled by SPI driver
            break;

        case U8X8_MSG_BYTE_END_TRANSFER:
            // Chip select is handled by SPI driver
            break;

        default:
            return 0;
    }

    return 1;
}

int spi_display_init(void) {
    // Setup U8g2 with SSD1322 256x64 constructor using our custom callbacks
    // U8G2_R1 = 90 degree rotation for portrait mode (64x256)
    u8g2_Setup_ssd1322_nhd_256x64_f(&u8g2, U8G2_R1,
        u8x8_byte_omega2_hw_spi,
        u8x8_gpio_and_delay_omega2);

    // Initialize display hardware
    u8g2_InitDisplay(&u8g2);

    // Wake up display (turn off power save mode)
    u8g2_SetPowerSave(&u8g2, 0);

    // Clear display buffer and send to display
    u8g2_ClearBuffer(&u8g2);
    u8g2_SendBuffer(&u8g2);

    return 0;
}

void spi_display_cleanup(void) {
    // Clear display
    u8g2_ClearBuffer(&u8g2);
    u8g2_SendBuffer(&u8g2);

    // Put display in power save mode
    u8g2_SetPowerSave(&u8g2, 1);

    // Close SPI device
    if (spi_fd >= 0) {
        close(spi_fd);
        spi_fd = -1;
    }
}

void spi_display_update(void) {
    int ret;
    const char *lat_str, *lon_str, *speed_str, *elevation_str;
    double lat, lon, speed_ms, speed_knots, elevation;
    char buffer[64];
    time_t now;
    struct tm *t;
    int y_pos = 0;

    // Fetch GPS data
    ret = gps_fetch_data();

    // Clear buffer for new frame
    u8g2_ClearBuffer(&u8g2);

    // Check if GPS data is available
    if (ret != 0 || !gps_callback_called || !gps_response_buf.head) {
        // Display error message (rotated to portrait: 64x256)
        u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);

        y_pos = 120;
        u8g2_DrawStr(&u8g2, 4, y_pos, "GPS Data");
        y_pos += 12;
        u8g2_DrawStr(&u8g2, 0, y_pos, "Unavailable");
        y_pos += 18;
        u8g2_DrawStr(&u8g2, 6, y_pos, "Check GPS");
        y_pos += 12;
        u8g2_DrawStr(&u8g2, 0, y_pos, "connection");

        u8g2_SendBuffer(&u8g2);
        return;
    }

    // Get current timestamp
    now = time(NULL);
    t = localtime(&now);

    // Portrait layout (64 pixels wide x 256 pixels tall)

    // === Title (Y=10) ===
    y_pos = 10;
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&u8g2, 8, y_pos, "GPS Mon");

    // === Time (Y=25) ===
    y_pos = 25;
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",
             t->tm_hour, t->tm_min, t->tm_sec);
    u8g2_DrawStr(&u8g2, 6, y_pos, buffer);

    // Separator line
    y_pos = 30;
    u8g2_DrawHLine(&u8g2, 0, y_pos, 64);

    // === Latitude (Y=45, 57) ===
    y_pos = 45;
    lat_str = gps_get_value("latitude");
    if (lat_str) {
        lat = atof(lat_str);
        u8g2_SetFont(&u8g2, u8g2_font_5x7_tf);
        u8g2_DrawStr(&u8g2, 0, y_pos, "Latitude");

        y_pos += 12;
        u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
        snprintf(buffer, sizeof(buffer), "%.5f%c",
                 (lat < 0 ? -lat : lat),
                 0xB0);  // Degree symbol
        u8g2_DrawStr(&u8g2, 0, y_pos, buffer);

        y_pos += 12;
        snprintf(buffer, sizeof(buffer), "%c", (lat >= 0 ? 'N' : 'S'));
        u8g2_DrawStr(&u8g2, 26, y_pos, buffer);
    }

    // === Longitude (Y=80, 92) ===
    y_pos = 80;
    lon_str = gps_get_value("longitude");
    if (lon_str) {
        lon = atof(lon_str);
        u8g2_SetFont(&u8g2, u8g2_font_5x7_tf);
        u8g2_DrawStr(&u8g2, 0, y_pos, "Longitude");

        y_pos += 12;
        u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
        snprintf(buffer, sizeof(buffer), "%.5f%c",
                 (lon < 0 ? -lon : lon),
                 0xB0);  // Degree symbol
        u8g2_DrawStr(&u8g2, 0, y_pos, buffer);

        y_pos += 12;
        snprintf(buffer, sizeof(buffer), "%c", (lon >= 0 ? 'E' : 'W'));
        u8g2_DrawStr(&u8g2, 26, y_pos, buffer);
    }

    // Separator line
    y_pos = 110;
    u8g2_DrawHLine(&u8g2, 0, y_pos, 64);

    // === Speed (Y=125, 137) ===
    y_pos = 125;
    speed_str = gps_get_value("speed");
    if (speed_str) {
        speed_ms = atof(speed_str);
        speed_knots = speed_ms * 1.94384;

        u8g2_SetFont(&u8g2, u8g2_font_5x7_tf);
        u8g2_DrawStr(&u8g2, 0, y_pos, "Speed");

        y_pos += 12;
        u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
        snprintf(buffer, sizeof(buffer), "%.1f m/s", speed_ms);
        u8g2_DrawStr(&u8g2, 0, y_pos, buffer);

        y_pos += 12;
        snprintf(buffer, sizeof(buffer), "%.1f kn", speed_knots);
        u8g2_DrawStr(&u8g2, 2, y_pos, buffer);
    }

    // === Elevation (Y=160, 172) ===
    y_pos = 160;
    elevation_str = gps_get_value("elevation");
    if (elevation_str) {
        elevation = atof(elevation_str);

        u8g2_SetFont(&u8g2, u8g2_font_5x7_tf);
        u8g2_DrawStr(&u8g2, 0, y_pos, "Elevation");

        y_pos += 12;
        u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
        snprintf(buffer, sizeof(buffer), "%.1f m", elevation);
        u8g2_DrawStr(&u8g2, 2, y_pos, buffer);
    }

    // Separator line
    y_pos = 185;
    u8g2_DrawHLine(&u8g2, 0, y_pos, 64);

    // === Date (Y=200) ===
    y_pos = 200;
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tf);
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    u8g2_DrawStr(&u8g2, 0, y_pos, buffer);

    // Send buffer to display
    u8g2_SendBuffer(&u8g2);
}
