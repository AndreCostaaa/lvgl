/**
 * @file lv_neon64_blend.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "lv_neon64_blend.h"

#if LV_USE_DRAW_SW_ASM == LV_DRAW_SW_ASM_NEON64

#include <arm_neon.h>
#include "../lv_draw_sw_blend_private.h"

/*********************
 *      DEFINES
 *********************/

#define NEON64_PARALLEL_8   8
#define NEON64_PARALLEL_16  16

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static inline uint16x8_t neon64_rgb565_from_rgb888(uint8x8x3_t rgb888);
static inline uint8x8x3_t neon64_rgb888_from_rgb565(uint16x8_t rgb565);
static inline uint32x4_t neon64_argb8888_from_rgb565(uint16x4_t rgb565);
static inline uint16x4_t neon64_rgb565_from_argb8888(uint32x4_t argb8888);
static inline uint16x8_t neon64_alpha_blend_rgb565(uint16x8_t src, uint16x8_t dst, uint8x8_t alpha);
static inline uint32x4_t neon64_alpha_blend_argb8888(uint32x4_t src, uint32x4_t dst, uint8x8_t alpha);
static inline lv_color32_t lv_color_32_32_mix_neon(lv_color32_t fg, lv_color32_t bg, lv_opa_t opa);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/* Color fill functions - RGB565 destination */

lv_result_t lv_color_blend_to_rgb565_neon64(lv_draw_sw_blend_fill_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint16_t color16 = lv_color_to_u16(dsc->color);
    const int32_t dest_stride = dsc->dest_stride;
    uint16_t * dest_buf_u16 = dsc->dest_buf;

    const uint16x8_t color_vec = vdupq_n_u16(color16);

    for(int32_t y = 0; y < h; y++) {
        uint16_t * row_ptr = dest_buf_u16;
        int32_t x = 0;

        /* Handle unaligned pixels at the beginning */
        const size_t offset = ((size_t)row_ptr) & 0xF;
        if(offset != 0) {
            int32_t pixel_alignment = (16 - offset) >> 1;
            pixel_alignment = (pixel_alignment > w) ? w : pixel_alignment;
            for(; x < pixel_alignment; x++) {
                row_ptr[x] = color16;
            }
        }

        /* Process 16 pixels at a time using NEON */
        for(; x < w - 15; x += 16) {
            vst1q_u16(&row_ptr[x], color_vec);
            vst1q_u16(&row_ptr[x + 8], color_vec);
        }

        /* Process 8 pixels at a time */
        for(; x < w - 7; x += 8) {
            vst1q_u16(&row_ptr[x], color_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            row_ptr[x] = color16;
        }

        dest_buf_u16 += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_color_blend_to_rgb565_with_opa_neon64(lv_draw_sw_blend_fill_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint16_t color16 = lv_color_to_u16(dsc->color);
    const lv_opa_t opa = dsc->opa;
    const int32_t dest_stride = dsc->dest_stride;
    uint16_t * dest_buf_u16 = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;
    if(opa >= LV_OPA_MAX) return lv_color_blend_to_rgb565_neon64(dsc);

    const uint16x8_t color_vec = vdupq_n_u16(color16);
    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        uint16_t * row_ptr = dest_buf_u16;
        int32_t x = 0;

        /* Process 8 pixels at a time using NEON */
        for(; x < w - 7; x += 8) {
            uint16x8_t dest_vec = vld1q_u16(&row_ptr[x]);
            uint16x8_t result_vec = neon64_alpha_blend_rgb565(color_vec, dest_vec, opa_vec);
            vst1q_u16(&row_ptr[x], result_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            row_ptr[x] = lv_color_16_16_mix(color16, row_ptr[x], opa);
        }

        dest_buf_u16 += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_color_blend_to_rgb565_with_mask_neon64(lv_draw_sw_blend_fill_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint16_t color16 = lv_color_to_u16(dsc->color);
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    const int32_t dest_stride = dsc->dest_stride;
    uint16_t * dest_buf_u16 = dsc->dest_buf;

    const uint16x8_t color_vec = vdupq_n_u16(color16);

    for(int32_t y = 0; y < h; y++) {
        uint16_t * row_ptr = dest_buf_u16;
        const lv_opa_t * mask_ptr = mask;
        int32_t x = 0;

        /* Process 8 pixels at a time using NEON */
        for(; x < w - 7; x += 8) {
            uint16x8_t dest_vec = vld1q_u16(&row_ptr[x]);
            uint8x8_t mask_vec = vld1_u8(&mask_ptr[x]);
            uint16x8_t result_vec = neon64_alpha_blend_rgb565(color_vec, dest_vec, mask_vec);
            vst1q_u16(&row_ptr[x], result_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            if(mask_ptr[x] > LV_OPA_MIN) {
                row_ptr[x] = lv_color_16_16_mix(color16, row_ptr[x], mask_ptr[x]);
            }
        }

        dest_buf_u16 += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_color_blend_to_rgb565_mix_mask_opa_neon64(lv_draw_sw_blend_fill_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint16_t color16 = lv_color_to_u16(dsc->color);
    const lv_opa_t opa = dsc->opa;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    const int32_t dest_stride = dsc->dest_stride;
    uint16_t * dest_buf_u16 = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;

    const uint16x8_t color_vec = vdupq_n_u16(color16);
    const uint16x8_t opa_vec = vdupq_n_u16(opa);

    for(int32_t y = 0; y < h; y++) {
        uint16_t * row_ptr = dest_buf_u16;
        const lv_opa_t * mask_ptr = mask;
        int32_t x = 0;

        /* Process 8 pixels at a time using NEON */
        for(; x < w - 7; x += 8) {
            uint16x8_t dest_vec = vld1q_u16(&row_ptr[x]);
            uint8x8_t mask_vec = vld1_u8(&mask_ptr[x]);
            
            /* Mix mask with opacity */
            uint16x8_t mask16_vec = vmovl_u8(mask_vec);
            uint16x8_t combined_alpha = vmulq_u16(mask16_vec, opa_vec);
            combined_alpha = vshrq_n_u16(combined_alpha, 8);
            uint8x8_t final_alpha = vmovn_u16(combined_alpha);
            
            uint16x8_t result_vec = neon64_alpha_blend_rgb565(color_vec, dest_vec, final_alpha);
            vst1q_u16(&row_ptr[x], result_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t final_opa = LV_OPA_MIX2(mask_ptr[x], opa);
            if(final_opa > LV_OPA_MIN) {
                row_ptr[x] = lv_color_16_16_mix(color16, row_ptr[x], final_opa);
            }
        }

        dest_buf_u16 += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

/* Color fill functions - ARGB8888 destination */

lv_result_t lv_color_blend_to_argb8888_neon64(lv_draw_sw_blend_fill_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint32_t color32 = lv_color_to_u32(dsc->color);
    const int32_t dest_stride = dsc->dest_stride;
    uint32_t * dest_buf = dsc->dest_buf;

    const uint32x4_t color_vec = vdupq_n_u32(color32);

    for(int32_t y = 0; y < h; y++) {
        uint32_t * row_ptr = dest_buf;
        int32_t x = 0;

        /* Process 16 pixels at a time using NEON */
        for(; x < w - 15; x += 16) {
            vst1q_u32(&row_ptr[x + 0], color_vec);
            vst1q_u32(&row_ptr[x + 4], color_vec);
            vst1q_u32(&row_ptr[x + 8], color_vec);
            vst1q_u32(&row_ptr[x + 12], color_vec);
        }

        /* Process 4 pixels at a time */
        for(; x < w - 3; x += 4) {
            vst1q_u32(&row_ptr[x], color_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            row_ptr[x] = color32;
        }

        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_color_blend_to_argb8888_with_opa_neon64(lv_draw_sw_blend_fill_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color32_t color_argb = lv_color_to_32(dsc->color, dsc->opa);
    const lv_opa_t opa = dsc->opa;
    const int32_t dest_stride = dsc->dest_stride;
    lv_color32_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;
    if(opa >= LV_OPA_MAX) return lv_color_blend_to_argb8888_neon64(dsc);

    const uint32_t color_u32 = (color_argb.alpha << 24) | (color_argb.red << 16) | (color_argb.green << 8) | color_argb.blue;
    const uint32x4_t color_vec = vdupq_n_u32(color_u32);
    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        uint32_t * row_ptr = (uint32_t *)dest_buf;
        int32_t x = 0;

        /* Process 4 pixels at a time using NEON */
        for(; x < w - 3; x += 4) {
            uint32x4_t dest_vec = vld1q_u32(&row_ptr[x]);
            uint32x4_t result_vec = neon64_alpha_blend_argb8888(color_vec, dest_vec, opa_vec);
            vst1q_u32(&row_ptr[x], result_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            dest_buf[x] = lv_color_32_32_mix_neon(color_argb, dest_buf[x], opa);
        }

        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_color_blend_to_argb8888_with_mask_neon64(lv_draw_sw_blend_fill_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color32_t color_argb = lv_color_to_32(dsc->color, 255);
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    const int32_t dest_stride = dsc->dest_stride;
    lv_color32_t * dest_buf = dsc->dest_buf;

    const uint32_t color_u32 = (color_argb.alpha << 24) | (color_argb.red << 16) | (color_argb.green << 8) | color_argb.blue;
    const uint32x4_t color_vec = vdupq_n_u32(color_u32);

    for(int32_t y = 0; y < h; y++) {
        uint32_t * row_ptr = (uint32_t *)dest_buf;
        const lv_opa_t * mask_ptr = mask;
        int32_t x = 0;

        /* Process 4 pixels at a time using NEON */
        for(; x < w - 3; x += 4) {
            uint32x4_t dest_vec = vld1q_u32(&row_ptr[x]);
            uint8x8_t mask_vec8 = vld1_u8(&mask_ptr[x]);
            uint32x4_t result_vec = neon64_alpha_blend_argb8888(color_vec, dest_vec, mask_vec8);
            vst1q_u32(&row_ptr[x], result_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            if(mask_ptr[x] > LV_OPA_MIN) {
                lv_color32_t color_with_mask = color_argb;
                color_with_mask.alpha = mask_ptr[x];
                dest_buf[x] = lv_color_32_32_mix_neon(color_with_mask, dest_buf[x], mask_ptr[x]);
            }
        }

        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_color_blend_to_argb8888_mix_mask_opa_neon64(lv_draw_sw_blend_fill_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color32_t color_argb = lv_color_to_32(dsc->color, 255);
    const lv_opa_t opa = dsc->opa;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    const int32_t dest_stride = dsc->dest_stride;
    lv_color32_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;

    const uint32_t color_u32 = (color_argb.alpha << 24) | (color_argb.red << 16) | (color_argb.green << 8) | color_argb.blue;
    const uint32x4_t color_vec = vdupq_n_u32(color_u32);
    const uint16x8_t opa_vec = vdupq_n_u16(opa);

    for(int32_t y = 0; y < h; y++) {
        uint32_t * row_ptr = (uint32_t *)dest_buf;
        const lv_opa_t * mask_ptr = mask;
        int32_t x = 0;

        /* Process 4 pixels at a time using NEON */
        for(; x < w - 3; x += 4) {
            uint32x4_t dest_vec = vld1q_u32(&row_ptr[x]);
            uint8x8_t mask_vec8 = vld1_u8(&mask_ptr[x]);
            
            /* Mix mask with opacity */
            uint16x8_t mask16_vec = vmovl_u8(mask_vec8);
            uint16x8_t combined_alpha = vmulq_u16(mask16_vec, opa_vec);
            combined_alpha = vshrq_n_u16(combined_alpha, 8);
            uint8x8_t final_alpha = vmovn_u16(combined_alpha);
            
            uint32x4_t result_vec = neon64_alpha_blend_argb8888(color_vec, dest_vec, final_alpha);
            vst1q_u32(&row_ptr[x], result_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t final_opa = LV_OPA_MIX2(mask_ptr[x], opa);
            if(final_opa > LV_OPA_MIN) {
                lv_color32_t color_with_alpha = color_argb;
                color_with_alpha.alpha = final_opa;
                dest_buf[x] = lv_color_32_32_mix_neon(color_with_alpha, dest_buf[x], final_opa);
            }
        }

        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

/* Image blending functions - RGB565 destination */

lv_result_t lv_rgb565_blend_normal_to_rgb565_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint16_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(uint16_t);
    const int32_t dest_stride = dsc->dest_stride / sizeof(uint16_t);
    uint16_t * dest_buf = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        const uint16_t * src_row = src_buf;
        uint16_t * dest_row = dest_buf;
        int32_t x = 0;

        /* Process 8 pixels at a time using NEON */
        for(; x < w - 7; x += 8) {
            uint16x8_t src_vec = vld1q_u16(&src_row[x]);
            vst1q_u16(&dest_row[x], src_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            dest_row[x] = src_row[x];
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb565_blend_normal_to_rgb565_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_opa_t opa = dsc->opa;
    const uint16_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(uint16_t);
    const int32_t dest_stride = dsc->dest_stride / sizeof(uint16_t);
    uint16_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;
    if(opa >= LV_OPA_MAX) return lv_rgb565_blend_normal_to_rgb565_neon64(dsc);

    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        const uint16_t * src_row = src_buf;
        uint16_t * dest_row = dest_buf;
        int32_t x = 0;

        /* Process 8 pixels at a time using NEON */
        for(; x < w - 7; x += 8) {
            uint16x8_t src_vec = vld1q_u16(&src_row[x]);
            uint16x8_t dest_vec = vld1q_u16(&dest_row[x]);
            uint16x8_t result_vec = neon64_alpha_blend_rgb565(src_vec, dest_vec, opa_vec);
            vst1q_u16(&dest_row[x], result_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            dest_row[x] = lv_color_16_16_mix(src_row[x], dest_row[x], opa);
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

/* Remaining optimized implementations */

lv_result_t lv_rgb565_blend_normal_to_rgb565_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint16_t * src_buf = dsc->src_buf;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(uint16_t);
    const int32_t mask_stride = dsc->mask_stride;
    const int32_t dest_stride = dsc->dest_stride / sizeof(uint16_t);
    uint16_t * dest_buf = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        const uint16_t * src_row = src_buf;
        uint16_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 8 pixels at a time using NEON */
        for(; x < w - 7; x += 8) {
            uint16x8_t src_vec = vld1q_u16(&src_row[x]);
            uint16x8_t dest_vec = vld1q_u16(&dest_row[x]);
            uint8x8_t mask_vec = vld1_u8(&mask_row[x]);
            uint16x8_t result_vec = neon64_alpha_blend_rgb565(src_vec, dest_vec, mask_vec);
            vst1q_u16(&dest_row[x], result_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            if(mask_row[x] > LV_OPA_MIN) {
                dest_row[x] = lv_color_16_16_mix(src_row[x], dest_row[x], mask_row[x]);
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb565_blend_normal_to_rgb565_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_opa_t opa = dsc->opa;
    const uint16_t * src_buf = dsc->src_buf;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(uint16_t);
    const int32_t mask_stride = dsc->mask_stride;
    const int32_t dest_stride = dsc->dest_stride / sizeof(uint16_t);
    uint16_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;

    const uint16x8_t opa_vec = vdupq_n_u16(opa);

    for(int32_t y = 0; y < h; y++) {
        const uint16_t * src_row = src_buf;
        uint16_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 8 pixels at a time using NEON */
        for(; x < w - 7; x += 8) {
            uint16x8_t src_vec = vld1q_u16(&src_row[x]);
            uint16x8_t dest_vec = vld1q_u16(&dest_row[x]);
            uint8x8_t mask_vec = vld1_u8(&mask_row[x]);
            
            /* Mix mask with opacity */
            uint16x8_t mask16_vec = vmovl_u8(mask_vec);
            uint16x8_t combined_alpha = vmulq_u16(mask16_vec, opa_vec);
            combined_alpha = vshrq_n_u16(combined_alpha, 8);
            uint8x8_t final_alpha = vmovn_u16(combined_alpha);
            
            uint16x8_t result_vec = neon64_alpha_blend_rgb565(src_vec, dest_vec, final_alpha);
            vst1q_u16(&dest_row[x], result_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t final_opa = LV_OPA_MIX2(mask_row[x], opa);
            if(final_opa > LV_OPA_MIN) {
                dest_row[x] = lv_color_16_16_mix(src_row[x], dest_row[x], final_opa);
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

/* RGB888 to RGB565 functions */
lv_result_t lv_rgb888_blend_normal_to_rgb565_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint8_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride;
    const int32_t dest_stride = dsc->dest_stride / sizeof(uint16_t);
    uint16_t * dest_buf = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        const uint8_t * src_row = src_buf;
        uint16_t * dest_row = dest_buf;
        int32_t x = 0;

        /* Process 8 pixels at a time using NEON */
        for(; x < w - 7; x += 8) {
            /* Load RGB888 pixels */
            uint8x8x3_t rgb888;
            if(src_px_size == 3) {
                rgb888 = vld3_u8(&src_row[x * 3]);
            } else { /* src_px_size == 4, skip alpha */
                uint8x8x4_t rgba = vld4_u8(&src_row[x * 4]);
                rgb888.val[0] = rgba.val[0]; /* R */
                rgb888.val[1] = rgba.val[1]; /* G */
                rgb888.val[2] = rgba.val[2]; /* B */
            }
            
            /* Convert RGB888 to RGB565 */
            uint16x8_t rgb565_vec = neon64_rgb565_from_rgb888(rgb888);
            vst1q_u16(&dest_row[x], rgb565_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_color_t src_color;
            if(src_px_size == 3) {
                src_color = lv_color_make(src_row[x * 3], src_row[x * 3 + 1], src_row[x * 3 + 2]);
            } else {
                src_color = lv_color_make(src_row[x * 4], src_row[x * 4 + 1], src_row[x * 4 + 2]);
            }
            dest_row[x] = lv_color_to_u16(src_color);
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb888_blend_normal_to_rgb565_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_opa_t opa = dsc->opa;
    const uint8_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride;
    const int32_t dest_stride = dsc->dest_stride / sizeof(uint16_t);
    uint16_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;
    if(opa >= LV_OPA_MAX) return lv_rgb888_blend_normal_to_rgb565_neon64(dsc, src_px_size);

    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        const uint8_t * src_row = src_buf;
        uint16_t * dest_row = dest_buf;
        int32_t x = 0;

        /* Process 8 pixels at a time using NEON */
        for(; x < w - 7; x += 8) {
            /* Load RGB888 pixels */
            uint8x8x3_t rgb888;
            if(src_px_size == 3) {
                rgb888 = vld3_u8(&src_row[x * 3]);
            } else {
                uint8x8x4_t rgba = vld4_u8(&src_row[x * 4]);
                rgb888.val[0] = rgba.val[0];
                rgb888.val[1] = rgba.val[1];
                rgb888.val[2] = rgba.val[2];
            }
            
            /* Convert RGB888 to RGB565 */
            uint16x8_t src_rgb565 = neon64_rgb565_from_rgb888(rgb888);
            uint16x8_t dest_vec = vld1q_u16(&dest_row[x]);
            uint16x8_t result_vec = neon64_alpha_blend_rgb565(src_rgb565, dest_vec, opa_vec);
            vst1q_u16(&dest_row[x], result_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_color_t src_color;
            if(src_px_size == 3) {
                src_color = lv_color_make(src_row[x * 3], src_row[x * 3 + 1], src_row[x * 3 + 2]);
            } else {
                src_color = lv_color_make(src_row[x * 4], src_row[x * 4 + 1], src_row[x * 4 + 2]);
            }
            dest_row[x] = lv_color_16_16_mix(lv_color_to_u16(src_color), dest_row[x], opa);
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb888_blend_normal_to_rgb565_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint8_t * src_buf = dsc->src_buf;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t src_stride = dsc->src_stride;
    const int32_t mask_stride = dsc->mask_stride;
    const int32_t dest_stride = dsc->dest_stride / sizeof(uint16_t);
    uint16_t * dest_buf = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        const uint8_t * src_row = src_buf;
        uint16_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 8 pixels at a time using NEON */
        for(; x < w - 7; x += 8) {
            /* Load RGB888 pixels */
            uint8x8x3_t rgb888;
            if(src_px_size == 3) {
                rgb888 = vld3_u8(&src_row[x * 3]);
            } else {
                uint8x8x4_t rgba = vld4_u8(&src_row[x * 4]);
                rgb888.val[0] = rgba.val[0];
                rgb888.val[1] = rgba.val[1];
                rgb888.val[2] = rgba.val[2];
            }
            
            /* Convert RGB888 to RGB565 */
            uint16x8_t src_rgb565 = neon64_rgb565_from_rgb888(rgb888);
            uint16x8_t dest_vec = vld1q_u16(&dest_row[x]);
            uint8x8_t mask_vec = vld1_u8(&mask_row[x]);
            
            uint16x8_t result_vec = neon64_alpha_blend_rgb565(src_rgb565, dest_vec, mask_vec);
            vst1q_u16(&dest_row[x], result_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            if(mask_row[x] > LV_OPA_MIN) {
                lv_color_t src_color;
                if(src_px_size == 3) {
                    src_color = lv_color_make(src_row[x * 3], src_row[x * 3 + 1], src_row[x * 3 + 2]);
                } else {
                    src_color = lv_color_make(src_row[x * 4], src_row[x * 4 + 1], src_row[x * 4 + 2]);
                }
                dest_row[x] = lv_color_16_16_mix(lv_color_to_u16(src_color), dest_row[x], mask_row[x]);
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb888_blend_normal_to_rgb565_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint8_t * src_buf = (uint8_t *)dsc->src_buf;
    const int32_t src_stride = dsc->src_stride;
    const int32_t dest_stride = dsc->dest_stride / sizeof(uint16_t);
    const lv_opa_t opa = dsc->opa;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    uint16_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;

    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        const uint8_t * src_row = src_buf;
        uint16_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 8 pixels at a time */
        for(x = 0; x <= w - 8; x += 8) {
            uint8x8_t mask_alpha = vld1_u8(mask_row + x);
            
            /* Skip if all mask values are zero */
            uint64_t mask_val = vget_lane_u64(vreinterpret_u64_u8(mask_alpha), 0);
            if(mask_val == 0) continue;

            /* Load source RGB888 data */
            uint8x8x3_t src_rgb = vld3_u8(src_row + x * src_px_size);
            
            /* Load destination RGB565 data */
            uint16x8_t dest_rgb565 = vld1q_u16(dest_row + x);
            
            /* Convert destination RGB565 to RGB888 */
            uint8x8x3_t dest_rgb = neon64_rgb888_from_rgb565(dest_rgb565);

            /* Combine mask and opacity: alpha = (mask * opa) / 255 */
            uint8x8_t alpha = vshrn_n_u16(vmull_u8(mask_alpha, opa_vec), 8);
            uint8x8_t inv_alpha = vsub_u8(vdup_n_u8(255), alpha);

            /* Blend RGB components */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_rgb.val[0], alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_rgb.val[1], alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_rgb.val[2], alpha), 8);

            /* Convert back to RGB565 and store */
            uint16x8_t result = neon64_rgb565_from_rgb888(dest_rgb);
            vst1q_u16(dest_row + x, result);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t mask_alpha = mask_row[x];
            if(mask_alpha <= LV_OPA_MIN) continue;

            /* Combine mask and opacity */
            lv_opa_t alpha = (mask_alpha * opa) / 255;
            if(alpha <= LV_OPA_MIN) continue;

            const uint8_t * src_px = src_row + x * src_px_size;
            uint16_t * dest_px = dest_row + x;
            
            if(alpha >= LV_OPA_MAX) {
                /* Full opacity - direct conversion */
                *dest_px = ((src_px[0] & 0xF8) << 8) | ((src_px[1] & 0xFC) << 3) | (src_px[2] >> 3);
            } else {
                /* Extract destination RGB565 */
                uint16_t dest_val = *dest_px;
                uint8_t dest_r = (dest_val >> 8) & 0xF8;
                uint8_t dest_g = (dest_val >> 3) & 0xFC;
                uint8_t dest_b = (dest_val << 3) & 0xF8;

                /* Blend */
                uint8_t blend_r = ((src_px[0] * alpha) + (dest_r * (255 - alpha))) / 255;
                uint8_t blend_g = ((src_px[1] * alpha) + (dest_g * (255 - alpha))) / 255;
                uint8_t blend_b = ((src_px[2] * alpha) + (dest_b * (255 - alpha))) / 255;

                /* Convert back to RGB565 */
                *dest_px = ((blend_r & 0xF8) << 8) | ((blend_g & 0xFC) << 3) | (blend_b >> 3);
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

/* ARGB8888 to RGB565 functions */
lv_result_t lv_argb8888_blend_normal_to_rgb565_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color32_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(lv_color32_t);
    const int32_t dest_stride = dsc->dest_stride / sizeof(uint16_t);
    uint16_t * dest_buf = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        const uint32_t * src_row = (uint32_t *)src_buf;
        uint16_t * dest_row = dest_buf;
        int32_t x = 0;

        /* Process 4 pixels at a time using NEON */
        for(; x < w - 3; x += 4) {
            uint32x4_t src_vec = vld1q_u32(&src_row[x]);
            uint16x4_t rgb565_vec = neon64_rgb565_from_argb8888(src_vec);
            
            /* Store to destination (need to handle partial store) */
            uint16_t temp[4];
            vst1_u16(temp, rgb565_vec);
            for(int i = 0; i < 4 && (x + i) < w; i++) {
                dest_row[x + i] = temp[i];
            }
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_color_t color = lv_color_make(src_buf[x].red, src_buf[x].green, src_buf[x].blue);
            dest_row[x] = lv_color_to_u16(color);
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_argb8888_blend_normal_to_rgb565_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_opa_t opa = dsc->opa;
    const lv_color32_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(lv_color32_t);
    const int32_t dest_stride = dsc->dest_stride / sizeof(uint16_t);
    uint16_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;
    if(opa >= LV_OPA_MAX) return lv_argb8888_blend_normal_to_rgb565_neon64(dsc);

    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        const uint32_t * src_row = (uint32_t *)src_buf;
        uint16_t * dest_row = dest_buf;
        int32_t x = 0;

        /* Process 4 pixels at a time using NEON */
        for(; x < w - 3; x += 4) {
            uint32x4_t src_vec = vld1q_u32(&src_row[x]);
            uint16x4_t dest_vec4 = vld1_u16(&dest_row[x]);
            uint16x8_t dest_vec = vcombine_u16(dest_vec4, dest_vec4);
            
            /* Convert ARGB8888 to RGB565 */
            uint16x4_t src_rgb565 = neon64_rgb565_from_argb8888(src_vec);
            uint16x8_t src_vec8 = vcombine_u16(src_rgb565, src_rgb565);
            
            /* Blend with opacity */
            uint16x8_t result_vec = neon64_alpha_blend_rgb565(src_vec8, dest_vec, opa_vec);
            vst1_u16(&dest_row[x], vget_low_u16(result_vec));
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_color_t src_color = lv_color_make(src_buf[x].red, src_buf[x].green, src_buf[x].blue);
            uint16_t src_rgb565 = lv_color_to_u16(src_color);
            dest_row[x] = lv_color_16_16_mix(src_rgb565, dest_row[x], opa);
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_argb8888_blend_normal_to_rgb565_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color32_t * src_buf = dsc->src_buf;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(lv_color32_t);
    const int32_t mask_stride = dsc->mask_stride;
    const int32_t dest_stride = dsc->dest_stride / sizeof(uint16_t);
    uint16_t * dest_buf = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        const uint32_t * src_row = (uint32_t *)src_buf;
        uint16_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 4 pixels at a time using NEON */
        for(; x < w - 3; x += 4) {
            uint32x4_t src_vec = vld1q_u32(&src_row[x]);
            uint16x4_t dest_vec4 = vld1_u16(&dest_row[x]);
            uint16x8_t dest_vec = vcombine_u16(dest_vec4, dest_vec4);
            uint8x8_t mask_vec8 = vld1_u8(&mask_row[x]);
            uint8x8_t mask_vec = vget_low_u8(vcombine_u8(mask_vec8, mask_vec8));
            
            /* Convert ARGB8888 to RGB565 */
            uint16x4_t src_rgb565 = neon64_rgb565_from_argb8888(src_vec);
            uint16x8_t src_vec8 = vcombine_u16(src_rgb565, src_rgb565);
            
            /* Blend with mask */
            uint16x8_t result_vec = neon64_alpha_blend_rgb565(src_vec8, dest_vec, mask_vec);
            vst1_u16(&dest_row[x], vget_low_u16(result_vec));
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            if(mask_row[x] > LV_OPA_MIN) {
                lv_color_t src_color = lv_color_make(src_buf[x].red, src_buf[x].green, src_buf[x].blue);
                uint16_t src_rgb565 = lv_color_to_u16(src_color);
                dest_row[x] = lv_color_16_16_mix(src_rgb565, dest_row[x], mask_row[x]);
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_argb8888_blend_normal_to_rgb565_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color32_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(lv_color32_t);
    const int32_t dest_stride = dsc->dest_stride / sizeof(uint16_t);
    const lv_opa_t opa = dsc->opa;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    uint16_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;

    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        const uint32_t * src_row = (uint32_t *)src_buf;
        uint16_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 4 pixels at a time (ARGB8888 is 4 bytes per pixel) */
        for(x = 0; x <= w - 4; x += 4) {
            uint8x8_t mask_alpha = vld1_u8(mask_row + x);
            mask_alpha = vget_low_u8(vcombine_u8(mask_alpha, mask_alpha)); /* Take only 4 values */
            
            /* Skip if all mask values are zero */
            uint32_t mask_val = vget_lane_u32(vreinterpret_u32_u8(mask_alpha), 0);
            if(mask_val == 0) continue;

            /* Load source ARGB8888 data */
            uint32x4_t src_argb = vld1q_u32(src_row + x);
            
            /* Extract alpha from source */
            uint8x8_t src_alpha = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(src_argb), 8));
            src_alpha = vget_low_u8(vcombine_u8(src_alpha, src_alpha));

            /* Load destination RGB565 data */
            uint16x4_t dest_rgb565 = vld1_u16(dest_row + x);
            
            /* Convert destination RGB565 to RGB888 */
            uint32x4_t dest_argb = neon64_argb8888_from_rgb565(dest_rgb565);
            uint8x8x3_t dest_rgb = {
                .val[0] = vmovn_u16(vreinterpretq_u16_u32(dest_argb)),          /* R */
                .val[1] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(dest_argb), 8)), /* G */
                .val[2] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(dest_argb), 16)) /* B */
            };

            /* Extract RGB from source */
            uint8x8x3_t src_rgb = {
                .val[0] = vmovn_u16(vreinterpretq_u16_u32(src_argb)),          /* R */
                .val[1] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(src_argb), 8)), /* G */
                .val[2] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(src_argb), 16)) /* B */
            };

            /* Combine source alpha, mask and opacity: alpha = (src_alpha * mask * opa) / (255 * 255) */
            uint16x8_t temp_alpha = vmull_u8(src_alpha, mask_alpha);
            uint8x8_t alpha = vshrn_n_u16(vmull_u8(vget_low_u8(vreinterpretq_u8_u16(temp_alpha)), opa_vec), 8);
            uint8x8_t inv_alpha = vsub_u8(vdup_n_u8(255), alpha);

            /* Blend RGB components */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_rgb.val[0], alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_rgb.val[1], alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_rgb.val[2], alpha), 8);

            /* Convert back to RGB565 and store */
            uint16x8_t result = neon64_rgb565_from_rgb888(dest_rgb);
            vst1_u16(dest_row + x, vget_low_u16(result));
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t mask_alpha = mask_row[x];
            if(mask_alpha <= LV_OPA_MIN) continue;

            const lv_color32_t * src_px = &src_buf[x];
            uint16_t * dest_px = dest_row + x;
            
            /* Combine source alpha, mask and opacity */
            lv_opa_t alpha = (src_px->alpha * mask_alpha * opa) / (255 * 255);
            if(alpha <= LV_OPA_MIN) continue;

            if(alpha >= LV_OPA_MAX) {
                /* Full opacity - direct conversion */
                *dest_px = ((src_px->red & 0xF8) << 8) | ((src_px->green & 0xFC) << 3) | (src_px->blue >> 3);
            } else {
                /* Extract destination RGB565 */
                uint16_t dest_val = *dest_px;
                uint8_t dest_r = (dest_val >> 8) & 0xF8;
                uint8_t dest_g = (dest_val >> 3) & 0xFC;
                uint8_t dest_b = (dest_val << 3) & 0xF8;

                /* Blend */
                uint8_t blend_r = ((src_px->red * alpha) + (dest_r * (255 - alpha))) / 255;
                uint8_t blend_g = ((src_px->green * alpha) + (dest_g * (255 - alpha))) / 255;
                uint8_t blend_b = ((src_px->blue * alpha) + (dest_b * (255 - alpha))) / 255;

                /* Convert back to RGB565 */
                *dest_px = ((blend_r & 0xF8) << 8) | ((blend_g & 0xFC) << 3) | (blend_b >> 3);
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

/* Color fill functions - RGB888 destination */
lv_result_t lv_color_blend_to_rgb888_neon64(lv_draw_sw_blend_fill_dsc_t * dsc, int32_t dst_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color_t color = dsc->color;
    const int32_t dest_stride = dsc->dest_stride;
    uint8_t * dest_buf = dsc->dest_buf;

    const uint8_t r = (lv_color_to_u32(color) >> 16) & 0xFF;
    const uint8_t g = (lv_color_to_u32(color) >> 8) & 0xFF;
    const uint8_t b = lv_color_to_u32(color) & 0xFF;

    for(int32_t y = 0; y < h; y++) {
        uint8_t * row_ptr = dest_buf;
        int32_t x = 0;

        if(dst_px_size == 3) {
            /* RGB888 format */
            const uint8x8x3_t color_vec = {{vdup_n_u8(r), vdup_n_u8(g), vdup_n_u8(b)}};
            
            /* Process 8 pixels at a time using NEON */
            for(; x < w - 7; x += 8) {
                vst3_u8(&row_ptr[x * 3], color_vec);
            }
            
            /* Handle remaining pixels */
            for(; x < w; x++) {
                row_ptr[x * 3] = r;
                row_ptr[x * 3 + 1] = g;
                row_ptr[x * 3 + 2] = b;
            }
        } else { /* dst_px_size == 4, RGBA format */
            const uint8x8x4_t color_vec = {{vdup_n_u8(r), vdup_n_u8(g), vdup_n_u8(b), vdup_n_u8(0xFF)}};
            
            /* Process 8 pixels at a time using NEON */
            for(; x < w - 7; x += 8) {
                vst4_u8(&row_ptr[x * 4], color_vec);
            }
            
            /* Handle remaining pixels */
            for(; x < w; x++) {
                row_ptr[x * 4] = r;
                row_ptr[x * 4 + 1] = g;
                row_ptr[x * 4 + 2] = b;
                row_ptr[x * 4 + 3] = 0xFF;
            }
        }

        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_color_blend_to_rgb888_with_opa_neon64(lv_draw_sw_blend_fill_dsc_t * dsc, int32_t dst_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color_t color = dsc->color;
    const lv_opa_t opa = dsc->opa;
    const int32_t dest_stride = dsc->dest_stride;
    uint8_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;
    if(opa >= LV_OPA_MAX) return lv_color_blend_to_rgb888_neon64(dsc, dst_px_size);

    /* Extract RGB components for the fill color */
    const uint8_t color_r = color.red;
    const uint8_t color_g = color.green;
    const uint8_t color_b = color.blue;

    /* Create NEON vectors for the color components */
    const uint8x8_t src_r = vdup_n_u8(color_r);
    const uint8x8_t src_g = vdup_n_u8(color_g);
    const uint8x8_t src_b = vdup_n_u8(color_b);
    const uint8x8_t alpha = vdup_n_u8(opa);
    const uint8x8_t inv_alpha = vdup_n_u8(255 - opa);

    for(int32_t y = 0; y < h; y++) {
        uint8_t * dest_row = dest_buf;
        int32_t x = 0;

        /* Process 8 pixels at a time */
        for(x = 0; x <= w - 8; x += 8) {
            uint8x8x3_t dest_rgb = vld3_u8(dest_row + x * 3);

            /* Blend: dest = ((src * alpha) + (dest * inv_alpha)) / 255 */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_r, alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_g, alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_b, alpha), 8);

            vst3_u8(dest_row + x * 3, dest_rgb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            uint8_t * dest_px = dest_row + x * 3;
            dest_px[0] = ((color_r * opa) + (dest_px[0] * (255 - opa))) / 255;
            dest_px[1] = ((color_g * opa) + (dest_px[1] * (255 - opa))) / 255;
            dest_px[2] = ((color_b * opa) + (dest_px[2] * (255 - opa))) / 255;
        }

        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_color_blend_to_rgb888_with_mask_neon64(lv_draw_sw_blend_fill_dsc_t * dsc, int32_t dst_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color_t color = dsc->color;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    const int32_t dest_stride = dsc->dest_stride;
    uint8_t * dest_buf = dsc->dest_buf;

    /* Extract RGB components for the fill color */
    const uint8_t color_r = color.red;
    const uint8_t color_g = color.green;
    const uint8_t color_b = color.blue;

    /* Create NEON vectors for the color components */
    const uint8x8_t src_r = vdup_n_u8(color_r);
    const uint8x8_t src_g = vdup_n_u8(color_g);
    const uint8x8_t src_b = vdup_n_u8(color_b);

    for(int32_t y = 0; y < h; y++) {
        uint8_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 8 pixels at a time */
        for(x = 0; x <= w - 8; x += 8) {
            uint8x8_t alpha = vld1_u8(mask_row + x);
            uint8x8_t inv_alpha = vsub_u8(vdup_n_u8(255), alpha);
            uint8x8x3_t dest_rgb = vld3_u8(dest_row + x * 3);

            /* Skip if all alpha values are zero */
            uint64_t alpha_mask = vget_lane_u64(vreinterpret_u64_u8(alpha), 0);
            if(alpha_mask == 0) continue;

            /* Blend: dest = ((src * alpha) + (dest * inv_alpha)) / 255 */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_r, alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_g, alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_b, alpha), 8);

            vst3_u8(dest_row + x * 3, dest_rgb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t alpha = mask_row[x];
            if(alpha <= LV_OPA_MIN) continue;
            
            uint8_t * dest_px = dest_row + x * 3;
            if(alpha >= LV_OPA_MAX) {
                dest_px[0] = color_r;
                dest_px[1] = color_g;
                dest_px[2] = color_b;
            } else {
                dest_px[0] = ((color_r * alpha) + (dest_px[0] * (255 - alpha))) / 255;
                dest_px[1] = ((color_g * alpha) + (dest_px[1] * (255 - alpha))) / 255;
                dest_px[2] = ((color_b * alpha) + (dest_px[2] * (255 - alpha))) / 255;
            }
        }

        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_color_blend_to_rgb888_mix_mask_opa_neon64(lv_draw_sw_blend_fill_dsc_t * dsc, int32_t dst_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color_t color = dsc->color;
    const lv_opa_t opa = dsc->opa;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    const int32_t dest_stride = dsc->dest_stride;
    uint8_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;

    /* Extract RGB components for the fill color */
    const uint8_t color_r = color.red;
    const uint8_t color_g = color.green;
    const uint8_t color_b = color.blue;

    /* Create NEON vectors for the color components */
    const uint8x8_t src_r = vdup_n_u8(color_r);
    const uint8x8_t src_g = vdup_n_u8(color_g);
    const uint8x8_t src_b = vdup_n_u8(color_b);
    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        uint8_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 8 pixels at a time */
        for(x = 0; x <= w - 8; x += 8) {
            uint8x8_t mask_alpha = vld1_u8(mask_row + x);
            
            /* Skip if all mask values are zero */
            uint64_t mask_val = vget_lane_u64(vreinterpret_u64_u8(mask_alpha), 0);
            if(mask_val == 0) continue;

            /* Combine mask and opacity: alpha = (mask * opa) / 255 */
            uint8x8_t alpha = vshrn_n_u16(vmull_u8(mask_alpha, opa_vec), 8);
            uint8x8_t inv_alpha = vsub_u8(vdup_n_u8(255), alpha);
            uint8x8x3_t dest_rgb = vld3_u8(dest_row + x * 3);

            /* Blend: dest = ((src * alpha) + (dest * inv_alpha)) / 255 */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_r, alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_g, alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_b, alpha), 8);

            vst3_u8(dest_row + x * 3, dest_rgb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t mask_alpha = mask_row[x];
            if(mask_alpha <= LV_OPA_MIN) continue;

            /* Combine mask and opacity */
            lv_opa_t alpha = (mask_alpha * opa) / 255;
            if(alpha <= LV_OPA_MIN) continue;
            
            uint8_t * dest_px = dest_row + x * 3;
            if(alpha >= LV_OPA_MAX) {
                dest_px[0] = color_r;
                dest_px[1] = color_g;
                dest_px[2] = color_b;
            } else {
                dest_px[0] = ((color_r * alpha) + (dest_px[0] * (255 - alpha))) / 255;
                dest_px[1] = ((color_g * alpha) + (dest_px[1] * (255 - alpha))) / 255;
                dest_px[2] = ((color_b * alpha) + (dest_px[2] * (255 - alpha))) / 255;
            }
        }

        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

/* Image blending functions - RGB888 destination */
lv_result_t lv_rgb565_blend_normal_to_rgb888_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint16_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(uint16_t);
    const int32_t dest_stride = dsc->dest_stride;
    uint8_t * dest_buf = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        const uint16_t * src_row = src_buf;
        uint8_t * dest_row = dest_buf;
        int32_t x = 0;

        /* Process 8 pixels at a time using NEON */
        for(; x < w - 7; x += 8) {
            uint16x8_t src_vec = vld1q_u16(&src_row[x]);
            uint8x8x3_t rgb888 = neon64_rgb888_from_rgb565(src_vec);
            
            if(dst_px_size == 3) {
                vst3_u8(&dest_row[x * 3], rgb888);
            } else { /* dst_px_size == 4 */
                uint8x8x4_t rgba = {{rgb888.val[0], rgb888.val[1], rgb888.val[2], vdup_n_u8(0xFF)}};
                vst4_u8(&dest_row[x * 4], rgba);
            }
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_color_t color = lv_color_make((src_row[x] >> 11) << 3, 
                                           ((src_row[x] >> 5) & 0x3F) << 2, 
                                           (src_row[x] & 0x1F) << 3);
            if(dst_px_size == 3) {
                dest_row[x * 3] = color.red;
                dest_row[x * 3 + 1] = color.green;
                dest_row[x * 3 + 2] = color.blue;
            } else {
                dest_row[x * 4] = color.red;
                dest_row[x * 4 + 1] = color.green;
                dest_row[x * 4 + 2] = color.blue;
                dest_row[x * 4 + 3] = 0xFF;
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb565_blend_normal_to_rgb888_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint16_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(uint16_t);
    const int32_t dest_stride = dsc->dest_stride;
    const lv_opa_t opa = dsc->opa;
    uint8_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;
    if(opa >= LV_OPA_MAX) return lv_rgb565_blend_normal_to_rgb888_neon64(dsc, dst_px_size);

    const uint8x8_t alpha = vdup_n_u8(opa);
    const uint8x8_t inv_alpha = vdup_n_u8(255 - opa);

    for(int32_t y = 0; y < h; y++) {
        const uint16_t * src_row = src_buf;
        uint8_t * dest_row = dest_buf;
        int32_t x = 0;

        /* Process 8 pixels at a time */
        for(x = 0; x <= w - 8; x += 8) {
            /* Load source RGB565 data */
            uint16x8_t src_rgb565 = vld1q_u16(src_row + x);
            
            /* Convert RGB565 to RGB888 */
            uint8x8x3_t src_rgb = neon64_rgb888_from_rgb565(src_rgb565);
            
            /* Load destination RGB888 data */
            uint8x8x3_t dest_rgb = vld3_u8(dest_row + x * dst_px_size);

            /* Blend RGB components */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_rgb.val[0], alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_rgb.val[1], alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_rgb.val[2], alpha), 8);

            vst3_u8(dest_row + x * dst_px_size, dest_rgb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            uint16_t src_val = src_row[x];
            uint8_t * dest_px = dest_row + x * dst_px_size;
            
            /* Convert RGB565 to RGB888 */
            uint8_t src_r = (src_val >> 8) & 0xF8;
            uint8_t src_g = (src_val >> 3) & 0xFC;
            uint8_t src_b = (src_val << 3) & 0xF8;

            /* Blend */
            dest_px[0] = ((src_r * opa) + (dest_px[0] * (255 - opa))) / 255;
            dest_px[1] = ((src_g * opa) + (dest_px[1] * (255 - opa))) / 255;
            dest_px[2] = ((src_b * opa) + (dest_px[2] * (255 - opa))) / 255;
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb565_blend_normal_to_rgb888_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint16_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(uint16_t);
    const int32_t dest_stride = dsc->dest_stride;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    uint8_t * dest_buf = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        const uint16_t * src_row = src_buf;
        uint8_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 8 pixels at a time */
        for(x = 0; x <= w - 8; x += 8) {
            uint8x8_t alpha = vld1_u8(mask_row + x);
            
            /* Skip if all alpha values are zero */
            uint64_t alpha_mask = vget_lane_u64(vreinterpret_u64_u8(alpha), 0);
            if(alpha_mask == 0) continue;

            /* Load source RGB565 data */
            uint16x8_t src_rgb565 = vld1q_u16(src_row + x);
            
            /* Convert RGB565 to RGB888 */
            uint8x8x3_t src_rgb = neon64_rgb888_from_rgb565(src_rgb565);
            
            /* Load destination RGB888 data */
            uint8x8x3_t dest_rgb = vld3_u8(dest_row + x * dst_px_size);

            uint8x8_t inv_alpha = vsub_u8(vdup_n_u8(255), alpha);

            /* Blend RGB components */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_rgb.val[0], alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_rgb.val[1], alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_rgb.val[2], alpha), 8);

            vst3_u8(dest_row + x * dst_px_size, dest_rgb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t alpha = mask_row[x];
            if(alpha <= LV_OPA_MIN) continue;

            uint16_t src_val = src_row[x];
            uint8_t * dest_px = dest_row + x * dst_px_size;
            
            if(alpha >= LV_OPA_MAX) {
                /* Full opacity - direct conversion */
                dest_px[0] = (src_val >> 8) & 0xF8;
                dest_px[1] = (src_val >> 3) & 0xFC;
                dest_px[2] = (src_val << 3) & 0xF8;
            } else {
                /* Convert RGB565 to RGB888 */
                uint8_t src_r = (src_val >> 8) & 0xF8;
                uint8_t src_g = (src_val >> 3) & 0xFC;
                uint8_t src_b = (src_val << 3) & 0xF8;

                /* Blend */
                dest_px[0] = ((src_r * alpha) + (dest_px[0] * (255 - alpha))) / 255;
                dest_px[1] = ((src_g * alpha) + (dest_px[1] * (255 - alpha))) / 255;
                dest_px[2] = ((src_b * alpha) + (dest_px[2] * (255 - alpha))) / 255;
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb565_blend_normal_to_rgb888_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint16_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(uint16_t);
    const int32_t dest_stride = dsc->dest_stride;
    const lv_opa_t opa = dsc->opa;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    uint8_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;

    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        const uint16_t * src_row = src_buf;
        uint8_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 8 pixels at a time */
        for(x = 0; x <= w - 8; x += 8) {
            uint8x8_t mask_alpha = vld1_u8(mask_row + x);
            
            /* Skip if all mask values are zero */
            uint64_t mask_val = vget_lane_u64(vreinterpret_u64_u8(mask_alpha), 0);
            if(mask_val == 0) continue;

            /* Combine mask and opacity: alpha = (mask * opa) / 255 */
            uint8x8_t alpha = vshrn_n_u16(vmull_u8(mask_alpha, opa_vec), 8);
            uint8x8_t inv_alpha = vsub_u8(vdup_n_u8(255), alpha);

            /* Load source RGB565 data */
            uint16x8_t src_rgb565 = vld1q_u16(src_row + x);
            
            /* Convert RGB565 to RGB888 */
            uint8x8x3_t src_rgb = neon64_rgb888_from_rgb565(src_rgb565);
            
            /* Load destination RGB888 data */
            uint8x8x3_t dest_rgb = vld3_u8(dest_row + x * dst_px_size);

            /* Blend RGB components */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_rgb.val[0], alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_rgb.val[1], alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_rgb.val[2], alpha), 8);

            vst3_u8(dest_row + x * dst_px_size, dest_rgb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t mask_alpha = mask_row[x];
            if(mask_alpha <= LV_OPA_MIN) continue;

            /* Combine mask and opacity */
            lv_opa_t alpha = (mask_alpha * opa) / 255;
            if(alpha <= LV_OPA_MIN) continue;

            uint16_t src_val = src_row[x];
            uint8_t * dest_px = dest_row + x * dst_px_size;
            
            if(alpha >= LV_OPA_MAX) {
                /* Full opacity - direct conversion */
                dest_px[0] = (src_val >> 8) & 0xF8;
                dest_px[1] = (src_val >> 3) & 0xFC;
                dest_px[2] = (src_val << 3) & 0xF8;
            } else {
                /* Convert RGB565 to RGB888 */
                uint8_t src_r = (src_val >> 8) & 0xF8;
                uint8_t src_g = (src_val >> 3) & 0xFC;
                uint8_t src_b = (src_val << 3) & 0xF8;

                /* Blend */
                dest_px[0] = ((src_r * alpha) + (dest_px[0] * (255 - alpha))) / 255;
                dest_px[1] = ((src_g * alpha) + (dest_px[1] * (255 - alpha))) / 255;
                dest_px[2] = ((src_b * alpha) + (dest_px[2] * (255 - alpha))) / 255;
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb888_blend_normal_to_rgb888_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size, int32_t src_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint8_t * src_buf = (uint8_t *)dsc->src_buf;
    const int32_t src_stride = dsc->src_stride;
    const int32_t dest_stride = dsc->dest_stride;
    uint8_t * dest_buf = dsc->dest_buf;

    /* Simple copy operation for RGB888 to RGB888 without blending */
    for(int32_t y = 0; y < h; y++) {
        const uint8_t * src_row = src_buf;
        uint8_t * dest_row = dest_buf;
        int32_t x = 0;

        /* Process 16 pixels at a time */
        for(x = 0; x <= w - 16; x += 16) {
            uint8x16x3_t src_rgb = vld3q_u8(src_row + x * src_px_size);
            vst3q_u8(dest_row + x * dst_px_size, src_rgb);
        }

        /* Process 8 pixels at a time */
        for(; x <= w - 8; x += 8) {
            uint8x8x3_t src_rgb = vld3_u8(src_row + x * src_px_size);
            vst3_u8(dest_row + x * dst_px_size, src_rgb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            const uint8_t * src_px = src_row + x * src_px_size;
            uint8_t * dest_px = dest_row + x * dst_px_size;
            dest_px[0] = src_px[0];
            dest_px[1] = src_px[1];
            dest_px[2] = src_px[2];
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb888_blend_normal_to_rgb888_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size, int32_t src_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint8_t * src_buf = (uint8_t *)dsc->src_buf;
    const int32_t src_stride = dsc->src_stride;
    const int32_t dest_stride = dsc->dest_stride;
    const lv_opa_t opa = dsc->opa;
    uint8_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;
    if(opa >= LV_OPA_MAX) return lv_rgb888_blend_normal_to_rgb888_neon64(dsc, dst_px_size, src_px_size);

    const uint8x8_t alpha = vdup_n_u8(opa);
    const uint8x8_t inv_alpha = vdup_n_u8(255 - opa);

    for(int32_t y = 0; y < h; y++) {
        const uint8_t * src_row = src_buf;
        uint8_t * dest_row = dest_buf;
        int32_t x = 0;

        /* Process 8 pixels at a time */
        for(x = 0; x <= w - 8; x += 8) {
            uint8x8x3_t src_rgb = vld3_u8(src_row + x * src_px_size);
            uint8x8x3_t dest_rgb = vld3_u8(dest_row + x * dst_px_size);

            /* Blend RGB components */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_rgb.val[0], alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_rgb.val[1], alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_rgb.val[2], alpha), 8);

            vst3_u8(dest_row + x * dst_px_size, dest_rgb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            const uint8_t * src_px = src_row + x * src_px_size;
            uint8_t * dest_px = dest_row + x * dst_px_size;
            
            dest_px[0] = ((src_px[0] * opa) + (dest_px[0] * (255 - opa))) / 255;
            dest_px[1] = ((src_px[1] * opa) + (dest_px[1] * (255 - opa))) / 255;
            dest_px[2] = ((src_px[2] * opa) + (dest_px[2] * (255 - opa))) / 255;
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb888_blend_normal_to_rgb888_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size, int32_t src_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint8_t * src_buf = (uint8_t *)dsc->src_buf;
    const int32_t src_stride = dsc->src_stride;
    const int32_t dest_stride = dsc->dest_stride;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    uint8_t * dest_buf = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        const uint8_t * src_row = src_buf;
        uint8_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 8 pixels at a time */
        for(x = 0; x <= w - 8; x += 8) {
            uint8x8_t alpha = vld1_u8(mask_row + x);
            
            /* Skip if all alpha values are zero */
            uint64_t alpha_mask = vget_lane_u64(vreinterpret_u64_u8(alpha), 0);
            if(alpha_mask == 0) continue;

            uint8x8x3_t src_rgb = vld3_u8(src_row + x * src_px_size);
            uint8x8x3_t dest_rgb = vld3_u8(dest_row + x * dst_px_size);
            uint8x8_t inv_alpha = vsub_u8(vdup_n_u8(255), alpha);

            /* Blend RGB components */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_rgb.val[0], alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_rgb.val[1], alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_rgb.val[2], alpha), 8);

            vst3_u8(dest_row + x * dst_px_size, dest_rgb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t alpha = mask_row[x];
            if(alpha <= LV_OPA_MIN) continue;

            const uint8_t * src_px = src_row + x * src_px_size;
            uint8_t * dest_px = dest_row + x * dst_px_size;
            
            if(alpha >= LV_OPA_MAX) {
                dest_px[0] = src_px[0];
                dest_px[1] = src_px[1];
                dest_px[2] = src_px[2];
            } else {
                dest_px[0] = ((src_px[0] * alpha) + (dest_px[0] * (255 - alpha))) / 255;
                dest_px[1] = ((src_px[1] * alpha) + (dest_px[1] * (255 - alpha))) / 255;
                dest_px[2] = ((src_px[2] * alpha) + (dest_px[2] * (255 - alpha))) / 255;
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb888_blend_normal_to_rgb888_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size, int32_t src_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint8_t * src_buf = (uint8_t *)dsc->src_buf;
    const int32_t src_stride = dsc->src_stride;
    const int32_t dest_stride = dsc->dest_stride;
    const lv_opa_t opa = dsc->opa;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    uint8_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;

    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        const uint8_t * src_row = src_buf;
        uint8_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 8 pixels at a time */
        for(x = 0; x <= w - 8; x += 8) {
            uint8x8_t mask_alpha = vld1_u8(mask_row + x);
            
            /* Skip if all mask values are zero */
            uint64_t mask_val = vget_lane_u64(vreinterpret_u64_u8(mask_alpha), 0);
            if(mask_val == 0) continue;

            /* Combine mask and opacity: alpha = (mask * opa) / 255 */
            uint8x8_t alpha = vshrn_n_u16(vmull_u8(mask_alpha, opa_vec), 8);
            uint8x8_t inv_alpha = vsub_u8(vdup_n_u8(255), alpha);

            uint8x8x3_t src_rgb = vld3_u8(src_row + x * src_px_size);
            uint8x8x3_t dest_rgb = vld3_u8(dest_row + x * dst_px_size);

            /* Blend RGB components */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_rgb.val[0], alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_rgb.val[1], alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_rgb.val[2], alpha), 8);

            vst3_u8(dest_row + x * dst_px_size, dest_rgb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t mask_alpha = mask_row[x];
            if(mask_alpha <= LV_OPA_MIN) continue;

            /* Combine mask and opacity */
            lv_opa_t alpha = (mask_alpha * opa) / 255;
            if(alpha <= LV_OPA_MIN) continue;

            const uint8_t * src_px = src_row + x * src_px_size;
            uint8_t * dest_px = dest_row + x * dst_px_size;
            
            if(alpha >= LV_OPA_MAX) {
                dest_px[0] = src_px[0];
                dest_px[1] = src_px[1];
                dest_px[2] = src_px[2];
            } else {
                dest_px[0] = ((src_px[0] * alpha) + (dest_px[0] * (255 - alpha))) / 255;
                dest_px[1] = ((src_px[1] * alpha) + (dest_px[1] * (255 - alpha))) / 255;
                dest_px[2] = ((src_px[2] * alpha) + (dest_px[2] * (255 - alpha))) / 255;
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_argb8888_blend_normal_to_rgb888_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color32_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(lv_color32_t);
    const int32_t dest_stride = dsc->dest_stride;
    uint8_t * dest_buf = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        const uint32_t * src_row = (uint32_t *)src_buf;
        uint8_t * dest_row = dest_buf;
        int32_t x = 0;

        /* Process 4 pixels at a time (ARGB8888 is 4 bytes per pixel) */
        for(x = 0; x <= w - 4; x += 4) {
            /* Load source ARGB8888 data */
            uint32x4_t src_argb = vld1q_u32(src_row + x);
            
            /* Extract alpha from source */
            uint8x8_t src_alpha = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(src_argb), 8));
            src_alpha = vget_low_u8(vcombine_u8(src_alpha, src_alpha));

            /* Load destination RGB888 data */
            uint8x8x3_t dest_rgb = vld3_u8(dest_row + x * dst_px_size);

            /* Extract RGB from source */
            uint8x8x3_t src_rgb = {
                .val[0] = vmovn_u16(vreinterpretq_u16_u32(src_argb)),          /* R */
                .val[1] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(src_argb), 8)), /* G */
                .val[2] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(src_argb), 16)) /* B */
            };

            uint8x8_t inv_alpha = vsub_u8(vdup_n_u8(255), src_alpha);

            /* Blend RGB components using source alpha */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_rgb.val[0], src_alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_rgb.val[1], src_alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_rgb.val[2], src_alpha), 8);

            vst3_u8(dest_row + x * dst_px_size, dest_rgb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            const lv_color32_t * src_px = &src_buf[x];
            uint8_t * dest_px = dest_row + x * dst_px_size;
            
            lv_opa_t alpha = src_px->alpha;
            if(alpha <= LV_OPA_MIN) continue;

            if(alpha >= LV_OPA_MAX) {
                dest_px[0] = src_px->red;
                dest_px[1] = src_px->green;
                dest_px[2] = src_px->blue;
            } else {
                dest_px[0] = ((src_px->red * alpha) + (dest_px[0] * (255 - alpha))) / 255;
                dest_px[1] = ((src_px->green * alpha) + (dest_px[1] * (255 - alpha))) / 255;
                dest_px[2] = ((src_px->blue * alpha) + (dest_px[2] * (255 - alpha))) / 255;
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_argb8888_blend_normal_to_rgb888_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color32_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(lv_color32_t);
    const int32_t dest_stride = dsc->dest_stride;
    const lv_opa_t opa = dsc->opa;
    uint8_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;
    if(opa >= LV_OPA_MAX) return lv_argb8888_blend_normal_to_rgb888_neon64(dsc, dst_px_size);

    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        const uint32_t * src_row = (uint32_t *)src_buf;
        uint8_t * dest_row = dest_buf;
        int32_t x = 0;

        /* Process 4 pixels at a time (ARGB8888 is 4 bytes per pixel) */
        for(x = 0; x <= w - 4; x += 4) {
            /* Load source ARGB8888 data */
            uint32x4_t src_argb = vld1q_u32(src_row + x);
            
            /* Extract alpha from source */
            uint8x8_t src_alpha = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(src_argb), 8));
            src_alpha = vget_low_u8(vcombine_u8(src_alpha, src_alpha));

            /* Combine source alpha with opacity: alpha = (src_alpha * opa) / 255 */
            uint8x8_t alpha = vshrn_n_u16(vmull_u8(src_alpha, opa_vec), 8);
            uint8x8_t inv_alpha = vsub_u8(vdup_n_u8(255), alpha);

            /* Load destination RGB888 data */
            uint8x8x3_t dest_rgb = vld3_u8(dest_row + x * dst_px_size);

            /* Extract RGB from source */
            uint8x8x3_t src_rgb = {
                .val[0] = vmovn_u16(vreinterpretq_u16_u32(src_argb)),          /* R */
                .val[1] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(src_argb), 8)), /* G */
                .val[2] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(src_argb), 16)) /* B */
            };

            /* Blend RGB components */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_rgb.val[0], alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_rgb.val[1], alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_rgb.val[2], alpha), 8);

            vst3_u8(dest_row + x * dst_px_size, dest_rgb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            const lv_color32_t * src_px = &src_buf[x];
            uint8_t * dest_px = dest_row + x * dst_px_size;
            
            /* Combine source alpha with opacity */
            lv_opa_t alpha = (src_px->alpha * opa) / 255;
            if(alpha <= LV_OPA_MIN) continue;

            if(alpha >= LV_OPA_MAX) {
                dest_px[0] = src_px->red;
                dest_px[1] = src_px->green;
                dest_px[2] = src_px->blue;
            } else {
                dest_px[0] = ((src_px->red * alpha) + (dest_px[0] * (255 - alpha))) / 255;
                dest_px[1] = ((src_px->green * alpha) + (dest_px[1] * (255 - alpha))) / 255;
                dest_px[2] = ((src_px->blue * alpha) + (dest_px[2] * (255 - alpha))) / 255;
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_argb8888_blend_normal_to_rgb888_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color32_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(lv_color32_t);
    const int32_t dest_stride = dsc->dest_stride;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    uint8_t * dest_buf = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        const uint32_t * src_row = (uint32_t *)src_buf;
        uint8_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 4 pixels at a time (ARGB8888 is 4 bytes per pixel) */
        for(x = 0; x <= w - 4; x += 4) {
            uint8x8_t mask_alpha = vld1_u8(mask_row + x);
            mask_alpha = vget_low_u8(vcombine_u8(mask_alpha, mask_alpha)); /* Take only 4 values */
            
            /* Skip if all mask values are zero */
            uint32_t mask_val = vget_lane_u32(vreinterpret_u32_u8(mask_alpha), 0);
            if(mask_val == 0) continue;

            /* Load source ARGB8888 data */
            uint32x4_t src_argb = vld1q_u32(src_row + x);
            
            /* Extract alpha from source */
            uint8x8_t src_alpha = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(src_argb), 8));
            src_alpha = vget_low_u8(vcombine_u8(src_alpha, src_alpha));

            /* Combine source alpha with mask: alpha = (src_alpha * mask) / 255 */
            uint8x8_t alpha = vshrn_n_u16(vmull_u8(src_alpha, mask_alpha), 8);
            uint8x8_t inv_alpha = vsub_u8(vdup_n_u8(255), alpha);

            /* Load destination RGB888 data */
            uint8x8x3_t dest_rgb = vld3_u8(dest_row + x * dst_px_size);

            /* Extract RGB from source */
            uint8x8x3_t src_rgb = {
                .val[0] = vmovn_u16(vreinterpretq_u16_u32(src_argb)),          /* R */
                .val[1] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(src_argb), 8)), /* G */
                .val[2] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(src_argb), 16)) /* B */
            };

            /* Blend RGB components */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_rgb.val[0], alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_rgb.val[1], alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_rgb.val[2], alpha), 8);

            vst3_u8(dest_row + x * dst_px_size, dest_rgb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t mask_alpha = mask_row[x];
            if(mask_alpha <= LV_OPA_MIN) continue;

            const lv_color32_t * src_px = &src_buf[x];
            uint8_t * dest_px = dest_row + x * dst_px_size;
            
            /* Combine source alpha with mask */
            lv_opa_t alpha = (src_px->alpha * mask_alpha) / 255;
            if(alpha <= LV_OPA_MIN) continue;

            if(alpha >= LV_OPA_MAX) {
                dest_px[0] = src_px->red;
                dest_px[1] = src_px->green;
                dest_px[2] = src_px->blue;
            } else {
                dest_px[0] = ((src_px->red * alpha) + (dest_px[0] * (255 - alpha))) / 255;
                dest_px[1] = ((src_px->green * alpha) + (dest_px[1] * (255 - alpha))) / 255;
                dest_px[2] = ((src_px->blue * alpha) + (dest_px[2] * (255 - alpha))) / 255;
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_argb8888_blend_normal_to_rgb888_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color32_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(lv_color32_t);
    const int32_t dest_stride = dsc->dest_stride;
    const lv_opa_t opa = dsc->opa;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    uint8_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;

    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        const uint32_t * src_row = (uint32_t *)src_buf;
        uint8_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 4 pixels at a time (ARGB8888 is 4 bytes per pixel) */
        for(x = 0; x <= w - 4; x += 4) {
            uint8x8_t mask_alpha = vld1_u8(mask_row + x);
            mask_alpha = vget_low_u8(vcombine_u8(mask_alpha, mask_alpha)); /* Take only 4 values */
            
            /* Skip if all mask values are zero */
            uint32_t mask_val = vget_lane_u32(vreinterpret_u32_u8(mask_alpha), 0);
            if(mask_val == 0) continue;

            /* Load source ARGB8888 data */
            uint32x4_t src_argb = vld1q_u32(src_row + x);
            
            /* Extract alpha from source */
            uint8x8_t src_alpha = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(src_argb), 8));
            src_alpha = vget_low_u8(vcombine_u8(src_alpha, src_alpha));

            /* Combine source alpha, mask and opacity: alpha = (src_alpha * mask * opa) / (255 * 255) */
            uint16x8_t temp_alpha = vmull_u8(src_alpha, mask_alpha);
            uint8x8_t alpha = vshrn_n_u16(vmull_u8(vget_low_u8(vreinterpretq_u8_u16(temp_alpha)), opa_vec), 8);
            uint8x8_t inv_alpha = vsub_u8(vdup_n_u8(255), alpha);

            /* Load destination RGB888 data */
            uint8x8x3_t dest_rgb = vld3_u8(dest_row + x * dst_px_size);

            /* Extract RGB from source */
            uint8x8x3_t src_rgb = {
                .val[0] = vmovn_u16(vreinterpretq_u16_u32(src_argb)),          /* R */
                .val[1] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(src_argb), 8)), /* G */
                .val[2] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(src_argb), 16)) /* B */
            };

            /* Blend RGB components */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_rgb.val[0], alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_rgb.val[1], alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_rgb.val[2], alpha), 8);

            vst3_u8(dest_row + x * dst_px_size, dest_rgb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t mask_alpha = mask_row[x];
            if(mask_alpha <= LV_OPA_MIN) continue;

            const lv_color32_t * src_px = &src_buf[x];
            uint8_t * dest_px = dest_row + x * dst_px_size;
            
            /* Combine source alpha, mask and opacity */
            lv_opa_t alpha = (src_px->alpha * mask_alpha * opa) / (255 * 255);
            if(alpha <= LV_OPA_MIN) continue;

            if(alpha >= LV_OPA_MAX) {
                dest_px[0] = src_px->red;
                dest_px[1] = src_px->green;
                dest_px[2] = src_px->blue;
            } else {
                dest_px[0] = ((src_px->red * alpha) + (dest_px[0] * (255 - alpha))) / 255;
                dest_px[1] = ((src_px->green * alpha) + (dest_px[1] * (255 - alpha))) / 255;
                dest_px[2] = ((src_px->blue * alpha) + (dest_px[2] * (255 - alpha))) / 255;
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

/* Image blending functions - ARGB8888 destination */
lv_result_t lv_rgb565_blend_normal_to_argb8888_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint16_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(uint16_t);
    const int32_t dest_stride = dsc->dest_stride / sizeof(lv_color32_t);
    lv_color32_t * dest_buf = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        const uint16_t * src_row = src_buf;
        uint32_t * dest_row = (uint32_t *)dest_buf;
        int32_t x = 0;

        /* Process 4 pixels at a time */
        for(x = 0; x <= w - 4; x += 4) {
            /* Load source RGB565 data */
            uint16x4_t src_rgb565 = vld1_u16(src_row + x);
            
            /* Convert RGB565 to ARGB8888 */
            uint32x4_t dest_argb = neon64_argb8888_from_rgb565(src_rgb565);
            
            /* Set alpha to 255 (fully opaque) */
            dest_argb = vorrq_u32(dest_argb, vdupq_n_u32(0xFF000000));
            
            vst1q_u32(dest_row + x, dest_argb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            uint16_t src_val = src_row[x];
            lv_color32_t * dest_px = &dest_buf[x];
            
            /* Convert RGB565 to RGB888 and set alpha */
            dest_px->red = (src_val >> 8) & 0xF8;
            dest_px->green = (src_val >> 3) & 0xFC;
            dest_px->blue = (src_val << 3) & 0xF8;
            dest_px->alpha = 0xFF;
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb565_blend_normal_to_argb8888_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint16_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(uint16_t);
    const int32_t dest_stride = dsc->dest_stride / sizeof(lv_color32_t);
    const lv_opa_t opa = dsc->opa;
    lv_color32_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;
    if(opa >= LV_OPA_MAX) return lv_rgb565_blend_normal_to_argb8888_neon64(dsc);

    const uint8x8_t alpha = vdup_n_u8(opa);
    const uint8x8_t inv_alpha = vdup_n_u8(255 - opa);

    for(int32_t y = 0; y < h; y++) {
        const uint16_t * src_row = src_buf;
        uint32_t * dest_row = (uint32_t *)dest_buf;
        int32_t x = 0;

        /* Process 4 pixels at a time */
        for(x = 0; x <= w - 4; x += 4) {
            /* Load source RGB565 data */
            uint16x4_t src_rgb565 = vld1_u16(src_row + x);
            
            /* Convert RGB565 to RGB888 */
            uint8x8x3_t src_rgb = neon64_rgb888_from_rgb565(vcombine_u16(src_rgb565, src_rgb565));
            
            /* Load destination ARGB8888 data */
            uint32x4_t dest_argb = vld1q_u32(dest_row + x);
            
            /* Extract RGB from destination */
            uint8x8x3_t dest_rgb = {
                .val[0] = vmovn_u16(vreinterpretq_u16_u32(dest_argb)),          /* R */
                .val[1] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(dest_argb), 8)), /* G */
                .val[2] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(dest_argb), 16)) /* B */
            };

            /* Blend RGB components - only use lower 4 values */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(vget_low_u8(dest_rgb.val[0]), inv_alpha), vget_low_u8(src_rgb.val[0]), alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(vget_low_u8(dest_rgb.val[1]), inv_alpha), vget_low_u8(src_rgb.val[1]), alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(vget_low_u8(dest_rgb.val[2]), inv_alpha), vget_low_u8(src_rgb.val[2]), alpha), 8);

            /* Reconstruct ARGB8888 with updated RGB and original alpha */
            uint32x4_t result_argb = vdupq_n_u32(0xFF000000); /* Keep alpha = 255 */
            result_argb = vorrq_u32(result_argb, vmovl_u16(vget_low_u16(vmovl_u8(dest_rgb.val[0])))); /* R */
            result_argb = vorrq_u32(result_argb, vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(dest_rgb.val[1]))), 8)); /* G */
            result_argb = vorrq_u32(result_argb, vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(dest_rgb.val[2]))), 16)); /* B */

            vst1q_u32(dest_row + x, result_argb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            uint16_t src_val = src_row[x];
            lv_color32_t * dest_px = &dest_buf[x];
            
            /* Convert RGB565 to RGB888 */
            uint8_t src_r = (src_val >> 8) & 0xF8;
            uint8_t src_g = (src_val >> 3) & 0xFC;
            uint8_t src_b = (src_val << 3) & 0xF8;

            /* Blend */
            dest_px->red = ((src_r * opa) + (dest_px->red * (255 - opa))) / 255;
            dest_px->green = ((src_g * opa) + (dest_px->green * (255 - opa))) / 255;
            dest_px->blue = ((src_b * opa) + (dest_px->blue * (255 - opa))) / 255;
            /* Keep original alpha */
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb565_blend_normal_to_argb8888_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint16_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(uint16_t);
    const int32_t dest_stride = dsc->dest_stride / sizeof(lv_color32_t);
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    lv_color32_t * dest_buf = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        const uint16_t * src_row = src_buf;
        uint32_t * dest_row = (uint32_t *)dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 4 pixels at a time */
        for(x = 0; x <= w - 4; x += 4) {
            uint8x8_t alpha = vld1_u8(mask_row + x);
            alpha = vget_low_u8(vcombine_u8(alpha, alpha)); /* Take only 4 values */
            
            /* Skip if all alpha values are zero */
            uint32_t alpha_mask = vget_lane_u32(vreinterpret_u32_u8(alpha), 0);
            if(alpha_mask == 0) continue;

            /* Load source RGB565 data */
            uint16x4_t src_rgb565 = vld1_u16(src_row + x);
            
            /* Convert RGB565 to RGB888 */
            uint8x8x3_t src_rgb = neon64_rgb888_from_rgb565(vcombine_u16(src_rgb565, src_rgb565));
            
            /* Load destination ARGB8888 data */
            uint32x4_t dest_argb = vld1q_u32(dest_row + x);
            
            /* Extract RGB from destination */
            uint8x8x3_t dest_rgb = {
                .val[0] = vmovn_u16(vreinterpretq_u16_u32(dest_argb)),          /* R */
                .val[1] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(dest_argb), 8)), /* G */
                .val[2] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(dest_argb), 16)) /* B */
            };

            uint8x8_t inv_alpha = vsub_u8(vdup_n_u8(255), alpha);

            /* Blend RGB components - only use lower 4 values */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(vget_low_u8(dest_rgb.val[0]), inv_alpha), vget_low_u8(src_rgb.val[0]), alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(vget_low_u8(dest_rgb.val[1]), inv_alpha), vget_low_u8(src_rgb.val[1]), alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(vget_low_u8(dest_rgb.val[2]), inv_alpha), vget_low_u8(src_rgb.val[2]), alpha), 8);

            /* Reconstruct ARGB8888 with updated RGB and original alpha */
            uint32x4_t result_argb = vdupq_n_u32(0xFF000000); /* Keep alpha = 255 */
            result_argb = vorrq_u32(result_argb, vmovl_u16(vget_low_u16(vmovl_u8(dest_rgb.val[0])))); /* R */
            result_argb = vorrq_u32(result_argb, vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(dest_rgb.val[1]))), 8)); /* G */
            result_argb = vorrq_u32(result_argb, vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(dest_rgb.val[2]))), 16)); /* B */

            vst1q_u32(dest_row + x, result_argb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t alpha = mask_row[x];
            if(alpha <= LV_OPA_MIN) continue;

            uint16_t src_val = src_row[x];
            lv_color32_t * dest_px = &dest_buf[x];
            
            if(alpha >= LV_OPA_MAX) {
                /* Full opacity - direct conversion */
                dest_px->red = (src_val >> 8) & 0xF8;
                dest_px->green = (src_val >> 3) & 0xFC;
                dest_px->blue = (src_val << 3) & 0xF8;
                dest_px->alpha = 0xFF;
            } else {
                /* Convert RGB565 to RGB888 */
                uint8_t src_r = (src_val >> 8) & 0xF8;
                uint8_t src_g = (src_val >> 3) & 0xFC;
                uint8_t src_b = (src_val << 3) & 0xF8;

                /* Blend */
                dest_px->red = ((src_r * alpha) + (dest_px->red * (255 - alpha))) / 255;
                dest_px->green = ((src_g * alpha) + (dest_px->green * (255 - alpha))) / 255;
                dest_px->blue = ((src_b * alpha) + (dest_px->blue * (255 - alpha))) / 255;
                /* Keep original alpha */
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb565_blend_normal_to_argb8888_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint16_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(uint16_t);
    const int32_t dest_stride = dsc->dest_stride / sizeof(lv_color32_t);
    const lv_opa_t opa = dsc->opa;
    const lv_opa_t * mask = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;
    lv_color32_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;

    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        const uint16_t * src_row = src_buf;
        uint32_t * dest_row = (uint32_t *)dest_buf;
        const lv_opa_t * mask_row = mask;
        int32_t x = 0;

        /* Process 4 pixels at a time */
        for(x = 0; x <= w - 4; x += 4) {
            uint8x8_t mask_alpha = vld1_u8(mask_row + x);
            mask_alpha = vget_low_u8(vcombine_u8(mask_alpha, mask_alpha)); /* Take only 4 values */
            
            /* Skip if all mask values are zero */
            uint32_t mask_val = vget_lane_u32(vreinterpret_u32_u8(mask_alpha), 0);
            if(mask_val == 0) continue;

            /* Combine mask and opacity: alpha = (mask * opa) / 255 */
            uint8x8_t alpha = vshrn_n_u16(vmull_u8(mask_alpha, opa_vec), 8);
            uint8x8_t inv_alpha = vsub_u8(vdup_n_u8(255), alpha);

            /* Load source RGB565 data */
            uint16x4_t src_rgb565 = vld1_u16(src_row + x);
            
            /* Convert RGB565 to RGB888 */
            uint8x8x3_t src_rgb = neon64_rgb888_from_rgb565(vcombine_u16(src_rgb565, src_rgb565));
            
            /* Load destination ARGB8888 data */
            uint32x4_t dest_argb = vld1q_u32(dest_row + x);
            
            /* Extract RGB from destination */
            uint8x8x3_t dest_rgb = {
                .val[0] = vmovn_u16(vreinterpretq_u16_u32(dest_argb)),          /* R */
                .val[1] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(dest_argb), 8)), /* G */
                .val[2] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(dest_argb), 16)) /* B */
            };

            /* Blend RGB components - only use lower 4 values */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(vget_low_u8(dest_rgb.val[0]), inv_alpha), vget_low_u8(src_rgb.val[0]), alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(vget_low_u8(dest_rgb.val[1]), inv_alpha), vget_low_u8(src_rgb.val[1]), alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(vget_low_u8(dest_rgb.val[2]), inv_alpha), vget_low_u8(src_rgb.val[2]), alpha), 8);

            /* Reconstruct ARGB8888 with updated RGB and original alpha */
            uint32x4_t result_argb = vdupq_n_u32(0xFF000000); /* Keep alpha = 255 */
            result_argb = vorrq_u32(result_argb, vmovl_u16(vget_low_u16(vmovl_u8(dest_rgb.val[0])))); /* R */
            result_argb = vorrq_u32(result_argb, vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(dest_rgb.val[1]))), 8)); /* G */
            result_argb = vorrq_u32(result_argb, vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(dest_rgb.val[2]))), 16)); /* B */

            vst1q_u32(dest_row + x, result_argb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t mask_alpha = mask_row[x];
            if(mask_alpha <= LV_OPA_MIN) continue;

            /* Combine mask and opacity */
            lv_opa_t alpha = (mask_alpha * opa) / 255;
            if(alpha <= LV_OPA_MIN) continue;

            uint16_t src_val = src_row[x];
            lv_color32_t * dest_px = &dest_buf[x];
            
            if(alpha >= LV_OPA_MAX) {
                /* Full opacity - direct conversion */
                dest_px->red = (src_val >> 8) & 0xF8;
                dest_px->green = (src_val >> 3) & 0xFC;
                dest_px->blue = (src_val << 3) & 0xF8;
                dest_px->alpha = 0xFF;
            } else {
                /* Convert RGB565 to RGB888 */
                uint8_t src_r = (src_val >> 8) & 0xF8;
                uint8_t src_g = (src_val >> 3) & 0xFC;
                uint8_t src_b = (src_val << 3) & 0xF8;

                /* Blend */
                dest_px->red = ((src_r * alpha) + (dest_px->red * (255 - alpha))) / 255;
                dest_px->green = ((src_g * alpha) + (dest_px->green * (255 - alpha))) / 255;
                dest_px->blue = ((src_b * alpha) + (dest_px->blue * (255 - alpha))) / 255;
                /* Keep original alpha */
            }
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb888_blend_normal_to_argb8888_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint8_t * src_buf = (uint8_t *)dsc->src_buf;
    const int32_t src_stride = dsc->src_stride;
    const int32_t dest_stride = dsc->dest_stride / sizeof(lv_color32_t);
    lv_color32_t * dest_buf = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        const uint8_t * src_row = src_buf;
        uint32_t * dest_row = (uint32_t *)dest_buf;
        int32_t x = 0;

        /* Process 4 pixels at a time */
        for(x = 0; x <= w - 4; x += 4) {
            /* Load source RGB888 data */
            uint8x8x3_t src_rgb = vld3_u8(src_row + x * src_px_size);
            
            /* Convert to ARGB8888 format with full alpha */
            uint32x4_t dest_argb = vdupq_n_u32(0xFF000000); /* Start with alpha = 255 */
            
            /* Pack RGB components into ARGB8888 */
            dest_argb = vorrq_u32(dest_argb, vmovl_u16(vget_low_u16(vmovl_u8(vget_low_u8(src_rgb.val[0]))))); /* R */
            dest_argb = vorrq_u32(dest_argb, vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(vget_low_u8(src_rgb.val[1])))), 8)); /* G */
            dest_argb = vorrq_u32(dest_argb, vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(vget_low_u8(src_rgb.val[2])))), 16)); /* B */
            
            vst1q_u32(dest_row + x, dest_argb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            const uint8_t * src_px = src_row + x * src_px_size;
            lv_color32_t * dest_px = &dest_buf[x];
            
            dest_px->red = src_px[0];
            dest_px->green = src_px[1];
            dest_px->blue = src_px[2];
            dest_px->alpha = 0xFF;
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb888_blend_normal_to_argb8888_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint8_t * src_buf = (uint8_t *)dsc->src_buf;
    const int32_t src_stride = dsc->src_stride;
    const int32_t dest_stride = dsc->dest_stride / sizeof(lv_color32_t);
    const lv_opa_t opa = dsc->opa;
    lv_color32_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;
    if(opa >= LV_OPA_MAX) return lv_rgb888_blend_normal_to_argb8888_neon64(dsc, src_px_size);

    const uint8x8_t alpha = vdup_n_u8(opa);
    const uint8x8_t inv_alpha = vdup_n_u8(255 - opa);

    for(int32_t y = 0; y < h; y++) {
        const uint8_t * src_row = src_buf;
        uint32_t * dest_row = (uint32_t *)dest_buf;
        int32_t x = 0;

        /* Process 4 pixels at a time */
        for(x = 0; x <= w - 4; x += 4) {
            /* Load source RGB888 data */
            uint8x8x3_t src_rgb = vld3_u8(src_row + x * src_px_size);
            
            /* Load destination ARGB8888 data */
            uint32x4_t dest_argb = vld1q_u32(dest_row + x);
            
            /* Extract RGB from destination */
            uint8x8x3_t dest_rgb = {
                .val[0] = vmovn_u16(vreinterpretq_u16_u32(dest_argb)),          /* R */
                .val[1] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(dest_argb), 8)), /* G */
                .val[2] = vmovn_u16(vshrq_n_u16(vreinterpretq_u16_u32(dest_argb), 16)) /* B */
            };

            /* Blend RGB components */
            dest_rgb.val[0] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[0], inv_alpha), src_rgb.val[0], alpha), 8);
            dest_rgb.val[1] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[1], inv_alpha), src_rgb.val[1], alpha), 8);
            dest_rgb.val[2] = vshrn_n_u16(vmlal_u8(vmull_u8(dest_rgb.val[2], inv_alpha), src_rgb.val[2], alpha), 8);

            /* Reconstruct ARGB8888 with updated RGB and original alpha */
            uint32x4_t result_argb = vdupq_n_u32(0xFF000000); /* Keep alpha = 255 */
            result_argb = vorrq_u32(result_argb, vmovl_u16(vget_low_u16(vmovl_u8(vget_low_u8(dest_rgb.val[0]))))); /* R */
            result_argb = vorrq_u32(result_argb, vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(vget_low_u8(dest_rgb.val[1])))), 8)); /* G */
            result_argb = vorrq_u32(result_argb, vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(vget_low_u8(dest_rgb.val[2])))), 16)); /* B */

            vst1q_u32(dest_row + x, result_argb);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            const uint8_t * src_px = src_row + x * src_px_size;
            lv_color32_t * dest_px = &dest_buf[x];
            
            dest_px->red = ((src_px[0] * opa) + (dest_px->red * (255 - opa))) / 255;
            dest_px->green = ((src_px[1] * opa) + (dest_px->green * (255 - opa))) / 255;
            dest_px->blue = ((src_px[2] * opa) + (dest_px->blue * (255 - opa))) / 255;
            /* Keep original alpha */
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb888_blend_normal_to_argb8888_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const uint8_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride;
    const int32_t dest_stride = dsc->dest_stride / sizeof(lv_color32_t);
    lv_color32_t * dest_buf = dsc->dest_buf;
    const lv_opa_t * mask_buf = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;

    for(int32_t y = 0; y < h; y++) {
        const uint8_t * src_row = src_buf;
        lv_color32_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask_buf;
        int32_t x = 0;

        /* Process 8 pixels at a time using NEON */
        for(; x < w - 7; x += 8) {
            /* Load 8 RGB888 pixels (24 bytes) */
            uint8x8x3_t src_rgb = vld3_u8(&src_row[x * 3]);
            
            /* Load 8 ARGB8888 destination pixels */
            uint32x4_t dest1 = vld1q_u32((uint32_t*)&dest_row[x]);
            uint32x4_t dest2 = vld1q_u32((uint32_t*)&dest_row[x + 4]);
            
            /* Load 8 mask values */
            uint8x8_t mask_vec = vld1_u8(&mask_row[x]);
            
            /* Skip pixels with zero mask */
            uint64_t mask_zero = vget_lane_u64(vreinterpret_u64_u8(vceq_u8(mask_vec, vdup_n_u8(0))), 0);
            if(mask_zero == 0xFFFFFFFFFFFFFFFFULL) {
                continue;
            }
            
            /* Convert RGB888 to ARGB8888 with full alpha and apply mask */
            uint8x8_t alpha_vec = mask_vec; /* Use mask as alpha */
            
            /* Interleave RGB with alpha to create ARGB8888 */
            uint8x8x4_t argb;
            argb.val[0] = src_rgb.val[2]; /* B */
            argb.val[1] = src_rgb.val[1]; /* G */
            argb.val[2] = src_rgb.val[0]; /* R */
            argb.val[3] = alpha_vec;      /* A */
            
            /* Store as two 128-bit vectors */
            uint8x16_t argb1 = vcombine_u8(
                vzip1_u8(vzip1_u8(argb.val[0], argb.val[2]), vzip1_u8(argb.val[1], argb.val[3])),
                vzip2_u8(vzip1_u8(argb.val[0], argb.val[2]), vzip1_u8(argb.val[1], argb.val[3]))
            );
            uint8x16_t argb2 = vcombine_u8(
                vzip1_u8(vzip2_u8(argb.val[0], argb.val[2]), vzip2_u8(argb.val[1], argb.val[3])),
                vzip2_u8(vzip2_u8(argb.val[0], argb.val[2]), vzip2_u8(argb.val[1], argb.val[3]))
            );
            
            uint32x4_t src1 = vreinterpretq_u32_u8(argb1);
            uint32x4_t src2 = vreinterpretq_u32_u8(argb2);
            
            /* Alpha blend using the mask as alpha */
            uint8x8_t alpha_low = vget_low_u8(vdupq_lane_u8(vget_low_u8(vcombine_u8(mask_vec, mask_vec)), 0));
            uint8x8_t alpha_high = vget_high_u8(vdupq_lane_u8(vget_high_u8(vcombine_u8(mask_vec, mask_vec)), 0));
            
            uint32x4_t result1 = neon64_alpha_blend_argb8888(src1, dest1, alpha_low);
            uint32x4_t result2 = neon64_alpha_blend_argb8888(src2, dest2, alpha_high);
            
            vst1q_u32((uint32_t*)&dest_row[x], result1);
            vst1q_u32((uint32_t*)&dest_row[x + 4], result2);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            if(mask_row[x] == 0) continue;
            
            lv_color32_t src_argb;
            src_argb.red = src_row[x * 3];
            src_argb.green = src_row[x * 3 + 1];
            src_argb.blue = src_row[x * 3 + 2];
            src_argb.alpha = mask_row[x];
            
            dest_row[x] = lv_color_32_32_mix_neon(src_argb, dest_row[x], mask_row[x]);
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask_buf += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_rgb888_blend_normal_to_argb8888_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_opa_t opa = dsc->opa;
    const uint8_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride;
    const int32_t dest_stride = dsc->dest_stride / sizeof(lv_color32_t);
    lv_color32_t * dest_buf = dsc->dest_buf;
    const lv_opa_t * mask_buf = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;
    if(opa >= LV_OPA_MAX) return lv_rgb888_blend_normal_to_argb8888_with_mask_neon64(dsc, src_px_size);

    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        const uint8_t * src_row = src_buf;
        lv_color32_t * dest_row = dest_buf;
        const lv_opa_t * mask_row = mask_buf;
        int32_t x = 0;

        /* Process 8 pixels at a time using NEON */
        for(; x < w - 7; x += 8) {
            /* Load 8 RGB888 pixels (24 bytes) */
            uint8x8x3_t src_rgb = vld3_u8(&src_row[x * 3]);
            
            /* Load 8 ARGB8888 destination pixels */
            uint32x4_t dest1 = vld1q_u32((uint32_t*)&dest_row[x]);
            uint32x4_t dest2 = vld1q_u32((uint32_t*)&dest_row[x + 4]);
            
            /* Load 8 mask values */
            uint8x8_t mask_vec = vld1_u8(&mask_row[x]);
            
            /* Skip pixels with zero mask */
            uint64_t mask_zero = vget_lane_u64(vreinterpret_u64_u8(vceq_u8(mask_vec, vdup_n_u8(0))), 0);
            if(mask_zero == 0xFFFFFFFFFFFFFFFFULL) {
                continue;
            }
            
            /* Combine mask with opacity */
            uint16x8_t mask16 = vmovl_u8(mask_vec);
            uint16x8_t opa16 = vmovl_u8(opa_vec);
            uint16x8_t combined = vmulq_u16(mask16, opa16);
            combined = vshrq_n_u16(combined, 8);
            uint8x8_t final_alpha = vmovn_u16(combined);
            
            /* Convert RGB888 to ARGB8888 with combined alpha */
            uint8x8x4_t argb;
            argb.val[0] = src_rgb.val[2]; /* B */
            argb.val[1] = src_rgb.val[1]; /* G */
            argb.val[2] = src_rgb.val[0]; /* R */
            argb.val[3] = final_alpha;    /* A */
            
            /* Store as two 128-bit vectors */
            uint8x16_t argb1 = vcombine_u8(
                vzip1_u8(vzip1_u8(argb.val[0], argb.val[2]), vzip1_u8(argb.val[1], argb.val[3])),
                vzip2_u8(vzip1_u8(argb.val[0], argb.val[2]), vzip1_u8(argb.val[1], argb.val[3]))
            );
            uint8x16_t argb2 = vcombine_u8(
                vzip1_u8(vzip2_u8(argb.val[0], argb.val[2]), vzip2_u8(argb.val[1], argb.val[3])),
                vzip2_u8(vzip2_u8(argb.val[0], argb.val[2]), vzip2_u8(argb.val[1], argb.val[3]))
            );
            
            uint32x4_t src1 = vreinterpretq_u32_u8(argb1);
            uint32x4_t src2 = vreinterpretq_u32_u8(argb2);
            
            /* Alpha blend using the combined alpha */
            uint8x8_t alpha_low = vget_low_u8(vcombine_u8(final_alpha, final_alpha));
            uint8x8_t alpha_high = vget_high_u8(vcombine_u8(final_alpha, final_alpha));
            
            uint32x4_t result1 = neon64_alpha_blend_argb8888(src1, dest1, alpha_low);
            uint32x4_t result2 = neon64_alpha_blend_argb8888(src2, dest2, alpha_high);
            
            vst1q_u32((uint32_t*)&dest_row[x], result1);
            vst1q_u32((uint32_t*)&dest_row[x + 4], result2);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t combined_alpha = LV_OPA_MIX2(mask_row[x], opa);
            if(combined_alpha == 0) continue;
            
            lv_color32_t src_argb;
            src_argb.red = src_row[x * 3];
            src_argb.green = src_row[x * 3 + 1];
            src_argb.blue = src_row[x * 3 + 2];
            src_argb.alpha = combined_alpha;
            
            dest_row[x] = lv_color_32_32_mix_neon(src_argb, dest_row[x], combined_alpha);
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask_buf += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_argb8888_blend_normal_to_argb8888_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color32_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(lv_color32_t);
    const int32_t dest_stride = dsc->dest_stride / sizeof(lv_color32_t);
    lv_color32_t * dest_buf = dsc->dest_buf;

    for(int32_t y = 0; y < h; y++) {
        const uint32_t * src_row = (uint32_t *)src_buf;
        uint32_t * dest_row = (uint32_t *)dest_buf;
        int32_t x = 0;

        /* Process 4 pixels at a time using NEON */
        for(; x < w - 3; x += 4) {
            uint32x4_t src_vec = vld1q_u32(&src_row[x]);
            vst1q_u32(&dest_row[x], src_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            dest_buf[x] = src_buf[x];
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_argb8888_blend_normal_to_argb8888_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_opa_t opa = dsc->opa;
    const lv_color32_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(lv_color32_t);
    const int32_t dest_stride = dsc->dest_stride / sizeof(lv_color32_t);
    lv_color32_t * dest_buf = dsc->dest_buf;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;
    if(opa >= LV_OPA_MAX) return lv_argb8888_blend_normal_to_argb8888_neon64(dsc);

    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        const uint32_t * src_row = (uint32_t *)src_buf;
        uint32_t * dest_row = (uint32_t *)dest_buf;
        int32_t x = 0;

        /* Process 4 pixels at a time using NEON */
        for(; x < w - 3; x += 4) {
            uint32x4_t src_vec = vld1q_u32(&src_row[x]);
            uint32x4_t dest_vec = vld1q_u32(&dest_row[x]);
            
            /* Extract alpha from source and combine with opacity */
            uint8x16_t src_bytes = vreinterpretq_u8_u32(src_vec);
            uint8x8_t src_alpha = vget_high_u8(vuzpq_u8(src_bytes, src_bytes).val[1]);
            
            /* Mix source alpha with opacity */
            uint16x8_t alpha16 = vmovl_u8(src_alpha);
            uint16x8_t opa16 = vmovl_u8(opa_vec);
            uint16x8_t combined = vmulq_u16(alpha16, opa16);
            combined = vshrq_n_u16(combined, 8);
            uint8x8_t final_alpha = vmovn_u16(combined);
            
            uint32x4_t result_vec = neon64_alpha_blend_argb8888(src_vec, dest_vec, final_alpha);
            vst1q_u32(&dest_row[x], result_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_color32_t src_with_opa = src_buf[x];
            src_with_opa.alpha = LV_OPA_MIX2(src_buf[x].alpha, opa);
            dest_buf[x] = lv_color_32_32_mix_neon(src_with_opa, dest_buf[x], src_with_opa.alpha);
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_argb8888_blend_normal_to_argb8888_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_color32_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(lv_color32_t);
    const int32_t dest_stride = dsc->dest_stride / sizeof(lv_color32_t);
    lv_color32_t * dest_buf = dsc->dest_buf;
    const lv_opa_t * mask_buf = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;

    for(int32_t y = 0; y < h; y++) {
        const uint32_t * src_row = (uint32_t *)src_buf;
        uint32_t * dest_row = (uint32_t *)dest_buf;
        const lv_opa_t * mask_row = mask_buf;
        int32_t x = 0;

        /* Process 4 pixels at a time using NEON */
        for(; x < w - 3; x += 4) {
            /* Load 4 ARGB8888 source pixels */
            uint32x4_t src_vec = vld1q_u32(&src_row[x]);
            
            /* Load 4 ARGB8888 destination pixels */
            uint32x4_t dest_vec = vld1q_u32(&dest_row[x]);
            
            /* Load 4 mask values */
            uint8x8_t mask_vec = vld1_u8(&mask_row[x]);
            
            /* Skip pixels with zero mask */
            uint32_t mask_zero = vget_lane_u32(vreinterpret_u32_u8(vceq_u8(mask_vec, vdup_n_u8(0))), 0);
            if((mask_zero & 0xFFFFFFFF) == 0xFFFFFFFF) {
                continue;
            }
            
            /* Extract source alpha and combine with mask */
            uint8x16_t src_bytes = vreinterpretq_u8_u32(src_vec);
            uint8x4_t src_alpha = vget_low_u8(vuzpq_u8(src_bytes, src_bytes).val[1]);
            uint8x4_t mask_alpha = vget_low_u8(vcombine_u8(mask_vec, mask_vec));
            
            /* Combine source alpha with mask */
            uint16x4_t alpha16 = vmovl_u8(src_alpha);
            uint16x4_t mask16 = vmovl_u8(mask_alpha);
            uint16x4_t combined = vmul_u16(alpha16, mask16);
            combined = vshr_n_u16(combined, 8);
            uint8x8_t final_alpha = vmovn_u16(vcombine_u16(combined, combined));
            
            /* Alpha blend with combined alpha */
            uint32x4_t result_vec = neon64_alpha_blend_argb8888(src_vec, dest_vec, final_alpha);
            vst1q_u32(&dest_row[x], result_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            if(mask_row[x] == 0) continue;
            
            lv_color32_t src_with_mask = src_buf[x];
            src_with_mask.alpha = LV_OPA_MIX2(src_buf[x].alpha, mask_row[x]);
            dest_buf[x] = lv_color_32_32_mix_neon(src_with_mask, dest_buf[x], src_with_mask.alpha);
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask_buf += mask_stride;
    }

    return LV_RESULT_OK;
}

lv_result_t lv_argb8888_blend_normal_to_argb8888_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc)
{
    const int32_t w = dsc->dest_w;
    const int32_t h = dsc->dest_h;
    const lv_opa_t opa = dsc->opa;
    const lv_color32_t * src_buf = dsc->src_buf;
    const int32_t src_stride = dsc->src_stride / sizeof(lv_color32_t);
    const int32_t dest_stride = dsc->dest_stride / sizeof(lv_color32_t);
    lv_color32_t * dest_buf = dsc->dest_buf;
    const lv_opa_t * mask_buf = dsc->mask_buf;
    const int32_t mask_stride = dsc->mask_stride;

    if(opa <= LV_OPA_MIN) return LV_RESULT_OK;
    if(opa >= LV_OPA_MAX) return lv_argb8888_blend_normal_to_argb8888_with_mask_neon64(dsc);

    const uint8x8_t opa_vec = vdup_n_u8(opa);

    for(int32_t y = 0; y < h; y++) {
        const uint32_t * src_row = (uint32_t *)src_buf;
        uint32_t * dest_row = (uint32_t *)dest_buf;
        const lv_opa_t * mask_row = mask_buf;
        int32_t x = 0;

        /* Process 4 pixels at a time using NEON */
        for(; x < w - 3; x += 4) {
            /* Load 4 ARGB8888 source pixels */
            uint32x4_t src_vec = vld1q_u32(&src_row[x]);
            
            /* Load 4 ARGB8888 destination pixels */
            uint32x4_t dest_vec = vld1q_u32(&dest_row[x]);
            
            /* Load 4 mask values */
            uint8x8_t mask_vec = vld1_u8(&mask_row[x]);
            
            /* Skip pixels with zero mask */
            uint32_t mask_zero = vget_lane_u32(vreinterpret_u32_u8(vceq_u8(mask_vec, vdup_n_u8(0))), 0);
            if((mask_zero & 0xFFFFFFFF) == 0xFFFFFFFF) {
                continue;
            }
            
            /* Extract source alpha and combine with mask and opacity */
            uint8x16_t src_bytes = vreinterpretq_u8_u32(src_vec);
            uint8x4_t src_alpha = vget_low_u8(vuzpq_u8(src_bytes, src_bytes).val[1]);
            uint8x4_t mask_alpha = vget_low_u8(vcombine_u8(mask_vec, mask_vec));
            uint8x4_t opa_alpha = vget_low_u8(vcombine_u8(opa_vec, opa_vec));
            
            /* Triple blend: source_alpha * mask * opacity */
            uint16x4_t alpha16 = vmovl_u8(src_alpha);
            uint16x4_t mask16 = vmovl_u8(mask_alpha);
            uint16x4_t opa16 = vmovl_u8(opa_alpha);
            
            uint16x4_t temp = vmul_u16(alpha16, mask16);
            temp = vshr_n_u16(temp, 8);
            uint16x4_t combined = vmul_u16(temp, opa16);
            combined = vshr_n_u16(combined, 8);
            
            uint8x8_t final_alpha = vmovn_u16(vcombine_u16(combined, combined));
            
            /* Alpha blend with combined alpha */
            uint32x4_t result_vec = neon64_alpha_blend_argb8888(src_vec, dest_vec, final_alpha);
            vst1q_u32(&dest_row[x], result_vec);
        }

        /* Handle remaining pixels */
        for(; x < w; x++) {
            lv_opa_t combined_alpha = LV_OPA_MIX3(src_buf[x].alpha, mask_row[x], opa);
            if(combined_alpha == 0) continue;
            
            lv_color32_t src_with_combined = src_buf[x];
            src_with_combined.alpha = combined_alpha;
            dest_buf[x] = lv_color_32_32_mix_neon(src_with_combined, dest_buf[x], combined_alpha);
        }

        src_buf += src_stride;
        dest_buf += dest_stride;
        mask_buf += mask_stride;
    }

    return LV_RESULT_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* Helper function to blend RGB565 pixels with alpha */
static inline uint16x8_t neon64_alpha_blend_rgb565(uint16x8_t src, uint16x8_t dst, uint8x8_t alpha)
{
    /* Extract RGB channels from RGB565 */
    uint16x8_t src_r = vshrq_n_u16(vandq_u16(src, vdupq_n_u16(0xF800)), 11);
    uint16x8_t src_g = vshrq_n_u16(vandq_u16(src, vdupq_n_u16(0x07E0)), 5);
    uint16x8_t src_b = vandq_u16(src, vdupq_n_u16(0x001F));
    
    uint16x8_t dst_r = vshrq_n_u16(vandq_u16(dst, vdupq_n_u16(0xF800)), 11);
    uint16x8_t dst_g = vshrq_n_u16(vandq_u16(dst, vdupq_n_u16(0x07E0)), 5);
    uint16x8_t dst_b = vandq_u16(dst, vdupq_n_u16(0x001F));
    
    /* Convert alpha to 16-bit */
    uint16x8_t alpha16 = vmovl_u8(alpha);
    uint16x8_t inv_alpha = vsubq_u16(vdupq_n_u16(255), alpha16);
    
    /* Blend each channel */
    uint16x8_t result_r = vshrq_n_u16(vaddq_u16(vmulq_u16(src_r, alpha16), vmulq_u16(dst_r, inv_alpha)), 8);
    uint16x8_t result_g = vshrq_n_u16(vaddq_u16(vmulq_u16(src_g, alpha16), vmulq_u16(dst_g, inv_alpha)), 8);
    uint16x8_t result_b = vshrq_n_u16(vaddq_u16(vmulq_u16(src_b, alpha16), vmulq_u16(dst_b, inv_alpha)), 8);
    
    /* Pack back to RGB565 */
    return vorrq_u16(vorrq_u16(vshlq_n_u16(result_r, 11), vshlq_n_u16(result_g, 5)), result_b);
}

/* Helper function to blend ARGB8888 pixels with alpha */
static inline uint32x4_t neon64_alpha_blend_argb8888(uint32x4_t src, uint32x4_t dst, uint8x8_t alpha)
{
    /* Extract ARGB channels */
    uint8x16_t src_bytes = vreinterpretq_u8_u32(src);
    uint8x16_t dst_bytes = vreinterpretq_u8_u32(dst);
    
    /* Get alpha values for first 4 pixels (alpha is already uint8x8_t, use first 4 elements) */
    uint8x8_t alpha_4 = alpha;
    
    /* Duplicate alpha to match ARGB layout - each alpha value repeated for R,G,B,A */
    uint8x8_t alpha_expanded = vzip_u8(alpha_4, alpha_4).val[0];  /* a0,a0,a1,a1,a2,a2,a3,a3 */
    uint8x16_t alpha16 = vcombine_u8(alpha_expanded, alpha_expanded);
    
    /* Create inverse alpha */
    uint8x16_t inv_alpha = vsubq_u8(vdupq_n_u8(255), alpha16);
    
    /* Perform alpha blending */
    uint16x8_t src_lo = vmull_u8(vget_low_u8(src_bytes), vget_low_u8(alpha16));
    uint16x8_t src_hi = vmull_u8(vget_high_u8(src_bytes), vget_high_u8(alpha16));
    
    uint16x8_t dst_lo = vmull_u8(vget_low_u8(dst_bytes), vget_low_u8(inv_alpha));
    uint16x8_t dst_hi = vmull_u8(vget_high_u8(dst_bytes), vget_high_u8(inv_alpha));
    
    uint16x8_t result_lo = vshrq_n_u16(vaddq_u16(src_lo, dst_lo), 8);
    uint16x8_t result_hi = vshrq_n_u16(vaddq_u16(src_hi, dst_hi), 8);
    
    uint8x16_t result_bytes = vcombine_u8(vmovn_u16(result_lo), vmovn_u16(result_hi));
    
    return vreinterpretq_u32_u8(result_bytes);
}

/* NEON optimized 32-bit color mixing function */
static inline lv_color32_t lv_color_32_32_mix_neon(lv_color32_t fg, lv_color32_t bg, lv_opa_t opa)
{
    if(opa <= LV_OPA_MIN) return bg;
    if(opa >= LV_OPA_MAX) return fg;
    
    /* Pack colors into 32-bit values */
    uint32_t fg_u32 = (fg.alpha << 24) | (fg.red << 16) | (fg.green << 8) | fg.blue;
    uint32_t bg_u32 = (bg.alpha << 24) | (bg.red << 16) | (bg.green << 8) | bg.blue;
    
    /* Load into NEON vectors */
    uint32x4_t fg_vec = vdupq_n_u32(fg_u32);
    uint32x4_t bg_vec = vdupq_n_u32(bg_u32);
    uint8x8_t opa_vec = vdup_n_u8(opa);
    
    /* Perform blending */
    uint32x4_t result_vec = neon64_alpha_blend_argb8888(fg_vec, bg_vec, opa_vec);
    
    /* Extract result */
    uint32_t result_u32 = vgetq_lane_u32(result_vec, 0);
    
    lv_color32_t result;
    result.blue = result_u32 & 0xFF;
    result.green = (result_u32 >> 8) & 0xFF;
    result.red = (result_u32 >> 16) & 0xFF;
    result.alpha = (result_u32 >> 24) & 0xFF;
    
    return result;
}

/* Placeholder helper functions for color format conversion */
static inline uint16x8_t neon64_rgb565_from_rgb888(uint8x8x3_t rgb888)
{
    /* Convert RGB888 to RGB565 using NEON */
    uint16x8_t r = vshrq_n_u16(vmovl_u8(rgb888.val[0]), 3);
    uint16x8_t g = vshrq_n_u16(vmovl_u8(rgb888.val[1]), 2);
    uint16x8_t b = vshrq_n_u16(vmovl_u8(rgb888.val[2]), 3);
    
    return vorrq_u16(vorrq_u16(vshlq_n_u16(r, 11), vshlq_n_u16(g, 5)), b);
}

static inline uint8x8x3_t neon64_rgb888_from_rgb565(uint16x8_t rgb565)
{
    /* Convert RGB565 to RGB888 using NEON */
    uint8x8x3_t result;
    
    result.val[0] = vmovn_u16(vshlq_n_u16(vshrq_n_u16(rgb565, 11), 3));
    result.val[1] = vmovn_u16(vshlq_n_u16(vandq_u16(vshrq_n_u16(rgb565, 5), vdupq_n_u16(0x3F)), 2));
    result.val[2] = vmovn_u16(vshlq_n_u16(vandq_u16(rgb565, vdupq_n_u16(0x1F)), 3));
    
    return result;
}

static inline uint32x4_t neon64_argb8888_from_rgb565(uint16x4_t rgb565)
{
    /* Convert RGB565 to ARGB8888 manually for 4 pixels */
    uint32_t result[4];
    uint16_t temp[4];
    vst1_u16(temp, rgb565);
    
    for(int i = 0; i < 4; i++) {
        uint8_t r = (temp[i] >> 11) << 3;
        uint8_t g = ((temp[i] >> 5) & 0x3F) << 2;
        uint8_t b = (temp[i] & 0x1F) << 3;
        result[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
    }
    
    return vld1q_u32(result);
}

static inline uint16x4_t neon64_rgb565_from_argb8888(uint32x4_t argb8888)
{
    /* Convert ARGB8888 to RGB565 using NEON */
    uint8x16_t bytes = vreinterpretq_u8_u32(argb8888);
    
    uint8x8_t r = vget_low_u8(vuzpq_u8(bytes, bytes).val[1]);
    uint8x8_t g = vget_low_u8(vuzpq_u8(bytes, bytes).val[0]);
    uint8x8_t b = vget_low_u8(bytes);
    
    uint16x4_t r16 = vget_low_u16(vshrq_n_u16(vmovl_u8(r), 3));
    uint16x4_t g16 = vget_low_u16(vshrq_n_u16(vmovl_u8(g), 2));
    uint16x4_t b16 = vget_low_u16(vshrq_n_u16(vmovl_u8(b), 3));
    
    return vorr_u16(vorr_u16(vshl_n_u16(r16, 11), vshl_n_u16(g16, 5)), b16);
}

#endif /* LV_USE_DRAW_SW_ASM == LV_DRAW_SW_ASM_NEON64 */
