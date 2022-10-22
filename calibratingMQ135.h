//Source: https://github.com/miguel5612/MQSensorsLib/blob/master/examples/Calibration/Calibration.ino

/*
  MQUnifiedsensor Library - calibrating an MQ2

  Demonstrates the use a MQ2 sensor.
  Library originally added 01 may 2019
  by Miguel A Califa, Yersson Carrillo, Ghiordy Contreras, Mario Rodriguez
 
  Added example
  modified 27 May 2019
  by Miguel Califa 

 This example code is in the public domain.

*/

//Include the library
#include <MQUnifiedsensor.h>
/************************Hardware Related Macros************************************/
#define         Board                   ("ESP-32")
#define         Pin                     (32)  //Analog input 2 of your arduino
/***********************Software Related Macros************************************/
#define         Type                    ("MQ-135") //MQ2
#define         Voltage_Resolution      (3.3)
#define         ADC_Bit_Resolution      (12) // For arduino UNO/MEGA/NANO

/*****************************Globals***********************************************/
double          alcoholPPM          =   (0);
//Defaults, uncomment if you need
//#define RatioMQ2CleanAir 9.83 //RS / R0 = 9.83 ppm 
//#define RatioMQ3CleanAir 60   //RS / R0 = 60 ppm 
//#define RatioMQ4CleanAir 4.4  //RS / R0 = 4.4 ppm 
//#define RatioMQ5CleanAir 6.5  //RS / R0 = 6.5 ppm 
//#define RatioMQ6CleanAir 10   //RS / R0 = 10 ppm 
//#define RatioMQ7CleanAir 27.5 //RS / R0 = 27.5 ppm  
//#define RatioMQ8CleanAir 70   //RS / R0 = 70 ppm   
//#define RatioMQ9CleanAir 9.6  //RS / R0 = 9.6 ppm    
//#define RatioMQ131CleanAir 15 //RS / R0 = 15 ppm
#define RatioMQ135CleanAir 3.6//RS / R0 = 3.6 ppm     
//#define RatioMQ303CleanAir 1  //RS / R0 = 1 ppm    
//#define RatioMQ309CleanAir 11 //RS / R0 = 11 ppm    
unsigned long contador = 0;
float R0 = 0;
/**************************Object_Sensor********************************************/

MQUnifiedsensor MQ135(Board, Voltage_Resolution, ADC_Bit_Resolution, Pin, Type);

void calibrateMQ135() {
  //Init serial port
  MQ135.setRegressionMethod(1); //_PPM =  a*ratio^b
  MQ135.setA(110.47); MQ135.setB(-2.862); // Configure the equation to to calculate LPG concentration
  MQ135.setRL(20);
  /* 
    //If the RL value is different from 10K please assign your RL value with the following method:
    MQ3.setRL(10);
  */
  //init the sensor
  MQ135.init(); 
  //Print in serial monitor
  Serial.println("Preheating...");
  delay(60000);
  Serial.println("Preheating finished");
  Serial.println("MQ135 - Calibracion");
  Serial.println("Note - Make sure you are in a clean room and the sensor has pre-heated almost 4 hours");
  Serial.println("Autonumeric | lecture (R0)");

  for(contador; contador < 300; contador++){
    MQ135.update();
    //Read the sensor and print in serial port
    float lecture =  MQ135.calibrate(RatioMQ135CleanAir);
    //Print in serial monitor
    Serial.print(contador);
    Serial.print(" | ");
    Serial.println(lecture);
    //Wait to measure next sample
    float percentage = contador / 300.0 * 100.0;
    Serial.print(percentage);
    Serial.println("%");
    R0 += lecture;
    delay(1000);
  }
  Serial.println("Calibration finished");
  Serial.print("R0 is equal to ");
  R0 = R0 / contador;
  Serial.println(R0);
  while(true);
}