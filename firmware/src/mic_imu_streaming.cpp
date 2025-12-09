#include "credentials.h"

#include <Arduino.h>
#include "driver/i2s.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebSocketsClient.h>

// Wifi credentials
const char *WIFI_SSID = SSID;
const char *WIFI_PASSWORD = PASSWORD; // delete when back on campus

// Server info (your PC running echo_server.py)
WebSocketsClient ws;
const char *WS_HOST = IP_ADDRESS;
const uint16_t WS_PORT = 8765;
const char *WS_PATH = "/ws";
unsigned long tstart;
int numData;

// Function declarations
void testWifiConnection();
void onWsEvent(WStype_t type, uint8_t *payload, size_t length);
void initWebsockets();
void initHardware();
void setupI2S();

// Initializes/tests wifi connection
void testWifiConnection()
{
  // Create a timetracker in milliseconds
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000)
  {
    Serial.print('.');
    delay(250);
  }

  // Print final status
  Serial.println();
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("Timeout.");
  }
}

// Handles WebSocket events
volatile bool wsConnected = false;
void onWsEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_CONNECTED:
    wsConnected = true;
    Serial.println("WS connected, sending handshake...");
    ws.sendTXT("requesting handshake...");
    break;

  case WStype_DISCONNECTED:
    wsConnected = false;
    Serial.print("[MCU] ");
    Serial.println("WS disconnected");
    break;

  case WStype_TEXT:
    numData++;
    if (numData / 100 >= 1)
    {
      Serial.printf("[SERVER] %i (mic, imu) values received, ending with ", numData);
      Serial.write(payload, length);
      Serial.println();
      numData = 0;
    }
    break;

  case WStype_ERROR:
    Serial.println("WS error");
    break;

  default:
    break;
  }
}

// Hardware initializations
Adafruit_MPU6050 mpu;
#define MIC_CLK 26 // SCK / BCLK
#define MIC_WS 25  // WS  / LRCLK
#define MIC_SD 34  // SD  / DOUT

// Sending initializations
const uint16_t BUFFER_SIZE = 16;
const uint16_t BUFFER_COUNT = 8; 
const int BATCH_SIZE = 1000;
const int SEND_SIZE = BATCH_SIZE * 20 + 4000; // rough estimate of required buffer size
static uint16_t batchIndex = 0;
static uint16_t micBatch[BATCH_SIZE];
static float imuBatch[BATCH_SIZE];

// ---- Audio + streaming settings ----
const i2s_port_t I2S_PORT = I2S_NUM_0;
const int SAMPLE_RATE = 16000; // mic sample rate (Hz)
const int DECIMATE = 4;        // print every Nth sample so Serial keeps up
const int BAUD = 115200;       // Serial Plotter baud

void setup()
{
  Serial.begin(BAUD);

  numData = -1;
  initWebsockets();
  initHardware();
  tstart = millis();
}

void loop()
{
  ws.loop();

  static int count = 0;

  int32_t buf[BUFFER_SIZE];
  size_t bytesRead = 0;
  i2s_read(I2S_PORT, buf, sizeof(buf), &bytesRead, portMAX_DELAY);

  int n = bytesRead / sizeof(int32_t);

  // mic RMS energy calculation
  int64_t micSumSq = 0;
  for (int i = 0; i < n; i++)
  {
    int32_t s32 = buf[i] >> 8;
    int16_t nextMicVal = (int16_t)s32;
    micSumSq += (int32_t)nextMicVal * (int32_t)nextMicVal;
  }
  float micRms = sqrtf((float)micSumSq / n);

  // IMU AVG VALUE CALCULATION
  sensors_event_t a, g, temp;
  unsigned long lastImu = 0;
  if (millis() - lastImu > 5) {  // 200 Hz IMU
      mpu.getEvent(&a, &g, &temp);
      lastImu = millis();
  }

  float xSquared = a.acceleration.x * a.acceleration.x;
  float ySquared = a.acceleration.y * a.acceleration.y;
  float zSquared = a.acceleration.z * a.acceleration.z;

  float imuMag = sqrtf(xSquared + ySquared + zSquared);

  // if batch index not full, add to batch and return (move to next iteration)
  if (batchIndex < BATCH_SIZE)
  {
    micBatch[batchIndex] = (uint16_t)micRms;
    imuBatch[batchIndex] = imuMag;
    batchIndex++;
    return;
  }

  // if batch index full, send batch over websocket
  if (batchIndex >= BATCH_SIZE && wsConnected)
  {
    static char msgBuf[SEND_SIZE];
    size_t offset = 0;

    for (int i = 0; i < BATCH_SIZE && offset < sizeof(msgBuf) - 1; i++)
    {
      int written = snprintf(
          msgBuf + offset,
          sizeof(msgBuf) - offset,
          "%u,%.3f\n",
          (unsigned)micBatch[i],
          imuBatch[i]);
      if (written < 0 || (size_t)written >= sizeof(msgBuf) - offset)
      {
        // Encoding error or buffer overflow
        break;
      }
      offset += (size_t)written;
    }

    // send entire batch
    ws.sendTXT(msgBuf, offset);
    unsigned long tnow = millis();
    float tdiff = (tnow - tstart) / BATCH_SIZE;
    Serial.printf("Sent batch of %d samples in %.3f seconds (%.3f samples/second)\n", BATCH_SIZE, tdiff, (float)BATCH_SIZE / tdiff);
    tstart = millis();

    // reset batch
    batchIndex = 0;
  }

}

// HELPER FUNCTIONS
void setupI2S()
{
  i2s_config_t cfg = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // I2S mics give 24 bits in 32-bit frames
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,  // L/R pin tied to GND
      .communication_format = I2S_COMM_FORMAT_STAND_MSB,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = BUFFER_COUNT,
      .dma_buf_len = BUFFER_SIZE,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0};

  i2s_pin_config_t pins = {
      .bck_io_num = MIC_CLK,
      .ws_io_num = MIC_WS,
      .data_out_num = -1,
      .data_in_num = MIC_SD};

  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_set_pin(I2S_PORT, &pins);
  i2s_zero_dma_buffer(I2S_PORT);
}

void initHardware()
{
  setupI2S();
  Wire.begin();

  // Initialize MPU6050
  if (!mpu.begin())
  {
    Serial.println("Failed to find MPU6050 chip!");
    return;
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU6050 initialized!");
}

void initWebsockets()
{
  // Configure WiFi
  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID);

  testWifiConnection();

  // Initialize WebSocket
  ws.begin(WS_HOST, WS_PORT, WS_PATH);
  ws.onEvent(onWsEvent);
  ws.setReconnectInterval(5000);
}