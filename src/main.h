#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include <HTTPClient.h>
#include <PubSubClient.h>

void my_disp_flush( lv_display_t *disp, const lv_area_t *area, uint8_t * px_map);
void my_touchpad_read( lv_indev_t * indev, lv_indev_data_t * data );
void internet_init();
void internet_needle(void);
void WifiConnect();
void MQTT_callback(char* topic, byte* payload, unsigned int length);
void Fritz_Data();