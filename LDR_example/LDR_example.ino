const int LDR_PIN = 7;            // ADC pin on ESP32
const float VREF = 3.3;           // ADC reference voltage
const int ADC_RESOLUTION = 4095;  // 12-bit ADC (0-4095)

const float R_FIXED = 10000.0;  // Fixed resistor value in ohms (10 kΩ)

void setup() 
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32 LDR Test");
}

void loop() 
{
    // 1. Read the raw ADC value
    int rawValue = analogRead(LDR_PIN);

    // 2. Convert to voltage
    float vOut = ((float)rawValue / ADC_RESOLUTION) * VREF;


    // 3. Calculate the LDR resistance (depends on divider wiring)
    // Assuming R_FIXED is between 3.3V and the ADC pin, and LDR to GND:
    // Vout = Vcc * (R_LDR / (R_FIXED + R_LDR))
    // Solve for R_LDR:
    float rLDR = (R_FIXED * vOut) / (VREF - vOut);

    // 4. Print the results
    Serial.print("Raw ADC: ");
    Serial.print(rawValue);
    Serial.print("\tVoltage: ");
    Serial.print(vOut, 3);
    Serial.print(" V\tLDR Resistance: ");
    Serial.print(rLDR, 0);
    Serial.println(" ohms");

    // Optional: use rawValue or rLDR to determine brightness thresholds
    // Example: turn on an LED if it's dark:
    // if (rawValue > 3000) digitalWrite(LED_PIN, HIGH); else digitalWrite(LED_PIN, LOW);

    delay(200);  // read twice per second
}
