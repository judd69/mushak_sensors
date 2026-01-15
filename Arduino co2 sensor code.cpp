#include <Wire.h>
#include "Adafruit_SCD30.h"

Adafruit_SCD30 scd30;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10); 
  }

  Serial.println("SCD30 Test Initializing...");

  Wire.begin();

 
  if (!scd30.begin()) {
    Serial.println("Failed to find SCD30 sensor. Check wiring!");
    while (1) {
      delay(10);
    }
  }
  Serial.println("SCD30 sensor found!");
}


void checkAirQuality(float co2_reading) {
  Serial.print("status: ");
  
  if (co2_reading <= 350) {
    Serial.println("healthy outside air level (excellent)");
  } 
  else if (co2_reading <= 600) {
    Serial.println("healthy indoor climate (good)");
  } 
  else if (co2_reading <= 800) {
    Serial.println("acceptable level (fair)");
  } 
  else if (co2_reading <= 1000) {
    Serial.println("ventilation required (poor)");
  } 
  else if (co2_reading <= 1200) {
    Serial.println("ventilation necessary (bad)");
  } 
  else if (co2_reading <= 2500) {
    Serial.println("negative health effects (very bad)");
  } 
  else {
    // the range is here
    Serial.println("hazardous (danger)");
  }
}

void loop() {
  //new data available?
  if (scd30.dataReady()) {
    Serial.println("---new data---");
    
    // reading sensordata
    if (!scd30.read()) {
      Serial.println("error reading sensor data");
      return;
    }

    Serial.print("CO2 (ppm): ");
    Serial.println(scd30.CO2);

    checkAirQuality(scd30.CO2);

    Serial.print("Temperature (C): ");
    Serial.println(scd30.temperature);

    Serial.print("Humidity (%): ");
    Serial.println(scd30.relative_humidity);
    Serial.println();

  } else {
    // Serial.println("No new data yet..."); 
    // commented out to reduce spam in monitor
  }

  delay(2000); // ig 2s delay is perfect 
}