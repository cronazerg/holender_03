#include <WiFiClient.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_camera.h>
#include <Arduino.h>
#include <esp_timer.h>
#include <FS.h>
#include "ESPAsyncWebServer.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <iostream>
#include <string>
#define CAMERA_MODEL_AI_THINKER
#include <SD.h>
#include <SPIFFS.h>
#define DIODA 33
#include "camera_pins.h"

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

const char* ssid = "";
const char* password = "";

int photoCounter = 1;

String formattedDate;
int godzinaStart = 32400;
int godzinaEnd = 66600;
int obecnaGodzina = 48200;
double sliderValue = 2;
bool photoWasMade = false;

AsyncWebServer server(80);

String send_html()
{
  String html = "";
  html += "<!DOCTYPE HTML><html><head><meta name='viewport' content='width=device-width, initial-scale=1'></head><body><h2>Galeria zdjec kamery ESP32</h2>";
  html += "<div id='picker'  class='picker'>";
  html += "<label for='vol'>Wybierz czestotliwosc robienia zdjec od 1 do 100 sekund </label>";
  html += "<label for='vol' id='volValue'></label>";
  html += "<input onchange='updateSlider(this)' type='range' id='vol' name='vol' min='1' max='100'>";
  html += "<label for='start-time'> Zdjecia rozpoczna sie: </label>";
  html += "<input onchange='updateStartTime(this)' id='start-time' type='time' name='start-time' value='09:00'>";
  html += "<label for='end-time'> Zdjecia zakoncza sie: </label>";
  html += "<input onchange='updateEndTime(this)' id='end-time' type='time' name='end-time' value='18:30'>";
  html += "<button type='submit' onClick='refreshPage()'>Odswiez strone</button>";
  html += "</div>";
  html += "<div id='gallery' style='display: flex; flex-wrap: wrap; margin-top: 20px;'>";
  if (photoWasMade) {
    for (int i = 2; i <= photoCounter; i++) {
      html += "<div>";
      html += "<img src='/fotka?src=/";
      html += String(i);
      html += ".jpg'>";
      html += "<p>";
      html += "zdjecie-";
      html += String(i);
      html += "</p>";
      html += "</div>";
    }
  }
  html += "</div>";
  html += "</body>";
  html += "<script>";
  html += "function refreshPage(){";
  html += "window.location.reload();";
  html += "}";
  html += "function updateSlider(element) {";
  html += "var sliderValue = document.getElementById('vol').value;";
  html += "document.getElementById('volValue').innerHTML = sliderValue;";
  html += "const xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/slider?value='+sliderValue, true);";
  html += "xhr.send();";
  html += "}";
  html += "function updateStartTime(element) {";
  html += "var startTimeElement = document.getElementById('start-time').value;";
  html += "var a = startTimeElement.split(':');";
  html += "var seconds = (+a[0]) * 60 * 60 + (+a[1]) * 60;";
  html += "const xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/timeStart?value='+seconds, true);";
  html += "xhr.send();";
  html += "}";
  html += "function updateEndTime(element) {";
  html += "var endTimeElement = document.getElementById('end-time').value;";
  html += "var a = endTimeElement.split(':');";
  html += "var seconds = (+a[0]) * 60 * 60 + (+a[1]) * 60;";
  html += "const xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/timeEnd?value='+seconds, true);";
  html += "xhr.send();";
  html += "}";
  html += "window.setInterval (() => {";
  html += "var today = new Date();";
  html += "var currentTime = today.getHours() * 60 * 60 + today.getMinutes() * 60 + today.getSeconds();";
  html += "const xhr = new XMLHttpRequest();";
  html += "xhr.open('GET', '/currentTime?value='+currentTime, true);";
  html += "xhr.send()}, 1000)";
  html += "</script>";

  return html;
}

bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open("/"+String(photoCounter)+".jpg");
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

void fotka()
{
  camera_fb_t * fb = NULL;
  bool ok = 0; 
  do {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
    }
    photoCounter++;

    Serial.printf("Picture file name: %s\n", "photo.jpg");
    File file = SPIFFS.open("/"+String(photoCounter)+".jpg", FILE_WRITE);

    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len);
      Serial.print("The picture has been saved in ");
      Serial.print("photo.jpg");
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
    }
    file.close();
    esp_camera_fb_return(fb);
    ok = checkPhoto(SPIFFS);
  } while (!ok);
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

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

  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  if (!SPIFFS.begin(true)) {
    Serial.println("Błąd montowania systemu plików");
    ESP.restart();
  }

  Serial.println("Pamięć ok");
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Błąd inicjacji kamery numer: 0x%x", err);
    return;
  }
  Serial.println("Kamera ok");

  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Połączono z WIFI");

  Serial.print("Kamera gotowa wejdź na adres: 'http://");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", send_html());
  });

  server.on("/fotka", HTTP_GET, [](AsyncWebServerRequest *request){
    for (uint8_t i = 0; i < request->args(); i++) {
      if (request-> argName(i) == "src") {
        request-> send(SPIFFS, request->arg(i), "image/jpg");
      }
    }
  });

  server.on("/slider", [](AsyncWebServerRequest *request) {
    String sliderValueStr = request->getParam(0)->value();
    sliderValue = sliderValueStr.toDouble();
    request-> send(200, "text/html", send_html());
  });

  server.on("/timeStart", [](AsyncWebServerRequest *request) {
    String godzinaStartStr = request->getParam(0)->value();
    godzinaStart = godzinaStartStr.toInt();
    request-> send(200, "text/html", send_html());
  });

  server.on("/timeEnd", [](AsyncWebServerRequest *request) {
    String godzinaEndStr = request->getParam(0)->value();
    godzinaEnd = godzinaEndStr.toInt();
    request-> send(200, "text/html", send_html());
  });

  server.on("/currentTime", [](AsyncWebServerRequest *request) {
    String obecnaGodzinaStr = request->getParam(0)->value();
    obecnaGodzina = obecnaGodzinaStr.toInt();
    request-> send(200, "text/html", send_html());
  });

  timeClient.begin();
  timeClient.setTimeOffset(3600);
  server.begin();
}

void loop() {
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }

  delay(sliderValue * 1000);

  if (godzinaStart <= obecnaGodzina && godzinaEnd >= obecnaGodzina) {
    fotka();
  }

  photoWasMade = true;
}
