const int PH_SENSOR_PIN = A0;

float VOLTAGE_AT_PH7 = 2.238;   
float VOLTAGE_AT_PH2_5 = 1.406; 

float getVoltage() {
  long sum = 0;
  for (int i = 0; i < 40; i++) { 
    sum += analogRead(PH_SENSOR_PIN);
    delay(10);
  }
  return (sum / 40.0) * (5.0 / 1024.0); 
}

void setup() {
  Serial.begin(9600);
  Serial.println("System: Custom pH Mapping Active");
  Serial.println("--------------------------------");
}

void loop() {
  float voltage = getVoltage();
  
  float slope = (7.0 - 2.5) / (VOLTAGE_AT_PH7 - VOLTAGE_AT_PH2_5);
  
  float pH = 7.0 - (slope * (VOLTAGE_AT_PH7 - voltage));

  Serial.print("Raw Voltage: ");
  Serial.print(voltage, 3);
  Serial.print(" V | Calculated pH: ");
  Serial.println(pH, 2);

  delay(1500);
}