#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <libubox/blobmsg.h>
#include "gps.h"
#include "common.h"

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

void gps_init(void) {
    memset(&gps_response_buf, 0, sizeof(gps_response_buf));
    gps_callback_called = 0;
    gps_response_status = 0;
}

void gps_cleanup(void) {
    blob_buf_free(&gps_response_buf);
}

int gps_fetch_data(void) {
    uint32_t id;
    int ret;

    if (!ctx) {
        if (!log_mode) fprintf(stderr, "UBus context not available\n");
        return -1;
    }

    ret = ubus_lookup_id(ctx, "gps", &id);
    if (ret != 0) {
        if (!log_mode) fprintf(stderr, "GPS service not found\n");
        return -1;
    }

    gps_callback_called = 0;
    gps_response_status = 0;
    blob_buf_free(&gps_response_buf);
    memset(&gps_response_buf, 0, sizeof(gps_response_buf));
    ret = ubus_invoke(ctx, id, "info", NULL, gps_data_cb, NULL, 1000);

    if (ret != 0) {
        if (!log_mode) fprintf(stderr, "Failed to call GPS info (error: %d)\n", ret);
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

const char *gps_get_value(const char *key) {
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