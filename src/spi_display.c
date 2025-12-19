#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <u8g2.h>
#include "spi_display.h"
#include "gps.h"
#include "common.h"

static u8g2_t u8g2;

int spi_display_init(void) {
    // Setup U8g2 with SSD1322 256x64 constructor
    u8g2_Setup_ssd1322_nhd_256x64_f(&u8g2, U8G2_R0,
        u8x8_byte_arm_linux_hw_spi,
        u8x8_gpio_and_delay_arm_linux);

    // Set SPI device path
    u8x8_SetDevice(u8g2_GetU8x8(&u8g2), "/dev/spidev0.1");

    // Configure GPIO pins
    // DC (Data/Command): GPIO11
    // RST (Reset): GPIO2
    // CS is handled by SPI hardware (/dev/spidev0.1)
    u8g2_SetPin(&u8g2, U8X8_PIN_DC, 11);
    u8g2_SetPin(&u8g2, U8X8_PIN_RESET, 2);

    // Initialize display hardware
    if (u8g2_InitDisplay(&u8g2) < 0) {
        fprintf(stderr, "Failed to initialize SSD1322 display\n");
        return -1;
    }

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
}

void spi_display_update(void) {
    int ret;
    const char *lat_str, *lon_str, *speed_str, *elevation_str;
    double lat, lon, speed_ms, speed_knots, elevation;
    char buffer[64];
    time_t now;
    struct tm *t;

    // Fetch GPS data
    ret = gps_fetch_data();

    // Clear buffer for new frame
    u8g2_ClearBuffer(&u8g2);

    // Check if GPS data is available
    if (ret != 0 || !gps_callback_called || !gps_response_buf.head) {
        // Display error message
        u8g2_SetFont(&u8g2, u8g2_font_7x13_tf);
        int text_width = u8g2_GetStrWidth(&u8g2, "GPS Data Unavailable");
        int x = (256 - text_width) / 2;
        u8g2_DrawStr(&u8g2, x, 30, "GPS Data Unavailable");

        u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
        text_width = u8g2_GetStrWidth(&u8g2, "Check GPS connection");
        x = (256 - text_width) / 2;
        u8g2_DrawStr(&u8g2, x, 45, "Check GPS connection");

        u8g2_SendBuffer(&u8g2);
        return;
    }

    // Get current timestamp
    now = time(NULL);
    t = localtime(&now);

    // === Title bar and time (Y=8) ===
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    u8g2_DrawStr(&u8g2, 0, 8, "GPS Monitor");

    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",
             t->tm_hour, t->tm_min, t->tm_sec);
    u8g2_DrawStr(&u8g2, 200, 8, buffer);

    // Horizontal separator line (Y=10)
    u8g2_DrawHLine(&u8g2, 0, 10, 256);

    // === Latitude (Y=22) ===
    lat_str = gps_get_value("latitude");
    if (lat_str) {
        lat = atof(lat_str);
        u8g2_SetFont(&u8g2, u8g2_font_7x13_tf);
        snprintf(buffer, sizeof(buffer), "Lat: %.6f%c%c",
                 (lat < 0 ? -lat : lat),
                 0xB0,  // Degree symbol
                 (lat >= 0 ? 'N' : 'S'));
        u8g2_DrawStr(&u8g2, 0, 22, buffer);
    }

    // === Longitude (Y=34) ===
    lon_str = gps_get_value("longitude");
    if (lon_str) {
        lon = atof(lon_str);
        u8g2_SetFont(&u8g2, u8g2_font_7x13_tf);
        snprintf(buffer, sizeof(buffer), "Lon: %.6f%c%c",
                 (lon < 0 ? -lon : lon),
                 0xB0,  // Degree symbol
                 (lon >= 0 ? 'E' : 'W'));
        u8g2_DrawStr(&u8g2, 0, 34, buffer);
    }

    // Horizontal separator line (Y=36)
    u8g2_DrawHLine(&u8g2, 0, 36, 256);

    // === Speed (Y=48) ===
    speed_str = gps_get_value("speed");
    if (speed_str) {
        speed_ms = atof(speed_str);
        speed_knots = speed_ms * 1.94384;  // Convert m/s to knots
        u8g2_SetFont(&u8g2, u8g2_font_7x13_tf);
        snprintf(buffer, sizeof(buffer), "Spd: %.1f m/s (%.1f kn)",
                 speed_ms, speed_knots);
        u8g2_DrawStr(&u8g2, 0, 48, buffer);
    }

    // === Elevation (Y=60) ===
    elevation_str = gps_get_value("elevation");
    if (elevation_str) {
        elevation = atof(elevation_str);
        u8g2_SetFont(&u8g2, u8g2_font_7x13_tf);
        snprintf(buffer, sizeof(buffer), "Elev: %.1f m", elevation);
        u8g2_DrawStr(&u8g2, 0, 60, buffer);
    }

    // === Date at bottom (Y=63) ===
    u8g2_SetFont(&u8g2, u8g2_font_5x7_tf);
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    u8g2_DrawStr(&u8g2, 190, 63, buffer);

    // Send buffer to display
    u8g2_SendBuffer(&u8g2);
}
