#ifndef COMMON_H
#define COMMON_H

#include <libubus.h>
#include <libubox/blobmsg.h>

// Global state
extern int running;
extern struct ubus_context *ctx;
extern int log_mode;

// GPS data buffer
extern struct blob_buf gps_response_buf;
extern int gps_callback_called;
extern int gps_response_status;

#endif // COMMON_H