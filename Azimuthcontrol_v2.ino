/*
   Версия 2
   расчёт азимута по максимальному напряжению на датчике (360 градусов)
   и минимальному напряжению на датчике (0 градусов)
   Значения сохраняются в EEPROM

*/
#include <ESP8266WiFi.h>                                                // Библиотека для создания Wi-Fi подключения (клиент или точка доступа)
#include <ESP8266WebServer.h>                                           // Библиотека для управления устройством по HTTP (например из браузера)
#include <FS.h>                                                         // Библиотека для работы с файловой системой
#include <ESP8266FtpServer.h>                                           // Библиотека для работы с SPIFFS по FTP
#include <ArduinoJson.h>                                                // Библиотека для работы с JSON

#define ADC_IN A0
#define PIN_PRIVOD_LEFT 16
#define PIN_PRIVOD_RIGHT 5
#define PIN_LED_WIFI_STATUS 4
#define PIN_LED_LOW_RSSI 0
#define RSSI_LOW_LEVEL -90
#define AZ_ACCURACY 2                                                    // точность установки азимута

String ssid     = "DIR-320NRU";
String password = "12345678";

int out_pins[] = {PIN_PRIVOD_LEFT, PIN_PRIVOD_RIGHT, PIN_LED_WIFI_STATUS, PIN_LED_LOW_RSSI};

int count = 0;
int countVlt = 0;
float currentVoltage;
float currentVoltageSumm = 0;
float CurrentAzSumm;
int currentAzimuth = 0;
float needAzimuth = 0;
bool needRotateAnt = false;
float uAdcMax = 1;               // максимальное напряжение на датчике
float uAdcMin = 0;               // минимальное напряжение на датчике
float voltOnDegree = 0.002;      // значение вольт на градус
bool inversionDegrees = false;   // false если минимальное напряжение с датчика угла поворота соответствует азимуту 0 градусов (например сопротивление датчика увеличивается при повороте по часовой)
// true если минимальное напряжение соответствует азимуту 360 градусов (например сопротивление датчика уменьшается при повороте по часовой)
bool wifiClientIsOn = false;

unsigned long prevAzCalcMillis = 0;
const long azCalcInterval = 10;
const long antRotateTimeout = 300000;    // если антенна не достигнла нужного азимута за время antRotateTimeout, то прекратить вращение антенны
unsigned long prevNeedAntRotate = 0;
unsigned long prevVoltMeasMillis = 0;
const long voltMeasInterval = 5;

ESP8266WebServer HTTP(80);                                              // Определяем объект и порт сервера для работы с HTTP
FtpServer ftpSrv;                                                       // Определяем объект для работы с модулем по FTP

String jsonConfig = "{}";                                               // Строка для работы с форматом json для сохранения конфигурации

void setup() {
  for (int i = 0; i < sizeof(out_pins); i++) {
    pinMode(out_pins[i], OUTPUT);                                       // Настраиваем пины в OUTPUT
  }
  Serial.begin(115200);                                                 // Инициализируем вывод данных на серийный порт со скоростью 115200 бод

  SPIFFS.begin();                                                       // Инициализируем работу с файловой системой
  HTTP.begin();                                                         // Инициализируем Web-сервер
  ftpSrv.begin("ham", "ham");                                           // Поднимаем FTP-сервер для удобства отладки работы HTML (логин: relay, пароль: relay)

  loadConfig();                                                         // загрузка конфигурации

  if (wifiClientIsOn) {                                                 // запуск WiFi в зависимости от настроек
    WiFi.mode(WIFI_AP_STA);
    Serial.println(F("----"));
    Serial.println(F("\nStarting AP"));
    WiFi.softAP(F("ESPAP"), F("87654321"));                             // SSID и пароль точки доступа - ESPAP, 87654321
    IPAddress myIP = WiFi.softAPIP();
    Serial.print(F("AP IP address: "));
    Serial.println(myIP);

    Serial.println(F("\nStarting WIFI client"));
    Serial.print(F("SSID: "));Serial.println(ssid);
    Serial.print(F("Password: "));Serial.println(password);
    WiFi.begin(ssid, password);
    int timeoutcount = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      timeoutcount++;
      Serial.print(".");
      if (timeoutcount >= 240) break;
    }
    if (timeoutcount >= 240) {
      Serial.println(F("\nCan't connect to WiFi network. Client is off"));
      wifiClientIsOn = false;
      WiFi.mode(WIFI_AP);
      saveConfig();
    }
    else {
      Serial.println(F("\nWiFi client is connected"));
      Serial.print(F("IP address: "));
      Serial.println(WiFi.localIP());
      Serial.print(F("RSSI: "));
      Serial.println(WiFi.RSSI());
    }
  }
  else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(F("ESPAP"));
    IPAddress myIP = WiFi.softAPIP();
    Serial.println(F("----"));
    Serial.println(F("\nStarting AP, client is off"));
    Serial.print(F("AP IP address: "));
    Serial.println(myIP);
  }

  // Обработка HTTP-запросов

  HTTP.on("/setminvoltage", setMinVoltage);
  HTTP.on("/setmaxvoltage", setMaxVoltage);
  HTTP.on("/changeinversion", changeInversion);
  HTTP.on("/data", handleData);
  HTTP.on("/setazimuth", setNeededAzimuth);
  HTTP.on("/get_wifi_cli_data", handleWifiData);
  HTTP.on("/wificliset", wifiCliSet);
  HTTP.on("/wifi_cli", wifiCliStatus);
  HTTP.on("/restart", handle_Restart);
  HTTP.on("/stopprivod", stopPrivod);
  
  HTTP.onNotFound([]() {                                                // Описываем действия при событии "Не найдено"
    if (!handleFileRead(HTTP.uri()))                                    // Если функция handleFileRead (описана ниже) возвращает значение false в ответ на поиск файла в файловой системе
      HTTP.send(404, F("text/plain"), F("Not Found"));                  // возвращаем на запрос текстовое сообщение "File isn't found" с кодом 404 (не найдено)
  });
}

void loop() {
  unsigned long currentMillis = millis();
  HTTP.handleClient();                                                // Обработчик HTTP-событий (отлавливает HTTP-запросы к устройству и обрабатывает их в соответствии с выше описанным алгоритмом)
  ftpSrv.handleFTP();                                                 // Обработчик FTP-соединений


  if (currentMillis - prevVoltMeasMillis >= voltMeasInterval) {
    prevVoltMeasMillis = currentMillis;

    voltageMeasure();

    if (WiFi.status() == WL_CONNECTED) digitalWrite(PIN_LED_WIFI_STATUS, HIGH);
    else digitalWrite(PIN_LED_WIFI_STATUS, LOW);
    if (WiFi.RSSI() < RSSI_LOW_LEVEL) digitalWrite(PIN_LED_LOW_RSSI, LOW);
    else digitalWrite(PIN_LED_LOW_RSSI, HIGH);
  }

  if (currentMillis - prevAzCalcMillis >= azCalcInterval) {
    prevAzCalcMillis = currentMillis;

    azimuthCalculate();

    if ((currentMillis - prevNeedAntRotate >= antRotateTimeout) && (needRotateAnt == true)) {
      needRotateAnt = false;
      digitalWrite(PIN_PRIVOD_RIGHT, LOW);    // если прошло antRotateTimeout мсекунд, то прекращаем поворот антенны
      digitalWrite(PIN_PRIVOD_LEFT, LOW);
    }

    if (needRotateAnt) {
      rotateAnt();
    }
  }
}


bool handleFileRead(String path) {                                      //  Функция работы с файловой системой
  if (path.endsWith(F("/"))) path += F("index.html");                   //  Если устройство вызывается по корневому адресу, то должен вызываться файл index.html (добавляем его в конец адреса)
  String contentType = getContentType(path);                            //  С помощью функции getContentType (описана ниже) определяем по типу файла (в адресе обращения) какой заголовок необходимо возвращать по его вызову
  if (SPIFFS.exists(path)) {                                            //  Если в файловой системе существует файл по адресу обращения
    File file = SPIFFS.open(path, "r");                                 //  Открываем файл для чтения
    size_t sent = HTTP.streamFile(file, contentType);                   //  Выводим содержимое файла по HTTP, указывая заголовок типа содержимого contentType
    file.close();                                                       //  Закрываем файл
    return true;                                                        //  Завершаем выполнение функции, возвращая результатом ее исполнения true (истина)
  }
  return false;                                                         //  Завершаем выполнение функции, возвращая результатом ее исполнения false (если не обработалось предыдущее условие)
}

void setNeededAzimuth() {
  needAzimuth = HTTP.arg("azimuth").toFloat();
  prevNeedAntRotate = millis();                                          // засекаем время начала поворота антенны
  needRotateAnt = true;
  HTTP.send(200, F("text/html"), F("OK"));
}


// функция вращения антенны
void rotateAnt() {
  if ( (currentAzimuth >= (needAzimuth - 1)) && (currentAzimuth <= (needAzimuth + 1)) ) {
    digitalWrite(PIN_PRIVOD_RIGHT, LOW);
    digitalWrite(PIN_PRIVOD_LEFT, LOW);
    needRotateAnt = false;
    return;
  }

  if ( (currentAzimuth <= needAzimuth) && (digitalRead(PIN_PRIVOD_RIGHT) == LOW ) ) {
    digitalWrite(PIN_PRIVOD_RIGHT, HIGH);
  }

  if ( (currentAzimuth >= needAzimuth) && (digitalRead(PIN_PRIVOD_LEFT) == LOW ) ) {
    digitalWrite(PIN_PRIVOD_LEFT, HIGH);
  }
}

// обработка запроса данных 
void handleData() {
  String jsonMessage = "{}";
  DynamicJsonBuffer jsonBuffer;
  //  вызовите парсер JSON через экземпляр jsonBuffer
  JsonObject& json = jsonBuffer.parseObject(jsonMessage);
  // Заполняем поля json
  json["curazimuth"] = currentAzimuth;
  json["curvoltage"] = currentVoltage;
  json["inversion"] = inversionDegrees ? 1 : 0;
  json["antrotate"] = needRotateAnt ? 1 : 0;
  json["adcmax"] = uAdcMax;
  json["adcmin"] = uAdcMin;
  json["voltondegree"] = voltOnDegree;
  json["clientipaddr"] = WiFi.localIP().toString();
  json["rssi"] = WiFi.RSSI();
  json["ssid"] = ssid;
  json["wificlistatus"] = wifiClientIsOn ? 1 : 0;
  // Помещаем созданный json в глобальную переменную json.printTo(jsonConfig);
  jsonMessage = "";
  json.printTo(jsonMessage);
  /*
    String message = F("{\"curazimuth\":");
    message += String(currentAzimuth) + F(",");
    message += F("\"curvoltage\":");
    message += String(analogRead(ADC_IN) / 1023.0) + F(",");
    message += F("\"inversion\":");
    if (inversionDegrees) message += String("1,");
    else message += String("0,");
    message += F("\"rssi\":");
    message += String(WiFi.RSSI()) + F(",");
    message += F("\"antrotate\":");
    message += needRotateAnt ? "1" : "0";
    message += F("}");
  */
  HTTP.send(200, F("text/html"), jsonMessage);
}

void handleWifiData() {
  String jsonMessage = "{}";
  DynamicJsonBuffer jsonBuffer;
  //  вызовите парсер JSON через экземпляр jsonBuffer
  JsonObject& json = jsonBuffer.parseObject(jsonMessage);
  // Заполняем поля json
  json["ssid"] = ssid;
  json["password"] = password;
  // Помещаем созданный json в глобальную переменную json.printTo(jsonConfig);
  jsonMessage = "";
  json.printTo(jsonMessage);
  HTTP.send(200, F("text/html"), jsonMessage);
}

// обработка запроса для установки настроек WiFi
void wifiCliSet() {
  ssid = HTTP.arg("ssid");
  password = HTTP.arg("password");
  wifiClientIsOn = true;
  saveConfig();
  Serial.println(F("WiFi pass and ssid received"));
  HTTP.send(200, F("text/html"), F("OK"));
}

// Перезагрузка модуля по запросу вида /restart?device=ok
void handle_Restart() {
  String restart = HTTP.arg("device");           // Получаем значение device из запроса
  if (restart == "ok") {                         // Если значение равно Ок
    HTTP.send(200, F("text / plain"), "Reset OK");  // Oтправляем ответ Reset OK
    Serial.println(F("ESP reset"));
    delay(100);
    ESP.restart();                               // перезагружаем модуль
  }
  else {                                        // иначе
    Serial.println(F("ESP no reset"));
    HTTP.send(200, F("text / plain"), "No Reset"); // Oтправляем ответ No Reset
  }
}

// обработка запроса на включение или выключение WiFi клиента
void wifiCliStatus() {
  String _command = HTTP.arg("status");
  if (_command == "on") {
    wifiClientIsOn = true;
    Serial.println(F("WiFi client status is on"));
  }
  if (_command == "off") {
    wifiClientIsOn = false;
    Serial.println(F("WiFi client status is off"));
  }
  saveConfig();
  HTTP.send(200, F("text/html"), F("OK"));
}

String getContentType(String filename) {                                // Функция, возвращающая необходимый заголовок типа содержимого в зависимости от расширения файла
  if (filename.endsWith(".html")) return "text/html";                   // Если файл заканчивается на ".html", то возвращаем заголовок "text/html" и завершаем выполнение функции
  else if (filename.endsWith(".css")) return "text/css";                // Если файл заканчивается на ".css", то возвращаем заголовок "text/css" и завершаем выполнение функции
  else if (filename.endsWith(".js")) return "application/javascript";   // Если файл заканчивается на ".js", то возвращаем заголовок "application/javascript" и завершаем выполнение функции
  else if (filename.endsWith(".png")) return "image/png";               // Если файл заканчивается на ".png", то возвращаем заголовок "image/png" и завершаем выполнение функции
  else if (filename.endsWith(".jpg")) return "image/jpeg";              // Если файл заканчивается на ".jpg", то возвращаем заголовок "image/jpg" и завершаем выполнение функции
  else if (filename.endsWith(".gif")) return "image/gif";               // Если файл заканчивается на ".gif", то возвращаем заголовок "image/gif" и завершаем выполнение функции
  else if (filename.endsWith(".ico")) return "image/x-icon";            // Если файл заканчивается на ".ico", то возвращаем заголовок "image/x-icon" и завершаем выполнение функции
  return "text/plain";                                                  // Если ни один из типов файла не совпал, то считаем что содержимое файла текстовое, отдаем соответствующий заголовок и завершаем выполнение функции
}

/*
  void azimuthCalculate() {
  float measuredU = (analogRead(ADC_IN) / 1023.0);
  float Azimuth = (measuredU - uAdcMin) / voltOnDegree;
  CurrentAzSumm += Azimuth;
  count++;
  if (count >= 30)
  {
    count = 0;
    if (inversionDegrees) currentAzimuth = 360 - CurrentAzSumm / 30;
    else currentAzimuth = CurrentAzSumm / 30;
    CurrentAzSumm = 0;
  }
  }
*/

// обработка запроса для остановки привода
void stopPrivod() {
    digitalWrite(PIN_PRIVOD_RIGHT, LOW);
    digitalWrite(PIN_PRIVOD_LEFT, LOW);
    needRotateAnt = false;
    HTTP.send(200, F("text/html"), "STOPPED");
}

// функция расчёта азимута
void azimuthCalculate() {
  float Azimuth = (currentVoltage - uAdcMin) / voltOnDegree;
  if (inversionDegrees) currentAzimuth = 360 - Azimuth;
  else currentAzimuth = Azimuth;
}

// обработка запроса для установки минимального напряжения датчика угла поворота
void setMinVoltage() {
  uAdcMin = currentVoltage;
  Serial.print("uAdcMin="); Serial.println(uAdcMin);
  HTTP.send(200, F("text/html"), "OK");
}

// обработка запроса для установки максимального напряжения датчика угла поворота
void setMaxVoltage() {
  uAdcMax = currentVoltage;
  Serial.print("uAdcMax="); Serial.println(uAdcMax);
  voltOnDegree = (uAdcMax - uAdcMin) / 360;
  Serial.print("voltOnDegree="); Serial.println(voltOnDegree);
  saveConfig();
  HTTP.send(200, F("text/html"), "OK");
}

// обработка запроса для установки или выключения инверсии показаний азимута
void changeInversion() {
  if (inversionDegrees == false) inversionDegrees = true;
  else inversionDegrees = false;
  saveConfig();
  HTTP.send(200, F("text/html"), "OK");
}

// функция для измерения напряжения
void voltageMeasure() {
  currentVoltageSumm += analogRead(ADC_IN) / 1023.0;
  countVlt++;
  if (countVlt >= 40) {
    currentVoltage = currentVoltageSumm / 40;
    countVlt = 0;
    currentVoltageSumm = 0;
  }
}
