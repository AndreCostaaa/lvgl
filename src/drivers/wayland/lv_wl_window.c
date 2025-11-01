/**
 * @file lv_wl_window.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_wl_window.h"

#if LV_USE_WAYLAND

#include <string.h>
#include "lv_wayland_private.h"
#include "lv_wayland_private.h"
#include "lv_wl_pointer.h"
#include "lv_wl_pointer_axis.h"
#include "lv_wl_touch.h"
#include "lv_wl_keyboard.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void refr_start_event(lv_event_t * e);
static void refr_end_event(lv_event_t * e);
static void res_changed_event(lv_event_t * e);


static lv_wl_surface_t * lv_wayland_surface_create(lv_wl_window_t * window, lv_wl_surface_type_t type,
                                                   lv_wl_surface_t * parent);
static void lv_wayland_surface_delete(lv_wl_surface_t * surface);

/* Create a window
 * @description Creates the graphical context for the window body, and then create a toplevel
 * wayland surface and commit it to obtain an XDG configuration event
 * @param width the height of the window w/decorations
 * @param height the width of the window w/decorations
 */
static lv_wl_window_t * create_window(struct lv_wayland_context * app, const char * title);

/**
 * The frame callback called when the compositor has finished rendering
 * a frame.It increments the frame counter and sets up the callback
 * for the next frame the frame counter is used to avoid needlessly
 * committing frames too fast on a slow system
 *
 * NOTE: this function is invoked by the wayland-server library within the compositor
 * the event is added to the queue, and then upon the next timer call it's
 * called indirectly from _lv_wayland_handle_input (via wl_display_dispatch_queue)
 * @param void data the user object defined that was tied to this event during
 * the configuration of the callback
 * @param struct wl_callback The callback that needs to be destroyed and re-created
 * @param time Timestamp of the event (unused)
 */
static void lv_window_graphic_obj_flush_done(void * data, struct wl_callback * cb, uint32_t time);

/**********************
 *  STATIC VARIABLES
 **********************/

static const struct wl_callback_listener wl_surface_frame_listener = {
    .done = lv_window_graphic_obj_flush_done,
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_display_t * lv_wayland_window_create(uint32_t hor_res, uint32_t ver_res, char * title,
                                        lv_wayland_display_close_cb_t close_cb)
{
    lv_wayland_init();

    lv_wl_window_t * window = create_window(&lv_wl_ctx, title);
    if(!window) {
        LV_LOG_ERROR("failed to create wayland window");
        return NULL;
    }

    window->close_cb = close_cb;

    /* Initialize display driver */
    window->lv_disp = lv_display_create(hor_res, ver_res);
    if(window->lv_disp == NULL) {
        LV_LOG_ERROR("failed to create lvgl display");
        return NULL;
    }

    void * backend_ddata = wl_backend_ops.init_display(lv_wl_ctx.backend_data, window->lv_disp, hor_res, ver_res);
    if(!backend_ddata) {
        LV_LOG_ERROR("Backend initialization failed");
        return NULL;
    }
    window->backend_display_data = backend_ddata;
    lv_display_set_driver_data(window->lv_disp, window);
    lv_wayland_xdg_shell_configure_surface(window);

#if LV_WAYLAND_USE_DMABUF
    lv_wayland_dmabuf_set_draw_buffers(&lv_wl_ctx.dmabuf_ctx, window->lv_disp);
    lv_display_set_flush_cb(window->lv_disp, lv_wayland_dmabuf_flush_full_mode);
#endif

    lv_display_add_event_cb(window->lv_disp, res_changed_event, LV_EVENT_RESOLUTION_CHANGED, window);
    lv_display_add_event_cb(window->lv_disp, refr_start_event, LV_EVENT_REFR_START, window);
    lv_display_add_event_cb(window->lv_disp, refr_end_event, LV_EVENT_REFR_READY, window);

    /* Register input */
    window->lv_indev_pointer = lv_wayland_pointer_create();

    lv_indev_set_display(window->lv_indev_pointer, window->lv_disp);

    if(!window->lv_indev_pointer) {
        LV_LOG_ERROR("failed to register pointer indev");
    }

    window->lv_indev_pointeraxis = lv_wayland_pointer_axis_create();
    lv_indev_set_display(window->lv_indev_pointeraxis, window->lv_disp);

    if(!window->lv_indev_pointeraxis) {
        LV_LOG_ERROR("failed to register pointeraxis indev");
    }

    window->lv_indev_touch = lv_wayland_touch_create();
    lv_indev_set_display(window->lv_indev_touch, window->lv_disp);

    if(!window->lv_indev_touch) {
        LV_LOG_ERROR("failed to register touch indev");
    }

    window->lv_indev_keyboard = lv_wayland_keyboard_create();
    lv_indev_set_display(window->lv_indev_keyboard, window->lv_disp);

    if(!window->lv_indev_keyboard) {
        LV_LOG_ERROR("failed to register keyboard indev");
    }
    return window->lv_disp;
}

void * lv_wayland_get_backend_display_data(lv_display_t * display)
{
    lv_wl_window_t * window = lv_display_get_driver_data(display);
    return window->backend_display_data;
}

struct wl_surface * lv_wayland_get_window_surface(lv_display_t * display)
{

    lv_wl_window_t * window = lv_display_get_driver_data(display);
    return window->body->wl_surface;
}

void lv_wayland_window_close(lv_display_t * disp)
{
    struct window * window = lv_display_get_driver_data(disp);
    if(!window || window->closed) {
        return;
    }
    window->shall_close = true;
    window->close_cb    = NULL;
    lv_wayland_deinit();
}

bool lv_wayland_window_is_open(lv_display_t * disp)
{
    struct window * window;
    bool open = false;

    if(disp == NULL) {
        LV_LL_READ(&lv_wl_ctx.window_ll, window) {
            if(!window->closed) {
                open = true;
                break;
            }
        }
    }
    else {
        window = lv_display_get_driver_data(disp);
        open   = (!window->closed);
    }

    return open;
}

void lv_wayland_window_set_maximized(lv_display_t * disp, bool maximized)
{
    struct window * window = lv_display_get_driver_data(disp);
    lv_result_t err        = LV_RESULT_INVALID;
    if(!window || window->closed) {
        return;
    }

    if(window->maximized != maximized) {
        err = lv_wayland_xdg_shell_set_maximized(window, maximized);
    }

    if(err == LV_RESULT_INVALID) {
        LV_LOG_WARN("Failed to maximize wayland window");
        return;
    }

    window->maximized     = maximized;
    window->flush_pending = true;
}

void lv_wayland_assign_physical_display(lv_display_t * disp, uint8_t display_number)
{
    if(!disp) {
        LV_LOG_ERROR("Invalid display");
        return;
    }

    struct window * window = lv_display_get_driver_data(disp);

    if(!window || window->closed) {
        LV_LOG_ERROR("Invalid window");
        return;
    }

    if(display_number >= lv_wl_ctx.wl_output_count) {
        LV_LOG_WARN("Invalid display number '%d'. Expected '0'..'%d'", display_number, lv_wl_ctx.wl_output_count - 1);
        return;
    }
    window->assigned_output = lv_wl_ctx.outputs[display_number].wl_output;
}

void lv_wayland_unassign_physical_display(lv_display_t * disp)
{

    if(!disp) {
        LV_LOG_ERROR("Invalid display");
        return;
    }

    struct window * window = lv_display_get_user_data(disp);

    if(!window || window->closed) {
        LV_LOG_ERROR("Invalid window");
        return;
    }
    window->assigned_output = NULL;
}

void lv_wayland_window_set_fullscreen(lv_display_t * disp, bool fullscreen)
{
    struct window * window = lv_display_get_driver_data(disp);
    lv_result_t err        = LV_RESULT_INVALID;
    if(!window || window->closed) {
        return;
    }

    if(window->fullscreen == fullscreen) {
        return;
    }
    err = lv_wayland_xdg_shell_set_fullscreen(window, fullscreen, window->assigned_output);

    if(err == LV_RESULT_INVALID) {
        LV_LOG_WARN("Failed to set wayland window to fullscreen");
        return;
    }

    window->fullscreen = fullscreen;
    window->flush_pending = true;
}

/**********************
 *   PRIVATE FUNCTIONS
 **********************/

int32_t lv_wayland_window_get_width(lv_wl_window_t * window)
{
    int32_t w = lv_display_get_horizontal_resolution(window->lv_disp);
    if(LV_WAYLAND_WINDOW_DECORATIONS && !lv_wl_ctx.opt_disable_decorations) {
        return w + (2 * BORDER_SIZE);
    }
    return w;

}
int32_t lv_wayland_window_get_height(lv_wl_window_t * window)
{
    int32_t h = lv_display_get_vertical_resolution(window->lv_disp);
    if(LV_WAYLAND_WINDOW_DECORATIONS && !lv_wl_ctx.opt_disable_decorations) {
        return h + (TITLE_BAR_HEIGHT + (2 * BORDER_SIZE));
    }
    return h;

}

lv_result_t lv_wayland_window_resize(lv_wl_window_t * window, int32_t width, int32_t height)
{
    if(LV_WAYLAND_WINDOW_DECORATIONS && !lv_wl_ctx.opt_disable_decorations && !window->fullscreen) {
        width -= (2 * BORDER_SIZE);
        height -= (TITLE_BAR_HEIGHT + (2 * BORDER_SIZE));
    }

    if(window->lv_disp) {
        lv_display_set_resolution(window->lv_disp, width, height);
#if 0
        window->body->input.pointer.x = LV_MIN((int32_t)window->body->input.pointer.x, (width - 1));
        window->body->input.pointer.y = LV_MIN((int32_t)window->body->input.pointer.y, (height - 1));
#endif
    }

    /* On the first resize call, the resolution of the display is already set, so there won't be a trigger on the resolution changed event.*/
    if(!window->is_window_configured) {
#if LV_WAYLAND_USE_DMABUF
        lv_result_t err = lv_wayland_dmabuf_resize_window(&window->wl_ctx->dmabuf_ctx, window, width, height);
        if(err != LV_RESULT_OK) {
            return err;
        }
#endif
    }

#if LV_WAYLAND_WINDOW_DECORATIONS
    if(!lv_wl_ctx.opt_disable_decorations && !window->fullscreen) {
        lv_wayland_window_decoration_create_all(window);
    }
    else if(!lv_wl_ctx.opt_disable_decorations) {
        /* Entering fullscreen, detach decorations to prevent xdg_wm_base error 4 */
        /* requested geometry larger than the configured fullscreen state */
        lv_wayland_window_decoration_detach_all(window);
    }
#endif

    return LV_RESULT_OK;
}

void lv_wayland_window_destroy(struct window * window)
{
    if(!window) {
        return;
    }

    lv_wayland_xdg_shell_destroy_window_toplevel(window);
    lv_wayland_xdg_shell_destroy_window_surface(window);

#if LV_WAYLAND_WINDOW_DECORATIONS
    for(size_t i = 0; i < NUM_DECORATIONS; i++) {
        if(!window->decoration[i]) {
            continue;
        }
        lv_wayland_surface_delete(window->decoration[i]);
        window->decoration[i] = NULL;
    }
#endif

    lv_wayland_surface_delete(window->body);
    lv_display_delete(window->lv_disp);
    lv_ll_remove(&lv_wl_ctx.window_ll, window);
}

const struct wl_callback_listener * lv_wayland_window_get_wl_surface_frame_listener(void)
{
    return &wl_surface_frame_listener;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void refr_start_event(lv_event_t * e)
{
    lv_wl_window_t * window = lv_event_get_user_data(e);
    lv_wayland_read_input_events();

    LV_LOG_TRACE("handle timer frame: %d", window->frame_counter);

    if(window != NULL && window->resize_pending) {
#if LV_WAYLAND_USE_DMABUF
        /* Check surface configuration state before resizing */
        if(!window->surface_configured) {
            LV_LOG_TRACE("Deferring resize - surface not configured yet");
            return;
        }
#endif
        LV_LOG_TRACE("Processing resize: %dx%d -> %dx%d",
                     lv_wayland_window_get_width(window), lv_wayland_window_get_height(window),
                     window->resize_width, window->resize_height);

        if(lv_wayland_window_resize(window, window->resize_width, window->resize_height) == LV_RESULT_OK) {
            window->resize_pending = false;
#if LV_WAYLAND_USE_DMABUF
            /* Reset synchronization flags after successful resize */
            window->surface_configured = false;
            window->dmabuf_resize_pending = false;
#endif
            LV_LOG_TRACE("Window resize completed successfully: %dx%d", lv_wayland_window_get_width(window),
                         lv_wayland_window_get_height(window));
        }
        else {
            LV_LOG_ERROR("Failed to resize window frame: %d", window->frame_counter);
        }
    }
    else if(window->shall_close) {
        /* Destroy graphical context and execute close_cb */
        lv_wayland_update_window(window);
        if(lv_ll_is_empty(&lv_wl_ctx.window_ll)) {
            lv_wayland_deinit();

        }
    }

}

static void refr_end_event(lv_event_t * e)
{
    struct window * window = lv_event_get_user_data(e);
    lv_wayland_update_window(window);
}

static void res_changed_event(lv_event_t * e)
{
    lv_display_t * display = (lv_display_t *) lv_event_get_target(e);
#if LV_WAYLAND_USE_DMABUF
    dmabuf_ctx_t * context = &window->wl_ctx->dmabuf_ctx;
    lv_wayland_dmabuf_resize_window(context, window, width, height);
#endif
    wl_backend_ops.resize_display(lv_wl_ctx.backend_data, display);
}

static lv_wl_window_t * create_window(struct lv_wayland_context * app, const char * title)
{
    lv_wl_window_t * window = lv_ll_ins_tail(&app->window_ll);
    LV_ASSERT_MALLOC(window);
    if(!window) {
        return NULL;
    }

    lv_memset(window, 0, sizeof(*window));

    window->body = lv_wayland_surface_create(window, OBJECT_WINDOW, NULL);
    if(!window->body) {
        LV_LOG_ERROR("cannot create window body");
        goto err_free_window;
    }

#if LV_WAYLAND_WINDOW_DECORATIONS
    if(!lv_wl_ctx.opt_disable_decorations) {
        for(size_t i = 0; i < NUM_DECORATIONS; i++) {
            window->decoration[i] = lv_wayland_surface_create(window, (FIRST_DECORATION + i), window->body);
            if(!window->decoration[i]) {
                LV_LOG_ERROR("Failed to create decoration %zu", i);
            }
        }
    }

#endif /*LV_WAYLAND_WINDOW_DECORATIONS*/

    if(lv_wayland_xdg_shell_create_window(app, window, title) != LV_RESULT_OK) {
        goto err_destroy_surface;
    }

    return window;

err_destroy_surface:
    wl_surface_destroy(window->body->wl_surface);

err_free_window:
    lv_ll_remove(&app->window_ll, window);
    lv_free(window);
    return NULL;
}

static lv_wl_surface_t * lv_wayland_surface_create(lv_wl_window_t * window, lv_wl_surface_type_t type,
                                                   lv_wl_surface_t * parent_surface)
{

    LV_UNUSED(parent_surface);
    lv_wl_surface_t * surface = lv_zalloc(sizeof(*surface));

    LV_ASSERT_MALLOC(surface);
    if(!surface) {
        LV_LOG_ERROR("Failed to allocate memory for new surface");
        return NULL;
    }

    surface->wl_surface = wl_compositor_create_surface(lv_wl_ctx.compositor);
    if(!surface->wl_surface) {
        LV_LOG_ERROR("Failed to create surface");
        lv_free(surface);
        return NULL;
    }
    wl_surface_set_user_data(surface->wl_surface, surface);

    surface->window = window;
    surface->type = type;
    return surface;
}

static void lv_wayland_surface_delete(lv_wl_surface_t * surface)
{
    if(surface->wl_subsurface) {
        wl_subsurface_destroy(surface->wl_subsurface);
    }

    wl_surface_destroy(surface->wl_surface);
    lv_free(surface);
}

static void lv_window_graphic_obj_flush_done(void * data, struct wl_callback * cb, uint32_t time)
{
    struct graphic_object * obj;
    struct window * window;

    LV_UNUSED(time);

    wl_callback_destroy(cb);

    obj    = (struct graphic_object *)data;
    window = obj->window;
    window->frame_counter++;

    LV_LOG_TRACE("frame: %d done, new frame: %d", window->frame_counter - 1, window->frame_counter);

    lv_display_flush_ready(window->lv_disp);
}

#endif /* LV_USE_WAYLAND */
