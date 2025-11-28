#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/select.h>
#include <ncurses.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <libubox/blobmsg.h>

static int running = 1;
static struct ubus_context *ctx = NULL;

void signal_handler(int sig);

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

// Helper function to draw a centered box
// Returns the x position where the box starts
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

static void gps_data_cb(struct ubus_request *req, int type, struct blob_attr *msg) {
    (void)req;
    gps_callback_called = 1;
    gps_response_status = type;
    
    // Process message if it exists, regardless of type (some responses may still contain data)
    if (msg) {
        // Copy the response into our buffer so it persists
        blob_buf_free(&gps_response_buf);
        blob_buf_init(&gps_response_buf, 0);
        
        // Copy all attributes from the message
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

static void display_gps_data(void) {
    uint32_t id;
    int ret;
    const char *lat_str, *lon_str, *speed_str, *elevation_str, *course_str, *age_str;
    double lat, lon, speed_ms, speed_knots, elevation, course;
    int maxy, maxx;
    
    // Get screen dimensions
    getmaxyx(stdscr, maxy, maxx);
    
    // Only erase content area (not status bar), starting from after title (line 2)
    // This avoids erasing the status bar and minimizes redraw
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
    
    // Look up the GPS service
    ret = ubus_lookup_id(ctx, "gps", &id);
    if (ret != 0) {
        mvprintw(0, 0, "GPS service not found");
        wnoutrefresh(stdscr);
        doupdate();
        return;
    }
    
    // Call the info method (pass NULL for empty request, not empty blob_buf)
    gps_callback_called = 0;
    gps_response_status = 0;
    blob_buf_free(&gps_response_buf);
    memset(&gps_response_buf, 0, sizeof(gps_response_buf));
    ret = ubus_invoke(ctx, id, "info", NULL, gps_data_cb, NULL, 1000);
    
    if (ret != 0) {
        mvprintw(0, 0, "Failed to call GPS info (error: %d)", ret);
        wnoutrefresh(stdscr);
        doupdate();
        return;
    }
    
    // Process ubus events to ensure callback is executed
    fd_set fds;
    struct timeval tv;
    int sock = ctx->sock.fd;
    int timeout_ms = 1000; // 1 second timeout
    
    // Wait for callback to be called, with timeout
    // Continue processing events until callback is called
    while (!gps_callback_called && timeout_ms > 0) {
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 10000; // 10ms increments
        
        int ready = select(sock + 1, &fds, NULL, NULL, &tv);
        if (ready > 0 && FD_ISSET(sock, &fds)) {
            ubus_handle_event(ctx);
        } else if (ready < 0) {
            break;
        }
        timeout_ms -= 10;
    }
    
    // Process any remaining events after callback to ensure full response
    if (gps_callback_called) {
        // Give a bit more time for any remaining data
        for (int i = 0; i < 10; i++) {
            FD_ZERO(&fds);
            FD_SET(sock, &fds);
            tv.tv_sec = 0;
            tv.tv_usec = 10000; // 10ms
            
            int ready = select(sock + 1, &fds, NULL, NULL, &tv);
            if (ready > 0 && FD_ISSET(sock, &fds)) {
                ubus_handle_event(ctx);
            } else {
                break;
            }
        }
    }
    
    int y = 0;
    
    // Title
    attron(A_BOLD | A_UNDERLINE);
    mvprintw(y++, (maxx - 13) / 2, "GPS Monitor");
    attroff(A_BOLD | A_UNDERLINE);
    y++;
    
    if (!gps_callback_called) {
        const int box_width = 60;
        int start_x = draw_centered_box_top(y++, box_width, maxx, 1);
        draw_centered_box_content(y++, start_x, box_width, "Timeout waiting for GPS response", 2);
        draw_centered_box_bottom(y++, start_x, box_width, 1);
        wnoutrefresh(stdscr);
        doupdate();
        return;
    }
    
    // Check if we got data, even if status wasn't OK
    if (gps_response_buf.head) {
        // Extract GPS values
        lat_str = get_gps_value("latitude");
        lon_str = get_gps_value("longitude");
        speed_str = get_gps_value("speed");
        elevation_str = get_gps_value("elevation");
        course_str = get_gps_value("course");
        age_str = get_gps_value("age");
        
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
    
    // Status bar at bottom with exit instructions (only update if changed)
    static char last_status_msg[64] = "";
    char status_msg[] = "Press 'q' or ESC to quit";
    y = maxy - 1;
    
    // Only redraw status bar if message changed or first time
    if (strcmp(last_status_msg, status_msg) != 0) {
        attron(A_REVERSE | A_BOLD);
        mvprintw(y, 0, "%s", status_msg);
        // Clear rest of line
        int msg_len = strlen(status_msg);
        for (int i = msg_len; i < maxx; i++) {
            mvaddch(y, i, ' ');
        }
        attroff(A_REVERSE | A_BOLD);
        strncpy(last_status_msg, status_msg, sizeof(last_status_msg) - 1);
        last_status_msg[sizeof(last_status_msg) - 1] = '\0';
    }
    
    // Use optimized refresh - prepare all updates, then do single screen update
    wnoutrefresh(stdscr);
    doupdate();
}

void signal_handler(int sig) {
    (void)sig;
    running = 0;
    if (ctx) {
        ubus_free(ctx);
        ctx = NULL;
    }
    endwin();
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE); // Non-blocking input
    keypad(stdscr, TRUE);
    curs_set(0); // Hide cursor
    
    // Initialize colors if available
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLACK);  // Border
        init_pair(2, COLOR_RED, COLOR_BLACK);    // Error
        init_pair(3, COLOR_GREEN, COLOR_BLACK);  // Data
    }
    
    // Set up signal handler for clean exit
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Connect to ubus
    ctx = ubus_connect(NULL);
    if (!ctx) {
        endwin();
        fprintf(stderr, "Failed to connect to ubus\n");
        return 1;
    }
    
    // Main loop
    while (running) {
        // Check for keyboard input
        int ch = getch();
        if (ch == 'q' || ch == 'Q' || ch == 27) { // 'q', 'Q', or ESC
            running = 0;
            break;
        }
        
        display_gps_data();
        
        // Wait 100ms before next update (faster response to input)
        usleep(100000);
    }
    
    // Cleanup
    if (ctx) {
        ubus_free(ctx);
    }
    
    endwin();
    
    return 0;
}
