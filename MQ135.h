// Source: https://github.com/miguel5612/MQSensorsLib

// This is an example of implementation using ESP8266
// Never connect the sensor direct to the ESP8266, sensor high level is 5V
// ADC of ESP8266 high level is 3.3
// To connect use a voltage divisor, where 5V will 3v3 on the middle point like
// this {{URL}}

/*
  MQUnifiedsensor Library - reading an MQSensor using ESP8266 board
  For this example wi will demonstrates the use a MQ3 sensor.
  Library originally added 01 may 2019
  by Miguel A Califa, Yersson Carrillo, Ghiordy Contreras, Mario Rodriguez
 
  Added ESP8266 example 
  29.03.2020
  Wiring:
  https://github.com/miguel5612/MQSensorsLib_Docs/blob/master/static/img/MQ_ESP8266.PNG
 This example code is in the public domain.
*/

//Include the library
#include <MQUnifiedsensor.h>
/************************Hardware Related Macros************************************/
#define         Board                   ("ESP-32") // Wemos ESP-32 or other board, whatever have ESP32 core.
#define         Pin                     (32)  //IO25 for your ESP32 WeMos Board, pinout here: https://i.pinimg.com/originals/66/9a/61/669a618d9435c702f4b67e12c40a11b8.jpg
/***********************Software Related Macros************************************/
#define         Type                    ("MQ-3") //MQ3 or other MQ Sensor, if change this verify your a and b values.
#define         Voltage_Resolution      (3.3) // 3V3 <- IMPORTANT. Source: https://randomnerdtutorials.com/esp32-adc-analog-read-arduino-ide/
#define         ADC_Bit_Resolution      (12) // ESP-32 bit resolution. Source: https://randomnerdtutorials.com/esp32-adc-analog-read-arduino-ide/
#define         RatioMQ3CleanAir        (12) // Ratio of your sensor, for this example an MQ-3
/*****************************Globals***********************************************/
MQUnifiedsensor MQ135(Board, Voltage_Resolution, ADC_Bit_Resolution, Pin, Type);
/*****************************Globals***********************************************/

/// Parameters to model temperature and humidity dependence
#define CORA 0.00035
#define CORB 0.02718
#define CORC 1.39538
#define CORD 0.0018

#define CO2atm (415.95) // ppm de CO2 en atmósfera    fuente: https://www.co2.earth/
#define R0 (465.24)

void startMQ135() {
  //Set math model to calculate the PPM concentration and the value of constants
  MQ135.setRegressionMethod(1); //_PPM =  a*ratio^b
  MQ135.setA(110.47); MQ135.setB(-2.862); // Configure the equation to to calculate CO2 concentration
  /*
    Exponential regression:
  Gas    | a      | b
  LPG    | 44771  | -3.245
  CH4    | 2*10^31| 19.01
  CO     | 521853 | -3.821
  Alcohol| 0.3934 | -1.504
  Benzene| 4.8387 | -2.68
  Hexane | 7585.3 | -2.849
  */

  /*****************************  MQ Init ********************************************/ 
  //Remarks: Configure the pin of arduino as input.
  /************************************************************************************/ 
  MQ135.init(); 
  MQ135.setR0(R0);
  //If the RL value is different from 10K please assign your RL value with the following method:
  MQ135.setRL(20);
  MQ135.serialDebug(true);
}

float getCorrectionFactor(float t, float h) {
  return CORA * t * t - CORB * t + CORC - (h-33.)*CORD;
}

float readCO2(float temp, float hum) {
  float cFactor = 0;
  cFactor = getCorrectionFactor(temp, hum);
  MQ135.update(); // Update data, the arduino will read the voltage from the analog pin
  float result = MQ135.readSensor(false, cFactor); // Sensor will read PPM concentration using the model, a and b values set previously or from the setup
  result += CO2atm;
  //Serial.print("CO2: ");
  //Serial.print(result);
  //Serial.println(" ppm");
  if(result >= 600.0){
    sendMessage("Se han detectado altas concentraciones de CO2");
  }
  return result;
}

int getCont(){
  int V = analogRead(Pin);
  //Serial.print("Contaminacion: ");
  //Serial.println(V);
  return V;
}
