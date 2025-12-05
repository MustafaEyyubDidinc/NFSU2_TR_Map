#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_memory_utils.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include "driver/twai.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "freertos/FreeRTOS.h"

#define _USE_MATH_DEFINES
#include <stdio.h>
#include <cmath>

#include "CANBus_Driver.h"

#include "gps_locator.h"

#include "images/maps_bg.h"
#include "images/car_icon.h"
#include "images/north_pointer.h"
#include "images/no_satellite.h"

#define DEG_TO_RAD(deg) ((deg) * M_PI / 180.0)
#define RAD_TO_DEG(rad) ((rad) * 180.0 / M_PI)

// declarations

LV_IMG_DECLARE(maps_bg)
LV_IMG_DECLARE(car_icon);
LV_IMG_DECLARE(north_pointer);
LV_IMG_DECLARE(no_satellite);

// screens
lv_obj_t *main_scr;

// global elements
lv_obj_t *no_satellite_bg;
lv_obj_t *map_container;

lv_obj_t *car_icon_img;
lv_obj_t *north_pointer_img;

void (*can_message_handler)(twai_message_t *message) = NULL;

// CONTROL VARIABLE INIT

#define MIN_MOVE_DISTANCE 2.0 // distance in meters to trigger icon rotation

float current_latitude        = 0.0;
float current_longitude       = 0.0;
float new_latitude            = 0.0;
float new_longitude           = 0.0;
int current_angle             = 0;
int new_angle                 = 0;
bool receiving_data           = false; // has the first data been received
volatile bool data_ready      = false; // new incoming data
bool init_anim_complete       = false; // needle sweep completed - tbc
bool location_initialized     = false; // has the initial GPS location been set

// general color palettes
const lv_color_t PALETTE_BLACK        = LV_COLOR_MAKE(0, 0, 0);
const lv_color_t PALETTE_BLUE         = LV_COLOR_MAKE(31, 104, 135); // fuel arc main
const lv_color_t PALETTE_BLUE_NEON    = LV_COLOR_MAKE(83, 252, 254); // fuel arc indicator
const lv_color_t PALETTE_DARK_GREY    = LV_COLOR_MAKE(24, 24, 24); // highlight button background
const lv_color_t PALETTE_RED          = LV_COLOR_MAKE(130, 35, 53); // redline
const lv_color_t PALETTE_GREEN        = LV_COLOR_MAKE(123, 207, 21); // buttons and text
const lv_color_t PALETTE_GREY         = LV_COLOR_MAKE(120, 120, 120); // button background
const lv_color_t PALETTE_WHITE        = LV_COLOR_MAKE(255, 255, 255);

// NFSU2 pickable colors
const lv_color_t PALETTE_NFS_WHITE    = LV_COLOR_MAKE(255, 255, 255);
const lv_color_t PALETTE_NFS_BLUE     = LV_COLOR_MAKE(52, 154, 227);
const lv_color_t PALETTE_NFS_CYAN     = LV_COLOR_MAKE(34, 199, 239);
const lv_color_t PALETTE_NFS_GREEN    = LV_COLOR_MAKE(93, 239, 39);
const lv_color_t PALETTE_NFS_CITRUS   = LV_COLOR_MAKE(221, 221, 37);
const lv_color_t PALETTE_NFS_LIME     = LV_COLOR_MAKE(148, 248, 38);
const lv_color_t PALETTE_NFS_ORANGE   = LV_COLOR_MAKE(244, 153, 37);
const lv_color_t PALETTE_NFS_RED      = LV_COLOR_MAKE(255, 42, 22);
const lv_color_t PALETTE_NFS_PURPLE   = LV_COLOR_MAKE(136, 86, 255);
const lv_color_t PALETTE_NFS_GREY     = LV_COLOR_MAKE(175, 181, 191);
const lv_color_t PALETTE_NFS_BLUE2    = LV_COLOR_MAKE(27, 173, 252);
const lv_color_t PALETTE_NFS_YELLOW   = LV_COLOR_MAKE(229, 223, 33);

// get bearing angle between two coordinates
double angle_from_coordinate(double lat1, double long1, double lat2, double long2) {
    double lat1_rad = DEG_TO_RAD(lat1);
    double lat2_rad = DEG_TO_RAD(lat2);
    double dlon_rad = DEG_TO_RAD(long2 - long1);

    double y = sin(dlon_rad) * cos(lat2_rad);
    double x = cos(lat1_rad)*sin(lat2_rad) - sin(lat1_rad)*cos(lat2_rad)*cos(dlon_rad);
    double bearing_rad = atan2(y, x);

    double bearing_deg = fmod(RAD_TO_DEG(bearing_rad) + 360.0, 360.0);

    int bearing_lvgl = (int)round(bearing_deg * 10.0);

    return bearing_lvgl;
}

// calculate distance between two coordinates in meters
double distance_between(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0; // Earth radius in meters
    double dLat = DEG_TO_RAD(lat2 - lat1);
    double dLon = DEG_TO_RAD(lon2 - lon1);
    double a = sin(dLat/2) * sin(dLat/2) +
               cos(DEG_TO_RAD(lat1)) * cos(DEG_TO_RAD(lat2)) *
               sin(dLon/2) * sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    return R * c;
}

// normalize angle to shortest rotation direction
float normalize_angle(float from, float to) {
    float diff = to - from;
    while (diff > 1800) diff -= 3600;
    while (diff < -1800) diff += 3600;
    return from + diff;
}

static void anim_set_r_cb(void * obj, int32_t v) {
    lv_img_set_angle((lv_obj_t *)obj, v);
}

void update_values(void) {
    if (location_initialized) {
        GPSLocator::move_location((double)new_latitude, (double)new_longitude);
    } else {            
        GPSLocator::show_initial_location((double)new_latitude, (double)new_longitude);
        location_initialized = true;
    }

    // TODO - animations in / out and lost connection handling
    lv_obj_set_style_opa(no_satellite_bg, LV_OPA_0, 0);
    lv_obj_set_style_opa(map_container, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(car_icon_img, LV_OPA_COVER, 0);

    double dist = distance_between(current_latitude, current_longitude, new_latitude, new_longitude);
  
    if (dist > MIN_MOVE_DISTANCE) {
        new_angle = angle_from_coordinate(current_latitude, current_longitude, new_latitude, new_longitude);
        float anim_target_angle = normalize_angle(current_angle, new_angle);

        lv_anim_t aa;
        lv_anim_init(&aa);
        lv_anim_set_var(&aa, car_icon_img);
        lv_anim_set_time(&aa, STEP_ANIMATION_DURATION);
        lv_anim_set_exec_cb(&aa, anim_set_r_cb);

        lv_anim_set_values(&aa, current_angle, anim_target_angle);
        lv_anim_start(&aa);

        current_angle = anim_target_angle;
    }

    current_latitude = new_latitude;
    current_longitude = new_longitude;
}

void make_screen(void) {
    main_scr = lv_obj_create(NULL);
    lv_obj_set_size(main_scr, 800, 800);
    lv_obj_set_style_bg_color(main_scr, PALETTE_BLACK, 0);
    lv_obj_set_style_pad_all(main_scr, 0, 0);
    lv_obj_set_style_border_width(main_scr, 0, 0);

    no_satellite_bg = lv_img_create(main_scr);
    lv_image_set_src(no_satellite_bg, &no_satellite);
    lv_obj_set_size(no_satellite_bg, 500, 500);
    lv_obj_align(no_satellite_bg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_opa(no_satellite_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(no_satellite_bg, PALETTE_BLACK, 0);
    lv_obj_set_style_pad_all(no_satellite_bg, 0, 0);
    lv_obj_set_style_border_width(no_satellite_bg, 0, 0);

    map_container = lv_obj_create(main_scr);
    lv_obj_set_size(map_container, 500, 500);
    lv_obj_align(map_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_opa(map_container, LV_OPA_0, 0);
    lv_obj_set_style_bg_color(map_container, PALETTE_BLACK, 0);
    lv_obj_set_style_pad_all(map_container, 0, 0);
    lv_obj_set_style_border_width(map_container, 0, 0);
    lv_obj_set_style_outline_color(map_container, PALETTE_GREY, 0);

    lv_obj_t *maps_bg_img = lv_img_create(main_scr);
    lv_image_set_src(maps_bg_img, &maps_bg);
    lv_obj_align(maps_bg_img, LV_ALIGN_CENTER, 0, 0);
}

void make_ui(void) {
    car_icon_img = lv_img_create(main_scr);
    lv_image_set_src(car_icon_img, &car_icon);
    lv_obj_set_style_opa(car_icon_img, LV_OPA_0, 0);
    lv_obj_align(car_icon_img, LV_ALIGN_CENTER, 0, 5);
    lv_obj_set_style_transform_pivot_x(car_icon_img, 24, 0);
    lv_obj_set_style_transform_pivot_y(car_icon_img, 21, 0);

    north_pointer_img = lv_img_create(main_scr);
    lv_image_set_src(north_pointer_img, &north_pointer);
    lv_obj_align(north_pointer_img, LV_ALIGN_CENTER, 0, -280);
    lv_obj_set_style_transform_pivot_x(north_pointer_img, 43, 0);
    lv_obj_set_style_transform_pivot_y(north_pointer_img, (280 + 30), 0);
}

void update_location(uint8_t* data) {
    new_latitude = 0.0f;
    new_longitude = 0.0f;

    memcpy(&new_latitude, data, sizeof(float));
    memcpy(&new_longitude, data + 4, sizeof(float));

    printf("Received GPS Data - Latitude: %f, Longitude: %f\n", new_latitude, new_longitude);
}

// NOTE
// My personal system for data passing uses a CANBus between devices
// The GPS module sends messages with ID 0x430 containing latitude and longitude as floats
void custom_can_message_handler(twai_message_t *message) {
    printf("Received CAN message with ID: 0x%03X\n", message->identifier);
    switch (message->identifier) {
        case 0x430:
            update_location(message->data);
            data_ready = true;
            break;
        default:
            break; 
    }
    receiving_data = true;
}

void mount_sd(void) {
    esp_err_t err = bsp_sdcard_mount();
    if (err != ESP_OK) {
        printf("Failed to mount SD card, error: %s\n", esp_err_to_name(err));
    }
}

// decouple movement updates from CAN receive task
void lvgl_timer(lv_timer_t * timer) {
    if (data_ready) {
        data_ready = false;
        update_values();
    }
}

extern "C" void app_main(void) {
    mount_sd();

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,
            .sw_rotate = false,
        }
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();
    bsp_display_brightness_set(100);

    can_message_handler = custom_can_message_handler;

    canbus_init();
    start_can_tasks();

    bsp_display_lock(0);

    make_screen();

    if (!GPSLocator::init(map_container)) {
        printf("Failed to initialize map\n");
        return;
    }

    make_ui();

    lv_screen_load(main_scr);
    
    bsp_display_unlock();

    lv_timer_t * timer = lv_timer_create(lvgl_timer, 10,  NULL);
    (void)timer;
}
