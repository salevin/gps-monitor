#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ncurses.h>
#include <libubus.h>
#include "display.h"
#include "gps.h"
#include "common.h"

// Helper function to draw a centered box top
static int draw_centered_box_top(int y, int box_width, int maxx, int color_pair) {
    int start_x = (maxx - box_width) / 2;
    if (start_x < 0) start_x = 0;

    attron(COLOR_PAIR(color_pair));
    mvaddch(y, start_x, ACS_ULCORNER);
    hline(ACS_HLINE, box_width - 2);
    mvaddch(y, start_x + box_width - 1, ACS_URCORNER);
    attroff(COLOR_PAIR(color_pair));
    return start_x;
}

// Helper function to draw box bottom
static void draw_centered_box_bottom(int y, int start_x, int box_width, int color_pair) {
    attron(COLOR_PAIR(color_pair));
    mvaddch(y, start_x, ACS_LLCORNER);
    hline(ACS_HLINE, box_width - 2);
    mvaddch(y, start_x + box_width - 1, ACS_LRCORNER);
    attroff(COLOR_PAIR(color_pair));
}

// Helper function to draw box title row
static void draw_centered_box_title(int y, int start_x, int box_width, const char *title, int color_pair) {
    attron(COLOR_PAIR(color_pair));
    mvaddch(y, start_x, ACS_VLINE);
    mvprintw(y, start_x + 2, "%s", title);
    mvaddch(y, start_x + box_width - 1, ACS_VLINE);
    attroff(COLOR_PAIR(color_pair));
}

// Helper function to draw box separator row
static void draw_centered_box_separator(int y, int start_x, int box_width, int color_pair) {
    attron(COLOR_PAIR(color_pair));
    mvaddch(y, start_x, ACS_LTEE);
    hline(ACS_HLINE, box_width - 2);
    mvaddch(y, start_x + box_width - 1, ACS_RTEE);
    attroff(COLOR_PAIR(color_pair));
}

// Helper function to draw box content row
static void draw_centered_box_content(int y, int start_x, int box_width, const char *content, int color_pair) {
    attron(COLOR_PAIR(color_pair));
    mvaddch(y, start_x, ACS_VLINE);
    mvprintw(y, start_x + 2, "%s", content);
    mvaddch(y, start_x + box_width - 1, ACS_VLINE);
    attroff(COLOR_PAIR(color_pair));
}

void display_init(void) {
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    curs_set(0);

    // Initialize colors if available
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLACK);  // Border
        init_pair(2, COLOR_RED, COLOR_BLACK);    // Error
        init_pair(3, COLOR_GREEN, COLOR_BLACK);  // Data
    }
}

void display_cleanup(void) {
    endwin();
}

void display_gps_data(void) {
    int ret;
    const char *lat_str, *lon_str, *speed_str, *elevation_str, *course_str, *age_str;
    double lat, lon, speed_ms, speed_knots, elevation, course;
    int maxy, maxx;

    // Get screen dimensions
    getmaxyx(stdscr, maxy, maxx);

    // Only erase content area (not status bar)
    const int content_start_y = 2;
    if (content_start_y < maxy - 1) {
        for (int i = content_start_y; i < maxy - 1; i++) {
            move(i, 0);
            clrtoeol();
        }
    }

    if (!ctx) {
        mvprintw(0, 0, "UBus context not available");
        wnoutrefresh(stdscr);
        doupdate();
        return;
    }

    // Fetch GPS data
    ret = gps_fetch_data();

    int y = 0;

    // Title
    attron(A_BOLD | A_UNDERLINE);
    mvprintw(y++, (maxx - 13) / 2, "New GPS Monitor");
    attroff(A_BOLD | A_UNDERLINE);
    y++;

    if (ret != 0 || !gps_callback_called) {
        const int box_width = 60;
        int start_x = draw_centered_box_top(y++, box_width, maxx, 1);
        draw_centered_box_content(y++, start_x, box_width,
                                 ret != 0 ? "Failed to fetch GPS data" : "Timeout waiting for GPS response", 2);
        draw_centered_box_bottom(y++, start_x, box_width, 1);
        wnoutrefresh(stdscr);
        doupdate();
        return;
    }

    // Check if we got data
    if (gps_response_buf.head) {
        // Extract GPS values
        lat_str = gps_get_value("latitude");
        lon_str = gps_get_value("longitude");
        speed_str = gps_get_value("speed");
        elevation_str = gps_get_value("elevation");
        course_str = gps_get_value("course");
        age_str = gps_get_value("age");

        // Parse and format latitude/longitude
        if (lat_str && lon_str) {
            lat = atof(lat_str);
            lon = atof(lon_str);

            // Location box
            const int box_width = 60;
            int start_x = draw_centered_box_top(y++, box_width, maxx, 1);
            draw_centered_box_title(y++, start_x, box_width, "Location", 1);
            draw_centered_box_separator(y++, start_x, box_width, 1);

            char lat_line[64];
            snprintf(lat_line, sizeof(lat_line), "Latitude:  %9.6f%c %c",
                   (lat < 0 ? -lat : lat), ACS_DEGREE, (lat >= 0 ? 'N' : 'S'));
            draw_centered_box_content(y++, start_x, box_width, lat_line, 3);

            char lon_line[64];
            snprintf(lon_line, sizeof(lon_line), "Longitude: %9.6f%c %c",
                   (lon < 0 ? -lon : lon), ACS_DEGREE, (lon >= 0 ? 'E' : 'W'));
            draw_centered_box_content(y++, start_x, box_width, lon_line, 3);

            draw_centered_box_bottom(y++, start_x, box_width, 1);
            y++;
        }

        // Parse and display speed in knots
        if (speed_str) {
            speed_ms = atof(speed_str);
            speed_knots = speed_ms * 1.94384; // Convert m/s to knots

            // Navigation box
            const int box_width = 60;
            int start_x = draw_centered_box_top(y++, box_width, maxx, 1);
            draw_centered_box_title(y++, start_x, box_width, "Navigation", 1);
            draw_centered_box_separator(y++, start_x, box_width, 1);

            char speed_line[64];
            snprintf(speed_line, sizeof(speed_line), "Speed:      %6.2f m/s  (%6.2f knots)",
                   speed_ms, speed_knots);
            draw_centered_box_content(y++, start_x, box_width, speed_line, 3);

            if (course_str) {
                course = atof(course_str);
                const char *direction = "N";
                if (course >= 337.5 || course < 22.5) direction = "N";
                else if (course >= 22.5 && course < 67.5) direction = "NE";
                else if (course >= 67.5 && course < 112.5) direction = "E";
                else if (course >= 112.5 && course < 157.5) direction = "SE";
                else if (course >= 157.5 && course < 202.5) direction = "S";
                else if (course >= 202.5 && course < 247.5) direction = "SW";
                else if (course >= 247.5 && course < 292.5) direction = "W";
                else if (course >= 292.5 && course < 337.5) direction = "NW";

                char course_line[64];
                snprintf(course_line, sizeof(course_line), "Course:     %6.1f%c (%s)",
                       course, ACS_DEGREE, direction);
                draw_centered_box_content(y++, start_x, box_width, course_line, 3);
            }

            if (elevation_str) {
                elevation = atof(elevation_str);
                char elev_line[64];
                snprintf(elev_line, sizeof(elev_line), "Elevation:  %6.1f m", elevation);
                draw_centered_box_content(y++, start_x, box_width, elev_line, 3);
            }

            draw_centered_box_bottom(y++, start_x, box_width, 1);
            y++;
        }

        // Display age if available
        if (age_str) {
            const int box_width = 60;
            int start_x = draw_centered_box_top(y++, box_width, maxx, 1);

            char age_line[64];
            snprintf(age_line, sizeof(age_line), "Data Age: %s seconds", age_str);
            draw_centered_box_content(y++, start_x, box_width, age_line, 3);

            draw_centered_box_bottom(y++, start_x, box_width, 1);
            y++;
        }

    } else {
        const int box_width = 60;
        int start_x = draw_centered_box_top(y++, box_width, maxx, 1);

        char error_line[64];
        if (gps_response_status != UBUS_STATUS_OK) {
            snprintf(error_line, sizeof(error_line), "Error: GPS service returned error: %d", gps_response_status);
        } else {
            snprintf(error_line, sizeof(error_line), "No GPS data available");
        }
        draw_centered_box_content(y++, start_x, box_width, error_line, 2);

        draw_centered_box_bottom(y++, start_x, box_width, 1);
        y++;
    }

    // Print timestamp
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    snprintf(time_str, sizeof(time_str), "%04d-%02d-%02d %02d:%02d:%02d",
           t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
           t->tm_hour, t->tm_min, t->tm_sec);

    const int box_width = 60;
    int start_x = draw_centered_box_top(y++, box_width, maxx, 1);

    attron(A_BOLD | COLOR_PAIR(1));
    mvaddch(y, start_x, ACS_VLINE);
    mvprintw(y, start_x + 2, "%s", time_str);
    mvaddch(y, start_x + box_width - 1, ACS_VLINE);
    attroff(A_BOLD | COLOR_PAIR(1));
    y++;

    draw_centered_box_bottom(y++, start_x, box_width, 1);

    // Clear any remaining lines below our content (but not status bar)
    if (y < maxy - 2) {
        for (int i = y; i < maxy - 1; i++) {
            move(i, 0);
            clrtoeol();
        }
    }

    // Status bar at bottom with exit instructions
    static char last_status_msg[64] = "";
    char status_msg[] = "Press 'q' or ESC to quit";
    y = maxy - 1;

    if (strcmp(last_status_msg, status_msg) != 0) {
        attron(A_REVERSE | A_BOLD);
        mvprintw(y, 0, "%s", status_msg);
        int msg_len = strlen(status_msg);
        for (int i = msg_len; i < maxx; i++) {
            mvaddch(y, i, ' ');
        }
        attroff(A_REVERSE | A_BOLD);
        strncpy(last_status_msg, status_msg, sizeof(last_status_msg) - 1);
        last_status_msg[sizeof(last_status_msg) - 1] = '\0';
    }

    wnoutrefresh(stdscr);
    doupdate();
}