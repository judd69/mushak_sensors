# Mushak Sensors

A collection of sensor codes for Team Mushak projects.

## Available Sensors

### SCD30 CO2 Sensor
Arduino code for reading CO2, temperature, and humidity data from the Adafruit SCD30 sensor.

**File:** `Arduino_CO2_Sensor.cpp`

**Features:**
- CO2 measurement (ppm)
- Temperature reading (°C)
- Humidity reading (%)
- Air quality assessment based on CO2 levels

**Hardware Requirements:**
- Arduino board (compatible with Wire library)
- Adafruit SCD30 sensor
- I2C connection

**Library Dependencies:**
- Wire.h
- Adafruit_SCD30.h

**Setup:**
1. Install the Adafruit SCD30 library through Arduino Library Manager
2. Connect the SCD30 sensor to your Arduino via I2C
3. Upload the code to your Arduino board
4. Open Serial Monitor at 115200 baud rate

**Air Quality Levels:**
- ≤350 ppm: Healthy outside air level (excellent)
- 351-600 ppm: Healthy indoor climate (good)
- 601-800 ppm: Acceptable level (fair)
- 801-1000 ppm: Ventilation required (poor)
- 1001-1200 ppm: Ventilation necessary (bad)
- 1201-2500 ppm: Negative health effects (very bad)
- >2500 ppm: Hazardous (danger)

## Contributing

Feel free to add more sensor codes to this repository following the same documentation format.

