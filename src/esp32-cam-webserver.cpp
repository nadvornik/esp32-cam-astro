/* This sketch is a extension/expansion/rework of the 'official' ESP32 Camera example
 *  sketch from Expressif:
 *  https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples/Camera/CameraWebServer
 *  and
 *  https://github.com/easytarget/esp32-cam-webserver
 *
 * note: Make sure that you have either selected ESP32 AI Thinker,
 *       or another board which has PSRAM enabled to use high resolution camera modes
*/

#include "esp_camera.h"
#include "esp_http_server.h"

#include <WiFi.h>
#include <WiFiUdp.h>

#include <ArduinoOTA.h>

#include "esp_log.h"
void flashLED(int flashtime);

static const char* TAG = "MyModule";



#include "FS.h" 
#include "SD_MMC.h" 

//List dir in SD card
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

//Create a dir in SD card
void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

//delete a dir in SD card
void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}

//Read a file in SD card
void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
}

//Write a file in SD card
void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
   
 
   //fwrite(fb->buf, 1, fb->len, file);
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
}

//Append to the end of file in SD card
void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
}

//Rename a file in SD card
void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("File renamed");
    } else {
        Serial.println("Rename failed");
    }
}

//Delete a file in SD card
void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}

//Test read and write speed using test.txt file
void testFileIO(fs::FS &fs, const char * path){
    File file = fs.open(path);
    static uint8_t buf[4096];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
        len = file.size();
        size_t flen = len;
        start = millis();
        while(len){
            size_t toRead = len;
            if(toRead > 4096){
                toRead = 4096;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        Serial.printf("%u bytes read for %u ms\n", flen, end);
        file.close();
    } else {
        Serial.println("Failed to open file for reading");
    }


    file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for(i=0; i<2048; i++){
        file.write(buf, 4096);
    }
    end = millis() - start;
    Serial.printf("%u bytes written for %u ms\n", 2048 * 4096, end);
    file.close();
}

//Find current image number
int findCurrentImageNumber(fs::FS &fs, const char * dirname){
    int current = 0;
    Serial.printf("Searching for images in directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return 0;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return 0;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
           if (strncmp(file.name(), "/img_", 5) == 0) {
               int i = atoi(file.name() + 5);
               if (current < i) current = i;
           }
        
        }
        file = root.openNextFile();
    }
    current += 1;
    Serial.printf("Current image: %d\n", current);
    return current;
}


void writeImage(const char *path, camera_fb_t * fb)
{
    const size_t buf_size = 65536;
    
    char* buf = (char *)heap_caps_malloc(buf_size, MALLOC_CAP_DMA);

    File file = SD_MMC.open(path, FILE_WRITE);

    snprintf(buf, buf_size, "P5 %d %d 255 ", fb->width, fb->height);
    size_t pos = strlen(buf);
    
    size_t fbpos = 0;
    
    while (fbpos < fb->len) {
        size_t to_write = buf_size - pos;
        if (fb->len - fbpos < to_write) to_write = fb->len - fbpos;
        memcpy(buf + pos, fb->buf + fbpos, to_write);
        pos += to_write;
        file.write((unsigned char *)buf, pos);
        pos = 0;
        fbpos += to_write;
    }
    file.close();
    free(buf);
}

static esp_err_t index_handler(httpd_req_t *req){

    Serial.println(req->uri);
    File file = SD_MMC.open(req->uri);

    if (!file) {
        return httpd_resp_send_404(req);
    }


    if (file.isDirectory()) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Content-Encoding", "identity");
        String out("<html><head><title>Index of /</title></head><body><h1>Index of /</h1>");
        file.rewindDirectory();
        while(true) {
            File entry = file.openNextFile();
            if (!entry) break;
            out += (entry.isDirectory()) ? "dir" : "file";
            out += " <a href='";
            out += entry.name();
            out += "'>";
            out += entry.name();
            out += "</a><br/>";
        }
        out = out + "</body></html>";
        file.close();
        return httpd_resp_sendstr(req, out.c_str());

    }

    httpd_resp_set_type(req, "image/pgm");
    
    const size_t buf_size = 32768;
    char* buf = (char *)malloc(buf_size);
    
    size_t r;
    while((r = file.read((unsigned char *)buf, buf_size)) > 0) {
        httpd_resp_send_chunk(req, buf, r);
    }
    file.close();
    free(buf);
    
    return httpd_resp_send_chunk(req, NULL, 0);
;
}


httpd_handle_t camera_httpd = NULL;

void startCameraServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    httpd_uri_t index_uri = {
        .uri       = "/*",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };

    log_i("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
    }
}



// Select camera board model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER

// Select camera module used on the board
#define CAMERA_MODULE_OV2640
//#define CAMERA_MODULE_OV3660

#if __has_include("myconfig.h")
  // I keep my settings in a seperate header file
  #include "myconfig.h"
#else
  const char* ssid = "ssid";
  const char* password = "password";
#endif

// A Name for the Camera. (can be set in myconfig.h)
#ifdef CAM_NAME
  char myName[] = CAM_NAME;
#else
  char myName[] = "ESP32 camera server";
#endif

// This will be displayed to identify the firmware
char myVer[] PROGMEM = __DATE__ " @ " __TIME__;


#include "camera_pins.h"

#define UDP_LOGGING_MAX_PAYLOAD_LEN 1024
char logbuf[UDP_LOGGING_MAX_PAYLOAD_LEN] = "";
WiFiUDP myUDP;

static int udp_logging_vprintf(const char *format, va_list args) {
  vsnprintf((char*) logbuf, UDP_LOGGING_MAX_PAYLOAD_LEN, format, args);
  myUDP.beginPacket("192.168.2.1", 9274);
  myUDP.write((uint8_t*) logbuf, strlen(logbuf));
  myUDP.endPacket();
  return vprintf(format, args);
}
/*
int log_printf(const char *format, ...)
{
    static char loc_buf[64];
    char * temp = loc_buf;
    int len;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(NULL, 0, format, arg);
    va_end(copy);
    if(len >= sizeof(loc_buf)){
        temp = (char*)malloc(len+1);
        if(temp == NULL) {
            return 0;
        }
    }
    vsnprintf(temp, len+1, format, arg);

    esp_log_write(ESP_LOG_DEBUG, TAG, "%s", temp);
    
    va_end(arg);
    if(len >= sizeof(loc_buf)){
        free(temp);
    }
    return len;
}
*/

int currentImage = 0;

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);

 Serial.println("SDcard Testing....");

   if(!SD_MMC.begin("/sdcard", true)){
        Serial.println("Card Mount Failed");
//        return;
    }
    uint8_t cardType = SD_MMC.cardType();

    if(cardType == CARD_NONE){
        Serial.println("No SD_MMC card attached");
//        return;
    }

    Serial.print("SD_MMC Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }


    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);

    listDir(SD_MMC, "/", 0);
/*    createDir(SD_MMC, "/mydir");
    listDir(SD_MMC, "/", 0);
    removeDir(SD_MMC, "/mydir");
    listDir(SD_MMC, "/", 2);
    writeFile(SD_MMC, "/hello.txt", "Hello ");
    appendFile(SD_MMC, "/hello.txt", "World!\n");
    readFile(SD_MMC, "/hello.txt");
    deleteFile(SD_MMC, "/foo.txt");
    renameFile(SD_MMC, "/hello.txt", "/foo.txt");
    readFile(SD_MMC, "/foo.txt");
*/
//    testFileIO(SD_MMC, "/test.txt");
    Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
    Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));  
  
  currentImage = findCurrentImageNumber(SD_MMC, "/");
  

  Serial.println();
  Serial.println("====");
  Serial.print("esp32-cam-webserver: ");
  Serial.println(myName);
  Serial.print("Code Built: ");
  Serial.println(myVer);

//  esp_log_level_set("*", ESP_LOG_VERBOSE);

#ifdef LED_PIN  // If we have a notification LED set it to output
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LED_OFF); 
#endif






  // Feedback that hardware init is complete and we are now attempting to connect
  Serial.println("");
  Serial.print("Connecting to Wifi Netowrk: ");
  Serial.println(ssid);
  flashLED(400);
  delay(100);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);  // Wait for Wifi to connect. If this fails wifi the code basically hangs here.
                 // - It would be good to do something else here as a future enhancement.
                 //   (eg: go to a captive AP config portal to configure the wifi)
  }

  // feedback that we are connected
  Serial.println("WiFi connected");
  Serial.println("");
  flashLED(200);
  delay(100);
  flashLED(200);
  delay(100);
  flashLED(200);
/*
  if (myUDP.begin(9888) == 1) {
      esp_log_set_vprintf(udp_logging_vprintf);
      esp_log_level_set("*", ESP_LOG_INFO);
  }
  log_i("output redirected");
*/
  esp_log_level_set("camera", ESP_LOG_VERBOSE);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 16000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    log_i("psram");
    config.pixel_format = PIXFORMAT_RAW;
    config.frame_size = FRAMESIZE_UXGA;
    config.fb_count = 1;
  } else {
    log_i("no psram");

    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif
/*

    ledc_channel_config_t ch_conf;
    ch_conf.gpio_num = config.pin_xclk;
    ch_conf.speed_mode = LEDC_HIGH_SPEED_MODE;
    ch_conf.channel = config.ledc_channel;
    ch_conf.intr_type = LEDC_INTR_DISABLE;
    ch_conf.timer_sel = config.ledc_timer;
    ch_conf.duty = 2;
    ch_conf.hpoint = 0;
    esp_err_t err = ledc_channel_config(&ch_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config failed, rc=%x", err);
    }

*/
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x ", err);
   // return;
  }

  sensor_t * s = esp_camera_sensor_get();
    Serial.printf("Sensor %x ", s->id.PID);
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);//flip it back
    s->set_brightness(s, 1);//up the blightness just a bit
    s->set_saturation(s, -2);//lower the saturation
  }


  s->set_exposure_ctrl(s, 0);
  int val = 10000;
  s->set_aec_value(s, val);
  s->set_reg(s, 0x12e, 0xff, (val >> 8) & 0xff);
  s->set_reg(s, 0x12d, 0xff, val & 0xff);

  s->set_reg(s, 0x113, 0xff, 0xc2);
  s->set_reg(s, 0x100, 0xff, 0x7f);

  

  //drop down frame size for higher initial frame rate
//  s->set_framesize(s, FRAMESIZE_SVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  // Start the Stream server, and the handler processes for the Web UI.
  startCameraServer();

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      log_i("Start updating %s", type);
    })
    .onEnd([]() {
      log_i("\nEnd");
      myUDP.stop();
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      log_i("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {log_i("Auth Failed");}
      else if (error == OTA_BEGIN_ERROR) {log_i("Begin Failed");}
      else if (error == OTA_CONNECT_ERROR) {log_i("Connect Failed");}
      else if (error == OTA_RECEIVE_ERROR) {log_i("Receive Failed");}
      else if (error == OTA_END_ERROR) {log_i("End Failed");}
    });

  ArduinoOTA.begin();
  
  Serial.print("Camera Ready!  Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
  log_i("Camera ready");
}

// Notification LED 
void flashLED(int flashtime)
{
#ifdef LED_PIN                    // If we have it; flash it.
  digitalWrite(LED_PIN, LED_ON);  // On at full power.
  delay(flashtime);               // delay
  digitalWrite(LED_PIN, LED_OFF); // turn Off
#else
  return;                         // No notifcation LED, do nothing, no delay
#endif
} 

void loop() {
  // Just loop forever.
  // The stream and URI handler processes initiated by the startCameraServer() call at the
  // end of setup() will handle the camera and UI processing from now on.
  ArduinoOTA.handle();
//  delay(50);

//    vTaskList(dbg);
//    Serial.println(dbg);

    camera_fb_t * fb = NULL;
    log_i("Camera capture start");

    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        log_i("Camera capture failed");
        return ;
    }

    log_i("BUF  %02x %02x %02x %02x %02x %02x %02x %02x \n", fb->buf[0], fb->buf[1], fb->buf[2], fb->buf[3], fb->buf[4], fb->buf[5], fb->buf[6], fb->buf[7]); 

    char path[100];
    snprintf(path, 100, "/img_%04d.pgm", currentImage++);
    Serial.printf("Writing file: %s\n", path);
    
    uint32_t start = millis();
    uint32_t end = start;

#if 1
    Serial.printf("real write\n");
    writeImage(path, fb);
#else
    Serial.printf("fake write\n");
    delay(2000);
#endif
    end = millis() - start;
    flashLED(200);
    Serial.printf("%u bytes written for %u ms\n", fb->len, end);
    esp_camera_fb_return(fb);
   /*
    while(true) {
      Serial.println("ok");
      delay(30000);
      }
*/
}
