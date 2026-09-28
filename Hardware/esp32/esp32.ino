#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "driver/twai.h"

// =====================================================
// WIFI
// =====================================================

const char* WIFI_SSID = "S21+ de Daniel";
const char* WIFI_PASSWORD = "ypgp1700";

// IP DEL PC DONDE ESTA MOSQUITTO
const char* MQTT_SERVER = "10.136.224.87";

const int MQTT_PORT = 1883;

const char* MQTT_USER = "drivecost";
const char* MQTT_PASSWORD = "SistemasDistribuidos2026";

const char* MQTT_TOPIC = "drivecost/telemetry";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);


// =====================================================
// GPS
// =====================================================

#define GPS_RX_PIN 21
#define GPS_TX_PIN 20

HardwareSerial GPSSerial(1);

char linea[128];
int pos = 0;

double latitudActual = 0.0;
double longitudActual = 0.0;

bool gpsValido = false;


// =====================================================
// CAN
// =====================================================

#define CAN_TX_PIN 22
#define CAN_RX_PIN 23

float consumoActual = 0.0;

bool consumoValido = false;


// =====================================================
// MQTT
// =====================================================

unsigned long ultimoEnvioMQTT = 0;

const unsigned long INTERVALO_MQTT = 1000;


// =====================================================
// GPS - convertir NMEA a decimal
// =====================================================

double nmeaToDecimal(const char *coord, char hemi) {

  double valor = atof(coord);

  int grados = (int)(valor / 100);

  double minutos =
    valor - grados * 100;

  double decimal =
    grados + minutos / 60.0;

  if (hemi == 'S' || hemi == 'W') {
    decimal = -decimal;
  }

  return decimal;
}


// =====================================================
// GPS - procesar GGA
// =====================================================

void procesarGGA(char *trama) {

  char *campos[15];

  int campo = 0;

  campos[campo++] = trama;


  // Separar campos conservando vacíos
  for (
    int i = 0;
    trama[i] != '\0' && campo < 15;
    i++
  ) {

    if (trama[i] == ',') {

      trama[i] = '\0';

      campos[campo++] =
        &trama[i + 1];
    }
  }


  if (campo < 7) {
    return;
  }


  // Campo 6 = calidad FIX
  if (atoi(campos[6]) == 0) {

    gpsValido = false;

    return;
  }


  if (
    strlen(campos[2]) == 0 ||
    strlen(campos[3]) == 0 ||
    strlen(campos[4]) == 0 ||
    strlen(campos[5]) == 0
  ) {

    gpsValido = false;

    return;
  }


  latitudActual =
    nmeaToDecimal(
      campos[2],
      campos[3][0]
    );


  longitudActual =
    nmeaToDecimal(
      campos[4],
      campos[5][0]
    );


  gpsValido = true;


  Serial.print("GPS: ");

  Serial.print(
    latitudActual,
    6
  );

  Serial.print(",");

  Serial.println(
    longitudActual,
    6
  );
}


// =====================================================
// CAN - iniciar
// =====================================================

bool iniciarCAN() {

  twai_general_config_t g_config =
    TWAI_GENERAL_CONFIG_DEFAULT(
      (gpio_num_t)CAN_TX_PIN,
      (gpio_num_t)CAN_RX_PIN,
      TWAI_MODE_NORMAL
    );


  twai_timing_config_t t_config =
    TWAI_TIMING_CONFIG_500KBITS();


  twai_filter_config_t f_config =
    TWAI_FILTER_CONFIG_ACCEPT_ALL();


  esp_err_t err =
    twai_driver_install(
      &g_config,
      &t_config,
      &f_config
    );


  if (err != ESP_OK) {

    Serial.print(
      "Error instalando CAN: "
    );

    Serial.println(
      esp_err_to_name(err)
    );

    return false;
  }


  err = twai_start();


  if (err != ESP_OK) {

    Serial.print(
      "Error iniciando CAN: "
    );

    Serial.println(
      esp_err_to_name(err)
    );

    return false;
  }


  Serial.println(
    "CAN iniciado a 500 kbps"
  );


  return true;
}


// =====================================================
// CAN - leer consumo
// =====================================================

void leerCAN() {

  twai_message_t mensaje;


  while (
    twai_receive(
      &mensaje,
      0
    ) == ESP_OK
  ) {


    if (
      mensaje.identifier == 0x100 &&
      mensaje.data_length_code >= 2
    ) {


      uint16_t valor =
        ((uint16_t)mensaje.data[0] << 8)
        |
        mensaje.data[1];


      consumoActual =
        valor / 100.0;


      consumoValido = true;


      Serial.print("Consumo: ");

      Serial.print(
        consumoActual,
        2
      );

      Serial.println(
        " L/100km"
      );
    }
  }
}


// =====================================================
// WIFI
// =====================================================

void conectarWiFi() {

  Serial.print(
    "Conectando a WiFi"
  );


  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );


  while (
    WiFi.status() != WL_CONNECTED
  ) {

    delay(500);

    Serial.print(".");
  }


  Serial.println();

  Serial.println(
    "WiFi conectado"
  );


  Serial.print(
    "IP ESP32: "
  );

  Serial.println(
    WiFi.localIP()
  );
}


// =====================================================
// MQTT
// =====================================================

void conectarMQTT() {

  while (
    !mqttClient.connected()
  ) {

    Serial.print(
      "Conectando a MQTT..."
    );


    String clientId =
      "DriveCost-";

    clientId +=
      String(
        random(0xffff),
        HEX
      );


    if (
      mqttClient.connect(
      clientId.c_str(),
      MQTT_USER,
      MQTT_PASSWORD
      )
    ) {

      Serial.println(
        " conectado"
      );

    } else {

      Serial.print(
        " ERROR: "
      );

      Serial.println(
        mqttClient.state()
      );


      delay(2000);
    }
  }
}


// =====================================================
// MQTT - enviar telemetría
// =====================================================

void enviarMQTT() {

  // No enviamos hasta tener GPS válido
  if (!gpsValido) {
    return;
  }


  char payload[200];


  snprintf(
    payload,
    sizeof(payload),

    "{\"latitud\":%.6f,"
    "\"longitud\":%.6f,"
    "\"consumo\":%.2f}",

    latitudActual,
    longitudActual,
    consumoActual
  );


  bool resultado =
    mqttClient.publish(
      MQTT_TOPIC,
      payload
    );


  if (resultado) {

    Serial.print(
      "MQTT -> "
    );

    Serial.println(
      payload
    );

  } else {

    Serial.println(
      "ERROR enviando MQTT"
    );
  }
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);


  Serial.println();
  Serial.println(
    "=== DriveCost ==="
  );


  // ---------------------------
  // GPS
  // ---------------------------

  GPSSerial.begin(
    9600,
    SERIAL_8N1,
    GPS_RX_PIN,
    GPS_TX_PIN
  );


  // ---------------------------
  // CAN
  // ---------------------------

  if (!iniciarCAN()) {

    Serial.println(
      "No se pudo iniciar CAN"
    );
  }


  // ---------------------------
  // WiFi
  // ---------------------------

  conectarWiFi();


  // ---------------------------
  // MQTT
  // ---------------------------

  mqttClient.setServer(
    MQTT_SERVER,
    MQTT_PORT
  );


  conectarMQTT();
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  // ===================================================
  // Mantener WiFi
  // ===================================================

  if (
    WiFi.status() != WL_CONNECTED
  ) {

    conectarWiFi();
  }


  // ===================================================
  // Mantener MQTT
  // ===================================================

  if (
    !mqttClient.connected()
  ) {

    conectarMQTT();
  }


  mqttClient.loop();


  // ===================================================
  // Leer GPS
  // ===================================================

  while (
    GPSSerial.available()
  ) {

    char c =
      GPSSerial.read();


    if (c == '\n') {

      linea[pos] = '\0';


      if (
        strncmp(
          linea,
          "$GPGGA",
          6
        ) == 0
        ||
        strncmp(
          linea,
          "$GNGGA",
          6
        ) == 0
      ) {

        procesarGGA(
          linea
        );
      }


      pos = 0;

    } else if (
      c != '\r'
    ) {


      if (
        pos <
        sizeof(linea) - 1
      ) {

        linea[pos++] = c;

      } else {

        pos = 0;
      }
    }
  }


  // ===================================================
  // Leer CAN
  // ===================================================

  leerCAN();


  // ===================================================
  // Enviar MQTT cada segundo
  // ===================================================

  unsigned long ahora =
    millis();


  if (
    ahora - ultimoEnvioMQTT
    >= INTERVALO_MQTT
  ) {

    ultimoEnvioMQTT =
      ahora;


    enviarMQTT();
  }
}