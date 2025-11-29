#ifndef GPS_H
#define GPS_H

// Initialize GPS data buffers
void gps_init(void);

// Cleanup GPS data buffers
void gps_cleanup(void);

// Fetch GPS data from ubus
int gps_fetch_data(void);

// Get GPS value by key
const char *gps_get_value(const char *key);

#endif // GPS_H