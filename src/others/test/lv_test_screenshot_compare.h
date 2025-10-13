/**
 * @file lv_test_screenshot_compare.h
 *
 */

#ifndef LV_TEST_SCREENSHOT_COMPARE_H
#define LV_TEST_SCREENSHOT_COMPARE_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../../lv_conf_internal.h"
#if LV_USE_TEST && defined(LV_USE_TEST_SCREENSHOT_COMPARE) && LV_USE_TEST_SCREENSHOT_COMPARE

#include "../../misc/lv_types.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Compare the current content of the test screen with a reference PNG image
 * - If the reference image is not found it will be created automatically from the rendered screen.
 * - If the compare fails an `<image_name>_err.png` file will be created with the rendered content next to the reference image.
 *
 * It requires libPNG.
 *
 * @param fn_ref    path to the reference image. Will be appended toREF_IMGS_PATH if set.
 * @return          true: the reference image and the display are the same; false: they are different (`<image_name>_err.png` is created).
 * @note            This function assumes that the default display is the test display that was created by
 *                  `lv_test_display_create()`
 */
bool lv_test_screenshot_compare(const char * fn_ref);

/**
 * Check if a screenshot exists in path
 *
 * @param path    path to the reference image. Will be appended to REF_IMGS_PATH if set.
 * @return          true: the reference image exists; false: the reference image does not exist.
 */
bool lv_test_screenshot_exists(const char * path);

/**
 * Generate a screenshot from the current display
 *
 * It requires lodepng.
 *
 * @param path    path to the output image. Will be appended to REF_IMGS_PATH if set.
 * @return          LV_RESULT_OK Screenshot generated successfully; LV_RESULT_INVALID Failed to generate screenshot
 * @note            This function assumes that the default display is the test display that was created by
 *                  `lv_test_display_create()`
 */
lv_result_t lv_test_screenshot_generate(const char * path);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_TEST_SCREENSHOT_COMPARE*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_TEST_SCREENSHOT_COMPARE_H*/
