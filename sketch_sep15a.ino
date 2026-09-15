/*
  Riego automático por umbral — Arduino Nano
  -------------------------------------------
  Fase 2: la bomba ya no depende de comandos Serial, decide sola según
  la lectura del sensor. Usa DOS umbrales (histéresis) en vez de uno solo,
  para que no esté encendiendo y apagando sin parar cuando el valor está
  justo en el límite.

  IMPORTANTE: los valores de UMBRAL_SECO / UMBRAL_HUMEDO de abajo son
  arbitrarios, puestos solo para que el demo funcione con el potenciómetro
  de Wokwi (rango 0-1023 completo). Con tu sensor real y sustrato real,
  cambia estos dos números por los que hayas anotado en tu calibración
  (el valor "seco" y el valor "recién regado" que ya sabes cómo sacar).

  Sigue habiendo comandos manuales por Serial:
    ON   -> fuerza la bomba encendida (la lógica automática puede
            volver a apagarla si detecta húmedo)
    OFF  -> apaga la bomba manualmente ahora mismo (pero si sigue seco,
            el riego automático puede volver a encenderla en el siguiente
            ciclo — esto es intencional, es el modo automático mandando)
*/

const int PIN_RELE = 7;
const int PIN_SENSOR = A0;

const int RELE_ON = HIGH;   // confirmado en tu módulo real / Wokwi
const int RELE_OFF = LOW;

// --- Umbrales de humedad (con histéresis) ---
// lectura ALTA = seco (menos conductividad) | lectura BAJA = húmedo
const int UMBRAL_SECO = 650;    // por encima de esto -> enciende la bomba
const int UMBRAL_HUMEDO = 400;  // por debajo de esto -> apaga la bomba
// Entre 400 y 650 no hace nada — se queda como estaba (esa "zona muerta"
// es la histéresis, evita parpadeos cuando el valor oscila cerca del límite)

const unsigned long MAX_RIEGO_MS = 15000UL;      // corte de seguridad: 15 s
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
  Serial.begin(9600);
  pinMode(PIN_RELE, OUTPUT);
  apagarBomba("inicio");
  Serial.println("Listo. Riego automático activo. Comandos manuales: ON / OFF");
}

void loop() {
  // --- Comandos manuales (override puntual) ---
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    if (cmd == "ON" && !bombaEncendida) encenderBomba("manual");
    else if (cmd == "OFF" && bombaEncendida) apagarBomba("manual");
  }

  // --- Corte de seguridad por tiempo máximo ---
  if (bombaEncendida && (millis() - tInicioRiego >= MAX_RIEGO_MS)) {
    apagarBomba("limite de tiempo");
  }

  // --- Lectura del sensor + lógica automática ---
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