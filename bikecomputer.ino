#include <EEPROM.h>
#include <GyverOLED.h>
#include <ArduinoWiFiServer.h>
#include <BearSSLHelpers.h>
#include <CertStoreBearSSL.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiAP.h>
#include <ESP8266WiFiGeneric.h>
#include <ESP8266WiFiGratuitous.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WiFiSTA.h>
#include <ESP8266WiFiScan.h>
#include <ESP8266WiFiType.h>
#include <ESP8266WebServer.h>
#include <dummy.h>

//Пины датчиков холла
#define HALL1 14
#define HALL2 12
#define PIN_STATE 13

// start Wifi shit epta
ESP8266WebServer server(80);

const char* ssid = "Bycicle pc";
const char* password = "12345678";

const char* PARAM_INPUT_1 = "input1";
// Finish wifi shit

//Viarables shit
int analog;

int a1, a2;

bool sart_running = false;
int start_timer = 0;

double distance = 0;
bool hall_speed = false, hall_rotation = false;
int radius = 2000;
int mid_speed = 0, mid_rotation = 0;
long long timer_speed = 0, timer_rotation = 0;

int state = 1;

GyverOLED<SSD1306_128x64, OLED_BUFFER> oled;

const char* req1 = R"rawliteral(
  <!DOCTYPE HTML><html><head>
    <title>ESP Input Form</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    </head><body>
    Радиус сейчас равен: %radius% мм
    <form action="/get">
      Введите новый радиус: <input type="text" name="input1">
      <input type="submit" value="Подтвердить">
    </form>
  </body></html>)rawliteral";

//Функции оброботки запросов для сервера
void handleRoot() {
  String ans = String(req1);
  ans.replace("%radius%", String(radius));

  server.send(200, "text/html; charset=utf-8", ans.c_str());
}

void handleGet(){
  String new_rad = server.arg("input1");
  int rad = new_rad.toInt();

  if (rad == 0){
    String ans = String(req1);
    ans.replace("%radius%", String(radius));

    server.send(200, "text/html; charset=utf-8", ans.c_str());
    return;
  }

  radius = rad;
  EEPROM.put(0, radius);
  EEPROM.commit();

  String ans = String(req1);
  ans.replace("%radius%", String(radius));

  server.send(200, "text/html; charset=utf-8", ans.c_str());
}

//Обработка аналоговых сигналов
int get_analog(){
  return analogRead(A0);
}

void blick(){
  bool static state = false;
  digitalWrite(2, state);
  state = !state;
}

void check_state(){
  if (digitalRead(PIN_STATE)){
    state = 1;
  }
  else{
    state = 2;
  }
}

void parse_analogs(int a_speed, int a_rotations){
  if (a_speed < 50){
    hall_speed = true;
  }

  if (a_speed >= 10 && hall_speed){
    hall_speed = false;
    
    float len = ((float) 2.0 * (float) PI * (float) radius);
    distance += len / 1000000;

    mid_speed = (float) (len / ((float) (((int) millis()) - timer_speed))) * 3.6; 
    timer_speed = millis();

    print_on_screen();
    sart_running = true;
  }



  if (a_rotations < 50){
    hall_rotation = true;
  }

  if (a_rotations >= 10 && hall_rotation && millis() - timer_rotation >= 200){
    hall_rotation = false;

    mid_rotation = 1 / ((float) millis() - (float) timer_rotation) * 60000; 
    timer_rotation = millis();

    print_on_screen();
    sart_running = true;
  }
}

void changehalls(){
  static bool state = false;

  digitalWrite(HALL1, state);
  digitalWrite(HALL2, !state);

  state = !state;
}

//Рисовать всякую фигню на экране
void print_on_screen(){
  oled.clear();
  oled.setScale(2);
  oled.setCursor(0, 2);
  oled.print("V: ");
  oled.print(mid_speed); oled.print(" км/ч");

  oled.setScale(3);
  oled.setCursor(0, 4);
  oled.print(mid_rotation); oled.print(" п/м");

  oled.setScale(2);
  oled.setCursor(0, 0);
  oled.print("L: ");
  oled.print(distance);
  // oled.setCursor(0, 6);
  // oled.print(radius);
  oled.update();
}

void print_hello(){
  oled.clear();
  static int scale = 0;
  scale = (scale % 4) + 1;
  oled.setScale(scale);
  oled.setCursor(0, 2);
  oled.print("Hello!");
  // oled.print(scale);
  oled.update();
}

void print_wifi_info(){
  oled.clear();
  oled.setScale(2);
  oled.setCursor(0, 0);
  oled.print("ssid:");
  oled.setCursor(0, 2);
  oled.print(ssid);
  oled.setCursor(0, 4);
  oled.print("password:");
  oled.setCursor(0, 6);
  oled.print(password);
  oled.update();
}

void print_IP(){
  oled.clear();
  oled.setScale(1);
  oled.setCursor(0, 3);
  oled.print(WiFi.softAPIP());
  oled.update();
}

void setup() {
  pinMode(PIN_STATE, INPUT_PULLUP);
  pinMode(HALL1, OUTPUT);
  pinMode(HALL2, OUTPUT);

  Serial.begin(57600);
  changehalls();


  EEPROM.begin(100);
  EEPROM.get(0, radius);
  EEPROM.commit();

  oled.init();   

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);

  delay(10);

  server.on("/", handleRoot);
  server.on("/get", handleGet);
}

void loop() {
  check_state();

  if (state == 1){
    if (!sart_running && millis() - start_timer >= 500){
      print_hello();
      start_timer = millis();
    }

    a1 = get_analog();
    changehalls();
    a2 = get_analog();
    changehalls();

    parse_analogs(a1, a2);
  }
  else if (state == 2){
    WiFi.softAP (ssid, password);
    server.begin();
    while (state == 2){
      check_state();

      if (WiFi.softAPgetStationNum() == 0)
        print_wifi_info();
      else
        print_IP();
      
      server.handleClient();
      delay(1);
    }
    server.close();
    WiFi.forceSleepBegin();
  }
}
