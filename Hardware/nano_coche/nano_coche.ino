#include <SPI.h>
#include <mcp_can.h>

// ===========================
// PINES
// ===========================

#define POT_PIN A0
#define CAN_CS_PIN 10

// ===========================
// MCP2515
// ===========================

MCP_CAN CAN(CAN_CS_PIN);

// ===========================
// SETUP
// ===========================

void setup() {
  Serial.begin(115200);

  Serial.println("Iniciando Arduino Nano...");
  Serial.println("Iniciando MCP2515...");

  // IMPORTANTE:
  // MCP_8MHZ si tu modulo MCP2515 lleva cristal de 8 MHz
  // MCP_16MHZ si lleva cristal de 16 MHz

  if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("MCP2515 iniciado correctamente");
  } else {
    Serial.println("ERROR iniciando MCP2515");

    while (1) {
      delay(1000);
    }
  }

  // Modo normal: permite transmitir
  CAN.setMode(MCP_NORMAL);

  Serial.println("CAN a 500 kbps");
  Serial.println("Listo");
}

// ===========================
// LOOP
// ===========================

void loop() {

  // Leer potenciometro
  int valorADC = analogRead(POT_PIN);

  // Convertir 0-1023 a 0-20 L/100km
  float consumo = valorADC * 20.0 / 1023.0;

  //DEBUG
  Serial.print("ADC: ");
  Serial.print(valorADC);

  Serial.print(" | Consumo: ");
  Serial.println(consumo, 2);
  
  // Multiplicamos por 100:
  //
  // 5.43 L/100km -> 543
  //
  uint16_t consumoCAN = (uint16_t)(consumo * 100.0);

  // Separar uint16_t en dos bytes
  byte data[2];

  data[0] = (consumoCAN >> 8) & 0xFF;
  data[1] = consumoCAN & 0xFF;

  // ID CAN = 0x100
  byte resultado = CAN.sendMsgBuf(
    0x100,      // ID
    0,          // 0 = ID estandar
    2,          // longitud
    data
  );

  if (resultado == CAN_OK) {

    Serial.print("ADC: ");
    Serial.print(valorADC);

    Serial.print(" | Consumo: ");
    Serial.print(consumo, 2);

    Serial.print(" L/100km");

    Serial.print(" | CAN: ");

    Serial.print(data[0], HEX);
    Serial.print(" ");
    Serial.println(data[1], HEX);

  } else {

    Serial.println("ERROR enviando CAN");
  }

  delay(500);
}