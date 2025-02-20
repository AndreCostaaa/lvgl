/**
 * @file lv_demo_benchmark.h
 *
 */

#ifndef LV_DEMO_BENCHMARK_H
#define LV_DEMO_BENCHMARK_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../lv_demos.h"

#if LV_USE_DEMO_BENCHMARK

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    const char * name;
    void (*create_cb)(void);
    uint32_t scene_time;
    uint32_t cpu_avg_usage;
    uint32_t fps_avg;
    uint32_t render_avg_time;
    uint32_t flush_avg_time;
    uint32_t measurement_cnt;
} lv_benchmark_scene_dsc_t;

typedef struct {
    /*
     * List of scenes
     * `create_cb` == NULL indicates the last stored scene
     * Must not be free'd
     */
    lv_benchmark_scene_dsc_t * scenes;

    int32_t total_avg_fps;
    int32_t total_avg_cpu;
    int32_t total_avg_render_time;
    int32_t total_avg_flush_time;
    int32_t valid_scene_cnt;
} lv_benchmark_summary_t;

typedef void (*lv_on_benchmark_end_cb_t)(const lv_benchmark_summary_t *);

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Run all benchmark scenes.
 *
 * On the summary end screen the values shall be interpreted according to the following:
 * - CPU usage:
 *    - If `LV_SYSMON_GET_IDLE` is not modified it's measured based on the time spent in
 *      `lv_timer_handler`.
 *    - If an (RT)OS is used `LV_SYSMON_GET_IDLE` can be changed to a custom function
 *      which returns the idle percentage of idle task.
 *
 * - FPS: LVGL attempted to render this many times in a second. It's limited based on `LV_DEF_REFR_PERIOD`
 *
 * - Render time: LVGL spent this much time with rendering only. It's not aware of task yielding,
 *   but simply the time difference between the start and end of the rendering is measured
 *
 * - Flush time: It's the sum of
 *     - the time spent in the `flush_cb` and
 *     - the time spent with waiting for flush ready.
 */
void lv_demo_benchmark(void);

/*
 * Register a function to call when the benchmark demo is over
 */
void lv_on_demo_benchmark_end(lv_on_benchmark_end_cb_t cb);


/*
 * Display and log the summary
 * This function is called automatically if `lv_on_benchmark_end_cb` is not set
 */
void lv_demo_benchmark_summary_display(const lv_benchmark_summary_t * summary);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_DEMO_BENCHMARK*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*LV_DEMO_BENCHMARK_H*/
