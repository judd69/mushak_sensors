/**
 * SCD30 CO2 Sensor Reader
 * 
 * This Arduino sketch reads CO2, temperature, and humidity data from the
 * Adafruit SCD30 sensor and provides air quality assessment.
 * 
 * Hardware: Adafruit SCD30 sensor connected via I2C
 * Libraries: Adafruit_SCD30, Wire
 * 
 * Author: Team Mushak
 * Date: 2026
 */

#include <Wire.h>
#include "Adafruit_SCD30.h"

Adafruit_SCD30 scd30;

void setup() {
  // Initialize serial communication at 115200 baud
  Serial.begin(115200);
  while (!Serial) {
    delay(10); 
  }

  Serial.println("SCD30 Test Initializing...");

  // Initialize I2C communication
  Wire.begin();

  // Initialize SCD30 sensor
  if (!scd30.begin()) {
    Serial.println("Failed to find SCD30 sensor. Check wiring!");
    while (1) {
      delay(10);
    }
  }
  Serial.println("SCD30 sensor found!");
}

/**
 * Check air quality based on CO2 reading
 * 
 * CO2 levels and their health implications:
 * - ≤350 ppm: Healthy outside air level (excellent)
 * - 351-600 ppm: Healthy indoor climate (good)
 * - 601-800 ppm: Acceptable level (fair)
 * - 801-1000 ppm: Ventilation required (poor)
 * - 1001-1200 ppm: Ventilation necessary (bad)
 * - 1201-2500 ppm: Negative health effects (very bad)
 * - >2500 ppm: Hazardous (danger)
 * 
 * @param co2_reading CO2 concentration in ppm
 */
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
    Serial.println("hazardous (danger)");
  }
}

void loop() {
  // Check if new data is available from sensor
  if (scd30.dataReady()) {
    Serial.println("---new data---");
    
    // Read sensor data
    if (!scd30.read()) {
      Serial.println("error reading sensor data");
      return;
    }

    // Display CO2 reading
    Serial.print("CO2 (ppm): ");
    Serial.println(scd30.CO2);

    // Check and display air quality status
    checkAirQuality(scd30.CO2);

    // Display temperature reading
    Serial.print("Temperature (C): ");
    Serial.println(scd30.temperature);

    // Display humidity reading
    Serial.print("Humidity (%): ");
    Serial.println(scd30.relative_humidity);
    Serial.println();

  } else {
    // No new data available yet
    // Serial output commented out to reduce spam in monitor
  }

  // Wait 2 seconds before next reading
  delay(2000);
}