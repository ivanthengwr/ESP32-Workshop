/*******************************************************
 * ESP32 LDR with Lux-Based LED Brightness Control
 *******************************************************/

const int LED_PIN = 6;      // LED GPIO
const int LDR_PIN = 7;      // ADC pin

// LDR characteristics (typical)
const float GAMMA = 0.5;          // LDR gamma
const float R_REF = 5e3;          // LDR resistance at reference lux (ohms)
const float LUX_REF = 10;       // Reference lux

const float VCC = 5;            // ESP32 supply voltage
const float R_FIXED = 10e3;       // Fixed resistor in voltage divider (ohms)

// LED PWM characteristics
const int PWM_FREQ = 5000;        // 5 kHz
const int PWM_RES = 8;            // 8-bit (0–255 duty)

// Lux range for mapping LED brightness
const float LUX_DARK = 10.0;      // lux at which LED is brightest
const float LUX_BRIGHT = 50;   // lux at which LED is off

void setup() {
    Serial.begin(115200);
    ledcAttach(LED_PIN, PWM_FREQ, PWM_RES);
    Serial.println("ESP32 LDR Lux Test - Brightness Control");
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

    // --- LED Brightness Calculation ---
    // Map lux to LED duty (inverse relation)
    int duty;
    if (lux <= LUX_DARK) {
      duty = 255;  // full brightness in the dark
    } else if (lux >= LUX_BRIGHT) {
      duty = 0;    // LED off in bright light
    } else {
      // Linear fade between dark and bright thresholds
      duty = map(lux, LUX_DARK, LUX_BRIGHT, 255, 0);
    }

    // Apply to LED
    ledcWrite(LED_PIN, duty);

    // --- Debug Output ---
    Serial.print("ADC: "); Serial.print(adc);
    Serial.print("\tVoltage: "); Serial.print(voltage, 3);
    Serial.print(" V\tR_LDR: "); Serial.print(rLDR, 0);
    Serial.print(" Ω\tLux: "); Serial.print(lux, 1);
    Serial.print("\tLED Duty: "); Serial.println(duty);

    delay(500);
}

