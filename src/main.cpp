#include <main.h>

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
SPIClass touchscreenSpi = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
uint16_t touchScreenMinimumX = 200, touchScreenMaximumX = 3700, touchScreenMinimumY = 240,touchScreenMaximumY = 3800;

#define CYD_LED_BLUE 17
#define CYD_LED_RED 4
#define CYD_LED_GREEN 16

/*Set to your screen resolution*/
#define TFT_HOR_RES   320
#define TFT_VER_RES   240

/*LVGL draw into this buffer, 1/10 screen size usually works well. The size is in bytes*/
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))

WiFiClient wifiClient;
WiFiClient wifiClient_mqtt;
const char* wifihostname = "CYD_Homedisplay";
#define NTP_SERVER "de.pool.ntp.org"
#define DefaultTimeZone "CET-1CEST,M3.5.0/02,M10.5.0/03"  
String MY_TZ = DefaultTimeZone ;

const char* mqtt_server = "192.168.0.46";
int8_t mqtterrorcounter=0;
// MQTT_User and MQTT_Pass defined via platform.ini, external file, not uploaded to github
PubSubClient mqttclient(wifiClient_mqtt);


/* LVGL calls it when a rendered image needs to copied to the display*/
void my_disp_flush( lv_display_t *disp, const lv_area_t *area, uint8_t * px_map)
{
    /*Call it to tell LVGL you are ready*/
    lv_disp_flush_ready(disp);
}

/*Read the touchpad*/
void my_touchpad_read( lv_indev_t * indev, lv_indev_data_t * data )
{
  if(touchscreen.touched())
  {
    TS_Point p = touchscreen.getPoint();
    //Some very basic auto calibration so it doesn't go out of range
    if(p.x < touchScreenMinimumX) touchScreenMinimumX = p.x;
    if(p.x > touchScreenMaximumX) touchScreenMaximumX = p.x;
    if(p.y < touchScreenMinimumY) touchScreenMinimumY = p.y;
    if(p.y > touchScreenMaximumY) touchScreenMaximumY = p.y;
    //Map this to the pixel position
    data->point.x = map(p.x,touchScreenMinimumX,touchScreenMaximumX,1,TFT_HOR_RES); /* Touchscreen X calibration */
    data->point.y = map(p.y,touchScreenMinimumY,touchScreenMaximumY,1,TFT_VER_RES); /* Touchscreen Y calibration */
    data->state = LV_INDEV_STATE_PRESSED;
    
    Serial.print("Touch x ");
    Serial.print(data->point.x);
    Serial.print(" y ");
    Serial.println(data->point.y);
    
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

lv_indev_t * indev; //Touchscreen input device
uint8_t* draw_buf;  //draw_buf is allocated on heap otherwise the static area is too big on ESP32 at compile
uint32_t lastTick = 0;  //Used to track the tick timer

// Internet page
lv_obj_t * top_label = NULL;
lv_obj_t * download_label = NULL;
lv_obj_t * upload_label = NULL;
lv_obj_t * upload_needle = NULL;
lv_obj_t * upload_scale_line = NULL;
lv_obj_t * download_needle = NULL;
lv_obj_t * download_scale_line = NULL;
long millicounter = 0;
short http_error_counter = 0;
LV_IMAGE_DECLARE(img_hand);

static uint32_t my_tick_get_cb(void) {
  //esp_timer_get_time() / 1000;
  return millis();
}

void internet_init() {
  top_label = lv_label_create(lv_screen_active());
  lv_label_set_text(top_label, "Internet");
  lv_obj_set_width(top_label, 150);    // Set smaller width to make the lines wrap
  lv_obj_set_style_text_align(top_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(top_label, LV_ALIGN_TOP_MID, 0, 0);


  download_label = lv_label_create(lv_screen_active());
  lv_label_set_long_mode(download_label, LV_LABEL_LONG_WRAP);    // Breaks the long lines
  lv_label_set_text(download_label, "Starting!");
  lv_obj_set_width(download_label, 150);    // Set smaller width to make the lines wrap
  lv_obj_set_style_text_align(download_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(download_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  upload_label = lv_label_create(lv_screen_active());
  lv_label_set_long_mode(upload_label, LV_LABEL_LONG_WRAP);    // Breaks the long lines
  lv_label_set_text(upload_label, "Starting!");
  lv_obj_set_width(upload_label, 150);    // Set smaller width to make the lines wrap
  lv_obj_set_style_text_align(upload_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(upload_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

void internet_needle(void)
{
    download_scale_line = lv_scale_create(lv_screen_active());

    lv_obj_set_size(download_scale_line, 150, 150);
    lv_scale_set_mode(download_scale_line, LV_SCALE_MODE_ROUND_INNER);
    lv_obj_set_style_bg_opa(download_scale_line, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(download_scale_line, lv_palette_lighten(LV_PALETTE_RED, 5), 0);
    lv_obj_set_style_radius(download_scale_line, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(download_scale_line, true, 0);
    lv_obj_align(download_scale_line, LV_ALIGN_LEFT_MID, LV_PCT(2), 0);

    lv_scale_set_label_show(download_scale_line, true);

    lv_scale_set_total_tick_count(download_scale_line, 31);
    lv_scale_set_major_tick_every(download_scale_line, 5);

    lv_obj_set_style_length(download_scale_line, 5, LV_PART_ITEMS);
    lv_obj_set_style_length(download_scale_line, 10, LV_PART_INDICATOR);
    lv_scale_set_range(download_scale_line, 0, 300);

    lv_scale_set_angle_range(download_scale_line, 270);
    lv_scale_set_rotation(download_scale_line, 135);

    /* image must point to the right. E.g. -O------>*/
    download_needle = lv_image_create(download_scale_line);
    lv_image_set_src(download_needle, &img_hand);
    lv_obj_align(download_needle, LV_ALIGN_CENTER, 47, -2);
    lv_image_set_pivot(download_needle, 3, 4);

lv_scale_set_image_needle_value(download_scale_line, download_needle, 30);

    //download_needle = lv_line_create(download_scale_line);
    //lv_obj_set_style_line_width(download_needle, 6, LV_PART_MAIN);
    //lv_obj_set_style_line_rounded(download_needle, true, LV_PART_MAIN);
    //lv_scale_set_line_needle_value(download_scale_line, download_needle, 60, 0);



    // upload
    upload_scale_line = lv_scale_create(lv_screen_active());

    lv_obj_set_size(upload_scale_line, 150, 150);
    lv_scale_set_mode(upload_scale_line, LV_SCALE_MODE_ROUND_INNER);
    lv_obj_set_style_bg_opa(upload_scale_line, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(upload_scale_line, lv_palette_lighten(LV_PALETTE_RED, 5), 0);
    lv_obj_set_style_radius(upload_scale_line, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(upload_scale_line, true, 0);
    lv_obj_align(upload_scale_line, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_scale_set_label_show(upload_scale_line, true);

    lv_scale_set_total_tick_count(upload_scale_line, 31);
    lv_scale_set_major_tick_every(upload_scale_line, 5);

    lv_obj_set_style_length(upload_scale_line, 5, LV_PART_ITEMS);
    lv_obj_set_style_length(upload_scale_line, 10, LV_PART_INDICATOR);
    lv_scale_set_range(upload_scale_line, 0, 150);

    lv_scale_set_angle_range(upload_scale_line, 270);
    lv_scale_set_rotation(upload_scale_line, 135);

    upload_needle = lv_line_create(upload_scale_line);

    lv_obj_set_style_line_width(upload_needle, 6, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(upload_needle, true, LV_PART_MAIN);

    lv_scale_set_line_needle_value(upload_scale_line, upload_needle, 60, 50);
}

void WifiConnect() {
    WiFi.disconnect();
    WiFi.setHostname(wifihostname);  
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    short counter = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      if (++counter > 50)
        ESP.restart();
    }

    IPAddress ip = WiFi.localIP();
    Serial.println(F("WiFi connected"));
    Serial.println(ip);
}

void mqttclient_connect() {
   Serial.printf("vor MQTT");
    mqttclient.setServer(mqtt_server, 1883);
    mqttclient.setCallback(MQTT_callback);
    mqttclient.setBufferSize(1024);
    mqttclient.setKeepAlive(60);
   if (mqttclient.connect(wifihostname, MQTT_User, MQTT_Pass)) {
      Serial.printf("MQTT connect successful"); 
      const char *TOPIC2 = "HomeServer/Internet/#";
      mqttclient.subscribe(TOPIC2);
      //mqttclient.subscribe("Haus/+/Power");
   }  
    else
       Serial.printf("MQTT connect error");  

   Serial.printf("nach MQTT2");
   Serial.flush();

}

void MQTT_callback(char* topic, byte* payload, unsigned int length) {

    String message = String(topic);
    //int8_t joblength = message.length()+1;// 0 char
    payload[length] = '\0';
    String value = String((char *) payload);

//Serial.println(message);

    if (message == "HomeServer/Internet/Download")  
    {  lv_label_set_text_fmt(download_label,  "Download: %s", payload); 
    Serial.println((char *)payload);
        return; }
    if (message == "HomeServer/Internet/Upload")  
    {  lv_label_set_text_fmt(upload_label,  "Upload: %s", payload);
     return; }

/*
    if (message == "HomeServer/Internet/DownloadRate")  
    {   float value = atof ((char *) payload);  
        long lvalue = value / 1000;
        lv_scale_set_line_needle_value(download_scale_line, download_needle, 60, lvalue);
        return; 
    }
    if (message == "HomeServer/Internet/UploadRate")  
    {   float value = atof ((char *) payload);  
        long lvalue = value / 1000;
        lv_scale_set_line_needle_value(upload_scale_line, upload_needle, 60, lvalue);
        return; 
    }  
*/      
}

void setup() {
  Serial.begin(115200);
  WifiConnect() ;

  pinMode(CYD_LED_RED, OUTPUT);  // all off
  pinMode(CYD_LED_GREEN, OUTPUT);
  pinMode(CYD_LED_BLUE, OUTPUT);
  digitalWrite(CYD_LED_RED, HIGH); 
  digitalWrite(CYD_LED_GREEN, HIGH);
  digitalWrite(CYD_LED_BLUE, HIGH);

  //Initialise the touchscreen
  touchscreenSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS); /* Start second SPI bus for touchscreen */
  touchscreen.begin(touchscreenSpi); /* Touchscreen init */
  touchscreen.setRotation(3); /* Inverted landscape orientation to match screen */

  //Initialise LVGL
  lv_init();
  lv_tick_set_cb(my_tick_get_cb);

  draw_buf = new uint8_t[DRAW_BUF_SIZE];
  lv_display_t * disp;
  disp = lv_tft_espi_create(TFT_HOR_RES, TFT_VER_RES, draw_buf, DRAW_BUF_SIZE);

  //Initialize the XPT2046 input device driver
  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);  
  lv_indev_set_read_cb(indev, my_touchpad_read);

  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFFFFFF), LV_PART_MAIN);


  internet_init();
  internet_needle();

   mqttclient_connect();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED)
      WifiConnect();

  if (!mqttclient.connected()) {
    mqttclient_connect();
  } 
  mqttclient.loop();       

  long curmillis = millis();
  if (millicounter < curmillis)
  {  Fritz_Data();
     //curmillis = millis();
     millicounter = curmillis+5000;
  }
    lv_timer_handler();               //Update the UI
    delay(5);   
}

void Fritz_Data() {
      HTTPClient http;
      http.setReuse(false);
      http.begin(wifiClient, "http://fritz.box:49000/igdupnp/control/WANCommonIFC1");
      http.addHeader("Content-Type", "text/xml");
      http.addHeader("SOAPACTION", "urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1#GetAddonInfos");
      String httpRequestData = "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"";
      httpRequestData += "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">";
      httpRequestData += "<s:Body><u:GetAddonInfos xmlns:u=\"urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1\">";
      httpRequestData += "</u:GetAddonInfos></s:Body></s:Envelope>";

      int httpResponseCode = http.POST(httpRequestData);     
      if (httpResponseCode>0) {
        //Serial.print("HTTP Response code: ");
        //Serial.println(httpResponseCode);
        String payload = http.getString();
        //Serial.println(payload);

        int pos1 = payload.indexOf("NewByteSendRate");
        String sub = payload.substring(pos1+16);
        //Serial.println("teil:");
        //Serial.println(sub);
        pos1 = sub.indexOf("<");
        sub = sub.substring(0, pos1);
        //Serial.print("zahl: ");
        //Serial.println(sub);
        float value = sub.toFloat() * 8 / 1024;  
        long lvalue = value / 1000;
        // Serial.println(lvalue);
        lv_scale_set_line_needle_value(upload_scale_line, upload_needle, 60, lvalue);

        // find <NewByteSendRate>11214</NewByteSendRate>
        // find <NewByteReceiveRate>144813</NewByteReceiveRate>

        pos1 = payload.indexOf("NewByteReceiveRate");
        sub = payload.substring(pos1+19);
        //Serial.println("teil:");
        //Serial.println(sub);
        pos1 = sub.indexOf("<");
        sub = sub.substring(0, pos1);
        //Serial.print("zahl: ");

        value = sub.toFloat() * 8 / 1024;  
        lvalue = value / 1000;

         lv_scale_set_image_needle_value(download_scale_line, download_needle, lvalue);
//Serial.print("vor Label set");  Serial.flush();       
//lv_label_set_text_fmt(download_label,  "D: %lu", lvalue);

        http_error_counter = 0;
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        if (http_error_counter++ > 1)
          WifiConnect();
      }
      // Free resources
      http.end();

   //Serial.println("End fritz"); Serial.flush();     
}