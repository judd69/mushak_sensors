#include <IBusBM.h>
#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

IBusBM ibus;
static const int IBUS_RX_PIN = 16;

static const char* WIFI_SSID = "YOUR_WIFI_SSID";
static const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
static const char* AP_SSID = "Mushak-ESP32";
static const char* AP_PASSWORD = "mushak123";
WebServer server(80);
bool wifiConnected = false;

SensirionI2cScd4x scd4x;
uint16_t co2 = 0;
float temperature = 0;
float humidity = 0;

static const int LF_PWM = 32;
static const int LF_DIR = 33;
static const int LR_PWM = 25;
static const int LR_DIR = 26;

static const int RF_PWM = 27;
static const int RF_DIR = 14;
static const int RR_PWM = 12;
static const int RR_DIR = 13;

static const int SERVO_PIN = 4;
static const int PUMP_PWM_PIN = 18;
static const int PUMP_DIR_PIN = 19;
static const int PH_SENSOR_PIN = 34;
static const int SOIL_SENSOR_PIN = 35;
static const int TOOL_BUTTON_PIN = 23;

static const float PH_STEP = 0.18f;
static const float PH_NEUTRAL_V = 2.5f;
static const int UPRIGHT_POS = 160;
static const int DOWN_POS = 20;
static const int PH_WATER_THRESHOLD = 1460;
static const unsigned long WATER_TIMEOUT_MS = 20000;

static const int SOIL_WET_RAW = 1400;
static const int SOIL_DRY_RAW = 3000;
static const float SOIL_DRY_PERCENT_THRESHOLD = 35.0f;

static const int PWM_FREQ = 20000;
static const int PWM_RES = 8;
static const int CH_LF = 0;
static const int CH_LR = 1;
static const int CH_RF = 2;
static const int CH_RR = 3;
static const int CH_PUMP = 4;

float maxSpeed = 0.5f;

enum CO2State { IDLE, MEASURING };
CO2State co2State = IDLE;

unsigned long co2StartTime = 0;
bool lastCO2Switch = false;
bool lastWaterState = false;
bool lastSoilState = false;

bool waterRequestPending = false;
bool soilRequestPending = false;
bool toolBusy = false;

Servo armServo;

bool buttonReading = HIGH;
bool buttonStable = HIGH;
unsigned long lastDebounceMs = 0;
unsigned long buttonPressStartMs = 0;
static const unsigned long BUTTON_DEBOUNCE_MS = 35;
static const unsigned long BUTTON_LONG_PRESS_MS = 1200;

float lastPH = 0.0f;
int lastPHRaw = 0;
int lastSoilRaw = 0;
float lastSoilPercent = 0.0f;
unsigned long waterRuns = 0;
unsigned long soilRuns = 0;

float normalize(int val) {
	float x = (val - 1500) / 500.0f;
	if (fabs(x) < 0.05f) {
		x = 0;
	}
	return constrain(x, -1.0f, 1.0f);
}

void setMotor(int pwmChannel, int dirPin, float value) {
	value = constrain(value, -1.0f, 1.0f);

	if (value >= 0) {
		digitalWrite(dirPin, HIGH);
	} else {
		digitalWrite(dirPin, LOW);
		value = -value;
	}

	ledcWrite(pwmChannel, (int)(value * 255));
}

float getFilteredPH() {
	const int numSamples = 20;
	long sum = 0;

	for (int i = 0; i < numSamples; i++) {
		sum += analogRead(PH_SENSOR_PIN);
		delay(10);
	}

	float averageRaw = (float)sum / (float)numSamples;
	float voltage = averageRaw * (3.3f / 4095.0f);
	float phValue = 7.0f + ((PH_NEUTRAL_V - voltage) / PH_STEP);

	lastPHRaw = (int)averageRaw;
	lastPH = phValue;

	Serial.print("[pH avgRaw: ");
	Serial.print(averageRaw);
	Serial.print(" | pH: ");
	Serial.print(phValue);
	Serial.print("] ");

	return phValue;
}

int getFilteredSoilRaw() {
	const int numSamples = 20;
	long sum = 0;

	for (int i = 0; i < numSamples; i++) {
		sum += analogRead(SOIL_SENSOR_PIN);
		delay(5);
	}

	return (int)(sum / numSamples);
}

float soilRawToPercent(int raw) {
	float percent = (float)(SOIL_DRY_RAW - raw) * 100.0f / (float)(SOIL_DRY_RAW - SOIL_WET_RAW);
	return constrain(percent, 0.0f, 100.0f);
}

void runWaterSamplingSequence() {
	toolBusy = true;
	waterRuns++;

	Serial.println("\n--- WATER SEQUENCE START ---");
	armServo.write(DOWN_POS);
	delay(2000);

	digitalWrite(PUMP_DIR_PIN, HIGH);
	ledcWrite(CH_PUMP, 255);

	bool waterDetected = false;
	int stableCount = 0;
	unsigned long startMs = millis();

	while (!waterDetected && (millis() - startMs) < WATER_TIMEOUT_MS) {
		getFilteredPH();
		int rawValue = analogRead(PH_SENSOR_PIN);
		lastPHRaw = rawValue;

		if (rawValue >= PH_WATER_THRESHOLD) {
			stableCount++;
			Serial.print("!");
		} else {
			stableCount = 0;
		}

		if (stableCount >= 10) {
			waterDetected = true;
		}

		server.handleClient();
		Serial.println();
	}

	if (waterDetected) {
		Serial.println("WATER DETECTED. Finalizing fill...");
		delay(1000);
	} else {
		Serial.println("WATER TIMEOUT. Stopping pump safely.");
	}

	ledcWrite(CH_PUMP, 0);
	armServo.write(UPRIGHT_POS);
	Serial.println("--- WATER SEQUENCE COMPLETE ---");
	toolBusy = false;
}

void runSoilSamplingTask() {
	toolBusy = true;
	soilRuns++;

	Serial.println("\n--- SOIL TASK START ---");
	lastSoilRaw = getFilteredSoilRaw();
	lastSoilPercent = soilRawToPercent(lastSoilRaw);

	Serial.print("Soil raw: ");
	Serial.print(lastSoilRaw);
	Serial.print(" | Moisture: ");
	Serial.print(lastSoilPercent, 1);
	Serial.println("%");

	if (lastSoilPercent < SOIL_DRY_PERCENT_THRESHOLD) {
		Serial.println("Soil status: DRY (watering recommended)");
	} else {
		Serial.println("Soil status: OK");
	}

	Serial.println("--- SOIL TASK COMPLETE ---");
	toolBusy = false;
}

void handleToolButton() {
	bool raw = digitalRead(TOOL_BUTTON_PIN);
	if (raw != buttonReading) {
		buttonReading = raw;
		lastDebounceMs = millis();
	}

	if ((millis() - lastDebounceMs) > BUTTON_DEBOUNCE_MS && buttonStable != buttonReading) {
		buttonStable = buttonReading;

		if (buttonStable == LOW) {
			buttonPressStartMs = millis();
		} else {
			unsigned long pressDuration = millis() - buttonPressStartMs;
			if (pressDuration >= BUTTON_LONG_PRESS_MS) {
				soilRequestPending = true;
				Serial.println("Button long press: SOIL task queued");
			} else {
				waterRequestPending = true;
				Serial.println("Button short press: WATER task queued");
			}
		}
	}
}

String buildStatusJson() {
	String state = (co2State == MEASURING) ? "measuring" : "idle";
	String wifiMode = wifiConnected ? "station" : "ap";
	String json = "{";
	json += "\"uptime_ms\":" + String(millis()) + ",";
	json += "\"tool_busy\":" + String(toolBusy ? "true" : "false") + ",";
	json += "\"co2_state\":\"" + state + "\",";
	json += "\"co2_ppm\":" + String(co2) + ",";
	json += "\"temperature_c\":" + String(temperature, 2) + ",";
	json += "\"humidity_pct\":" + String(humidity, 2) + ",";
	json += "\"ph\":" + String(lastPH, 2) + ",";
	json += "\"ph_raw\":" + String(lastPHRaw) + ",";
	json += "\"soil_raw\":" + String(lastSoilRaw) + ",";
	json += "\"soil_moisture_pct\":" + String(lastSoilPercent, 1) + ",";
	json += "\"water_runs\":" + String(waterRuns) + ",";
	json += "\"soil_runs\":" + String(soilRuns) + ",";
	json += "\"wifi_mode\":\"" + wifiMode + "\"";
	json += "}";
	return json;
}

void registerWebRoutes() {
	server.on("/", HTTP_GET, []() {
		const char* page =
			"<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
			"<title>Mushak Sensors</title>"
			"<style>"
			"body{font-family:Verdana,sans-serif;background:#f4f6f8;padding:16px;color:#1b2430}"
			"h1{margin:0 0 12px}"
			".card{background:#fff;border-radius:12px;padding:14px;box-shadow:0 4px 18px rgba(0,0,0,.08);max-width:620px}"
			"button{margin:6px 6px 6px 0;padding:10px 14px;border:0;border-radius:8px;background:#0b7d5d;color:#fff;font-weight:700}"
			"pre{background:#111827;color:#d1fae5;padding:12px;border-radius:8px;overflow:auto}"
			"</style></head><body>"
			"<h1>Mushak ESP32 Dashboard</h1>"
			"<div class='card'><button onclick=\"fetch('/api/water/start').then(load)\">Run Water Task</button>"
			"<button onclick=\"fetch('/api/soil/read').then(load)\">Run Soil Task</button>"
			"<pre id='out'>Loading...</pre></div>"
			"<script>"
			"async function load(){let r=await fetch('/api/status');let j=await r.json();"
			"document.getElementById('out').textContent=JSON.stringify(j,null,2);}"
			"setInterval(load,1000);load();"
			"</script></body></html>";
		server.send(200, "text/html", page);
	});

	server.on("/api/status", HTTP_GET, []() {
		server.send(200, "application/json", buildStatusJson());
	});

	server.on("/api/water/start", HTTP_GET, []() {
		waterRequestPending = true;
		server.send(200, "application/json", "{\"queued\":\"water\"}");
	});

	server.on("/api/soil/read", HTTP_GET, []() {
		soilRequestPending = true;
		server.send(200, "application/json", "{\"queued\":\"soil\"}");
	});

	server.begin();
}

void connectWiFi() {
	WiFi.mode(WIFI_STA);
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

	Serial.print("Connecting to WiFi");
	unsigned long start = millis();
	while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
		delay(400);
		Serial.print(".");
	}

	if (WiFi.status() == WL_CONNECTED) {
		wifiConnected = true;
		Serial.println();
		Serial.print("WiFi connected. IP: ");
		Serial.println(WiFi.localIP());
	} else {
		WiFi.mode(WIFI_AP);
		WiFi.softAP(AP_SSID, AP_PASSWORD);
		wifiConnected = false;
		Serial.println();
		Serial.print("STA failed. AP started. IP: ");
		Serial.println(WiFi.softAPIP());
	}
}

void setup() {
	Serial.begin(115200);
	analogReadResolution(12);
	analogSetPinAttenuation(PH_SENSOR_PIN, ADC_11db);
	analogSetPinAttenuation(SOIL_SENSOR_PIN, ADC_11db);

	Serial2.begin(115200, SERIAL_8N1, IBUS_RX_PIN, -1);
	ibus.begin(Serial2);

	Wire.begin(21, 22);
	scd4x.begin(Wire, 0x62);
	scd4x.stopPeriodicMeasurement();

	ledcSetup(CH_LF, PWM_FREQ, PWM_RES);
	ledcSetup(CH_LR, PWM_FREQ, PWM_RES);
	ledcSetup(CH_RF, PWM_FREQ, PWM_RES);
	ledcSetup(CH_RR, PWM_FREQ, PWM_RES);
	ledcSetup(CH_PUMP, PWM_FREQ, PWM_RES);

	ledcAttachPin(LF_PWM, CH_LF);
	ledcAttachPin(LR_PWM, CH_LR);
	ledcAttachPin(RF_PWM, CH_RF);
	ledcAttachPin(RR_PWM, CH_RR);
	ledcAttachPin(PUMP_PWM_PIN, CH_PUMP);

	pinMode(LF_DIR, OUTPUT);
	pinMode(LR_DIR, OUTPUT);
	pinMode(RF_DIR, OUTPUT);
	pinMode(RR_DIR, OUTPUT);
	pinMode(PUMP_DIR_PIN, OUTPUT);
	pinMode(TOOL_BUTTON_PIN, INPUT_PULLUP);

	armServo.setPeriodHertz(50);
	armServo.attach(SERVO_PIN, 500, 2400);
	armServo.write(UPRIGHT_POS);

	connectWiFi();
	registerWebRoutes();

	Serial.println("System Ready");
}

void loop() {
	server.handleClient();
	handleToolButton();

	int ch1 = ibus.readChannel(0);
	int ch2 = ibus.readChannel(1);
	int ch3 = ibus.readChannel(2);
	int ch4 = ibus.readChannel(3);
	int ch5 = ibus.readChannel(4);
	int ch6 = ibus.readChannel(5);

	float steering = normalize(ch1);
	float throttle = normalize(ch2);

	bool brakeOff = (ch3 > 1750);
	bool brake = !brakeOff;

	if (brake) {
		throttle = 0;
		steering = 0;
	}

	float left = constrain(throttle + steering, -1.0f, 1.0f);
	float right = constrain(throttle - steering, -1.0f, 1.0f);

	bool limiterDisabled = (ch4 > 1750) && brakeOff;
	float scale = limiterDisabled ? 1.0f : maxSpeed;
	left *= scale;
	right *= scale;

	setMotor(CH_LF, LF_DIR, left);
	setMotor(CH_LR, LR_DIR, left);
	setMotor(CH_RF, RF_DIR, right);
	setMotor(CH_RR, RR_DIR, right);

	bool co2Switch = (ch5 > 1800);
	if (co2Switch && !lastCO2Switch && co2State == IDLE) {
		Serial.println("CO2 Measurement Started (30s)");
		scd4x.startPeriodicMeasurement();
		co2StartTime = millis();
		co2State = MEASURING;
	}
	lastCO2Switch = co2Switch;

	bool waterSwitch = (ch5 > 1300 && ch5 < 1700);
	if (waterSwitch && !lastWaterState) {
		waterRequestPending = true;
		Serial.println("Water task queued from RC switch");
	}
	lastWaterState = waterSwitch;

	bool soilSwitch = (ch6 > 1500);
	if (soilSwitch && !lastSoilState) {
		soilRequestPending = true;
		Serial.println("Soil task queued from RC switch");
	}
	lastSoilState = soilSwitch;

	if (co2State == MEASURING) {
		bool ready = false;
		scd4x.getDataReadyStatus(ready);

		if (ready) {
			scd4x.readMeasurement(co2, temperature, humidity);
		}

		if (millis() - co2StartTime >= 30000) {
			scd4x.stopPeriodicMeasurement();

			Serial.println("===== CO2 RESULT =====");
			Serial.print("CO2: ");
			Serial.print(co2);
			Serial.println(" ppm");
			Serial.print("Temp: ");
			Serial.print(temperature);
			Serial.println(" C");
			Serial.print("Humidity: ");
			Serial.print(humidity);
			Serial.println(" %");
			Serial.println("======================");

			co2State = IDLE;
		}
	}

	if (!toolBusy) {
		if (waterRequestPending) {
			waterRequestPending = false;
			runWaterSamplingSequence();
		} else if (soilRequestPending) {
			soilRequestPending = false;
			runSoilSamplingTask();
		}
	}

	delay(20);
}
