/**
 * @file lv_wl_shm_backend.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_wayland_private.h"
#include <src/display/lv_display.h>
#include <src/lv_conf_internal.h>
#include <src/misc/lv_area.h>
#include <src/misc/lv_types.h>

#if LV_USE_WAYLAND
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    struct wl_shm * shm;
} lv_wl_shm_ctx_t;

typedef struct {
    int fd;
    void * mmap_ptr;
    size_t mmap_size;
    struct wl_shm_pool * pool;
    struct wl_buffer * wl_buffers[LV_WAYLAND_BUF_COUNT];
    size_t curr_wl_buffer_idx;
} lv_wl_shm_display_data_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void * shm_init(void);
static void shm_deinit(void *);
static lv_result_t shm_init_display(void * backend_data, lv_display_t * display, int32_t width, int32_t height);
static lv_result_t shm_resize_display(void * backend_data, lv_display_t * display);
static void shm_deinit_display(void * backend_data, lv_display_t * display);
static void shm_global_handler(void * backend_data, struct wl_registry * registry, uint32_t name,
                               const char * interface, uint32_t version);
static void shm_buffer_release(void * data, struct wl_buffer * wl_buffer);

static int create_shm_file(size_t size);
static void shm_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
static lv_wl_shm_display_data_t * shm_create_display_data(lv_wl_shm_ctx_t * ctx, lv_display_t * display, int32_t width,
                                                          int32_t height);

/**********************
 *  STATIC VARIABLES
 **********************/

static lv_wl_shm_ctx_t shm_ctx;

static const struct wl_buffer_listener shm_buffer_listener = {
    .release = shm_buffer_release,
};

const lv_wayland_backend_ops_t wl_backend_ops = {
    .init = shm_init,
    .deinit = shm_deinit,
    .global_handler = shm_global_handler,
    .init_display = shm_init_display,
    .deinit_display = shm_deinit_display,
    .resize_display = shm_resize_display,
};


static void shm_buffer_release(void * data, struct wl_buffer * wl_buffer)
{
    LV_UNUSED(wl_buffer);
    lv_display_flush_ready(data);
}

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**********************
 *   STATIC FUNCTIONS
 **********************/

static uint32_t lv_cf_to_shm_cf(lv_color_format_t cf)
{
    switch(cf) {
        case LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED:
        case LV_COLOR_FORMAT_ARGB8888:
            return WL_SHM_FORMAT_ARGB8888;
        case LV_COLOR_FORMAT_XRGB8888:
            return WL_SHM_FORMAT_XRGB8888;
        case LV_COLOR_FORMAT_RGB565:
            return WL_SHM_FORMAT_RGB565;
        default:
            LV_ASSERT_FORMAT_MSG(0, "Unsupported color format %d", cf);
    }
}

static void * shm_init(void)
{
    lv_memzero(&shm_ctx, sizeof(shm_ctx));
    return &shm_ctx;
}

static void shm_deinit(void * backend_data)
{
    LV_UNUSED(backend_data);
}

static lv_wl_shm_display_data_t * shm_create_display_data(lv_wl_shm_ctx_t * ctx, lv_display_t * display, int32_t width,
                                                          int32_t height)
{
    lv_wl_shm_display_data_t * display_data = lv_zalloc(sizeof(*display_data));
    if(!display_data) {
        LV_LOG_ERROR("Failed to allocate data for display");
        return NULL;
    }

    const lv_color_format_t cf = lv_display_get_color_format(display);
    const uint32_t stride = lv_draw_buf_width_to_stride(width, cf);
    const size_t buf_size = stride * height;
    display_data->mmap_size = buf_size * LV_WAYLAND_BUF_COUNT;

    display_data->fd = create_shm_file(display_data->mmap_size);
    if(display_data->fd < 0) {
        LV_LOG_ERROR("Failed to create shm file");
        goto shm_file_err;
    }
    display_data->mmap_ptr = mmap(NULL, display_data->mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, display_data->fd, 0);
    if(display_data->mmap_ptr == MAP_FAILED) {
        LV_LOG_ERROR("Failed to map shm file: %s", strerror(errno));
        goto mmap_err;
    }
    display_data->pool = wl_shm_create_pool(ctx->shm, display_data->fd, display_data->mmap_size);
    if(!display_data->pool) {
        LV_LOG_ERROR("Failed to create wl_shm_pool");
        goto shm_pool_err;
    }
    uint32_t shm_cf = lv_cf_to_shm_cf(cf);

    for(size_t i = 0; i < LV_WAYLAND_BUF_COUNT; ++i) {
        size_t offset = i * buf_size;
        display_data->wl_buffers[i] = wl_shm_pool_create_buffer(display_data->pool, offset, width, height, stride, shm_cf);
        if(!display_data->wl_buffers[i]) {
            LV_LOG_ERROR("Failed to create wl_buffer %zu", i);
            goto pool_buffer_err;
        }
        wl_buffer_add_listener(display_data->wl_buffers[i], &shm_buffer_listener, display);
    }

pool_buffer_err:
    wl_shm_pool_destroy(display_data->pool);
shm_pool_err:
    munmap(display_data->mmap_ptr, display_data->mmap_size);
mmap_err:
    close(display_data->fd);
shm_file_err:
    lv_free(display_data);
    return NULL;
}

static void shm_destroy_display_data(lv_wl_shm_display_data_t * ddata)
{

    for(size_t i = 0; i < LV_WAYLAND_BUF_COUNT; ++i) {
        if(ddata->wl_buffers[i]) {
            wl_buffer_destroy(ddata->wl_buffers[i]);
            ddata->wl_buffers[i] = NULL;
        }
    }

    if(ddata->pool) {
        wl_shm_pool_destroy(ddata->pool);
        ddata->pool = NULL;
    }

    if(ddata->mmap_ptr != MAP_FAILED) {
        munmap(ddata->mmap_ptr, ddata->mmap_size);
        ddata->mmap_ptr = MAP_FAILED;
    }

    if(ddata->fd >= 0) {
        close(ddata->fd);
        ddata->fd = -1;
    }

    lv_free(ddata);
}

static lv_result_t shm_init_display(void * backend_data, lv_display_t * display, int32_t width, int32_t height)
{
    lv_wl_shm_ctx_t * ctx = (lv_wl_shm_ctx_t *)backend_data;
    if(!ctx->shm) {
        LV_LOG_ERROR("wl_shm not available");
        return LV_RESULT_INVALID;
    }


    lv_wl_shm_display_data_t * ddata = shm_create_display_data(ctx, display, width, height);
    if(!ddata) {
        LV_LOG_ERROR("Failed to allocate data for display");
        return LV_RESULT_INVALID;
    }
    size_t buf_size = ddata->mmap_size / LV_WAYLAND_BUF_COUNT;

    if(LV_WAYLAND_BUF_COUNT == 1) {
        lv_display_set_buffers(display, ddata->mmap_ptr, NULL, buf_size,
                               LV_DISPLAY_RENDER_MODE_DIRECT);
    }
    else {
        lv_display_set_buffers(display, ddata->mmap_ptr, (uint8_t *)ddata->mmap_ptr + buf_size,
                               buf_size, LV_DISPLAY_RENDER_MODE_DIRECT);
    }
    lv_display_set_flush_cb(display, shm_flush_cb);

    return LV_RESULT_OK;
}

static lv_result_t shm_resize_display(void * backend_data, lv_display_t * display)
{
    lv_wl_shm_ctx_t * ctx = (lv_wl_shm_ctx_t *)backend_data;

    const int32_t new_width = lv_display_get_horizontal_resolution(display);
    const int32_t new_height = lv_display_get_vertical_resolution(display);

    lv_wl_shm_display_data_t * ddata = shm_create_display_data(ctx, display, new_width, new_height);

    if(!ddata) {
        LV_LOG_ERROR("Failed to allocate data for new display resolution");
        return LV_RESULT_INVALID;
    }

    size_t buf_size = ddata->mmap_size / LV_WAYLAND_BUF_COUNT;
    if(LV_WAYLAND_BUF_COUNT == 1) {
        lv_display_set_buffers(display, ddata->mmap_ptr, NULL, buf_size,
                               LV_DISPLAY_RENDER_MODE_DIRECT);
    }
    else {
        lv_display_set_buffers(display, ddata->mmap_ptr, (uint8_t *)ddata->mmap_ptr + buf_size,
                               buf_size, LV_DISPLAY_RENDER_MODE_DIRECT);
    }

    lv_wl_shm_display_data_t * curr_ddata = lv_display_get_driver_data(display);
    shm_destroy_display_data(curr_ddata);

    lv_display_set_driver_data(display, ddata);

    return LV_RESULT_INVALID;
}

static void shm_deinit_display(void * backend_data, lv_display_t * display)
{
    LV_UNUSED(backend_data);
    lv_wl_shm_display_data_t * ddata = lv_display_get_driver_data(display);
    if(!ddata) {
        return;
    }
    shm_destroy_display_data(ddata);
    lv_display_set_driver_data(display, NULL);
}

static int create_shm_file(size_t size)
{
    int fd = -1;
    char name[255];
    snprintf(name, sizeof(name), "/lvgl-wayland-%d-%ld", getpid(), (long)lv_tick_get());

    fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if(fd < 0) {
        LV_LOG_ERROR("shm_open failed: %s", strerror(errno));
        return -1;
    }

    shm_unlink(name);

    if(ftruncate(fd, size) < 0) {
        LV_LOG_ERROR("ftruncate failed: %s", strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

static void shm_global_handler(void * backend_data, struct wl_registry * registry, uint32_t name,
                               const char * interface, uint32_t version)
{
    LV_UNUSED(version);
    lv_wl_shm_ctx_t * ctx = (lv_wl_shm_ctx_t *)backend_data;

    if(lv_streq(interface, wl_shm_interface.name)) {
        LV_LOG_INFO("Bound to wl_shm");
        ctx->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
        LV_LOG_USER("SHM is\n", ctx->shm);
    }
}

static void shm_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    LV_UNUSED(px_map);
    lv_wl_shm_display_data_t * ddata = lv_display_get_driver_data(disp);
    struct wl_surface * surface = lv_wayland_get_drawing_surface();
    if(!surface) {
        lv_display_flush_ready(disp);
        return;
    }
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    wl_surface_damage(surface, area->x1, area->y1, w, h);

    if(!lv_display_flush_is_last(disp)) {
        lv_display_flush_ready(disp);
        return;
    }

    struct wl_buffer * wl_buf = ddata->wl_buffers[ddata->curr_wl_buffer_idx];
    wl_surface_attach(surface, wl_buf, 0, 0);
    wl_surface_commit(surface);
    ddata->curr_wl_buffer_idx = (ddata->curr_wl_buffer_idx + 1) % LV_WAYLAND_BUF_COUNT;
}

#endif /*LV_USE_WAYLAND*/
