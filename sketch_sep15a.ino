/*
  Riego automático — versión ESP32 (prueba en Wokwi)
  -----------------------------------------------------
  Misma lógica que riego_automatico.ino del Nano, adaptada a los pines
  del diagrama ESP32: relé en D18, sensor/potenciómetro en D34.

  IMPORTANTE: el ADC del ESP32 es de 12 bits (0-4095) por defecto, distinto
  a los 10 bits (0-1023) del Nano. Para no tener que recalcular tus umbrales,
  forzamos analogReadResolution(10) en setup() — así el rango sigue siendo
  0-1023, igual que en el Nano, y los mismos números de calibración sirven
  en ambos.

  Comandos por Serial (115200 baudios, "Newline" activado):
    ON / OFF -> igual que antes
*/

const int PIN_RELE = 18;
const int PIN_SENSOR = 34;

const int RELE_ON = HIGH;
const int RELE_OFF = LOW;

// --- Umbrales de humedad (con histéresis) — mismos valores que en el Nano ---
const int UMBRAL_SECO = 650;
const int UMBRAL_HUMEDO = 400;

const unsigned long MAX_RIEGO_MS = 15000UL;
const unsigned long INTERVALO_LECTURA_MS = 500UL;

bool bombaEncendida = false;
unsigned long tInicioRiego = 0;
unsigned long tUltimaLectura = 0;

void encenderBomba(const char* motivo) {
  digitalWrite(PIN_RELE, RELE_ON);
  bombaEncendida = true;
  tInicioRiego = millis();
  Serial.print(">> BOMBA ON (");
  Serial.print(motivo);
  Serial.println(")");
}

void apagarBomba(const char* motivo) {
  digitalWrite(PIN_RELE, RELE_OFF);
  bombaEncendida = false;
  Serial.print(">> BOMBA OFF (");
  Serial.print(motivo);
  Serial.println(")");
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(10);  // 0-1023, igual que el Nano
  pinMode(PIN_RELE, OUTPUT);
  apagarBomba("inicio");
  Serial.println("Listo (ESP32). Riego automático activo. Comandos: ON / OFF");
}

void loop() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    if (cmd == "ON" && !bombaEncendida) encenderBomba("manual");
    else if (cmd == "OFF" && bombaEncendida) apagarBomba("manual");
  }

  if (bombaEncendida && (millis() - tInicioRiego >= MAX_RIEGO_MS)) {
    apagarBomba("limite de tiempo");
  }

  if (millis() - tUltimaLectura >= INTERVALO_LECTURA_MS) {
    tUltimaLectura = millis();
    int lectura = analogRead(PIN_SENSOR);

    Serial.print("Humedad (raw): ");
    Serial.print(lectura);
    Serial.print("  | Bomba: ");
    Serial.println(bombaEncendida ? "ON" : "OFF");

    if (!bombaEncendida && lectura >= UMBRAL_SECO) {
      encenderBomba("sensor: seco");
    } else if (bombaEncendida && lectura <= UMBRAL_HUMEDO) {
      apagarBomba("sensor: humedo");
    }
  }
}