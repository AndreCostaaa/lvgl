/**
 * @file lv_wl_xdg_shell.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_wayland.h"

#if LV_USE_WAYLAND
#include "lv_wayland_private.h"


#include <linux/input-event-codes.h>
#include "wayland_xdg_shell.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void xdg_surface_handle_configure(void * data, struct xdg_surface * xdg_surface, uint32_t serial);
static void xdg_toplevel_handle_configure(void * data, struct xdg_toplevel * xdg_toplevel, int32_t width,
                                          int32_t height, struct wl_array * states);
static void xdg_toplevel_handle_close(void * data, struct xdg_toplevel * xdg_toplevel);
static void xdg_wm_base_ping(void * data, struct xdg_wm_base * xdg_wm_base, uint32_t serial);

/**********************
 *  STATIC VARIABLES
 **********************/

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_handle_configure,
};

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_handle_configure,
    .close     = xdg_toplevel_handle_close,
};

static const struct xdg_wm_base_listener xdg_wm_base_listener = {.ping = xdg_wm_base_ping};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**********************
 *   PRIVATE FUNCTIONS
 **********************/

/**********************
 *   Shell
 **********************/

void lv_wayland_xdg_shell_deinit(void)
{

    if(lv_wl_ctx.xdg_wm) {
        xdg_wm_base_destroy(lv_wl_ctx.xdg_wm);
    }
}

/**********************
 *   Listeners
 **********************/

const struct xdg_wm_base_listener * lv_wayland_xdg_shell_get_wm_base_listener(void)
{
    return &xdg_wm_base_listener;
}

const struct xdg_surface_listener * lv_wayland_xdg_shell_get_surface_listener(void)
{
    return &xdg_surface_listener;
}

const struct xdg_toplevel_listener * lv_wayland_xdg_shell_get_toplevel_listener(void)
{
    return &xdg_toplevel_listener;
}

/**********************
 *   Shell Window
 **********************/

lv_result_t lv_wayland_xdg_shell_set_fullscreen(struct window * window, bool fullscreen, struct wl_output * output)
{

    if(!window->xdg_toplevel) {
        return LV_RESULT_INVALID;
    }
    if(fullscreen) {
        xdg_toplevel_set_fullscreen(window->xdg_toplevel, output);
    }
    else {
        xdg_toplevel_unset_fullscreen(window->xdg_toplevel);
    }
    return LV_RESULT_OK;
}

lv_result_t lv_wayland_xdg_shell_set_maximized(struct window * window, bool maximized)
{
    if(!window->xdg_toplevel) {
        return LV_RESULT_INVALID;
    }

    if(maximized) {
        xdg_toplevel_set_maximized(window->xdg_toplevel);
    }
    else {
        xdg_toplevel_unset_maximized(window->xdg_toplevel);
    }

    return LV_RESULT_OK;
}

lv_result_t lv_wayland_xdg_shell_set_minimized(struct window * window)
{
    if(!window->xdg_toplevel) {
        return LV_RESULT_INVALID;
    }

    xdg_toplevel_set_minimized(window->xdg_toplevel);
    return LV_RESULT_OK;
}

#if LV_WAYLAND_USE_DMABUF
void lv_wayland_xdg_shell_ack_configure(struct window * window, uint32_t serial)
{
    if(window->xdg_surface && serial > 0) {
        xdg_surface_ack_configure(window->xdg_surface, serial);
        LV_LOG_TRACE("XDG surface configure acknowledged (serial=%u)", serial);
    }
}
#endif

lv_result_t lv_wayland_xdg_shell_create_window(struct lv_wayland_context * app, struct window * window,
                                               const char * title)
{
    if(!app->xdg_wm) {
        return LV_RESULT_INVALID;
    }

    window->xdg_surface = xdg_wm_base_get_xdg_surface(app->xdg_wm, window->body->wl_surface);
    if(!window->xdg_surface) {
        LV_LOG_ERROR("Failed to create XDG surface");
        return LV_RESULT_INVALID;
    }

    xdg_surface_add_listener(window->xdg_surface, lv_wayland_xdg_shell_get_surface_listener(), window);

    window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
    if(!window->xdg_toplevel) {
        xdg_surface_destroy(window->xdg_surface);
        LV_LOG_ERROR("Failed to acquire XDG toplevel surface");
        return LV_RESULT_INVALID;
    }

    xdg_toplevel_add_listener(window->xdg_toplevel, lv_wayland_xdg_shell_get_toplevel_listener(), window);
    xdg_toplevel_set_title(window->xdg_toplevel, title);
    xdg_toplevel_set_app_id(window->xdg_toplevel, title);

    return LV_RESULT_OK;
}

void lv_wayland_xdg_shell_configure_surface(struct window * window)
{
    // XDG surfaces need to be configured before a buffer can be attached.
    // An (XDG) surface commit (without an attached buffer) triggers this
    // configure event
    window->is_window_configured = false;
    wl_surface_commit(window->body->wl_surface);
    wl_display_roundtrip(lv_wl_ctx.compositor_connection);
    LV_ASSERT_MSG(window->is_window_configured, "Failed to receive the xdg_surface configuration event");
}

lv_result_t lv_wayland_xdg_shell_destroy_window_surface(struct window * window)
{

    if(!window->xdg_surface) {
        return LV_RESULT_INVALID;
    }
    xdg_surface_destroy(window->xdg_surface);
    return LV_RESULT_OK;
}

lv_result_t lv_wayland_xdg_shell_destroy_window_toplevel(struct window * window)
{

    if(!window->xdg_toplevel) {
        return LV_RESULT_INVALID;
    }
    xdg_toplevel_destroy(window->xdg_toplevel);
    return LV_RESULT_OK;
}

/**********************
 *   Shell Input
 **********************/

void lv_wayland_xdg_shell_handle_pointer_event(const lv_wl_seat_pointer_t * seat_pointer, uint32_t serial,
                                               uint32_t button,
                                               uint32_t state)
{
    lv_wl_window_t * window = seat_pointer->current_pointed_obj->window;
    int32_t window_height = lv_wayland_window_get_height(window);
    int32_t window_width = lv_wayland_window_get_width(window);

    int pos_x = seat_pointer->point.x;
    int pos_y = seat_pointer->point.y;

    switch(seat_pointer->current_pointed_obj->type) {
        case OBJECT_TITLEBAR:
            if((button == BTN_LEFT) && (state == WL_POINTER_BUTTON_STATE_PRESSED)) {
                if(window->xdg_toplevel) {
                    xdg_toplevel_move(window->xdg_toplevel, lv_wl_ctx.seat.wl_seat, serial);
                    window->flush_pending = true;
                }
            }
            break;
        case OBJECT_BUTTON_MAXIMIZE:
            if((button == BTN_LEFT) && (state == WL_POINTER_BUTTON_STATE_RELEASED)) {
                if(lv_wayland_xdg_shell_set_maximized(window, !window->maximized) == LV_RESULT_OK) {
                    window->maximized     = !window->maximized;
                    window->flush_pending = true;
                }
            }
            break;
        case OBJECT_BUTTON_MINIMIZE:
            if((button == BTN_LEFT) && (state == WL_POINTER_BUTTON_STATE_RELEASED)) {
                if(lv_wayland_xdg_shell_set_minimized(window) == LV_RESULT_OK) {
                    window->flush_pending = true;
                }
            }
            break;
        case OBJECT_BORDER_TOP:
            if((button == BTN_LEFT) && (state == WL_POINTER_BUTTON_STATE_PRESSED)) {
                if(window->xdg_toplevel && !window->maximized) {
                    uint32_t edge;
                    if(pos_x < (BORDER_SIZE * 5)) {
                        edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
                    }
                    else if(pos_x >= (lv_wayland_window_get_width(window) + BORDER_SIZE - (BORDER_SIZE * 5))) {
                        edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
                    }
                    else {
                        edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP;
                    }
                    xdg_toplevel_resize(window->xdg_toplevel, lv_wl_ctx.seat.wl_seat, serial, edge);
                    window->flush_pending = true;
                }
            }
            break;
        case OBJECT_BORDER_BOTTOM:
            if((button == BTN_LEFT) && (state == WL_POINTER_BUTTON_STATE_PRESSED)) {
                if(window->xdg_toplevel && !window->maximized) {
                    uint32_t edge;
                    if(pos_x < (BORDER_SIZE * 5)) {
                        edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
                    }
                    else if(pos_x >= (window_width + BORDER_SIZE - (BORDER_SIZE * 5))) {
                        edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
                    }
                    else {
                        edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
                    }
                    xdg_toplevel_resize(window->xdg_toplevel, lv_wl_ctx.seat.wl_seat, serial, edge);
                    window->flush_pending = true;
                }
            }
            break;
        case OBJECT_BORDER_LEFT:
            if((button == BTN_LEFT) && (state == WL_POINTER_BUTTON_STATE_PRESSED)) {
                if(window->xdg_toplevel && !window->maximized) {
                    uint32_t edge;
                    if(pos_y < (BORDER_SIZE * 5)) {
                        edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
                    }
                    else if(pos_y >= (window_height + BORDER_SIZE - (BORDER_SIZE * 5))) {
                        edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
                    }
                    else {
                        edge = XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
                    }
                    xdg_toplevel_resize(window->xdg_toplevel, lv_wl_ctx.seat.wl_seat, serial, edge);
                    window->flush_pending = true;
                }
            }
            break;
        case OBJECT_BORDER_RIGHT:
            if((button == BTN_LEFT) && (state == WL_POINTER_BUTTON_STATE_PRESSED)) {
                if(window->xdg_toplevel && !window->maximized) {
                    uint32_t edge;
                    if(pos_y < (BORDER_SIZE * 5)) {
                        edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
                    }
                    else if(pos_y >= (lv_wayland_window_get_height(window) + BORDER_SIZE - (BORDER_SIZE * 5))) {
                        edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
                    }
                    else {
                        edge = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
                    }
                    xdg_toplevel_resize(window->xdg_toplevel, lv_wl_ctx.seat.wl_seat, serial, edge);
                    window->flush_pending = true;
                }
            }
            break;
        case OBJECT_BUTTON_CLOSE:
        case OBJECT_WINDOW:
            /* These events are handled in the main pointer callback */
            break;
    }
}

const char * lv_wayland_xdg_shell_get_cursor_name(const lv_wl_seat_pointer_t * seat_pointer)
{

    if(!LV_WAYLAND_WINDOW_DECORATIONS || !seat_pointer->current_pointed_obj || lv_wl_ctx.opt_disable_decorations) {
        return LV_WAYLAND_DEFAULT_CURSOR_NAME;
    }
    int pos_x = seat_pointer->point.x;
    int pos_y = seat_pointer->point.y;

    struct window * window = seat_pointer->current_pointed_obj->window;
    int32_t window_height = lv_wayland_window_get_height(window);
    int32_t window_width = lv_wayland_window_get_width(window);

    switch(seat_pointer->current_pointed_obj->type) {
        case OBJECT_BORDER_TOP:
            if(window->maximized) {
                // do nothing
            }
            else if(pos_x < (BORDER_SIZE * 5)) {
                return "top_left_corner";
            }
            else if(pos_x >= (window_width + BORDER_SIZE - (BORDER_SIZE * 5))) {
                return "top_right_corner";
            }
            else {
                return "top_side";
            }
            break;
        case OBJECT_BORDER_BOTTOM:
            if(window->maximized) {
                // do nothing
            }
            else if(pos_x < (BORDER_SIZE * 5)) {
                return "bottom_left_corner";
            }
            else if(pos_x >= (window_width + BORDER_SIZE - (BORDER_SIZE * 5))) {
                return "bottom_right_corner";
            }
            else {
                return "bottom_side";
            }
            break;
        case OBJECT_BORDER_LEFT:
            if(window->maximized) {
                // do nothing
            }
            else if(pos_y < (BORDER_SIZE * 5)) {
                return "top_left_corner";
            }
            else if(pos_y >= (window_height + BORDER_SIZE - (BORDER_SIZE * 5))) {
                return "bottom_left_corner";
            }
            else {
                return "left_side";
            }
            break;
        case OBJECT_BORDER_RIGHT:
            if(window->maximized) {
                // do nothing
            }
            else if(pos_y < (BORDER_SIZE * 5)) {
                return "top_right_corner";
            }
            else if(pos_y >= (window_height + BORDER_SIZE - (BORDER_SIZE * 5))) {
                return "bottom_right_corner";
            }
            else {
                return "right_side";
            }
            break;
        default:
            break;
    }

    return LV_WAYLAND_DEFAULT_CURSOR_NAME;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void xdg_surface_handle_configure(void * data, struct xdg_surface * xdg_surface, uint32_t serial)
{
    lv_wl_window_t * window = (lv_wl_window_t *)data;

    xdg_surface_ack_configure(xdg_surface, serial);
    if(!window->is_window_configured) {
        /* This branch is executed at launch */
        if(!window->resize_pending) {
            /* Use the size passed to the create_window function */
            lv_wayland_window_resize(window, lv_wayland_window_get_width(window),
                                     lv_wayland_window_get_height(window));
        }
        else {

            /* Handle early maximization or fullscreen, */
            /* by using the size communicated by the compositor */
            /* when the initial xdg configure event arrives  */
            lv_wayland_window_resize(window, window->resize_width, window->resize_height);
            window->resize_pending = false;
        }
    }
    window->is_window_configured = true;
}

static void xdg_toplevel_handle_configure(void * data, struct xdg_toplevel * xdg_toplevel, int32_t width,
                                          int32_t height, struct wl_array * states)
{
    struct window * window = (struct window *)data;

    LV_UNUSED(xdg_toplevel);
    LV_UNUSED(states);

    LV_LOG_USER("XDG toplevel configure: w=%d h=%d (current: %dx%d)",
                width, height, lv_wayland_window_get_width(window), lv_wayland_window_get_height(window));
    LV_LOG_USER("current body w:%d h:%d", window->body->width, window->body->height);

    if((width < 0) || (height < 0)) {
        LV_LOG_USER("will not resize to w:%d h:%d", width, height);
        return;
    }

    if(width == 0 && height == 0) {
        window->resize_pending = true;
        window->resize_width   = lv_wayland_window_get_width(window);
        window->resize_height  = lv_wayland_window_get_height(window);
        return;
    }

    if((width != lv_wayland_window_get_width(window)) || (height != lv_wayland_window_get_height(window))) {
        window->resize_width   = width;
        window->resize_height  = height;
        window->resize_pending = true;
#if LV_WAYLAND_USE_DMABUF
        window->dmabuf_resize_pending = true;
#endif
    }
    else {
        LV_LOG_USER("resize_pending not set w:%d h:%d", width, height);
    }
}

static void xdg_toplevel_handle_close(void * data, struct xdg_toplevel * xdg_toplevel)
{
    struct window * window = (struct window *)data;
    window->shall_close    = true;

    LV_UNUSED(xdg_toplevel);
}

static void xdg_wm_base_ping(void * data, struct xdg_wm_base * xdg_wm_base, uint32_t serial)
{
    LV_UNUSED(data);

    xdg_wm_base_pong(xdg_wm_base, serial);

    return;
}

#endif /* LV_USE_WAYLAND */
