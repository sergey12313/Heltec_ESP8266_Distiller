#include <Sensor.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x3F, 16, 2);
ESP8266WebServer server(80);

String wifiPoints;
String sensors;
String currentSSID;
IPAddress currentIP;
char eepromSSID[32] = "SSID";
char eepromPassword[32] = "password";
char apSSID[20] = "SmartDistiller";
char apPassword[20] = "password";
unsigned long millis_overload = 4294967295; //Максимальное значение беззнакового 32 битного числа
unsigned long millis_loopGetData;
unsigned long millis_loopDisplay;
unsigned int disp = 0;
unsigned int sensorCount = 0;
float sensorValue[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };



//Получаем список доступных WiFi сетей
bool scanWiFi(String ssid) {
  WiFi.disconnect(); // обрываем WIFI соединения
  WiFi.softAPdisconnect(); // отключаем точку доступа, если она была
  WiFi.mode(WIFI_OFF); // отключаем WIFI
  WiFi.mode(WIFI_STA);
  delay(500);

  bool result = false;
  int n = WiFi.scanNetworks();
  wifiPoints = "[";
  if (n == 0) {
    wifiPoints = "[]";
  }
  else {
    for (int i = 0; i < n; ++i) {
      if (i > 0) wifiPoints += ",";
      wifiPoints += "{\"Name\":\"" + WiFi.SSID(i) + "\",\"Level\":\"" + WiFi.RSSI(i) + "\"}";
      if (ssid == String(WiFi.SSID(i))) result = true;
    }
  }
  wifiPoints = "{\"Points\":" + wifiPoints + "]}";
  return result;
}

//Получаем список доступных датчиков температуры
void scanSensors() {
  Sensor sensor;
  sensorCount = 0;
  sensors = "[";
  while (sensor.searchSensor() && sensorCount < 5) {
    sensorValue[sensorCount] = sensor.getTemperature();
    if (sensors != "[") sensors += ",";
    sensors += sensor.json;
    sensorCount++;
  }
  sensors = "{\"Sensors\":" + sensors + "]}";
}

//Обрабатываем root запрос
void handleRoot() {
  char temp[128];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf(temp, 128, "{\"Device\":\"%s\",\"Uptime\":\"%02d:%02d:%02d\"}", String(WiFi.macAddress()).c_str(), hr, min % 60, sec % 60);
  server.send(200, "application/json", temp);
}

//Обрабатываем запрос работоспособности
void handlePing() {
  server.send(200, "application/json", "{\"uid\":\"" + String(WiFi.macAddress()) + "\"}");
}

//Ообрабатываем запрос списка доступных WiFi сетей
void handleNetworks() {
  server.send(200, "application/json", wifiPoints);
}

//Обрабатываем запрос досупных датчиков
void handleSensors() {
  server.send(200, "application/json", sensors);
}

//Устанавливаем ssid выбранной точки доступа
void setSSID(String val) {
  for (int i = 0; i < 32; i++) {
    if (i < val.length()) eepromSSID[i] = val[i];
    else eepromSSID[i] = 0;
  }
}

//Устанавливаем password выбранной точки доступа
void setPassword(String val) {
  for (int i = 0; i < 32; i++) {
    if (i < val.length()) eepromPassword[i] = val[i];
    else eepromPassword[i] = 0;
  }
}

//Обрабатываем запрос получения данных датчика
void handleValue() {
  String json = "{\"Chip\":\"\",\"Code\":\"\",\"Celsius\":\"\"}";
  Sensor sensor;
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    if (server.argName(i) == "uid") {
      while (sensor.searchSensor()) {
        if (sensor.addrStr == server.arg(i)) {
          sensor.getTemperature();
          json = sensor.json;
        }
      }
    }
  }
  server.send ( 200, "application/json", json );
}

//Обрабатываем запрос установки параметров выбранной точки доступа
void handleSetWiFi() {
  char buf[32] = "";
  EEPROM.begin(64);
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    if (server.argName(i) == "ssid") {
      setSSID(server.arg(i));
      EEPROM.put(0, eepromSSID);
    }
    if (server.argName(i) == "password") {
      setPassword(server.arg(i));
      EEPROM.put(32, eepromPassword);
    }
  }
  EEPROM.end();
  server.send ( 200, "application/json", "\"result\":\"succesful\"" );
  setWiFi();
}

//Инициализируеп WiFi и Web сервер
void setWiFi() {
  EEPROM.begin(64);
  EEPROM.get(0, eepromSSID);
  EEPROM.get(32, eepromPassword);
  EEPROM.end();
  
  char *ssid = eepromSSID;
  char *password = eepromPassword;
  Serial.println(ssid);
  Serial.println(password);

  bool exist = scanWiFi(String(eepromSSID));
  delay(1000);


  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Init WiFi:");

  if (exist) {
    WiFi.mode(WIFI_STA);
    delay(500);
    WiFi.begin(ssid, password);
  
    int i = 0;
    String strCounter = "00";
    while (WiFi.status() != WL_CONNECTED && i < 60) {
      if(i < 10) {
        strCounter = "0" + String(i);
      } else {
        strCounter = String(i);
      }
      lcd.setCursor(0, 1);
      lcd.print("attempt: " + strCounter);
      delay(1000);
      i++;
    }
  }

  if (!exist || WiFi.status() != WL_CONNECTED) {
    WiFi.softAP(apSSID, apPassword);
    currentSSID = String(apSSID);
    currentIP = WiFi.softAPIP();
  } else {
    currentSSID = String(ssid);
    currentIP = WiFi.localIP();
  }

  printSSIDandIP();
  
  Serial.print("IP address: ");
  Serial.println(currentIP);
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());

  MDNS.begin("esp8266");
  server.onNotFound(handleRoot);
  server.on("/ping", handlePing);
  server.on("/networks", handleNetworks);
  server.on("/sensors", handleSensors);
  server.on("/value", handleValue);
  server.on("/ssid", handleSetWiFi);
  server.begin();
}

//Отображаем ssid и IP адрес на экране
void printSSIDandIP() {
  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print(currentSSID.c_str());
   lcd.setCursor(0, 1);
   lcd.print(currentIP.toString().c_str());

}

//Отображаем MAC адрес на экране
void printMAC() {
  String mac = String(WiFi.macAddress());
  mac.remove(14, 1);
  mac.remove(8, 1);
  mac.remove(2, 1);
  mac.replace(":", ".");

  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print("MAC address:");
  lcd.setCursor(0, 1);
  lcd.print(mac.c_str());
}

//Отображаем данные с датчика на экране
void printSensorValue(unsigned int i) {
    lcd.clear();
     lcd.setCursor(0, 0); 
  lcd.print(String("Sensor " + String(i + 1)).c_str());
  lcd.setCursor(0, 1); 
  lcd.print(String(String(sensorValue[i]) + char(176) + "C").c_str());

}

void setup() {
  Serial.begin(115200);

  lcd.begin(16,2);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  setWiFi();

  millis_loopDisplay = millis() + 10000;
}

void loop() {
  //Учитываем возможность переполнения millis
  if (millis_loopGetData >= millis_overload) millis_loopGetData = millis();
  if (millis_loopDisplay >= millis_overload) millis_loopDisplay = millis();
  
  if (millis_loopGetData < millis()) {
    millis_loopGetData = millis() + 2000;
    scanSensors();
  }
  if (millis_loopDisplay < millis()) {
    millis_loopDisplay = millis() + 10000;
    if (disp < sensorCount) printSensorValue(disp);
    if (disp == sensorCount) printSSIDandIP();
    if (disp == sensorCount + 1) printMAC();
    disp++;
    if (disp > sensorCount + 1) disp = 0;
  }
  server.handleClient();
}
