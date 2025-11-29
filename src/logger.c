#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "logger.h"
#include "gps.h"
#include "common.h"

static FILE *csv_file = NULL;

int logger_init(const char *output_file) {
    int file_exists = (access(output_file, F_OK) == 0);

    csv_file = fopen(output_file, "a");
    if (!csv_file) {
        fprintf(stderr, "Failed to open output file: %s\n", output_file);
        return -1;
    }

    // Write CSV header if file is new
    if (!file_exists) {
        fprintf(csv_file, "timestamp,latitude,longitude,speed,elevation,course,age\n");
        fflush(csv_file);
    }

    return 0;
}

void logger_cleanup(void) {
    if (csv_file) {
        fclose(csv_file);
        csv_file = NULL;
    }
}

void logger_log_gps_data(void) {
    if (!csv_file) return;

    if (gps_fetch_data() != 0) {
        return;
    }

    if (!gps_response_buf.head) {
        return;
    }

    // Get current timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
           t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
           t->tm_hour, t->tm_min, t->tm_sec);

    // Extract GPS values
    const char *lat_str = gps_get_value("latitude");
    const char *lon_str = gps_get_value("longitude");
    const char *speed_str = gps_get_value("speed");
    const char *elevation_str = gps_get_value("elevation");
    const char *course_str = gps_get_value("course");
    const char *age_str = gps_get_value("age");

    // Write to CSV: timestamp,latitude,longitude,speed,elevation,course,age
    fprintf(csv_file, "%s,%s,%s,%s,%s,%s,%s\n",
            timestamp,
            lat_str ? lat_str : "",
            lon_str ? lon_str : "",
            speed_str ? speed_str : "",
            elevation_str ? elevation_str : "",
            course_str ? course_str : "",
            age_str ? age_str : "");

    fflush(csv_file);
}

FILE *logger_get_file(void) {
    return csv_file;
}