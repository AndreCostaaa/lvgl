/**
 * @file lv_blend_neon64.h
 *
 */

#ifndef LV_BLEND_NEON64_H
#define LV_BLEND_NEON64_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../../../lv_conf_internal.h"

#ifdef LV_DRAW_SW_NEON_CUSTOM_INCLUDE
#include LV_DRAW_SW_NEON_CUSTOM_INCLUDE
#endif

#if LV_USE_DRAW_SW_ASM == LV_DRAW_SW_ASM_NEON64

#include "../../../../misc/lv_types.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      MACROS
 **********************/

#ifndef LV_DRAW_SW_COLOR_BLEND_TO_RGB565
#define LV_DRAW_SW_COLOR_BLEND_TO_RGB565(dsc) lv_color_blend_to_rgb565_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_COLOR_BLEND_TO_RGB565_WITH_OPA
#define LV_DRAW_SW_COLOR_BLEND_TO_RGB565_WITH_OPA(dsc) lv_color_blend_to_rgb565_with_opa_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_COLOR_BLEND_TO_RGB565_WITH_MASK
#define LV_DRAW_SW_COLOR_BLEND_TO_RGB565_WITH_MASK(dsc) lv_color_blend_to_rgb565_with_mask_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_COLOR_BLEND_TO_RGB565_MIX_MASK_OPA
#define LV_DRAW_SW_COLOR_BLEND_TO_RGB565_MIX_MASK_OPA(dsc) lv_color_blend_to_rgb565_mix_mask_opa_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB565
#define LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB565(dsc) lv_rgb565_blend_normal_to_rgb565_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB565_WITH_OPA
#define LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB565_WITH_OPA(dsc) lv_rgb565_blend_normal_to_rgb565_with_opa_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB565_WITH_MASK
#define LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB565_WITH_MASK(dsc) lv_rgb565_blend_normal_to_rgb565_with_mask_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB565_MIX_MASK_OPA
#define LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB565_MIX_MASK_OPA(dsc) lv_rgb565_blend_normal_to_rgb565_mix_mask_opa_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB565
#define LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB565(dsc, src_px_size) lv_rgb888_blend_normal_to_rgb565_neon64(dsc, src_px_size)
#endif

#ifndef LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB565_WITH_OPA
#define LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB565_WITH_OPA(dsc, src_px_size) lv_rgb888_blend_normal_to_rgb565_with_opa_neon64(dsc, src_px_size)
#endif

#ifndef LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB565_WITH_MASK
#define LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB565_WITH_MASK(dsc, src_px_size) lv_rgb888_blend_normal_to_rgb565_with_mask_neon64(dsc, src_px_size)
#endif

#ifndef LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB565_MIX_MASK_OPA
#define LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB565_MIX_MASK_OPA(dsc, src_px_size) lv_rgb888_blend_normal_to_rgb565_mix_mask_opa_neon64(dsc, src_px_size)
#endif

#ifndef LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB565
#define LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB565(dsc) lv_argb8888_blend_normal_to_rgb565_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB565_WITH_OPA
#define LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB565_WITH_OPA(dsc) lv_argb8888_blend_normal_to_rgb565_with_opa_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB565_WITH_MASK
#define LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB565_WITH_MASK(dsc) lv_argb8888_blend_normal_to_rgb565_with_mask_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB565_MIX_MASK_OPA
#define LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB565_MIX_MASK_OPA(dsc) lv_argb8888_blend_normal_to_rgb565_mix_mask_opa_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_COLOR_BLEND_TO_RGB888
#define LV_DRAW_SW_COLOR_BLEND_TO_RGB888(dsc, dst_px_size) lv_color_blend_to_rgb888_neon64(dsc, dst_px_size)
#endif

#ifndef LV_DRAW_SW_COLOR_BLEND_TO_RGB888_WITH_OPA
#define LV_DRAW_SW_COLOR_BLEND_TO_RGB888_WITH_OPA(dsc, dst_px_size) lv_color_blend_to_rgb888_with_opa_neon64(dsc, dst_px_size)
#endif

#ifndef LV_DRAW_SW_COLOR_BLEND_TO_RGB888_WITH_MASK
#define LV_DRAW_SW_COLOR_BLEND_TO_RGB888_WITH_MASK(dsc, dst_px_size) lv_color_blend_to_rgb888_with_mask_neon64(dsc, dst_px_size)
#endif

#ifndef LV_DRAW_SW_COLOR_BLEND_TO_RGB888_MIX_MASK_OPA
#define LV_DRAW_SW_COLOR_BLEND_TO_RGB888_MIX_MASK_OPA(dsc, dst_px_size) lv_color_blend_to_rgb888_mix_mask_opa_neon64(dsc, dst_px_size)
#endif

#ifndef LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB888
#define LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB888(dsc, dst_px_size) lv_rgb565_blend_normal_to_rgb888_neon64(dsc, dst_px_size)
#endif

#ifndef LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB888_WITH_OPA
#define LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB888_WITH_OPA(dsc, dst_px_size) lv_rgb565_blend_normal_to_rgb888_with_opa_neon64(dsc, dst_px_size)
#endif

#ifndef LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB888_WITH_MASK
#define LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB888_WITH_MASK(dsc, dst_px_size) lv_rgb565_blend_normal_to_rgb888_with_mask_neon64(dsc, dst_px_size)
#endif

#ifndef LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB888_MIX_MASK_OPA
#define LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_RGB888_MIX_MASK_OPA(dsc, dst_px_size) lv_rgb565_blend_normal_to_rgb888_mix_mask_opa_neon64(dsc, dst_px_size)
#endif

#ifndef LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB888
#define LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB888(dsc, dst_px_size, src_px_size) lv_rgb888_blend_normal_to_rgb888_neon64(dsc, dst_px_size, src_px_size)
#endif

#ifndef LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB888_WITH_OPA
#define LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB888_WITH_OPA(dsc, dst_px_size, src_px_size) lv_rgb888_blend_normal_to_rgb888_with_opa_neon64(dsc, dst_px_size, src_px_size)
#endif

#ifndef LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB888_WITH_MASK
#define LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB888_WITH_MASK(dsc, dst_px_size, src_px_size) lv_rgb888_blend_normal_to_rgb888_with_mask_neon64(dsc, dst_px_size, src_px_size)
#endif

#ifndef LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB888_MIX_MASK_OPA
#define LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_RGB888_MIX_MASK_OPA(dsc, dst_px_size, src_px_size) lv_rgb888_blend_normal_to_rgb888_mix_mask_opa_neon64(dsc, dst_px_size, src_px_size)
#endif

#ifndef LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB888
#define LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB888(dsc, dst_px_size) lv_argb8888_blend_normal_to_rgb888_neon64(dsc, dst_px_size)
#endif

#ifndef LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB888_WITH_OPA
#define LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB888_WITH_OPA(dsc, dst_px_size) lv_argb8888_blend_normal_to_rgb888_with_opa_neon64(dsc, dst_px_size)
#endif

#ifndef LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB888_WITH_MASK
#define LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB888_WITH_MASK(dsc, dst_px_size) lv_argb8888_blend_normal_to_rgb888_with_mask_neon64(dsc, dst_px_size)
#endif

#ifndef LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB888_MIX_MASK_OPA
#define LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_RGB888_MIX_MASK_OPA(dsc, dst_px_size) lv_argb8888_blend_normal_to_rgb888_mix_mask_opa_neon64(dsc, dst_px_size)
#endif

#ifndef LV_DRAW_SW_COLOR_BLEND_TO_ARGB8888
#define LV_DRAW_SW_COLOR_BLEND_TO_ARGB8888(dsc) lv_color_blend_to_argb8888_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_COLOR_BLEND_TO_ARGB8888_WITH_OPA
#define LV_DRAW_SW_COLOR_BLEND_TO_ARGB8888_WITH_OPA(dsc) lv_color_blend_to_argb8888_with_opa_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_COLOR_BLEND_TO_ARGB8888_WITH_MASK
#define LV_DRAW_SW_COLOR_BLEND_TO_ARGB8888_WITH_MASK(dsc) lv_color_blend_to_argb8888_with_mask_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_COLOR_BLEND_TO_ARGB8888_MIX_MASK_OPA
#define LV_DRAW_SW_COLOR_BLEND_TO_ARGB8888_MIX_MASK_OPA(dsc) lv_color_blend_to_argb8888_mix_mask_opa_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_ARGB8888
#define LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_ARGB8888(dsc) lv_rgb565_blend_normal_to_argb8888_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_ARGB8888_WITH_OPA
#define LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_ARGB8888_WITH_OPA(dsc) lv_rgb565_blend_normal_to_argb8888_with_opa_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_ARGB8888_WITH_MASK
#define LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_ARGB8888_WITH_MASK(dsc) lv_rgb565_blend_normal_to_argb8888_with_mask_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_ARGB8888_MIX_MASK_OPA
#define LV_DRAW_SW_RGB565_BLEND_NORMAL_TO_ARGB8888_MIX_MASK_OPA(dsc) lv_rgb565_blend_normal_to_argb8888_mix_mask_opa_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_ARGB8888
#define LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_ARGB8888(dsc, src_px_size) lv_rgb888_blend_normal_to_argb8888_neon64(dsc, src_px_size)
#endif

#ifndef LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_ARGB8888_WITH_OPA
#define LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_ARGB8888_WITH_OPA(dsc, src_px_size) lv_rgb888_blend_normal_to_argb8888_with_opa_neon64(dsc, src_px_size)
#endif

#ifndef LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_ARGB8888_WITH_MASK
#define LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_ARGB8888_WITH_MASK(dsc, src_px_size) lv_rgb888_blend_normal_to_argb8888_with_mask_neon64(dsc, src_px_size)
#endif

#ifndef LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_ARGB8888_MIX_MASK_OPA
#define LV_DRAW_SW_RGB888_BLEND_NORMAL_TO_ARGB8888_MIX_MASK_OPA(dsc, src_px_size) lv_rgb888_blend_normal_to_argb8888_mix_mask_opa_neon64(dsc, src_px_size)
#endif

#ifndef LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_ARGB8888
#define LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_ARGB8888(dsc) lv_argb8888_blend_normal_to_argb8888_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_ARGB8888_WITH_OPA
#define LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_ARGB8888_WITH_OPA(dsc) lv_argb8888_blend_normal_to_argb8888_with_opa_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_ARGB8888_WITH_MASK
#define LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_ARGB8888_WITH_MASK(dsc) lv_argb8888_blend_normal_to_argb8888_with_mask_neon64(dsc)
#endif

#ifndef LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_ARGB8888_MIX_MASK_OPA
#define LV_DRAW_SW_ARGB8888_BLEND_NORMAL_TO_ARGB8888_MIX_MASK_OPA(dsc) lv_argb8888_blend_normal_to_argb8888_mix_mask_opa_neon64(dsc)
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/* Color fill functions */
lv_result_t lv_color_blend_to_rgb565_neon64(lv_draw_sw_blend_fill_dsc_t * dsc);
lv_result_t lv_color_blend_to_rgb565_with_opa_neon64(lv_draw_sw_blend_fill_dsc_t * dsc);
lv_result_t lv_color_blend_to_rgb565_with_mask_neon64(lv_draw_sw_blend_fill_dsc_t * dsc);
lv_result_t lv_color_blend_to_rgb565_mix_mask_opa_neon64(lv_draw_sw_blend_fill_dsc_t * dsc);

lv_result_t lv_color_blend_to_rgb888_neon64(lv_draw_sw_blend_fill_dsc_t * dsc, int32_t dst_px_size);
lv_result_t lv_color_blend_to_rgb888_with_opa_neon64(lv_draw_sw_blend_fill_dsc_t * dsc, int32_t dst_px_size);
lv_result_t lv_color_blend_to_rgb888_with_mask_neon64(lv_draw_sw_blend_fill_dsc_t * dsc, int32_t dst_px_size);
lv_result_t lv_color_blend_to_rgb888_mix_mask_opa_neon64(lv_draw_sw_blend_fill_dsc_t * dsc, int32_t dst_px_size);

lv_result_t lv_color_blend_to_argb8888_neon64(lv_draw_sw_blend_fill_dsc_t * dsc);
lv_result_t lv_color_blend_to_argb8888_with_opa_neon64(lv_draw_sw_blend_fill_dsc_t * dsc);
lv_result_t lv_color_blend_to_argb8888_with_mask_neon64(lv_draw_sw_blend_fill_dsc_t * dsc);
lv_result_t lv_color_blend_to_argb8888_mix_mask_opa_neon64(lv_draw_sw_blend_fill_dsc_t * dsc);

/* Image blending functions - RGB565 destination */
lv_result_t lv_rgb565_blend_normal_to_rgb565_neon64(lv_draw_sw_blend_image_dsc_t * dsc);
lv_result_t lv_rgb565_blend_normal_to_rgb565_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc);
lv_result_t lv_rgb565_blend_normal_to_rgb565_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc);
lv_result_t lv_rgb565_blend_normal_to_rgb565_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc);

lv_result_t lv_rgb888_blend_normal_to_rgb565_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size);
lv_result_t lv_rgb888_blend_normal_to_rgb565_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size);
lv_result_t lv_rgb888_blend_normal_to_rgb565_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size);
lv_result_t lv_rgb888_blend_normal_to_rgb565_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size);

lv_result_t lv_argb8888_blend_normal_to_rgb565_neon64(lv_draw_sw_blend_image_dsc_t * dsc);
lv_result_t lv_argb8888_blend_normal_to_rgb565_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc);
lv_result_t lv_argb8888_blend_normal_to_rgb565_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc);
lv_result_t lv_argb8888_blend_normal_to_rgb565_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc);

/* Image blending functions - RGB888 destination */
lv_result_t lv_rgb565_blend_normal_to_rgb888_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size);
lv_result_t lv_rgb565_blend_normal_to_rgb888_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size);
lv_result_t lv_rgb565_blend_normal_to_rgb888_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size);
lv_result_t lv_rgb565_blend_normal_to_rgb888_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size);

lv_result_t lv_rgb888_blend_normal_to_rgb888_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size, int32_t src_px_size);
lv_result_t lv_rgb888_blend_normal_to_rgb888_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size, int32_t src_px_size);
lv_result_t lv_rgb888_blend_normal_to_rgb888_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size, int32_t src_px_size);
lv_result_t lv_rgb888_blend_normal_to_rgb888_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size, int32_t src_px_size);

lv_result_t lv_argb8888_blend_normal_to_rgb888_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size);
lv_result_t lv_argb8888_blend_normal_to_rgb888_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size);
lv_result_t lv_argb8888_blend_normal_to_rgb888_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size);
lv_result_t lv_argb8888_blend_normal_to_rgb888_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t dst_px_size);

/* Image blending functions - ARGB8888 destination */
lv_result_t lv_rgb565_blend_normal_to_argb8888_neon64(lv_draw_sw_blend_image_dsc_t * dsc);
lv_result_t lv_rgb565_blend_normal_to_argb8888_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc);
lv_result_t lv_rgb565_blend_normal_to_argb8888_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc);
lv_result_t lv_rgb565_blend_normal_to_argb8888_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc);

lv_result_t lv_rgb888_blend_normal_to_argb8888_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size);
lv_result_t lv_rgb888_blend_normal_to_argb8888_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size);
lv_result_t lv_rgb888_blend_normal_to_argb8888_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size);
lv_result_t lv_rgb888_blend_normal_to_argb8888_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc, int32_t src_px_size);

lv_result_t lv_argb8888_blend_normal_to_argb8888_neon64(lv_draw_sw_blend_image_dsc_t * dsc);
lv_result_t lv_argb8888_blend_normal_to_argb8888_with_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc);
lv_result_t lv_argb8888_blend_normal_to_argb8888_with_mask_neon64(lv_draw_sw_blend_image_dsc_t * dsc);
lv_result_t lv_argb8888_blend_normal_to_argb8888_mix_mask_opa_neon64(lv_draw_sw_blend_image_dsc_t * dsc);

#endif /* LV_USE_DRAW_SW_ASM == LV_DRAW_SW_ASM_NEON64 */

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_BLEND_NEON_H*/
