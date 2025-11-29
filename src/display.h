#ifndef DISPLAY_H
#define DISPLAY_H

// Initialize ncurses display
void display_init(void);

// Cleanup ncurses display
void display_cleanup(void);

// Display GPS data on screen
void display_gps_data(void);

#endif // DISPLAY_H