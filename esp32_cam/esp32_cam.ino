#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
//#include <Wire.h>
//#include <Adafruit_GFX.h>
//#include <Adafruit_SSD1306.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ============================================
// CONFIGURARE ECRAN OLED (DEZACTIVAT TEMPORAR)
// ============================================
//#define SCREEN_WIDTH 128
//#define SCREEN_HEIGHT 64
//#define OLED_RESET    -1
//Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Credențiale rețea
const char* ssid = "Aloe";        
const char* password = "Osozne_033";                 
const char* serverName = "https://esp32-82af.onrender.com/upload"; 

// Pin buton declanșator (Conectat între GPIO 12 și GND)
#define BUTTON_PIN 12

// Pinuri cameră (AI-Thinker)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

bool systemReady = false;

void initCamera() {
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
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;   
    config.jpeg_quality = 10;            
    config.fb_count = 1;                 
  } else {
    config.frame_size = FRAMESIZE_CIF;   
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera initialization failed: 0x%x\n", err);
    return;
  }
}

//void updateDisplay(String text) {
//  display.clearDisplay();
//  display.setTextSize(1);
//  display.setTextColor(SSD1306_WHITE);
//  display.setCursor(0, 0);
//  display.println(text);
//  display.display();
//}

bool transmitPhoto(uint8_t *buf, size_t len) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting...");
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
      delay(500);
      attempts++;
    }
    if (WiFi.status() != WL_CONNECTED) return false;
  }

  WiFiClientSecure client;
  client.setInsecure(); 
  client.setTimeout(120); 
  
  HTTPClient http;
  http.setTimeout(120000); 
  
  bool success = false;
  
  if (http.begin(client, serverName)) {
    http.addHeader("Content-Type", "image/jpeg");
    http.addHeader("Content-Length", String(len));
    
    Serial.println("Trimitem poza la server...");
    int httpCode = http.POST(buf, len);
    
    if (httpCode > 0) {
      Serial.printf("Serverul a raspuns cu codul: %d\n", httpCode);
      String response = http.getString();
      Serial.println("Ce a zis serverul:");
      Serial.println(response);
      
      if (httpCode == 200) {
        //updateDisplay(response);
        success = true; 
      }
    } else {
      Serial.printf("Eroare la trimitere: %d\n", httpCode);
      if (httpCode == -1 || httpCode == -3) {
        Serial.println("Serverul probabil doarme (cold start). Asteptam...");
        delay(5000); 
      }
    }
    http.end();
  }
  return success;
}

void executeTask() {
  //updateDisplay("Capturing image...");
  
  // Eliminare cadru preliminar (rezolvă cadrul întunecat la prima captură)
  camera_fb_t * dummy_fb = esp_camera_fb_get();
  if (dummy_fb) esp_camera_fb_return(dummy_fb);
  
  // Capturare poză o singură dată
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Eroare: nu am putut face poza!");
    //updateDisplay("Capture Error");
    return;
  }
  
  Serial.printf("Am facut poza: %d bytes (octeti)\n", fb->len);
  //updateDisplay("Sending to server...");

  // Încercăm să trimitem *ACEAEAȘI POZĂ* de până la 3 ori în caz de cold start
  bool sent = false;
  for (int i = 0; i < 3 && !sent; i++) {
    if (i > 0) {
      Serial.printf("Incercarea numarul %d...\n", i + 1);
      delay(3000);
    }
    sent = transmitPhoto(fb->buf, fb->len);
  }
  
  // Eliberare memorie cadru foto
  esp_camera_fb_return(fb);

  if (sent) {
    Serial.println("=== AM REUSIT! ===");
  } else {
    Serial.println("=== NU AM REUSIT DUPA 3 INCERCARI ===");
    //updateDisplay("Transmission Error");
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Pornim ESP32-CAM ===");
  
  // Inițializare I2C și OLED dezactivată
  //Wire.begin(13, 14); 
  //if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
  //	Serial.println("OLED initialization failed.");
  //}

  //updateDisplay("Initializing...");
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Conectare WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Ne conectam la Wi-Fi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nNu merge Wi-Fi! Repornim...");
    //updateDisplay("WiFi Error");
    return;
  }
  
  Serial.println("\nConectat la Wi-Fi!");
  Serial.print("Adresa noastra IP: ");
  Serial.println(WiFi.localIP());

  initCamera();
  
  systemReady = true;
  Serial.println("Sistem pregatit. Asteptare apasare buton...");
  //updateDisplay("Ready.\nPress button.");
}

void loop() {
  if (!systemReady) return;

  // Detectare apăsare buton (GND la GPIO 12)
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("\n--- Event: Buton apasat ---");
      executeTask();
      
      // Așteptare eliberare buton pentru a preveni declanșări repetate accidentale
      while (digitalRead(BUTTON_PIN) == LOW) {
        delay(10);
      }
    }
  }
}
