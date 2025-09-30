#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"
#include "AppInsights.h"

const char *service_name = "PROV_1234";
const char *pop = "abcd1234";

#if CONFIG_IDF_TARGET_ESP32C3
static int inbuilt_led_pin = 8;
#elif CONFIG_IDF_TARGET_ESP32S3
static int inbuilt_led_pin = 48;
#endif

static int led_1_pin = 5;
static int ldr_sensor_pin = 6;

/* LED characteristics */
const int freq = 5000;     // PWM Frequency
const int resolution = 8;  //PWM resolution (8 bits: 0-255)

/* LDR characteristics */
const float GAMMA = 0.5;     // LDR gamma
const float R_REF = 5e3;     // LDR resistance at reference lux (ohms)
const float LUX_REF = 500;  // Reference lux
float lux = LUX_REF;

/* Setup values */
const float VCC = 5.0;       //ESP32 supply voltage
const float R_FIXED = 10e3;  // Fixed resistor in voltage divider (ohms)


// GPIO for push button
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6
static int inbuilt_switch = 9;  //Boot Button
#else
static int inbuilt_switch = 0;  //Boot Button
#endif

#define DEFAULT_POWER_STATE true
#define DEFAULT_DUTY_CYCLE 200

/* Volatile Variables*/
volatile bool provision_flag_success = false;
volatile bool switch_state_1 = true;
volatile bool switch_state_2 = true;
volatile int brt_led_1 = DEFAULT_DUTY_CYCLE;
volatile int brt_led_2 = DEFAULT_DUTY_CYCLE;

/*
We will be creating 3 devices: 
1. led_1
2. inbuilt_led_2
3. ldr_sensor
*/

static LightBulb *led_1 = NULL;
static LightBulb *inbuilt_led_2 = NULL;
static Device *ldr_sensor = NULL;

unsigned long lastReportTime = 0;
const unsigned long reportInterval = 5000; // Report every 5 seconds

// WARNING: sysProvEvent is called from a separate FreeRTOS task (thread)!
void sysProvEvent(arduino_event_t *sys_event) {
  switch (sys_event->event_id) {
    case ARDUINO_EVENT_PROV_START:
#if CONFIG_IDF_TARGET_ESP32S2
      Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on SoftAP\n", service_name, pop);
      WiFiProv.printQR(service_name, pop, "softap");
#else
      Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n", service_name, pop);
      WiFiProv.printQR(service_name, pop, "ble");
#endif
      break;
    case ARDUINO_EVENT_PROV_INIT: WiFiProv.disableAutoStop(10000); break;
    case ARDUINO_EVENT_PROV_CRED_SUCCESS:
      provision_flag_success = true;
      WiFiProv.endProvision();
      break;
    default:;
  }
}

void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx) {
  const char *device_name = device->getDeviceName();
  const char *param_name = param->getParamName();
  if (strcmp(device_name, "LED_1") == 0) {
    if (strcmp(param_name, "Power") == 0) {
      Serial.printf("Received value = %s for %s - %s\n", val.val.b ? "true" : "false", device_name, param_name);
      switch_state_1 = val.val.b;
      (switch_state_1 == false) ? ledcWrite(led_1_pin, 0) : ledcWrite(led_1_pin, brt_led_1);
    } else if ((strcmp(param_name, "Brightness") == 0) && switch_state_1) {
      Serial.printf("Received value = %d for %s - %s\n", val.val.i, device_name, param_name);
      brt_led_1 = val.val.i;
      ledcWrite(led_1_pin, brt_led_1);
    }
  } else if (strcmp(device_name, "INBUILT_LED_2") == 0) {
    if (strcmp(param_name, "Power") == 0) {
      Serial.printf("Received value = %s for %s - %s\n", val.val.b ? "true" : "false", device_name, param_name);
      switch_state_2 = val.val.b;
      (switch_state_2 == false) ? rgbLedWrite(inbuilt_led_pin, 0, 0, 0) : rgbLedWrite(inbuilt_led_pin, 60, 60, 60);
    } else if (strcmp(param_name, "Brightness") == 0 && switch_state_2) {
      Serial.printf("Received value = %d for %s - %s\n", val.val.i, device_name, param_name);
      brt_led_2 = val.val.i;
      rgbLedWrite(inbuilt_led_pin, brt_led_2, brt_led_2, brt_led_2);
    }
  }
  param->updateAndReport(val);
}

float lux_handler(int ldr_pin) {
  // Read ADC value (0–4095)
  int adc = analogRead(ldr_pin);
  // Convert ADC to voltage
  float voltage = (adc / 4095.0) * VCC;
  // Calculate LDR resistance
  float rLDR = R_FIXED * voltage / (VCC - voltage);
  // Convert resistance to lux
  float lux = LUX_REF * pow(R_REF / rLDR, 1.0 / GAMMA);
  return lux;
}

void setup() {
  Serial.begin(115200);
  pinMode(inbuilt_switch, INPUT_PULLUP);
  ledcAttach(led_1_pin, freq, resolution);
  ledcWrite(led_1_pin, DEFAULT_DUTY_CYCLE);
  rgbLedWrite(inbuilt_led_pin, DEFAULT_DUTY_CYCLE, DEFAULT_DUTY_CYCLE, DEFAULT_DUTY_CYCLE);

  Node my_node;
  my_node = RMaker.initNode("ESP RainMaker Node");

  /* Initialize led_1, inbuilt_led_2, ldr_sensor devices */
  led_1 = new LightBulb("LED_1", &led_1_pin, DEFAULT_POWER_STATE);  // Name of device, priv data that is used for callback function, default power state
  if (!led_1) {
    return;
  }

  inbuilt_led_2 = new LightBulb("INBUILT_LED_2", &inbuilt_led_pin, DEFAULT_POWER_STATE);
  if (!inbuilt_led_2) {
    return;
  }

  // Initialise ldr_sensor
  ldr_sensor = new Device("BRIGHTNESS_SENSOR", ESP_RMAKER_DEVICE_TEMP_SENSOR, NULL);
  if (!ldr_sensor) {
    return;
  }

  // Add brightness parameter for led_1 & inbuilt_led_2
  Param brightness("Brightness", ESP_RMAKER_PARAM_BRIGHTNESS, value(DEFAULT_DUTY_CYCLE), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);  // Or we can use a standard param addBrightnessParam(200)
  brightness.addBounds(value(0), value(255), value(1));
  brightness.addUIType(ESP_RMAKER_UI_SLIDER);

  // Add Ambient Light Sensor paramter for ldr_sensor

  ldr_sensor->addNameParam();
  ldr_sensor->assignPrimaryParam(ldr_sensor->getParamByName(ESP_RMAKER_DEF_POWER_NAME));
  Param sensor("Sensor", ESP_RMAKER_PARAM_TEMPERATURE, value(lux), PROP_FLAG_READ | PROP_FLAG_TIME_SERIES);

  led_1->addParam(brightness);
  inbuilt_led_2->addParam(brightness);
  ldr_sensor->addParam(sensor);


  // Add both light devices callback function
  inbuilt_led_2->addCb(write_callback);
  led_1->addCb(write_callback);

  // Add devices to the node
  my_node.addDevice(*led_1);
  my_node.addDevice(*inbuilt_led_2);
  my_node.addDevice(*ldr_sensor);

  // This is optional
  RMaker.enableOTA(OTA_USING_TOPICS);
  // If you want to enable scheduling, set time zone for your region using
  // setTimeZone(). The list of available values are provided here
  // https://rainmaker.espressif.com/docs/time-service.html
  // RMaker.setTimeZone("Asia/Shanghai");
  //  Alternatively, enable the Timezone service and let the phone apps set the
  //  appropriate timezone
  RMaker.enableTZService();

  RMaker.enableSchedule();

  RMaker.enableScenes();
  // Enable ESP Insights. Insteads of using the default http transport, this function will
  // reuse the existing MQTT connection of Rainmaker, thereby saving memory space.
  //initAppInsights();

  RMaker.enableSystemService(SYSTEM_SERV_FLAGS_ALL, 2, 2, 2);

  RMaker.start();

  WiFi.onEvent(sysProvEvent);
  WiFiProv.beginProvision(NETWORK_PROV_SCHEME_BLE, NETWORK_PROV_SCHEME_HANDLER_FREE_BTDM,
                          NETWORK_PROV_SECURITY_1, pop, service_name);
}

void loop() {
  if (ARDUINO_EVENT_PROV_CRED_SUCCESS) {
    lux = lux_handler(ldr_sensor_pin);

    if (millis() - lastReportTime >= reportInterval) {
      lastReportTime = millis();
      ldr_sensor->updateAndReportParam("Sensor", lux);
    }

    if (digitalRead(inbuilt_switch) == LOW) {  // Push button pressed
      // Key debounce handling
      delay(100);
      int startTime = millis();
      while (digitalRead(inbuilt_switch) == LOW) {
        delay(50);
      }
      int endTime = millis();

      if ((endTime - startTime) > 7000) {
        // If key pressed for more than 7secs, reset all
        Serial.printf("Reset to factory.\n");
        RMakerFactoryReset(2);
      } else if ((endTime - startTime) > 3000) {
        Serial.printf("Reset Wi-Fi.\n");
        // If key pressed for more than 3secs, but less than 10, reset Wi-Fi
        RMakerWiFiReset(2);
      } else {
        // Toggle switch state;
        switch_state_2 = !switch_state_2;
        Serial.printf("Toggle State to %s.\n", switch_state_2 ? "true" : "false");
        if (inbuilt_led_2) {
          inbuilt_led_2->updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME,
                                              switch_state_2);
        }
        (switch_state_2 == false) ? rgbLedWrite(inbuilt_led_pin, 0, 0, 0)
                                  : rgbLedWrite(inbuilt_led_pin, 60, 60, 60);
      }
    }
  }
}
