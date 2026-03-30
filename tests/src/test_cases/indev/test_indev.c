#if LV_BUILD_TEST
#include "../lvgl.h"
#include "unity/unity.h"

void setUp(void)
{
    /* Function run before every test */
}

void tearDown(void)
{
    /* Function run after every test */
    lv_obj_clean(lv_screen_active());
}

static void indev_reset_event_cb(lv_event_t * e)
{
    lv_obj_t * scroll_obj = lv_event_get_target(e);
    lv_obj_t * act_obj = lv_obj_get_child(scroll_obj, 0);
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_SCROLL && act_obj) {
        lv_obj_delete(act_obj);
    }
    else if(code == LV_EVENT_INDEV_RESET) {
        uint32_t * indev_reset_count = lv_event_get_user_data(e);
        (*indev_reset_count)++;
    }
}

void test_indev_obj_delete_reset(void)
{
    lv_obj_t * scroll_obj = lv_obj_create(lv_screen_active());
    lv_obj_set_size(scroll_obj, 300, 300);
    lv_obj_align(scroll_obj, LV_ALIGN_LEFT_MID, 0, 0);

    uint32_t indev_reset_count = 0;
    lv_obj_add_event_cb(scroll_obj, indev_reset_event_cb, LV_EVENT_ALL, &indev_reset_count);

    lv_obj_t * act_obj = lv_obj_create(scroll_obj);
    lv_obj_set_size(act_obj, 400, 200);
    lv_obj_align(act_obj, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_update_layout(scroll_obj);

    lv_test_mouse_move_to(200, 200);
    lv_test_mouse_press();
    lv_test_wait(50);

    lv_test_mouse_move_by(-20, 0);
    lv_test_mouse_press();
    lv_test_wait(50);

    lv_test_mouse_move_by(-20, 0);
    lv_test_mouse_press();
    lv_test_wait(50);

    lv_test_mouse_release();

    TEST_ASSERT_EQUAL_UINT32(1, indev_reset_count);
}

static void indev_wait_release_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    uint32_t * pressed_count = lv_event_get_user_data(e);

    switch(lv_event_get_code(e)) {
        case LV_EVENT_PRESSED:
            (*pressed_count)++;
            lv_indev_wait_release(lv_indev_active());
            break;
        case LV_EVENT_PRESS_LOST:
            lv_indev_reset(lv_indev_active(), obj);
            break;
        default:
            break;
    }
}

void test_indev_wait_release(void)
{
    uint32_t pressed_count = 0;
    lv_obj_t * btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn, 100, 100);
    lv_obj_add_event_cb(btn, indev_wait_release_event_cb, LV_EVENT_ALL, &pressed_count);

    lv_test_mouse_release();
    lv_test_wait(50);
    lv_test_mouse_move_to(50, 50);

    lv_test_mouse_press();
    lv_test_wait(50);
    lv_test_mouse_release();
    lv_test_wait(50);

    lv_test_mouse_press();
    lv_test_wait(50);
    lv_test_mouse_release();
    lv_test_wait(50);

    TEST_ASSERT_EQUAL_UINT32(2, pressed_count);
}

static void indev_long_pressed_event_cb(lv_event_t * e)
{
    uint32_t * long_pressed_cnt = lv_event_get_user_data(e);
    (*long_pressed_cnt)++;
}

void test_indev_long_pressed(void)
{
    uint32_t long_pressed_cnt = 0;
    lv_obj_t * btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn, 100, 100);
    lv_obj_add_event_cb(btn, indev_long_pressed_event_cb, LV_EVENT_LONG_PRESSED, &long_pressed_cnt);
    lv_test_mouse_release();
    lv_test_wait(50);
    lv_test_mouse_move_to(50, 50);
    lv_test_mouse_press();
    lv_test_wait(500);
    lv_test_mouse_release();
    lv_test_wait(50);
    TEST_ASSERT_EQUAL_UINT32(1, long_pressed_cnt);
}

static void indev_scroll_press_event_cb(lv_event_t * e)
{
    uint32_t * pressed_count = lv_event_get_user_data(e);

    switch(lv_event_get_code(e)) {
        case LV_EVENT_PRESSED:
            (*pressed_count)++;
            break;
        default:
            break;
    }
}

static void indev_scroll_press_lost_event_cb(lv_event_t * e)
{
    uint32_t * press_lost_count = lv_event_get_user_data(e);

    switch(lv_event_get_code(e)) {
        case LV_EVENT_PRESS_LOST:
            (*press_lost_count)++;
            break;
        default:
            break;
    }
}

void test_indev_scroll_between_two_buttons_with_and_without_press_lock(void)
{
    uint32_t pressed_count_1 = 0;
    uint32_t pressed_lost_count_1 = 0;
    uint32_t pressed_count_2 = 0;
    uint32_t pressed_lost_count_2 = 0;

    lv_obj_t * btn1 = lv_button_create(lv_screen_active());
    lv_obj_remove_flag(btn1, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_size(btn1, 120, 100);
    lv_obj_set_pos(btn1, 300, 200);
    lv_obj_add_event_cb(btn1, indev_scroll_press_event_cb, LV_EVENT_PRESSED,
                        &pressed_count_1);
    lv_obj_add_event_cb(btn1, indev_scroll_press_lost_event_cb,
                        LV_EVENT_PRESS_LOST, &pressed_lost_count_1);

    lv_obj_t * btn2 = lv_button_create(lv_screen_active());
    lv_obj_set_style_bg_color(btn2, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_size(btn2, 120, 100);
    lv_obj_set_pos(btn2, 900, 200);
    lv_obj_add_event_cb(btn2, indev_scroll_press_event_cb, LV_EVENT_PRESSED,
                        &pressed_count_2);
    lv_obj_add_event_cb(btn2, indev_scroll_press_lost_event_cb,
                        LV_EVENT_PRESS_LOST, &pressed_lost_count_2);

    TEST_ASSERT_EQUAL_SCREENSHOT("scroll_initial.png");

    lv_test_mouse_move_to(350, 250);
    lv_test_mouse_press();
    lv_test_wait(50);
    TEST_ASSERT_EQUAL_UINT32(1, pressed_count_1);

    lv_test_mouse_move_by(-300, 0);
    lv_test_wait(50);
    lv_test_mouse_release();
    lv_test_wait(1000);
    TEST_ASSERT_EQUAL_UINT32(1, pressed_lost_count_1);
    TEST_ASSERT_EQUAL_SCREENSHOT("scroll_after.png");

    lv_test_mouse_move_to(750, 250);
    lv_test_mouse_press();
    lv_test_wait(50);
    TEST_ASSERT_EQUAL_UINT32(1, pressed_count_2);

    lv_test_mouse_move_by(300, 0);
    lv_test_wait(50);
    lv_test_mouse_release();
    lv_test_wait(1000);
    TEST_ASSERT_EQUAL_UINT32(0, pressed_lost_count_2);
    TEST_ASSERT_EQUAL_SCREENSHOT("scroll_initial.png");
}

#define MAX_KEYS 6

struct key_buffer {
    size_t count;
    lv_key_t keys[MAX_KEYS];
};

static void key_buffer_add(struct key_buffer * key_buffer, lv_key_t key)
{
    assert(key_buffer->count + 1 <= MAX_KEYS);

    key_buffer->keys[key_buffer->count++] = key;
}

void setUp(void)
{
    /* Function run before every test */
}

void tearDown(void)
{
    /* Function run after every test */
    lv_obj_clean(lv_screen_active());
}

static void event_cb(lv_event_t * e)
{
    struct key_buffer * key_buffer = lv_event_get_user_data(e);

    switch(lv_event_get_code(e)) {
        case LV_EVENT_KEY:
            key_buffer_add(key_buffer, lv_indev_get_key(lv_indev_active()));
            break;
        default:
            break;
    }
}

static lv_key_t remap_cb(lv_indev_t * i, lv_key_t key)
{
    LV_UNUSED(i);

    switch((int)key) {
        case 'a':
            return LV_KEY_LEFT;
        case LV_KEY_ESC:
            return 'Z';
        default:
            return key;
    }
}

void test_indev_key_remap(void)
{
    struct key_buffer key_buffer = {
        .count = 0,
    };

    lv_group_t * group = lv_group_create();
    lv_indev_t * indev = lv_test_indev_get_indev(LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_group(indev, group);

    lv_obj_t * btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn, 100, 100);
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_ALL, &key_buffer);
    lv_group_add_obj(group, btn);

    lv_test_key_hit('a');
    lv_test_key_hit(LV_KEY_ESC);

    /* Remap 'a' -> LV_KEY_LEFT, LV_KEY_ESC -> 'Z' */

    lv_indev_set_key_remap_cb(indev, remap_cb);

    lv_test_key_hit('a');
    lv_test_key_hit(LV_KEY_ESC);

    /* Disable remapping */

    lv_indev_set_key_remap_cb(indev, NULL);

    lv_test_key_hit('a');
    lv_test_key_hit(LV_KEY_ESC);

    TEST_ASSERT_EQUAL_UINT32(6, key_buffer.count);
    TEST_ASSERT_EQUAL_UINT32('a', key_buffer.keys[0]);
    TEST_ASSERT_EQUAL_UINT32(LV_KEY_ESC, key_buffer.keys[1]);
    TEST_ASSERT_EQUAL_UINT32(LV_KEY_LEFT, key_buffer.keys[2]);
    TEST_ASSERT_EQUAL_UINT32('Z', key_buffer.keys[3]);
    TEST_ASSERT_EQUAL_UINT32('a', key_buffer.keys[4]);
    TEST_ASSERT_EQUAL_UINT32(LV_KEY_ESC, key_buffer.keys[5]);
}
#endif
