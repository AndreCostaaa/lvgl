#if LV_BUILD_TEST
#include "../../lvgl.h"

#if LV_USE_GLTF

#include "unity/unity.h"

#define ASSET(name) "A:src/test_assets/gltf/" name

#define VIEW_SIZE 160

void setUp(void)
{
    /* A known screen color makes it possible to tell rendered pixels from the background */
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
}

void tearDown(void)
{
    lv_obj_clean(lv_screen_active());
}

static lv_color32_t pixel_at(const lv_draw_buf_t * buf, int32_t x, int32_t y)
{
    const uint8_t * px = lv_draw_buf_goto_xy((lv_draw_buf_t *)buf, x, y);
    lv_color32_t c;
    c.blue = px[0];
    c.green = px[1];
    c.red = px[2];
    c.alpha = 0xFF;
    return c;
}

/* Renders a frame. The screen is invalidated first so that a frame is always
 * produced, even when nothing changed since the previous one. */
static void render_frame(void)
{
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(NULL);
}

/* Number of pixels inside `area` that are not black, after rendering a frame */
static uint32_t count_lit_pixels(const lv_area_t * area)
{
    render_frame();

    lv_draw_buf_t * buf = lv_display_get_buf_active(NULL);
    TEST_ASSERT_NOT_NULL(buf);

    uint32_t lit = 0;
    for(int32_t y = area->y1; y <= area->y2; y++) {
        for(int32_t x = area->x1; x <= area->x2; x++) {
            lv_color32_t c = pixel_at(buf, x, y);
            if(c.red || c.green || c.blue) {
                lit++;
            }
        }
    }
    return lit;
}

/* Cheap fingerprint of the rendered area, used to tell that a setting changed something */
static uint32_t area_checksum(const lv_area_t * area)
{
    render_frame();

    lv_draw_buf_t * buf = lv_display_get_buf_active(NULL);
    TEST_ASSERT_NOT_NULL(buf);

    uint32_t sum = 0;
    for(int32_t y = area->y1; y <= area->y2; y++) {
        for(int32_t x = area->x1; x <= area->x2; x++) {
            lv_color32_t c = pixel_at(buf, x, y);
            sum = sum * 31u + (uint32_t)((c.red << 16) | (c.green << 8) | c.blue);
        }
    }
    return sum;
}

static lv_obj_t * create_view(void)
{
    lv_obj_t * gltf = lv_gltf_create(lv_screen_active());
    lv_obj_set_size(gltf, VIEW_SIZE, VIEW_SIZE);
    lv_obj_set_pos(gltf, 0, 0);
    return gltf;
}

static void view_area(lv_obj_t * gltf, lv_area_t * area)
{
    lv_obj_update_layout(gltf);
    lv_obj_get_coords(gltf, area);
}

/* An empty viewer must render without touching the rest of the screen */
void test_gltf_render_without_model(void)
{
    lv_obj_t * gltf = create_view();
    lv_area_t area;
    view_area(gltf, &area);

    TEST_ASSERT_EQUAL(0, count_lit_pixels(&area));
}

/* This is the first test of the file on purpose: rendering a glTF view as the very
 * first thing in the process caught the view leaving the framebuffer binding at 0. */
void test_gltf_render_draws_into_the_view(void)
{
    lv_obj_t * gltf = create_view();
    lv_area_t area;
    view_area(gltf, &area);

    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(gltf, ASSET("minimal_triangle.gltf")));

    /* The environment background alone covers the whole view */
    uint32_t lit = count_lit_pixels(&area);
    TEST_ASSERT_GREATER_THAN(VIEW_SIZE * VIEW_SIZE / 2, lit);
}

/* Everything outside the view keeps being drawn by the normal draw unit. A glTF view
 * renders through a framebuffer of its own, and it must be restored afterwards. */
void test_gltf_render_keeps_the_rest_of_the_screen(void)
{
    lv_obj_t * gltf = create_view();
    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(gltf, ASSET("minimal_triangle.gltf")));

    lv_obj_t * marker = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(marker);
    lv_obj_set_size(marker, 40, 40);
    lv_obj_set_pos(marker, VIEW_SIZE + 20, 20);
    lv_obj_set_style_bg_color(marker, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_bg_opa(marker, LV_OPA_COVER, 0);

    lv_area_t marker_area;
    view_area(marker, &marker_area);
    TEST_ASSERT_EQUAL(40 * 40, count_lit_pixels(&marker_area));

    lv_color32_t c = pixel_at(lv_display_get_buf_active(NULL), marker_area.x1 + 20, marker_area.y1 + 20);
    TEST_ASSERT_EQUAL(0x00, c.red);
    TEST_ASSERT_EQUAL(0xFF, c.green);
    TEST_ASSERT_EQUAL(0x00, c.blue);
}

void test_gltf_render_solid_background_uses_the_style_color(void)
{
    lv_obj_t * gltf = create_view();
    lv_obj_set_style_bg_color(gltf, lv_color_hex(0x0000FF), 0);
    /* The solid background is cleared with the style color and the style opacity */
    lv_obj_set_style_bg_opa(gltf, LV_OPA_COVER, 0);
    lv_gltf_set_background_mode(gltf, LV_GLTF_BG_MODE_SOLID);

    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(gltf, ASSET("minimal_triangle.gltf")));

    lv_area_t area;
    view_area(gltf, &area);
    render_frame();

    /* A corner of the view is background only, the triangle is in the middle */
    lv_color32_t c = pixel_at(lv_display_get_buf_active(NULL), area.x1 + 2, area.y1 + 2);
    TEST_ASSERT_GREATER_THAN(c.red, c.blue);
    TEST_ASSERT_GREATER_THAN(c.green, c.blue);
}

void test_gltf_render_camera_change_changes_the_image(void)
{
    lv_obj_t * gltf = create_view();
    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(gltf, ASSET("pbr_textures.gltf")));

    lv_area_t area;
    view_area(gltf, &area);

    uint32_t before = area_checksum(&area);

    lv_gltf_set_yaw(gltf, 90.0f);
    lv_gltf_set_pitch(gltf, 30.0f);
    lv_obj_invalidate(gltf);
    uint32_t after = area_checksum(&area);

    TEST_ASSERT_NOT_EQUAL(before, after);
}

void test_gltf_render_distance_changes_the_image(void)
{
    lv_obj_t * gltf = create_view();
    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(gltf, ASSET("pbr_textures.gltf")));
    lv_gltf_set_background_mode(gltf, LV_GLTF_BG_MODE_SOLID);

    lv_area_t area;
    view_area(gltf, &area);

    uint32_t near_lit = count_lit_pixels(&area);

    lv_gltf_set_distance(gltf, 8.0f);
    lv_obj_invalidate(gltf);
    uint32_t far_lit = count_lit_pixels(&area);

    /* The model covers fewer pixels when the camera moves away from it */
    TEST_ASSERT_GREATER_THAN(far_lit, near_lit);
}

void test_gltf_render_all_material_features(void)
{
    lv_obj_t * gltf = create_view();
    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(gltf, ASSET("materials.gltf")));

    lv_area_t area;
    view_area(gltf, &area);
    TEST_ASSERT_GREATER_THAN(0, count_lit_pixels(&area));
}

/* The sheen material of this asset has a black base color and no dielectric specular,
 * so everything that shows up comes from the sheen lobe. It stays black when the sheen
 * lighting tables are not generated. */
void test_gltf_render_sheen_material_is_lit(void)
{
    lv_obj_t * gltf = create_view();
    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(gltf, ASSET("sheen.gltf")));
    lv_gltf_set_background_mode(gltf, LV_GLTF_BG_MODE_SOLID);
    /* Sheen shows up at grazing angles, so the cube is turned away from the camera, and
     * the environment is turned up to lift the dim lobe above the 8 bit quantization */
    lv_gltf_set_yaw(gltf, 45.0f);
    lv_gltf_set_pitch(gltf, 35.0f);
    lv_gltf_set_env_brightness(gltf, 1000);
    lv_gltf_set_image_exposure(gltf, 4.0f);

    lv_area_t area;
    view_area(gltf, &area);

    TEST_ASSERT_GREATER_THAN(200, count_lit_pixels(&area));
}

void test_gltf_render_lights_and_skin(void)
{
    lv_obj_t * lights = create_view();
    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(lights, ASSET("lights.gltf")));

    lv_obj_t * skin = create_view();
    lv_obj_set_pos(skin, VIEW_SIZE, 0);
    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(skin, ASSET("skin.gltf")));

    lv_area_t area;
    view_area(skin, &area);
    TEST_ASSERT_GREATER_THAN(0, count_lit_pixels(&area));
}

void test_gltf_render_antialiasing_modes(void)
{
    lv_obj_t * gltf = create_view();
    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(gltf, ASSET("minimal_triangle.gltf")));

    lv_area_t area;
    view_area(gltf, &area);

    const lv_gltf_aa_mode_t modes[] = {
        LV_GLTF_AA_MODE_OFF, LV_GLTF_AA_MODE_ON, LV_GLTF_AA_MODE_DYNAMIC
    };
    for(uint32_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        lv_gltf_set_antialiasing_mode(gltf, modes[i]);
        lv_obj_invalidate(gltf);
        TEST_ASSERT_GREATER_THAN(0, count_lit_pixels(&area));
    }
}

void test_gltf_render_animation_advances(void)
{
    lv_obj_t * gltf = create_view();
    lv_gltf_model_t * model = lv_gltf_load_model_from_file(gltf, ASSET("animation.gltf"));
    TEST_ASSERT_NOT_NULL(model);
    lv_gltf_set_background_mode(gltf, LV_GLTF_BG_MODE_SOLID);

    lv_area_t area;
    view_area(gltf, &area);

    TEST_ASSERT_EQUAL(LV_RESULT_OK, lv_gltf_model_play_animation(model, 0));
    uint32_t start = area_checksum(&area);

    lv_test_fast_forward(500);
    uint32_t later = area_checksum(&area);
    TEST_ASSERT_NOT_EQUAL(start, later);

    /* Once paused the image must stay the same */
    lv_gltf_model_pause_animation(model);
    uint32_t paused = area_checksum(&area);
    lv_test_fast_forward(500);
    TEST_ASSERT_EQUAL(paused, area_checksum(&area));
}

/* Turning a node around X, Y and Z are three different rotations, and each of them
 * changes the image in its own way */
void test_gltf_render_node_rotation_axes(void)
{
    lv_obj_t * gltf = create_view();
    lv_gltf_model_t * model = lv_gltf_load_model_from_file(gltf, ASSET("pbr_textures.gltf"));
    TEST_ASSERT_NOT_NULL(model);
    lv_gltf_set_background_mode(gltf, LV_GLTF_BG_MODE_SOLID);

    lv_gltf_model_node_t * cube = lv_gltf_model_node_get_by_index(model, 0);
    TEST_ASSERT_NOT_NULL(cube);

    lv_area_t area;
    view_area(gltf, &area);
    uint32_t unrotated = area_checksum(&area);

    const float quarter_turn = 0.7854f;
    lv_gltf_model_node_set_rotation_x(cube, quarter_turn);
    uint32_t around_x = area_checksum(&area);
    lv_gltf_model_node_set_rotation_x(cube, 0.0f);
    lv_gltf_model_node_set_rotation_y(cube, quarter_turn);
    uint32_t around_y = area_checksum(&area);
    lv_gltf_model_node_set_rotation_y(cube, 0.0f);
    lv_gltf_model_node_set_rotation_z(cube, quarter_turn);
    uint32_t around_z = area_checksum(&area);

    TEST_ASSERT_NOT_EQUAL(unrotated, around_x);
    TEST_ASSERT_NOT_EQUAL(unrotated, around_y);
    TEST_ASSERT_NOT_EQUAL(unrotated, around_z);
    TEST_ASSERT_NOT_EQUAL(around_x, around_y);
    TEST_ASSERT_NOT_EQUAL(around_x, around_z);
    TEST_ASSERT_NOT_EQUAL(around_y, around_z);
}

void test_gltf_render_two_models_in_one_view(void)
{
    lv_obj_t * gltf = create_view();
    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(gltf, ASSET("minimal_triangle.gltf")));

    lv_area_t area;
    view_area(gltf, &area);
    uint32_t one_model = area_checksum(&area);

    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(gltf, ASSET("cameras.gltf")));
    TEST_ASSERT_EQUAL(2, lv_gltf_get_model_count(gltf));

    lv_obj_invalidate(gltf);
    TEST_ASSERT_NOT_EQUAL(one_model, area_checksum(&area));
}

void test_gltf_render_two_views_side_by_side(void)
{
    lv_obj_t * left = create_view();
    lv_obj_t * right = create_view();
    lv_obj_set_pos(right, VIEW_SIZE, 0);

    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(left, ASSET("minimal_triangle.gltf")));
    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(right, ASSET("materials.gltf")));

    lv_area_t left_area;
    lv_area_t right_area;
    view_area(left, &left_area);
    view_area(right, &right_area);

    TEST_ASSERT_GREATER_THAN(0, count_lit_pixels(&left_area));
    TEST_ASSERT_GREATER_THAN(0, count_lit_pixels(&right_area));
}

/* Deleting a view while another one keeps rendering must not disturb the survivor */
void test_gltf_render_after_deleting_another_view(void)
{
    lv_obj_t * first = create_view();
    lv_obj_t * second = create_view();
    lv_obj_set_pos(second, VIEW_SIZE, 0);

    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(first, ASSET("minimal_triangle.gltf")));
    TEST_ASSERT_NOT_NULL(lv_gltf_load_model_from_file(second, ASSET("minimal_triangle.gltf")));

    lv_area_t area;
    view_area(second, &area);
    TEST_ASSERT_GREATER_THAN(0, count_lit_pixels(&area));

    lv_obj_delete(first);
    TEST_ASSERT_GREATER_THAN(0, count_lit_pixels(&area));
}

#else /*LV_USE_GLTF*/

void setUp(void)
{
}

void tearDown(void)
{
}

void test_gltf_render_without_model(void)
{
}

void test_gltf_render_draws_into_the_view(void)
{
}

void test_gltf_render_keeps_the_rest_of_the_screen(void)
{
}

void test_gltf_render_solid_background_uses_the_style_color(void)
{
}

void test_gltf_render_camera_change_changes_the_image(void)
{
}

void test_gltf_render_distance_changes_the_image(void)
{
}

void test_gltf_render_all_material_features(void)
{
}

void test_gltf_render_sheen_material_is_lit(void)
{
}

void test_gltf_render_lights_and_skin(void)
{
}

void test_gltf_render_antialiasing_modes(void)
{
}

void test_gltf_render_animation_advances(void)
{
}

void test_gltf_render_node_rotation_axes(void)
{
}

void test_gltf_render_two_models_in_one_view(void)
{
}

void test_gltf_render_two_views_side_by_side(void)
{
}

void test_gltf_render_after_deleting_another_view(void)
{
}

#endif /*LV_USE_GLTF*/

#endif /*LV_BUILD_TEST*/
