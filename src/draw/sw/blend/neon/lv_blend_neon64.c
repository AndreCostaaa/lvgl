/**
 * @file lv_blend_neon64.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_blend_neon64.h"

#if LV_USE_DRAW_SW_ASM == LV_DRAW_SW_ASM_NEON64

#include <arm_neon.h>

#include "../lv_draw_sw_blend_private.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_result_t lv_color_blend_to_rgb565_neon64(lv_draw_sw_blend_fill_dsc_t * dsc)
{
    const int32_t w           = dsc->dest_w;
    const int32_t h           = dsc->dest_h;
    const uint16_t color16    = lv_color_to_u16(dsc->color);
    const lv_opa_t opa        = dsc->opa;
    const lv_opa_t * mask     = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    uint16_t * dest_buf_u16   = dsc->dest_buf;
    const int32_t dest_stride = dsc->dest_stride;

    const int32_t x;
    const int32_t y;
    const uint16_t * dest_end_final = dest_buf_u16 + w;
    const uint32_t * dest_end_mid   = (uint32_t *)((uint16_t *)dest_buf_u16 + ((w - 1) & ~(0xF)));

    const uint16x8_t color_vec = vdupq_n_u16(color16);
    for(int32_t y = 0; y < h; y++) {
        uint16_t * row_ptr  = dest_buf_u16;
        int32_t x           = 0;
        const size_t offset = ((size_t)row_ptr) & 0xF;
        if(offset != 0) {
            int32_t pixel_alignment = (16 - offset) >> 1;
            pixel_alignment         = (pixel_alignment > w) ? w : pixel_alignment;
            for(; x < pixel_alignment; x++) {
                row_ptr[x] = color16;
            }
        }

        for(; x < w - 15; x += 16) {
            vst1q_u16(&row_ptr[x], color_vec);
            vst1q_u16(&row_ptr[x + 8], color_vec);
        }

        for(; x < w; x++) {
            row_ptr[x] = color16;
        }

        dest_buf_u16 += dest_stride;
    }
}
lv_result_t lv_color_blend_to_argb8888_neon64(lv_draw_sw_blend_fill_dsc_t * dsc)
{
    const int32_t w             = dsc->dest_w;
    const int32_t h             = dsc->dest_h;
    const uint32_t color32      = lv_color_to_u32(dsc->color);
    const int32_t dest_stride   = dsc->dest_stride;
    const uint32_t n            = w - (w % 16);
    const uint32_t n2           = w - (w % 8);
    const uint32x4_t color_vec4 = vdupq_n_u32(color32);
    const uint32x2_t color_vec2 = vdup_n_u32(color32);
    uint32_t * dest_buf         = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        int32_t x;
        for(x = 0; x < n; x += 16) {
            vst1q_u32(&dest_buf[x + 0], color_vec4);
            vst1q_u32(&dest_buf[x + 4], color_vec4);
            vst1q_u32(&dest_buf[x + 8], color_vec4);
            vst1q_u32(&dest_buf[x + 12], color_vec4);
        }
        for(; x < n2; x += 8) {
            vst1_u32(&dest_buf[x + 0], color_vec2);
            vst1_u32(&dest_buf[x + 4], color_vec2);
        }
        for(; x < w; x++) {
            dest_buf[x] = color32;
        }
        dest_buf += dest_stride;
    }
    return LV_RESULT_OK;
}

lv_result_t lv_color_blend_to_argb8888_opa_neon64(lv_draw_sw_blend_fill_dsc_t * dsc)
{

    const lv_color32_t color_argb = lv_color_to_32(dsc->color, dsc->opa);
    const int32_t w               = dsc->dest_w;
    const int32_t h               = dsc->dest_h;
    const uint32_t color32        = lv_color_to_u32(dsc->color);
    const int32_t dest_stride     = dsc->dest_stride;
    const uint32_t n              = w - (w % 16);
    const uint32_t n2             = w - (w % 8);
    const uint32x4_t color_vec4   = vdupq_n_u32(color32);
    const uint32x2_t color_vec2   = vdup_n_u32(color32);
    lv_color32_t * dest_buf       = dsc->dest_buf;

    for(uint32_t y = 0; y < h; y++) {
        for(uint32_t x = 0; x < w; x++) {
            /*dest_buf[x] = lv_color_32_32_mix(color_argb, dest_buf[x]);*/
        }

        dest_buf += dest_stride;
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /* LV_USE_DRAW_SW_ASM == LV_DRAW_SW_ASM_NEON64 */
