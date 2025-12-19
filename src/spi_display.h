#ifndef SPI_DISPLAY_H
#define SPI_DISPLAY_H

// Initialize SPI display and U8g2 library
int spi_display_init(void);

// Cleanup SPI display and free resources
void spi_display_cleanup(void);

// Update SPI display with current GPS data
void spi_display_update(void);

#endif // SPI_DISPLAY_H
