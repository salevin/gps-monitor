#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <getopt.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <libubox/blobmsg.h>

static int running = 1;
static struct ubus_context *ctx = NULL;
static FILE *csv_file = NULL;
static const char *output_file = "/tmp/gps-log.csv";

static struct blob_buf gps_response_buf = {};
static int gps_callback_called = 0;
static int gps_response_status = 0;

// Helper function to get GPS value as string
static const char *get_gps_value(const char *key) {
    struct blob_attr *attr;
    int rem;

    if (!gps_response_buf.head) return NULL;

    blobmsg_for_each_attr(attr, gps_response_buf.head, rem) {
        const char *name = blobmsg_name(attr);
        if (name && strcmp(name, key) == 0) {
            if (blobmsg_type(attr) == BLOBMSG_TYPE_STRING) {
                return blobmsg_get_string(attr);
            }
        }
    }
    return NULL;
}

static void gps_data_cb(struct ubus_request *req, int type, struct blob_attr *msg) {
    (void)req;
    gps_callback_called = 1;
    gps_response_status = type;

    if (msg) {
        blob_buf_free(&gps_response_buf);
        blob_buf_init(&gps_response_buf, 0);

        struct blob_attr *attr;
        int rem;
        blobmsg_for_each_attr(attr, msg, rem) {
            const char *name = blobmsg_name(attr);
            if (!name) continue;

            enum blobmsg_type attr_type = blobmsg_type(attr);
            if (attr_type == BLOBMSG_TYPE_STRING) {
                blobmsg_add_string(&gps_response_buf, name, blobmsg_get_string(attr));
            } else if (attr_type == BLOBMSG_TYPE_INT32) {
                blobmsg_add_u32(&gps_response_buf, name, blobmsg_get_u32(attr));
            } else if (attr_type == BLOBMSG_TYPE_INT64) {
                blobmsg_add_u64(&gps_response_buf, name, blobmsg_get_u64(attr));
            } else if (attr_type == BLOBMSG_TYPE_DOUBLE) {
                blobmsg_add_double(&gps_response_buf, name, blobmsg_get_double(attr));
            }
        }
    } else {
        blob_buf_free(&gps_response_buf);
        memset(&gps_response_buf, 0, sizeof(gps_response_buf));
    }
}

static int fetch_gps_data(void) {
    uint32_t id;
    int ret;

    if (!ctx) {
        fprintf(stderr, "UBus context not available\n");
        return -1;
    }

    ret = ubus_lookup_id(ctx, "gps", &id);
    if (ret != 0) {
        fprintf(stderr, "GPS service not found\n");
        return -1;
    }

    gps_callback_called = 0;
    gps_response_status = 0;
    blob_buf_free(&gps_response_buf);
    memset(&gps_response_buf, 0, sizeof(gps_response_buf));
    ret = ubus_invoke(ctx, id, "info", NULL, gps_data_cb, NULL, 1000);

    if (ret != 0) {
        fprintf(stderr, "Failed to call GPS info (error: %d)\n", ret);
        return -1;
    }

    // Process ubus events to ensure callback is executed
    fd_set fds;
    struct timeval tv;
    int sock = ctx->sock.fd;
    int timeout_ms = 1000;

    while (!gps_callback_called && timeout_ms > 0) {
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 10000;

        int ready = select(sock + 1, &fds, NULL, NULL, &tv);
        if (ready > 0 && FD_ISSET(sock, &fds)) {
            ubus_handle_event(ctx);
        } else if (ready < 0) {
            break;
        }
        timeout_ms -= 10;
    }

    if (gps_callback_called) {
        for (int i = 0; i < 10; i++) {
            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            tv.tv_sec = 0;
            tv.tv_usec = 10000;

            int ready = select(sock + 1, &fds, NULL, NULL, &tv);
            if (ready > 0 && FD_ISSET(sock, &fds)) {
                ubus_handle_event(ctx);
            } else {
                break;
            }
        }
    }

    return gps_callback_called ? 0 : -1;
}

static void log_gps_data(void) {
    if (!csv_file) return;

    if (fetch_gps_data() != 0) {
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
    const char *lat_str = get_gps_value("latitude");
    const char *lon_str = get_gps_value("longitude");
    const char *speed_str = get_gps_value("speed");
    const char *elevation_str = get_gps_value("elevation");
    const char *course_str = get_gps_value("course");
    const char *age_str = get_gps_value("age");

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

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

static void daemonize(void) {
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Fork failed\n");
        exit(1);
    }

    if (pid > 0) {
        // Parent process exits
        exit(0);
    }

    // Child process continues
    if (setsid() < 0) {
        fprintf(stderr, "setsid failed\n");
        exit(1);
    }

    // Change working directory to root
    if (chdir("/") < 0) {
        fprintf(stderr, "chdir failed\n");
        exit(1);
    }

    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

static void print_usage(const char *prog_name) {
    printf("GPS Logger - Log GPS coordinates to CSV file\n\n");
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Options:\n");
    printf("  -i, --interval <seconds>  Logging interval in seconds (default: 30)\n");
    printf("  -o, --output <file>       Output CSV file path (default: /tmp/gps-log.csv)\n");
    printf("  -d, --daemon              Run as daemon in background\n");
    printf("  -h, --help                Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s                        Log every 30s to /tmp/gps-log.csv\n", prog_name);
    printf("  %s -i 60 -o /tmp/gps.csv  Log every 60s to /tmp/gps.csv\n", prog_name);
    printf("  %s -d -i 10               Run as daemon, log every 10s\n\n", prog_name);
    printf("CSV Format:\n");
    printf("  timestamp,latitude,longitude,speed,elevation,course,age\n");
}

int main(int argc, char **argv) {
    int interval = 30;
    int daemon_mode = 0;
    int opt;

    static struct option long_options[] = {
        {"interval", required_argument, 0, 'i'},
        {"output",   required_argument, 0, 'o'},
        {"daemon",   no_argument,       0, 'd'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "i:o:dh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i':
                interval = atoi(optarg);
                if (interval <= 0) {
                    fprintf(stderr, "Invalid interval: %s\n", optarg);
                    return 1;
                }
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'd':
                daemon_mode = 1;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Connect to ubus
    ctx = ubus_connect(NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to connect to ubus\n");
        return 1;
    }

    // Open CSV file
    int file_exists = (access(output_file, F_OK) == 0);
    csv_file = fopen(output_file, "a");
    if (!csv_file) {
        fprintf(stderr, "Failed to open output file: %s\n", output_file);
        ubus_free(ctx);
        return 1;
    }

    // Write CSV header if file is new
    if (!file_exists) {
        fprintf(csv_file, "timestamp,latitude,longitude,speed,elevation,course,age\n");
        fflush(csv_file);
    }

    if (daemon_mode) {
        daemonize();
    } else {
        printf("GPS Logger started\n");
        printf("Logging to: %s\n", output_file);
        printf("Interval: %d seconds\n", interval);
        printf("Press Ctrl+C to stop\n\n");
    }

    // Main logging loop
    while (running) {
        log_gps_data();

        // Sleep for the specified interval, checking for signals periodically
        for (int i = 0; i < interval && running; i++) {
            sleep(1);
        }
    }

    // Cleanup
    if (csv_file) {
        fclose(csv_file);
    }

    if (ctx) {
        blob_buf_free(&gps_response_buf);
        ubus_free(ctx);
    }

    if (!daemon_mode) {
        printf("\nGPS Logger stopped\n");
    }

    return 0;
}