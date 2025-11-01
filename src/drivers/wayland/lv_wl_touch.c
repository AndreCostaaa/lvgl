/**
 * @file lv_wl_touch.c
 *
 */

#include "lv_wl_touch.h"

#if LV_USE_WAYLAND

#include "lv_wayland_private.h"

#include <wayland-client-protocol.h>
#include <wayland-cursor.h>

/*********************
 *      INCLUDES
 *********************/

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void touch_read(lv_indev_t * drv, lv_indev_data_t * data);

static void touch_handle_down(void * data, struct wl_touch * wl_touch, uint32_t serial, uint32_t time,
                              struct wl_surface * surface, int32_t id, wl_fixed_t x_w, wl_fixed_t y_w);

static void touch_handle_up(void * data, struct wl_touch * wl_touch, uint32_t serial, uint32_t time, int32_t id);

static void touch_handle_motion(void * data, struct wl_touch * wl_touch, uint32_t time, int32_t id, wl_fixed_t x_w,
                                wl_fixed_t y_w);

static void touch_handle_frame(void * data, struct wl_touch * wl_touch);

static void touch_handle_cancel(void * data, struct wl_touch * wl_touch);

/**********************
 *  STATIC VARIABLES
 **********************/

static const struct wl_touch_listener touch_listener = {
    .down   = touch_handle_down,
    .up     = touch_handle_up,
    .motion = touch_handle_motion,
    .frame  = touch_handle_frame,
    .cancel = touch_handle_cancel,
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_indev_t * lv_wayland_touch_create(void)
{

    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read);

    return indev;
}

lv_indev_t * lv_wayland_get_touchscreen(lv_display_t * display)
{
    struct window * window = lv_display_get_driver_data(display);
    if(!window) {
        return NULL;
    }
    return window->lv_indev_touch;
}

/**********************
 *   PRIVATE FUNCTIONS
 **********************/

lv_wl_seat_touch_t * lv_wayland_seat_touch_create(struct wl_seat * seat)
{

    struct wl_touch * touch = wl_seat_get_touch(seat);
    if(!touch) {
        LV_LOG_WARN("Failed to get seat touch");
        return NULL;
    }
    lv_wl_seat_touch_t * wl_seat_touch = lv_zalloc(sizeof(*wl_seat_touch));
    LV_ASSERT_MALLOC(wl_seat_touch);
    if(!wl_seat_touch) {
        LV_LOG_WARN("Failed to allocate memory for wayland touch");
        wl_touch_destroy(touch);
        return NULL;
    }
    wl_touch_add_listener(touch, &touch_listener, NULL);
    wl_touch_set_user_data(touch, wl_seat_touch);

    wl_seat_touch->wl_touch = touch;
    lv_wayland_update_indevs(touch_read, wl_seat_touch);

    return wl_seat_touch;
}
void lv_wayland_seat_touch_delete(lv_wl_seat_touch_t * seat_touch)
{
    lv_wayland_update_indevs(touch_read, NULL);
    wl_touch_destroy(seat_touch->wl_touch);
    lv_free(seat_touch);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void touch_read(lv_indev_t * indev, lv_indev_data_t * data)
{

    lv_wl_seat_touch_t * tdata = lv_indev_get_driver_data(indev);

    if(!tdata) {
        return;
    }
#if LV_USE_GESTURE_RECOGNITION
    /* Collect touches if there are any - send them to the gesture recognizer */
    lv_indev_gesture_recognizers_update(indev, tdata->touches, tdata->touch_event_cnt);

    LV_LOG_TRACE("collected touch events: %d", window->body->input.touch_event_cnt);

    tdata->touch_event_cnt = 0;

    /* Set the gesture information, before returning to LVGL */
    lv_indev_gesture_recognizers_set_data(indev, data);

    if(tdata->touch_event_cnt > 0) {
        data->point = tdata->touches[0].point;
    }
    else {
        data->point.x = data->point.y = 0;
    }
#else
    data->point = tdata->point;
    data->state = tdata->state;
#endif
}

static void touch_handle_down(void * data, struct wl_touch * wl_touch, uint32_t serial, uint32_t time,
                              struct wl_surface * surface, int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
    LV_UNUSED(data);
    LV_UNUSED(id);
    LV_UNUSED(time);
    LV_UNUSED(serial);

    lv_wl_seat_touch_t * tdata = wl_touch_get_user_data(wl_touch);

    if(!surface) {
        return;
    }

#if LV_USE_GESTURE_RECOGNITION
    uint8_t i = tdata->event_cnt;

    tdata->s[i].point.x   = wl_fixed_to_int(x_w);
    tdata->s[i].point.y   = wl_fixed_to_int(y_w);
    tdata->s[i].id        = id;
    tdata->s[i].timestamp = time;
    tdata->s[i].state     = LV_INDEV_STATE_PRESSED;
    tdata->event_cnt++;
#else
    tdata->point.x = wl_fixed_to_int(x_w);
    tdata->point.y = wl_fixed_to_int(y_w);
    tdata->state = LV_INDEV_STATE_PRESSED;
#endif

#if LV_WAYLAND_WINDOW_DECORATIONS
    struct window * window = tdata->current_pointed_obj->window;
    switch(tdata->current_pointed_obj->type) {
        case OBJECT_TITLEBAR:
            if(window->xdg_toplevel) {
                xdg_toplevel_move(window->xdg_toplevel, lv_wl_ctx.seat.wl_seat, serial);
                window->flush_pending = true;
            }
            break;
        default:
            break;
    }
#endif
}

static void touch_handle_up(void * data, struct wl_touch * wl_touch, uint32_t serial, uint32_t time, int32_t id)
{
    LV_UNUSED(serial);
    LV_UNUSED(time);
    LV_UNUSED(id);
    LV_UNUSED(data);
    lv_wl_seat_touch_t * tdata = wl_touch_get_user_data(wl_touch);

    /* Create a released event */
#if LV_USE_GESTURE_RECOGNITION
    uint8_t i = tdata->event_cnt;

    tdata->s[i].point.x   = 0;
    tdata->s[i].point.y   = 0;
    tdata->s[i].id        = id;
    tdata->s[i].timestamp = time;
    tdata->s[i].state     = LV_INDEV_STATE_RELEASED;

    tdata->event_cnt++;
#else
    tdata->state = LV_INDEV_STATE_RELEASED;
#endif

#if LV_WAYLAND_WINDOW_DECORATIONS
    struct window * window = tdata->current_pointed_obj->window;
    switch(tdata->current_pointed_obj->type) {
        case OBJECT_BUTTON_CLOSE:
            window->shall_close = true;
            break;
        case OBJECT_BUTTON_MAXIMIZE:
            if(window->xdg_toplevel) {
                if(window->maximized) {
                    xdg_toplevel_unset_maximized(window->xdg_toplevel);
                }
                else {
                    xdg_toplevel_set_maximized(window->xdg_toplevel);
                }
                window->maximized ^= true;
            }
            break;
        case OBJECT_BUTTON_MINIMIZE:
            if(window->xdg_toplevel) {
                xdg_toplevel_set_minimized(window->xdg_toplevel);
                window->flush_pending = true;
            }
        default:
            break;
    }
#endif /* LV_WAYLAND_WINDOW_DECORATIONS */
}

static void touch_handle_motion(void * data, struct wl_touch * wl_touch, uint32_t time, int32_t id, wl_fixed_t x_w,
                                wl_fixed_t y_w)
{

    LV_UNUSED(id);
    LV_UNUSED(time);
    LV_UNUSED(data);
    lv_wl_seat_touch_t * tdata = wl_touch_get_user_data(wl_touch);

#if LV_USE_GESTURE_RECOGNITION
    /* Update the contact point of the corresponding id with the latest coordinate */
    lv_indev_touch_data_t * touch = &tdata->s[0];
    lv_indev_touch_data_t * cur = NULL;

    for(uint8_t i = 0; i < tdata->event_cnt; i++) {
        if(touch->id == id) {
            cur = touch;
        }
        touch++;
    }

    if(cur == NULL) {
        uint8_t i = tdata->event_cnt;
        tdata->s[i].point.x   = wl_fixed_to_int(x_w);
        tdata->s[i].point.y   = wl_fixed_to_int(y_w);
        tdata->s[i].id        = id;
        tdata->s[i].timestamp = time;
        tdata->s[i].state     = LV_INDEV_STATE_PRESSED;
        tdata->event_cnt++;
    }
    else {
        cur->point.x   = wl_fixed_to_int(x_w);
        cur->point.y   = wl_fixed_to_int(y_w);
        cur->id        = id;
        cur->timestamp = time;
    }
#else
    tdata->point.x = wl_fixed_to_int(x_w);
    tdata->point.y = wl_fixed_to_int(y_w);
#endif
}

static void touch_handle_frame(void * data, struct wl_touch * wl_touch)
{
    LV_UNUSED(wl_touch);
    LV_UNUSED(data);
}

static void touch_handle_cancel(void * data, struct wl_touch * wl_touch)
{
    LV_UNUSED(wl_touch);
    LV_UNUSED(data);
}

#endif /* LV_USE_WAYLAND */
