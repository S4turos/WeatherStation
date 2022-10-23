#include <WiFi.h>
#include <WiFiClient.h>
#include "myconfig.h"
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "LiquidCrystal.h"
#include <Keypad.h>
#include <EnvironmentCalculations.h>
#include <BME280I2C.h> //https://github.com/finitespace/BME280
#include <Wire.h>
#include "ThingSpeak.h" //https://github.com/mathworks/thingspeak-arduino
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include "time.h"
#include <TimeLib.h> //https://github.com/PaulStoffregen/Time
#include "TelegramBot.h"
#include "LCD_char.h"
#include "MQ135.h"

#define SERIAL_BAUD 115200

// CONSULTA DE LA HORA
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
int checkMin;
int offset;

// CONSULTA DE LA API DE OPEN WEATHER MAP
String jsonBuffer;
String weather; // descripción del tiempo
double gTemperature, mTemperature, sensacionT; //distintas consultas de temperatura
unsigned long salida, puesta, duracion;// salida y puesta de Sol y duración del día
time_t salidaT, puestaT;
String urlG = "http://api.openweathermap.org/data/2.5/weather?lat=37.15&lon=-3.6&units=metric&lang=es&appid=" + openWeatherMapApiKey;
//String urlM = "http://api.openweathermap.org/data/2.5/weather?lat=36.75&lon=-3.52&units=metric&lang=es&appid=" + openWeatherMapApiKey;

// PÁGINA WEB
WebServer server(80);
const char* host = "esp32";
String measurements; // Measurements table for local server

/*
 * Server Index Page
 */

const char* serverIndex =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

// BME280

#define SEALEVELPRESSURE_HPA (1013.25)
BME280I2C bme;
float outsideTemp; //temperatura exterior para realizar comparaciones

bool preheating = true;
unsigned int preheatingMQ = 30000; //milisegundos de precalentamiento del sensor MQ135 (30 segundos)

//INTERVALOS
unsigned long previousMillis = 0UL;
unsigned long previousCompTH = 0UL;
unsigned long previousCompP = 0UL;
unsigned long previousAPI = 0UL;
unsigned long intervalLT = 1000UL; // cada cuantos milisegundos se actualiza la hora
unsigned long previousLT = 0UL;
unsigned long previousSecond = 0UL;
unsigned long pSensors = 0UL;
unsigned long previousTS = 0UL;
unsigned long pScreen = 0UL;
unsigned long temperatureAlert = 600000UL; // periodo refractario del aviso de la subida de temperatura (10 minutos)
unsigned long pTempAlert = 0UL;

//WIFI
/*Put your SSID & Password*/
const char* ssid = SECRET_SSID;  // Enter SSID here
const char* password = SECRET_PASS;  //Enter Password here
WiFiClient  client;

//ThingSpeak Channel
unsigned long myChannelNumber = SECRET_CH_ID;
const char* myWriteAPIKey = SECRET_WRITE_APIKEY;

// PANTALLA LCD
// Create An LCD Object. Signals: [ RS, EN, D4, D5, D6, D7 ]
LiquidCrystal lcd(13, 12, 14, 27, 26, 25);
const int lcdLightPin =  33;    // the number of the light pin of the LCD

// TECLADO
const byte ROWS = 4;// Filas
const byte COLS = 4;// Columnas
char keys[ROWS][COLS]={
  {'1','5','9','13'},
  {'2','6','10','14'},
  {'3','7','11','15'},
  {'4','8','12','16'}
};
byte rowPins[ROWS] = {15,2,0,4};
byte colPins[COLS] = {16,17,5,18};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
char screen = '1';//por defecto 1, la pantalla de inicio

//VARIABLES GLOBALES para poder acceder a ellas en todo el código
float temperature, humidity, pressure, altitude, ptemperature, phumidity, absHumidity, heatIndex, seaLevelPres, dewPoint, pPressure, CO2;
int tTemperature, tHumidity, tPres, cont, ilum;
float steepT = 0.15 / 60.0;
float steepH = 1.5 / 60.0;
float steepP = 0.3 / 600.0;
int lowPressure = 0;
int contar = 0;
int contC = 0;
int ilumC = 0;
float CO2C = 0;
float temperatureC = 0.0;
float humidityC = 0.0;
float pressureC = 0.0;

// fororresistencia     Fuente: https://www.luisllamas.es/medir-nivel-luz-con-arduino-y-fotoresistencia-ldr/
//const long A = 1000;     //Resistencia en oscuridad en KΩ
//const int B = 15;        //Resistencia a la luz (10 Lux) en KΩ
//const int Rc = 10;       //Resistencia calibracion en KΩ
const int LDRPin = 34;   //Pin del LDR

void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(lcdLightPin, OUTPUT);
  digitalWrite(lcdLightPin, HIGH); //activando la pantalla
  pinMode(LEDpin, OUTPUT);
  digitalWrite(LEDpin, LOW);
  lcd.begin(16, 2); // Initialize The LCD. Parameters: [ Columns, Rows ]
  lcd.createChar(1, arrowUp);
  lcd.createChar(2, arrowDown);
  lcd.createChar(3, sunRise);
  lcd.createChar(4, sunSet);
  lcd.createChar(5, happy);
  lcd.createChar(6, sad);
  Wire.begin();
  initBME280();
  //calibrateMQ135();
  startMQ135();
  initWiFi();
  sendMessage("Se reinició el microcontrolador");
  ThingSpeak.begin(client); // Initialize ThingSpeak
  APIcall(urlG);
  readSensors(); //first sensor reading
  getData();
  updateIndex();
  ptemperature = temperature; //para poder comparar después
  phumidity = humidity;
  pPressure = pressure;
  if(LCDlight){
    printLCD(screen); //first print of the main screen
  }else{
    digitalWrite(lcdLightPin, LOW);
    lcd.noDisplay();
  }
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); //configuración del reloj
  initServer();
}

void loop() {
  char key = keypad.getKey(); //constantemente está leyendo la entrada del teclado
  if ((key != NO_KEY && key != screen) || key == '6'){//se ejecutra el código si detecta una tecla y distinta a la que está por defecto
    printLCD(key);
  }
  unsigned long currentMillis = millis(); //milisegundos desde que se inició el sketch

  if(currentMillis - pSensors > intSensors){ //lectura de sensores
    readSensors();
    pSensors = currentMillis;

    if(currentMillis - pScreen > intScreen){
      getData();
      if(screen == '1' || screen == '2' || screen == '4' || screen == '7'){//actualización de pantalla
        printLCD(screen);
      }
      updateIndex(); //Se actualiza la página web
      pScreen = currentMillis;
    }
  }

  if(currentMillis - previousAPI > APIinterval){//API de Open Weather Map
    APIcall(urlG);
    //APIcall(urlM);
    previousAPI = currentMillis;
  }

  if(currentMillis - previousTS > intervalTS){//se suben los datos a TS

    if(uploadTS){
      getData();
      uploadThingSpeak();
    }

    resetCounter();
    if(preheating && (currentMillis > preheatingMQ)){
      preheating = false; //en el siguiente ciclo ya se harán los cálculos para subir los datos del sensor MQ135
    }
    previousTS = currentMillis;

  }

  if((currentMillis - previousLT > intervalLT) && screen == '3'){//el temporizador del reloj
    //Serial.println("Se activa temporizador del reloj");
    printLocalTime();
    previousLT = currentMillis;
  }

  if(currentMillis - previousCompTH > compTH){ //se compara la temperatura y la humedad con la última medida
    comp_TH();
    previousCompTH = currentMillis;
  }
  
  if(currentMillis - previousCompP > compP){//se compara la presión con la última medida
    comp_P();
    previousCompP = currentMillis;
  }

  server.handleClient();
  delay(1);

}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  Serial.println("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println("WiFi connected..!");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.macAddress());
}

void initBME280 (){

  while(!bme.begin())
  {
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
  }

  switch(bme.chipModel())
  {
     case BME280::ChipModel_BME280:
       Serial.println("Found BME280 sensor! Success.");
       break;
     case BME280::ChipModel_BMP280:
       Serial.println("Found BMP280 sensor! No Humidity available.");
       break;
     default:
       Serial.println("Found UNKNOWN sensor! Error!");
  }

}

void uploadThingSpeak(){

  ThingSpeak.setField(1, temperature);
  ThingSpeak.setField(2, humidity);
  ThingSpeak.setField(3, pressure);
  ThingSpeak.setField(4, seaLevelPres);
  ThingSpeak.setField(5, dewPoint);
  if(!preheating){
    ThingSpeak.setField(6, CO2);
    ThingSpeak.setField(7, cont);
  }
  ThingSpeak.setField(8, ilum);

  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if(x == 200){
    Serial.println("ThingSpeak: channel update successful.");
    Serial.println();
  }
  else{
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
 }

void initServer(){
   /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", measurements);
  });
  server.on("/updateCode", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
 }

 void printLCD(char k){
    //Serial.print("Input recogido: ");
    //Serial.println(k);

    switch(k){

      case '1': //Pantalla 1
        //Serial.println("Escribiendo pantalla 1");
        lcd.clear();
        lcd.print(temperature, 1);
        lcd.print((char)223);
        lcd.print(" ");
        if(tTemperature == 1){
          lcd.write(1);
        }else if (tTemperature == 2){
          lcd.write(2);
        }else if (tTemperature == 3){
          lcd.print("=");
        }else if(tTemperature == 4){ //subida brusca
          lcd.write(1);
          lcd.write(1);
        }else if (tTemperature == 5){ //descenso brusco
          lcd.write(2);
          lcd.write(2);
        }
        lcd.setCursor(10, 0);
        lcd.print(humidity, 0);
        lcd.print("% ");
        if(tHumidity == 1){
          lcd.write(1);
        }else if (tHumidity == 2){
          lcd.write(2);
        }else if (tHumidity == 3){
          lcd.print("=");
        }else if (tHumidity == 4){ //subida brusca
          lcd.write(1);
          lcd.write(1);
        }else if (tHumidity == 5){ //descenso brusco
          lcd.write(2);
          lcd.write(2);
        }
        lcd.setCursor(0, 1);
        lcd.print(seaLevelPres, 1);
        lcd.print(" hPa ");
        if(tPres == 1){
          lcd.write(1);
        }else if (tPres == 2){
          lcd.write(2);
        }else if (tPres == 3){
          lcd.print("=");
        }else if (tPres == 4){ //subida brusca
          lcd.write(1);
          lcd.write(1);
        }else if (tPres == 5){ //descenso brusco
          lcd.write(2);
          lcd.write(2);
        }
        lcd.setCursor(14, 1);
        if(temperature < 20.0 || temperature >= 30.0 || humidity < 30.0 || humidity >= 70.0 || cont >= 375){ //disconfort
          lcd.write(6); //sad face
        }else{
          lcd.write(5); //happy face
        }
        screen = k;
        break;

      case '2': //Pantalla 2
        //Serial.println("Escribiendo pantalla 2");
        lcd.clear();
        lcd.print("Int:");
        lcd.print(temperature, 1);
        lcd.print((char)223);
        lcd.print("(");
        lcd.print(heatIndex, 1);
        lcd.print((char)223);
        lcd.print(")");
        lcd.setCursor(0, 1);
        lcd.print("Ext:");
        lcd.print(outsideTemp, 1);
        lcd.print((char)223);
        lcd.print("(");
        lcd.print(sensacionT, 1);
        lcd.print((char)223);
        lcd.print(")");
        screen = k;
        break;

      case '3':
        if(screen != k){ //que solo se ejecute una vez al pulsar el botón
          //Serial.println("Escribiendo pantalla 3");
          //reiniciamos la variable que nos permite actualizar la hora cada minuto
          checkMin = -1;
          lcd.clear();
          printLocalTime();
          screen = k;
          previousLT = millis(); //iniciamos contador;
        }
        
        break;

      case '4':
        lcd.clear();
        lcd.print("CO2:");
        lcd.print(CO2, 0);
        lcd.print(" ppm(");
        lcd.print(cont);
        lcd.print(")");
        lcd.setCursor(0, 1);
        if(cont < 200){ //habitación limpia
          lcd.print("Calidad: buena");
        }else if(cont >= 200 && cont < 300){
          lcd.print("Calidad: normal");
        }else if(cont >= 300 && cont < 400){
          lcd.print("Calidad: mala");
        }else if(cont >= 400){
          lcd.print("Calidad:muy mala");
        }
        screen = k;
        break;

      case '5':
        //Serial.println("Escribiendo pantalla 5");
        lcd.clear();
        if (WiFi.status() != WL_CONNECTED) {
          lcd.print("No conectado");
        }else{
          lcd.print(ssid);
          lcd.setCursor(0, 1);
          lcd.print(WiFi.localIP());
        }
        screen = k;
        break;

      case '6':
        //Serial.println("Escribiendo pantalla 6");
        lcd.clear();
        if(uploadTS){
          uploadTS = false;
          lcd.print("Desconectado de");
          lcd.setCursor(0, 1);
          lcd.print("ThingSpeak");
        }else{
          uploadTS = true;
          lcd.print("Conectado a");
          lcd.setCursor(0, 1);
          lcd.print("ThingSpeak");
        }
        screen = k;
        break;

      case '7':
        lcd.clear();
        lcd.print("Ilum: ");
        lcd.print(ilum);
        lcd.print(" ADC");
        
        /* EXPERIMENTAL
        setTime(salidaT);
        int h_salida, m_salida, h_puesta, m_puesta;
        h_salida = hour();
        m_salida = minute();
        setTime(puestaT);
        h_puesta = hour();
        m_puesta = minute();
        struct tm timeinfo;
        if(!getLocalTime(&timeinfo)){
          Serial.println("Failed to obtain time");
          return;
        }
        char timeHour[3];
        char timeMin[3];
        strftime(timeHour,3, "%H", &timeinfo);
        strftime(timeMin,3, "%M", &timeinfo);
        int actualHour;
        actualHour = atoi(timeHour);
        int actualMin;
        actualMin = atoi(timeMin);
        if((actualHour > h_salida && actualHour < h_puesta) || (actualHour == h_salida && actualMin >= m_salida) || (actualHour == h_puesta && actualMin <= m_puesta)){ //es de día
          
          if(ilum >= 2500 && ilum <= 3500){ //día nublado
            lcd.setCursor(0, 1);
            lcd.print("Nublado");
          }

        }
        */

        screen = k;
        break;

      case '8':
        if(LCDlight){
          Serial.println("Desactivando luz");
          digitalWrite(lcdLightPin, LOW);
          lcd.noDisplay();
          LCDlight = false;
        }else{
          Serial.println("Activando luz");
          digitalWrite(lcdLightPin, HIGH);
          lcd.display();
          LCDlight = true;
        }
        break;

      default:
        Serial.println("Boton no programado");
        break;

    }
 }

void comp_TH(){
  float actualTemp = bme.temp();
  float seconds = compTH / 1000.0;
  float differenceT = actualTemp - ptemperature;
  Serial.print("Diferencia de temperatura: ");
  Serial.println(differenceT);
  Serial.println(steepT * seconds);
  if(differenceT > 0.02 && differenceT < steepT * seconds){
    tTemperature = 1;
  }else if (differenceT > -steepT * seconds && differenceT < -0.02){
    tTemperature = 2;
  }else if (differenceT >= steepT * seconds){ //subida brusca de la temperatura
    tTemperature = 4;
    if(pTempAlert == 0 || (millis() - pTempAlert > temperatureAlert)){
      sendMessage("Se ha detectado una subida brusca de la temperatura interior");
      pTempAlert = millis(); //se activa el periodo refractario
    }
  }else if (differenceT <= -steepT * seconds){
    tTemperature = 5;
  }else{
    tTemperature = 3;
  }

  float actualHum = bme.hum();
  float differenceH = actualHum - phumidity;
  Serial.print("Diferencia de humedad: ");
  Serial.println(differenceH);
  Serial.println(steepH * seconds);
  if(differenceH > 0.2 && differenceH < steepH * seconds){
        tHumidity = 1;
      }else if (differenceH > -steepH * seconds && differenceH < -0.2){
        tHumidity = 2;
      }else if (differenceH >= steepH * seconds){
        tHumidity = 4;
      }else if (differenceH <= -steepH * seconds){
        tHumidity = 5;
      }else{
        tHumidity = 3;
      }

  // reiniciamos variables
  ptemperature = actualTemp;
  phumidity = actualHum;
}

void comp_P(){
  float actualPres = bme.pres();
  float seconds = compP / 1000.0;
  float difference = actualPres - pPressure;
  Serial.print("Diferencia de presion: ");
  Serial.println(difference);
  Serial.println(steepP * seconds);
  if(difference > 0.03 && difference < steepP * seconds){
    tPres = 1;
    lowPressure = 0;
  }else if (difference > -steepP * seconds && difference < -0.03){
    tPres = 2;
    lowPressure = 0;
  }else if (difference >= steepP * seconds){
    tPres = 4;
    lowPressure = 0;
  }else if (difference <= -steepP * seconds){ //caída brusca de presión
    tPres = 5;
    lowPressure += 1;
    /*if(lowPressure == 2){
      sendMessage("Se ha detectado una caída brusca de la presión atmosférica");
    }*/
  }else{
    tPres = 3;
    lowPressure = 0;
  }

  pPressure = actualPres;
}

void updateIndex(){ //HTML probado en https://jsfiddle.net/
  measurements =
  "<meta charset='UTF-8'>"
  "<h1 align='center'>Estación meteorológica</h1>"
  "<table align='center' width='25%' bgcolor='99ccff' border='1px solid black'><tr><th>Variables</th><th>Valores</th></tr>"
  "<tr><td>Temperatura interior: </td><td align='center'>"
  ;
  measurements += String(temperature,1);
  switch (tTemperature){
    case 1: // ligero ascenso
      measurements += " ºC &uarr;</td></tr>";
      break;

    case 2: // ligera caída
      measurements += " ºC &darr;</td></tr>";
      break;

    case 3: // igual
      measurements += " ºC =</td></tr>";
      break;

    case 4: //subida brusca
      measurements += " ºC &uarr;&uarr;</td></tr>";
      break;

    case 5: //descenso brusco
      measurements += " ºC &darr;&darr;</td></tr>";
      break;
    default:
      measurements += " ºC</td></tr>";
      break;
  }
  measurements += "<tr><td>Índice de calor:</td><td align='center'>";
  measurements += String(heatIndex,1);
  measurements +=
  " ºC</td></tr>"
  "<tr><td>Temperatura exterior:</td><td align='center'>"
  ;
  measurements += String(outsideTemp,1);
  measurements +=
  " ºC</td></tr>"
  "<tr><td>Humedad relativa:</td><td align='center'>"
  ;
  measurements += String(humidity,0);
  switch (tHumidity){
    case 1: // ligero ascenso
      measurements += "% &uarr;</td></tr>";
      break;

    case 2: // ligera caída
      measurements += "% &darr;</td></tr>";
      break;

    case 3: // igual
      measurements += "% =</td></tr>";
      break;

    case 4: //subida brusca
      measurements += "% &uarr;&uarr;</td></tr>";
      break;

    case 5: //descenso brusco
      measurements += "% &darr;&darr;</td></tr>";
      break;

    default:
      measurements += "%</td></tr>";
      break;
  }
  measurements += "<tr><td>Humedad absoluta:</td><td align='center'>";
  measurements += String(absHumidity);
  measurements += " g/m<sup>3</sup></td></tr>"
  "<tr><td>Presión atmosférica:</td><td align='center'>"
  ;
  measurements += String(pressure,1);
  measurements +=
  " hPa</td></tr>"
  "<tr><td>Presión atmosférica equivalente:</td><td align='center'>"
  ;
  measurements += String(seaLevelPres,1);
  switch (tPres){
    case 1: // ligero ascenso
      measurements += " hPa &uarr;</td></tr>";
      break;

    case 2: // ligera caída
      measurements += " hPa &darr;</td></tr>";
      break;

    case 3: // igual
      measurements += " hPa =</td></tr>";
      break;

    case 4: //subida brusca
      measurements += " hPa &uarr;&uarr;</td></tr>";
      break;

    case 5: //descenso brusco
      measurements += " hPa &darr;&darr;</td></tr>";
      break;

    default:
      measurements += " hPa</td></tr>";
      break;
  }
  measurements += "<tr><td>Punto de rocío:</td><td align='center'>";
  measurements += String(dewPoint,1);
  measurements +=
  " ºC</td></tr>"
  "<tr><td>Altitud aproximada:</td><td align='center'>"
  ;
  measurements += String(altitude, 0);
  measurements +=
  " m<tr><td>Altitud real:</td><td align='center'>"
  ;
  measurements += String(realAltitude);
  measurements +=
  " m</td></tr>"
  "<tr><td>Concentración de CO<sub>2</sub>:</td><td align='center'>"
  ;
  measurements += String(CO2, 0);
  measurements +=
  " ppm</td></tr>"
  "<tr><td>Contaminación del aire:</td><td align='center'>";
  measurements += String(cont);
  measurements += "</td></tr>"
  "<tr><td>Salida del Sol:</td><td align='center'>"
  ;
  setTime(salidaT);
  measurements += hour();
  measurements += ":";
  measurements += minute();
  measurements += "</td></tr>";
  measurements += "<tr><td>Puesta del Sol:</td><td align='center'>"
  ;
  setTime(puestaT);
  measurements += hour();
  measurements += ":";
  measurements += minute();
  measurements += "</td></tr>";
  measurements += "<tr><td>Iluminación:</td><td align='center'>";
  measurements += String(ilum);
  measurements += "</td></tr>";

  measurements +=
  "<script>"
    "window.setInterval('refresh()', "
  ;
  measurements += String(upIndex);
  measurements += ");"
    "function refresh() {"
      "window .location.reload();"
    "}"
  "</script>"
  ;
}

void APIcall(String url){

  if(WiFi.status()== WL_CONNECTED){
      String serverPath = url;
      
      jsonBuffer = httpGETRequest(serverPath.c_str());
      Serial.println(jsonBuffer);
      JSONVar myObject = JSON.parse(jsonBuffer);
  
      // JSON.typeof(jsonVar) can be used to get the type of the var
      if (JSON.typeof(myObject) == "undefined") {
        Serial.println("Parsing input failed!");
        return;
      }
    
      Serial.print("JSON object = ");
      Serial.println(myObject);
      gTemperature = (double) myObject["main"]["temp"]; // necesita ser una variable de tipo double para hacer la conversión
      outsideTemp = gTemperature;
      sensacionT = (double) myObject["main"]["feels_like"];
      weather = myObject["weather"][0]["description"];
      salida = (long) myObject["sys"]["sunrise"];
      puesta = (long) myObject["sys"]["sunset"];
      offset = (int) myObject["timezone"];
      salidaT = salida + offset;
      puestaT = puesta + offset;
    }
    else {
      Serial.println("WiFi Disconnected");
    }

}

String httpGETRequest(const char* serverName) {
  WiFiClient cliente;
  HTTPClient http;
    
  // Your Domain name with URL path or IP address with path
  http.begin(cliente, serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

void printLocalTime(){ //https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  char timeMin[3];
  strftime(timeMin,3, "%M", &timeinfo);

  int actualMin;
  actualMin = atoi(timeMin);

  if(actualMin != checkMin){
    Serial.println("Se actualiza el reloj");
    lcd.clear();
    lcd.print(&timeinfo, "%R %a %d/%m");
    lcd.setCursor(0, 1);
    setTime(salidaT);
    lcd.write(3);
    lcd.print(" ");
    lcd.print(hour());
    lcd.print(":");
    lcd.print(minute());
    lcd.print(" ");
    lcd.write(4);
    lcd.print(" ");
    setTime(puestaT);
    lcd.print(hour());
    lcd.print(":");
    lcd.print(minute());

    checkMin = actualMin;
  }

}

int getIlu(){ // Fuente: https://blog.330ohms.com/2020/05/16/como-conectar-una-fotoresistencia-ldr-a-arduino/
    int ADC = analogRead(LDRPin);
    //Serial.print("Iluminacion ADC: ");
    //Serial.println(ADC);
    /*
    //Serial.print("Voltaje: ");
    //Serial.println(V);
    int ilu = 0;
    if(V >= 4095){
      ilu = 4095;
    }else{
      //ilum = ((long)(1024-V)*A*10)/((long)B*Rc*V);  //usar si LDR entre GND y A0 
       ilu = ((long)V*A*10)/((long)B*Rc*(4095-V));    //usar si LDR entre A0 y Vcc (como en el esquema anterior) Fuente: https://www.luisllamas.es/medir-nivel-luz-con-arduino-y-fotoresistencia-ldr/
    }

   Serial.print("Iluminacion: ");
   Serial.println(ilu);
   return ilu;
   */
   return ADC;
}

void readSensors(){
  contar++; //número de lecturas para luego hacer la media
  ilumC += getIlu();
  contC += getCont();
  CO2C += readCO2(temperature, humidity);
  temperatureC += bme.temp();
  humidityC += bme.hum();
  pressureC += bme.pres();
}

void calculations(){
  EnvironmentCalculations::AltitudeUnit envAltUnit  =  EnvironmentCalculations::AltitudeUnit_Meters;
  EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
  altitude = EnvironmentCalculations::Altitude(pressure, envAltUnit, SEALEVELPRESSURE_HPA, outsideTemp, envTempUnit);
  seaLevelPres = EnvironmentCalculations::EquivalentSeaLevelPressure(realAltitude,temperature,pressure);
  dewPoint = EnvironmentCalculations::DewPoint(temperature,humidity);
  absHumidity = EnvironmentCalculations::AbsoluteHumidity(temperature, humidity, envTempUnit);
  heatIndex = EnvironmentCalculations::HeatIndex(temperature, humidity);
}

void getData(){
  if(contar == 0){
    Serial.println("ERROR: contar es igual a 0");
  }else{
    ilum = ilumC / contar;
    cont = contC / contar;
    CO2 = CO2C / float(contar);
    temperature = temperatureC / float(contar);
    humidity = humidityC / float(contar);
    pressure = pressureC / float(contar);
    calculations();

    Serial.print(F("Temperatura = "));
    Serial.print(temperature);
    Serial.println(" *C");
    Serial.print(F("Humedad relativa = "));
    Serial.print(humidity);
    Serial.println(" %");
    Serial.print(F("Presion = "));
    Serial.print(pressure);
    Serial.println(" hPa");
    Serial.print(F("Presion equivalente = "));
    Serial.print(seaLevelPres);
    Serial.println(" hPa");
    Serial.print("Altitud = ");
    Serial.print(realAltitude);
    Serial.println(" m");
    Serial.print("Altitud aproximada = ");
    Serial.print(altitude);
    Serial.println(" m");
    Serial.print("Punto de rocio = ");
    Serial.print(dewPoint);
    Serial.println(" *C");
    Serial.print("Humedad absoluta: ");
    Serial.print(absHumidity);
    Serial.println(" g/m3");
    Serial.print("Indice de calor: ");
    Serial.print(heatIndex);
    Serial.println(" *C");
    Serial.print("CO2: ");
    Serial.print(CO2);
    Serial.println(" ppm");
    Serial.print("Contaminacion: ");
    Serial.println(cont);
    Serial.print("Iluminacion: ");
    Serial.println(ilum);
    Serial.println();
    Serial.println("-----------------------------------------");
    Serial.println();
  }
}

void resetCounter(){
  contar = 0;
  ilumC = 0;
  contC = 0;
  CO2C = 0;
  temperatureC = 0;
  humidityC = 0;
  pressureC = 0;
}
