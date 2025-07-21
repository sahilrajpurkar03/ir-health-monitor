#include <WiFi.h>
#include <HTTPClient.h>

// SparkFun MAX30102 library
#include <SparkFun_MAX3010x_Sensor_Library.h>
MAX30105 particleSensor;

// Pololu MLX90640 library
#include <Wire.h>
#include <mlx90640_API.h>
#include <mlx90640_I2C_Driver.h>

#define MLX90640_ADDR 0x33

// WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// Server endpoint
const char* serverURL = "http://yourserver.com/api/data";

float mlx90640Frame[768];  // MLX90640 frame buffer (32x24)

uint16_t frameCount = 0;

paramsMLX90640 mlx90640;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected!");

  // Initialize MAX30102 sensor
  if (particleSensor.begin() == false) {
    Serial.println("MAX30102 not found. Check wiring.");
    while(1);
  }
  particleSensor.setup(); // Use default configuration for SpO2 and HR
  Serial.println("MAX30102 initialized.");

  // Initialize MLX90640
  Wire.begin();
  if (mlx90640_DumpEE(MLX90640_ADDR, mlx90640.ee) != 0) {
    Serial.println("Failed to dump EEPROM");
    while(1);
  }
  if (mlx90640_ExtractParameters(&mlx90640) != 0) {
    Serial.println("Parameter extraction failed");
    while(1);
  }
  Serial.println("MLX90640 initialized.");
  mlx90640_SetRefreshRate(MLX90640_ADDR, 0x03); // 4Hz refresh rate
}

void loop() {
  // Read SpO2 and breath rate from MAX30102
  long irValue = particleSensor.getIR();
  long redValue = particleSensor.getRed();

  // For simplicity, just print raw values (implement real SpO2 algorithm or use library features)
  Serial.print("IR: "); Serial.print(irValue);
  Serial.print(" Red: "); Serial.println(redValue);

  // Read MLX90640 frame (skin temperature)
  if (mlx90640_GetFrameData(MLX90640_ADDR, mlx90640Frame) != 0) {
    Serial.println("Failed to get frame data");
    return;
  }

  float vmin = 1000;
  float vmax = 0;
  float skinTempSum = 0;
  int count = 0;

  // Calculate average skin temperature from the 32x24 pixels
  for (int i = 0; i < 768; i++) {
    float temp = mlx90640Frame[i];
    skinTempSum += temp;
    if (temp < vmin) vmin = temp;
    if (temp > vmax) vmax = temp;
    count++;
  }
  float avgSkinTemp = skinTempSum / count;

  Serial.print("Average Skin Temp: ");
  Serial.println(avgSkinTemp);

  // Prepare data JSON string
  String postData = "{";
  postData += "\"irValue\":" + String(irValue) + ",";
  postData += "\"redValue\":" + String(redValue) + ",";
  postData += "\"skinTemp\":" + String(avgSkinTemp, 2);
  postData += "}";

  Serial.println("Sending data to server: " + postData);

  // Send data to server via HTTP POST
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(postData);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Server response: " + response);
    } else {
      Serial.println("Error sending POST: " + String(httpResponseCode));
    }
    http.end();
  } else {
    Serial.println("WiFi disconnected");
  }

  delay(5000); // Wait 5 seconds before next reading
}
