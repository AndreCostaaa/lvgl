/**
 * @file lv_wl_shm_backend.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_wayland_private.h"
#include <src/display/lv_display.h>

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
} lv_wl_shm_display_data_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void * shm_init(void);
static void shm_deinit(void *);
static lv_result_t shm_init_display(void * backend_data, lv_display_t * display, int32_t width, int32_t height);
static lv_result_t shm_resize_display(void * backend_data, lv_display_t * display, int32_t width, int32_t height);
static lv_result_t shm_deinit_display(void * backend_data, lv_display_t * display);
static void shm_global_handler(void * backend_data, struct wl_registry * registry, uint32_t name,
                               const char * interface, uint32_t version);
static void shm_buffer_release(void * data, struct wl_buffer * wl_buffer);

static int create_shm_file(size_t size);

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
    lv_wl_shm_ctx_t * ctx = backend_data;
    if(ctx->shm) {
        wl_shm_destroy(ctx->shm);
        ctx->shm = NULL;
    }
}

static lv_result_t shm_init_display(void * backend_data, lv_display_t * display, int32_t width, int32_t height)
{
    lv_wl_shm_ctx_t * ctx = (lv_wl_shm_ctx_t *)backend_data;
    if(!ctx->shm) {
        LV_LOG_ERROR("wl_shm not available");
        return LV_RESULT_INVALID;
    }

    lv_wl_shm_display_data_t * display_data = lv_zalloc(sizeof(*display_data));
    if(!display_data) {
        LV_LOG_ERROR("Failed to allocate data for display");
        return LV_RESULT_INVALID;
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
        wl_buffer_add_listener(display_data->wl_buffers[i], &shm_buffer_listener, display_data);
    }

    if(LV_WAYLAND_BUF_COUNT == 1) {
        lv_display_set_buffers(display, display_data->mmap_ptr, NULL, buf_size,
                               LV_DISPLAY_RENDER_MODE_DIRECT);
    }
    else {
        lv_display_set_buffers(display, display_data->mmap_ptr, (uint8_t *)display_data->mmap_ptr + buf_size,
                               buf_size, LV_DISPLAY_RENDER_MODE_DIRECT);
    }

    return LV_RESULT_OK;

pool_buffer_err:
    wl_shm_pool_destroy(display_data->pool);
shm_pool_err:
    munmap(display_data->mmap_ptr, display_data->mmap_size);
mmap_err:
    close(display_data->fd);
shm_file_err:
    lv_free(display_data);
    return LV_RESULT_INVALID;
}

static lv_result_t shm_resize_display(void * backend_data, lv_display_t * display, int32_t width, int32_t height)
{

    return LV_RESULT_INVALID;
}

static lv_result_t shm_deinit_display(void * backend_data, lv_display_t * display)
{

    return LV_RESULT_INVALID;

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
        LV_LOG_INFO("bound to wl_shm");
        ctx->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    }
}

#endif /*LV_USE_WAYLAND*/
