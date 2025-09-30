/*******************************************************
 * ESP32 LDR with Lux-Based LED Control
 *******************************************************/

#if CONFIG_IDF_TARGET_ESP32S3
  const int LED_PIN = 5;      // LED GPIO
  const int LDR_PIN = 4;      // ADC pin
#else
  const int LED_PIN = 14;
  const int LDR_PIN = 12;
#endif

// LDR characteristics (typical)
const float GAMMA = 0.5;          // LDR gamma
const float R_REF = 5e3;          // LDR resistance at reference lux (ohms)
const float LUX_REF = 10.0;       // Reference lux

const float VCC = 3.3;            // ESP32 supply voltage
const float R_FIXED = 10e3;       // Fixed resistor in voltage divider (ohms)

void setup() {
  Serial.begin(115200);
  ledcAttach(LED_PIN, 5000, 8);
  Serial.println("ESP32 LDR Lux Test");
}

void loop() {
  // Read ADC value (0–4095)
  int adc = analogRead(LDR_PIN);

  // Convert ADC to voltage
  float voltage = (adc / 4095.0) * VCC;

  // Calculate LDR resistance
  float rLDR = R_FIXED * voltage / (VCC - voltage);

  // Convert resistance to lux
  float lux = LUX_REF * pow(R_REF / rLDR, 1.0 / GAMMA);

  // Print values for monitoring
  Serial.print("ADC: "); Serial.print(adc);
  Serial.print("\tVoltage: "); Serial.print(voltage, 3);
  Serial.print(" V\tR_LDR: "); Serial.print(rLDR, 0);
  Serial.print(" Ω\tLux: "); Serial.println(lux, 1);

  // LED logic based on lux
  if (lux < 25.0) {          // Dark threshold (adjustable)
    ledcWrite(LED_PIN, 200);   // LED ON
  } else {
    ledcWrite(LED_PIN, 0);    // LED OFF
  }

  delay(500);
}