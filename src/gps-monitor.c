#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <ncurses.h>
#include <libubus.h>
#include <libubox/blobmsg.h>
#include "common.h"
#include "gps.h"
#include "display.h"
#include "logger.h"
#include "spi_display.h"

// Global state variables
int running = 1;
struct ubus_context *ctx = NULL;
int log_mode = 0;
int spi_mode = 0;

// GPS data buffers
struct blob_buf gps_response_buf = {};
int gps_callback_called = 0;
int gps_response_status = 0;

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
    printf("GPS Monitor - Display or log GPS coordinates\n\n");
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Options:\n");
    printf("  -l, --log                 Enable logging mode (log to CSV file)\n");
    printf("  -s, --spi-display         Display on SPI OLED (SSD1322 256x64)\n");
    printf("  -i, --interval <seconds>  Logging interval in seconds (default: 30)\n");
    printf("  -o, --output <file>       Output CSV file path (default: /tmp/gps-log.csv)\n");
    printf("  -d, --daemon              Run as daemon in background (requires -l)\n");
    printf("  -h, --help                Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s                        Display GPS data interactively\n", prog_name);
    printf("  %s -s                     Display on SPI OLED\n", prog_name);
    printf("  %s -l                     Log every 30s to /tmp/gps-log.csv\n", prog_name);
    printf("  %s -l -i 60 -o /tmp/gps.csv  Log every 60s to custom file\n", prog_name);
    printf("  %s -l -d -i 10            Run as daemon, log every 10s\n\n", prog_name);
    printf("CSV Format:\n");
    printf("  timestamp,latitude,longitude,speed,elevation,course,age\n");
}

void signal_handler(int sig) {
    (void)sig;
    running = 0;

    logger_cleanup();

    if (ctx) {
        gps_cleanup();
        ubus_free(ctx);
        ctx = NULL;
    }

    if (!log_mode && !spi_mode) {
        display_cleanup();
    } else if (spi_mode) {
        spi_display_cleanup();
    }
}

static void run_display_mode(void) {
    display_init();

    // Main display loop
    while (running) {
        // Check for keyboard input
        int ch = getch();
        if (ch == 'q' || ch == 'Q' || ch == 27) { // 'q', 'Q', or ESC
            running = 0;
            break;
        }

        display_gps_data();

        // Wait 100ms before next update
        usleep(100000);
    }

    display_cleanup();
}

static void run_logging_mode(int interval, const char *output_file, int daemon_mode) {
    if (logger_init(output_file) != 0) {
        return;
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
        logger_log_gps_data();

        // Sleep for the specified interval, checking for signals periodically
        for (int i = 0; i < interval && running; i++) {
            sleep(1);
        }
    }

    logger_cleanup();

    if (!daemon_mode) {
        printf("\nGPS Logger stopped\n");
    }
}

static void run_spi_display_mode(void) {
    if (spi_display_init() != 0) {
        fprintf(stderr, "Failed to initialize SPI display\n");
        return;
    }

    printf("SPI Display mode started\n");
    printf("Press Ctrl+C to stop\n\n");

    // Main SPI display loop
    while (running) {
        spi_display_update();

        // Update every 1 second
        sleep(1);
    }

    spi_display_cleanup();
    printf("\nSPI Display stopped\n");
}

int main(int argc, char **argv) {
    int interval = 30;
    int daemon_mode = 0;
    const char *output_file = "/tmp/gps-log.csv";
    int opt;

    static struct option long_options[] = {
        {"log",         no_argument,       0, 'l'},
        {"spi-display", no_argument,       0, 's'},
        {"interval",    required_argument, 0, 'i'},
        {"output",      required_argument, 0, 'o'},
        {"daemon",      no_argument,       0, 'd'},
        {"help",        no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    // Parse command-line arguments
    while ((opt = getopt_long(argc, argv, "lsi:o:dh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'l':
                log_mode = 1;
                break;
            case 's':
                spi_mode = 1;
                break;
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

    // Validate daemon mode requires log mode
    if (daemon_mode && !log_mode) {
        fprintf(stderr, "Error: -d/--daemon requires -l/--log\n");
        return 1;
    }

    // Validate mutually exclusive modes
    int mode_count = log_mode + spi_mode;
    if (mode_count > 1) {
        fprintf(stderr, "Error: Cannot use -l/--log and -s/--spi-display together\n");
        fprintf(stderr, "       Choose one display/logging mode\n");
        return 1;
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

    // Initialize GPS
    gps_init();

    // Run appropriate mode
    if (log_mode) {
        run_logging_mode(interval, output_file, daemon_mode);
    } else if (spi_mode) {
        run_spi_display_mode();
    } else {
        run_display_mode();
    }

    // Cleanup
    if (ctx) {
        gps_cleanup();
        ubus_free(ctx);
    }

    return 0;
}