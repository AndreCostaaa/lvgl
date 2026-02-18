#include "../../lv_conf_internal.h"

#if LV_USE_EGL && defined(__EMSCRIPTEN__)

#include <emscripten.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "lv_opengles_egl_private.h"

EGLDisplay lv_opengles_egl_create_emscripten_egl_display(lv_opengles_egl_t * ctx)
{
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if(display == EGL_NO_DISPLAY) {
        LV_LOG_ERROR("Failed to get EGL display. Error: %#x", eglGetError());
        return NULL;
    }

    EGLint egl_major, egl_minor;
    if(!eglInitialize(display, &egl_major, &egl_minor)) {
        LV_LOG_ERROR("Failed to initialize EGL. Error: %#x", eglGetError());
        return NULL;
    }

    LV_LOG_INFO("EGL version %d.%d", egl_major, egl_minor);
    return display;
}

void lv_opengles_egl_emscripten_load_functions(
    PFNEGLGETPROCADDRESSPROC * egl_get_proc_address,
    PFNEGLGETDISPLAYPROC * egl_get_display,
    PFNEGLGETCURRENTDISPLAYPROC * egl_get_current_display,
    PFNEGLQUERYSTRINGPROC * egl_query_string,
    PFNEGLGETERRORPROC * egl_get_error
)
{
    *egl_get_proc_address = eglGetProcAddress;
    *egl_get_display = eglGetDisplay;
    *egl_get_current_display = eglGetCurrentDisplay;
    *egl_query_string = eglQueryString;
    *egl_get_error = eglGetError;
}

#endif /*LV_USE_EGL && defined(__EMSCRIPTEN__)*/
