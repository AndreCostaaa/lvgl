/**
 * @file lv_wayland_private.h
 *
 */

#ifndef LV_WAYLAND_PRIVATE_H
#define LV_WAYLAND_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "lv_wayland.h"
#if LV_USE_WAYLAND


#include <sys/poll.h>
#include <wayland-client-protocol.h>
#include <wayland_xdg_shell.h>
#include "lv_wl_backend.h"

#if LV_WAYLAND_USE_DMABUF
#include <sys/mman.h>
#include <string.h>
#endif

/*********************
 *      DEFINES
 *********************/

#define LV_WAYLAND_DEFAULT_CURSOR_NAME "left_ptr"
#define LV_WAYLAND_MAX_OUTPUTS 8

#define LVGL_DRAW_BUFFER_DIV (8)
#define DMG_CACHE_CAPACITY (32)
#define TAG_LOCAL (0)
#define TAG_BUFFER_DAMAGE (1)

#if LV_WAYLAND_WINDOW_DECORATIONS
#define TITLE_BAR_HEIGHT 24
#define BORDER_SIZE 2
#else
#define TITLE_BAR_HEIGHT 0
#define BORDER_SIZE 0
#endif

#define BUTTON_MARGIN LV_MAX((TITLE_BAR_HEIGHT / 6), BORDER_SIZE)
#define BUTTON_PADDING LV_MAX((TITLE_BAR_HEIGHT / 8), BORDER_SIZE)
#define BUTTON_SIZE (TITLE_BAR_HEIGHT - (2 * BUTTON_MARGIN))

#ifndef LV_WAYLAND_CYCLE_PERIOD
#define LV_WAYLAND_CYCLE_PERIOD LV_MIN(LV_DEF_REFR_PERIOD, 1)
#endif

/**********************
 *      TYPEDEFS
 **********************/

enum object_type {
    OBJECT_TITLEBAR = 0,
    OBJECT_BUTTON_CLOSE,
    OBJECT_BUTTON_MAXIMIZE,
    OBJECT_BUTTON_MINIMIZE,
    OBJECT_BORDER_TOP,
    OBJECT_BORDER_BOTTOM,
    OBJECT_BORDER_LEFT,
    OBJECT_BORDER_RIGHT,
    OBJECT_WINDOW,
};

#define FIRST_DECORATION (OBJECT_TITLEBAR)
#define LAST_DECORATION (OBJECT_BORDER_RIGHT)
#define NUM_DECORATIONS (LAST_DECORATION - FIRST_DECORATION + 1)

#if LV_WAYLAND_USE_DMABUF
#define MAX_BUFFER_PLANES 4
#endif

struct window;

typedef struct {
    struct graphic_object * current_pointed_obj;
    struct wl_pointer * wl_pointer;
    struct wl_surface * cursor_surface;
    lv_point_t point;
    lv_indev_state_t left_btn_state;
    lv_indev_state_t right_btn_state;
    lv_indev_state_t wheel_btn_state;
    int16_t wheel_diff;
} lv_wl_seat_pointer_t;

typedef struct {
    struct graphic_object * current_pointed_obj;
    struct wl_touch * wl_touch;

#if LV_USE_GESTURE_RECOGNITION
    lv_indev_touch_data_t touches[10];
    uint8_t touch_event_cnt;
    uint8_t primary_id;
#else
    lv_point_t point;
    lv_indev_state_t state;
#endif /*LV_USE_GESTURE_RECOGNITION*/
} lv_wl_seat_touch_t;

typedef struct {
    struct wl_keyboard * wl_keyboard;
    struct xkb_keymap * xkb_keymap;
    struct xkb_state * xkb_state;

    lv_key_t key;
    lv_indev_state_t state;
} lv_wl_seat_keyboard_t;


typedef struct {
    struct wl_seat * wl_seat;

    lv_wl_seat_pointer_t * pointer;
    lv_wl_seat_touch_t * touch;
    lv_wl_seat_keyboard_t * keyboard;
} lv_wl_seat_t;

struct graphic_object {
    struct window * window;
    struct wl_surface * surface;
    struct wl_subsurface * subsurface;
    enum object_type type;
    int width;
    int height;
};

typedef struct {
    struct buffer * buffers;
    struct zwp_linux_dmabuf_v1 * handler;
    uint32_t format;
    uint8_t last_used;
} dmabuf_ctx_t;

typedef struct {
    struct wl_shm * handler;
    uint32_t format;
} shm_ctx_t;

struct output_info {
    struct wl_output * wl_output;
    char name[64];
    int width;
    int height;
    int refresh;
    int scale;
    int flags;
};

struct lv_wayland_context {
    struct wl_display * compositor_connection;
    struct wl_registry * registry;
    struct wl_compositor * compositor;
    struct wl_shm * wl_shm;
    lv_wl_seat_t seat;

    void * backend_data;
    shm_ctx_t shm_ctx;
    struct output_info outputs[LV_WAYLAND_MAX_OUTPUTS];
    uint8_t wl_output_count;

#if LV_WAYLAND_USE_DMABUF
    dmabuf_ctx_t dmabuf_ctx;
#endif

    struct xdg_wm_base * xdg_wm;

#ifdef LV_WAYLAND_WINDOW_DECORATIONS
    /* TODO: Move this to window*/
    bool opt_disable_decorations;
#endif

    lv_ll_t window_ll;
    lv_timer_t * cycle_timer;

    struct pollfd wayland_pfd;
    bool cursor_flush_pending;
};

struct window {
    lv_display_t * lv_disp;
    lv_indev_t * lv_indev_pointer;
    lv_indev_t * lv_indev_pointeraxis;
    lv_indev_t * lv_indev_touch;
    lv_indev_t * lv_indev_keyboard;

    lv_draw_buf_t * lv_draw_buf;

    lv_wayland_display_close_f_t close_cb;
    struct lv_wayland_context * wl_ctx;

    /* The current physical assigned output */
    struct wl_output * assigned_output;
    struct xdg_surface * xdg_surface;
    struct xdg_toplevel * xdg_toplevel;
    uint32_t wm_capabilities;

    struct graphic_object * body;
    struct {
        lv_area_t cache[DMG_CACHE_CAPACITY];
        unsigned char start;
        unsigned char end;
        unsigned size;
    } dmg_cache;

#if LV_WAYLAND_WINDOW_DECORATIONS
    struct graphic_object * decoration[NUM_DECORATIONS];
#endif

    int width;
    int height;

    bool resize_pending;
    int resize_width;
    int resize_height;

    bool flush_pending;
    bool shall_close;
    bool closed;
    bool maximized;
    bool fullscreen;
    uint32_t frame_counter;
    bool is_window_configured;

#if LV_WAYLAND_USE_DMABUF
    /* XDG/DMABUF synchronization fields */
    bool dmabuf_resize_pending;
    bool surface_configured;
    bool configure_acknowledged;
    uint32_t configure_serial;
#if LV_WAYLAND_WINDOW_DECORATIONS
    struct buffer * decorators_buf[8];
#endif
#endif
};

#if LV_WAYLAND_USE_DMABUF
struct buffer {
    int busy;
    struct window * window;
    int plane_count;
    int dmabuf_fds[MAX_BUFFER_PLANES];
    uint32_t strides[MAX_BUFFER_PLANES];
    uint32_t offsets[MAX_BUFFER_PLANES];
    struct wl_buffer * buffer;
    uint32_t width;
    uint32_t height;
    void * buf_base[MAX_BUFFER_PLANES];
    lv_draw_buf_t * lv_draw_buf;
};
#endif

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

extern struct lv_wayland_context lv_wl_ctx;

/**********************
 *      Driver
 **********************/

void lv_wayland_init(void);
void lv_wayland_deinit(void);
void lv_wayland_wait_flush_cb(lv_display_t * disp);
void lv_wayland_read_input_events(void);
void lv_wayland_update_window(struct window * window);

/**********************
 *      Window
 **********************/

const struct wl_callback_listener * lv_wayland_window_get_wl_surface_frame_listener(void);

void lv_wayland_window_draw(struct window * window, uint32_t width, uint32_t height);
lv_result_t lv_wayland_window_resize(struct window * window, int width, int height);
void lv_wayland_window_destroy(struct window * window);
#if LV_WAYLAND_WINDOW_DECORATIONS
uint32_t lv_wayland_window_decoration_create_all(struct window * window);
void lv_wayland_window_decoration_detach_all(struct window * window);
bool lv_wayland_window_decoration_create(struct window * window, struct graphic_object * decoration, int window_width,
                                         int window_height);
bool lv_wayland_window_decoration_attach(struct window * window, struct graphic_object * decoration,
                                         void * decoration_buffer, struct graphic_object * parent);
void lv_wayland_window_decoration_detach(struct window * window, struct graphic_object * decoration);
#endif

/**********************
 *  Window Management
 **********************/

const struct xdg_surface_listener * lv_wayland_xdg_shell_get_surface_listener(void);
const struct xdg_toplevel_listener * lv_wayland_xdg_shell_get_toplevel_listener(void);
const struct xdg_wm_base_listener * lv_wayland_xdg_shell_get_wm_base_listener(void);
lv_result_t lv_wayland_xdg_shell_set_maximized(struct window * window, bool maximized);
lv_result_t lv_wayland_xdg_shell_set_minimized(struct window * window);
lv_result_t lv_wayland_xdg_shell_set_fullscreen(struct window * window, bool fullscreen, struct wl_output * output);
#if LV_WAYLAND_USE_DMABUF
void lv_wayland_xdg_shell_ack_configure(struct window * window, uint32_t serial);
#endif
lv_result_t lv_wayland_xdg_shell_create_window(struct lv_wayland_context * app, struct window * window,
                                               const char * title);
void lv_wayland_xdg_shell_configure_surface(struct window * window);
lv_result_t lv_wayland_xdg_shell_destroy_window_toplevel(struct window * window);
lv_result_t lv_wayland_xdg_shell_destroy_window_surface(struct window * window);

void lv_wayland_xdg_shell_handle_pointer_event(const lv_wl_seat_pointer_t * seat_pointer, uint32_t serial,
                                               uint32_t button, uint32_t state);

const char * lv_wayland_xdg_shell_get_cursor_name(const lv_wl_seat_pointer_t * seat_pointer);
void lv_wayland_xdg_shell_deinit(void);

/**********************
 *      SHM
 **********************/

struct wl_shm * lv_wayland_get_shm(void);

void lv_wayland_shm_set_interface(shm_ctx_t * context, struct wl_registry * registry, uint32_t name,
                                  const char * interface, uint32_t version);

struct graphic_object * lv_wayland_shm_on_graphical_object_creation(shm_ctx_t * context, struct graphic_object * obj);
void lv_wayland_shm_on_graphical_object_destruction(shm_ctx_t * context, struct graphic_object * obj);
lv_result_t lv_wayland_shm_set_draw_buffers(shm_ctx_t * context, lv_display_t * display, struct window * window);
lv_result_t lv_wayland_shm_create_draw_buffers(shm_ctx_t * context, struct window * window);
lv_result_t lv_wayland_shm_resize_window(shm_ctx_t * context, struct window * window, int32_t width, int32_t height);
lv_result_t lv_wayland_shm_is_ready(shm_ctx_t * context);

void lv_wayland_shm_delete_draw_buffers(shm_ctx_t * context, struct window * window);
void lv_wayland_shm_initalize_context(shm_ctx_t * context);
void lv_wayland_shm_deinit(shm_ctx_t * context);
void lv_wayland_shm_flush_partial_mode(lv_display_t * disp, const lv_area_t * area, unsigned char * color_p);

struct wl_cursor_theme * lv_wayland_shm_load_cursor_theme(shm_ctx_t * context);

/**********************
 *      DMABUF
 **********************/

void lv_wayland_dmabuf_set_interface(dmabuf_ctx_t * context, struct wl_registry * registry, uint32_t name,
                                     const char * interface, uint32_t version);

struct graphic_object * lv_wayland_dmabuf_on_graphical_object_creation(dmabuf_ctx_t * context,
                                                                       struct graphic_object * obj);

void lv_wayland_dmabuf_on_graphical_object_destruction(dmabuf_ctx_t * context, struct graphic_object * obj);
lv_result_t lv_wayland_dmabuf_set_draw_buffers(dmabuf_ctx_t * context, lv_display_t * display);
lv_result_t lv_wayland_dmabuf_create_draw_buffers(dmabuf_ctx_t * context, struct window * window);
lv_result_t lv_wayland_dmabuf_resize_window(dmabuf_ctx_t * context, struct window * window, int width, int height);
lv_result_t lv_wayland_dmabuf_is_ready(dmabuf_ctx_t * context);
void destroy_decorators_buf(struct window * window, struct graphic_object * decoration);
void lv_wayland_dmabuf_destroy_draw_buffers(dmabuf_ctx_t * context, struct window * window);
void lv_wayland_dmabuf_initalize_context(dmabuf_ctx_t * context);
void lv_wayland_dmabuf_deinit(dmabuf_ctx_t * context);
void lv_wayland_dmabuf_flush_full_mode(lv_display_t * disp, const lv_area_t * area, unsigned char * color_p);

#if LV_WAYLAND_WINDOW_DECORATIONS
struct buffer * dmabuf_acquire_pool_buffer(struct window * window, struct graphic_object * decoration);
#endif
/**********************
 *      SME
 **********************/

const struct smm_events * lv_wayland_sme_get_events(void);

/**********************
 *      Input
 **********************/

void lv_wayland_seat_init(lv_wl_seat_t * seat, struct wl_registry * registry, uint32_t name, uint32_t version);
void lv_wayland_seat_deinit(lv_wl_seat_t * seat);

const struct wl_keyboard_listener * lv_wayland_keyboard_get_listener(void);
const struct wl_pointer_listener * lv_wayland_pointer_get_listener(void);
const struct wl_touch_listener * lv_wayland_touch_get_listener(void);

lv_wl_seat_pointer_t * lv_wayland_seat_pointer_create(struct wl_seat * seat, struct wl_surface * surface);
void lv_wayland_seat_pointer_delete(lv_wl_seat_pointer_t * seat_pointer);

lv_wl_seat_touch_t * lv_wayland_seat_touch_create(struct wl_seat * seat);
void lv_wayland_seat_touch_delete(lv_wl_seat_touch_t * seat_touch);

lv_wl_seat_keyboard_t * lv_wayland_seat_keyboard_create(struct wl_seat * seat);
void lv_wayland_seat_keyboard_delete(lv_wl_seat_keyboard_t * seat_keyboard);

/* Updates indev's driver data with the given 'read_cb' to 'new_driver_data' */
void lv_wayland_update_indevs(lv_indev_read_cb_t read_cb, void * new_driver_data);

#endif /* LV_USE_WAYLAND */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_WAYLAND_PRIVATE_H*/
