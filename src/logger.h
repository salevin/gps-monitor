#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

// Initialize logger with output file
int logger_init(const char *output_file);

// Cleanup logger and close file
void logger_cleanup(void);

// Log current GPS data to CSV
void logger_log_gps_data(void);

// Get the CSV file pointer (for cleanup in signal handler)
FILE *logger_get_file(void);

#endif // LOGGER_H