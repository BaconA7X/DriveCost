#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "driver/twai.h"

// =====================================================
// WIFI
// =====================================================

const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";
// IP DEL PC DONDE ESTA MOSQUITTO

const char* MQTT_SERVER = "";
const int MQTT_PORT = 1883;
const char* MQTT_USER = "";
const char* MQTT_PASSWORD = "";

// Topic donde enviamos:
//
// {
//   "latitud": ...,
//   "longitud": ...,
//   "consumo": ...
// }
//
// RETAINED:
// Mosquitto guarda siempre el último mensaje.
//
const char* MQTT_TOPIC =
  "drivecost/telemetry";


// Topic de estado del dispositivo
//
// {"estado":"online"}
// {"estado":"offline"}
//
const char* MQTT_STATUS_TOPIC =
  "drivecost/status";


// =====================================================
// LAST WILL TESTAMENT
// =====================================================

// Mosquitto publicará esto si el ESP32
// desaparece inesperadamente.

const char* MQTT_WILL_MESSAGE =
  "{\"estado\":\"offline\"}";


// Esto lo publica el propio ESP32
// cuando consigue conectarse.

const char* MQTT_ONLINE_MESSAGE =
  "{\"estado\":\"online\"}";


// QoS del Last Will
const int MQTT_WILL_QOS = 1;


// Guardar último estado
const bool MQTT_WILL_RETAIN = true;


// =====================================================
// CLIENTES WIFI / MQTT
// =====================================================

WiFiClient wifiClient;

PubSubClient mqttClient(wifiClient);


// =====================================================
// DEBUG
// =====================================================

const bool DEBUG = true;


// =====================================================
// GPS
// =====================================================

#define GPS_RX_PIN 21
#define GPS_TX_PIN 20


HardwareSerial GPSSerial(1);


char linea[128];

int pos = 0;


// =====================================================
// UBICACIÓN POR DEFECTO
// =====================================================
//
// Se utiliza hasta que llega
// la primera ubicación GPS.
//

double latitudActual =
  40.389700;

double longitudActual =
  -3.627894;


// true porque queremos utilizar
// la ubicación por defecto inicialmente
bool gpsValido = true;


// =====================================================
// CAN
// =====================================================

#define CAN_TX_PIN 22
#define CAN_RX_PIN 23


float consumoActual = 0.0;

bool consumoValido = false;


// =====================================================
// MQTT - INTERVALO
// =====================================================

unsigned long ultimoEnvioMQTT = 0;

const unsigned long INTERVALO_MQTT = 1000;


// =====================================================
// GPS - CONVERTIR NMEA A DECIMAL
// =====================================================

double nmeaToDecimal(
  const char* coord,
  char hemi
) {

  double valor =
    atof(coord);


  int grados =
    (int)(valor / 100);


  double minutos =
    valor - grados * 100;


  double decimal =
    grados + minutos / 60.0;


  if (
    hemi == 'S' ||
    hemi == 'W'
  ) {

    decimal =
      -decimal;
  }


  return decimal;
}


// =====================================================
// GPS - PROCESAR GGA
// =====================================================

void procesarGGA(
  char* trama
) {

  char* campos[15];

  int campo = 0;


  campos[campo++] =
    trama;


  // -------------------------------------------------
  // Separar campos conservando campos vacíos
  // -------------------------------------------------

  for (
    int i = 0;
    trama[i] != '\0' &&
    campo < 15;
    i++
  ) {

    if (
      trama[i] == ','
    ) {

      trama[i] =
        '\0';


      campos[campo++] =
        &trama[i + 1];
    }
  }


  if (
    campo < 7
  ) {

    return;
  }


  // -------------------------------------------------
  // Campo 6 = calidad del FIX
  // -------------------------------------------------

  if (
    atoi(campos[6]) == 0
  ) {

    return;
  }


  // -------------------------------------------------
  // Comprobar campos GPS
  // -------------------------------------------------

  if (
    strlen(campos[2]) == 0 ||
    strlen(campos[3]) == 0 ||
    strlen(campos[4]) == 0 ||
    strlen(campos[5]) == 0
  ) {

    return;
  }


  // -------------------------------------------------
  // Latitud
  // -------------------------------------------------

  latitudActual =
    nmeaToDecimal(
      campos[2],
      campos[3][0]
    );


  // -------------------------------------------------
  // Longitud
  // -------------------------------------------------

  longitudActual =
    nmeaToDecimal(
      campos[4],
      campos[5][0]
    );


  gpsValido =
    true;


  // -------------------------------------------------
  // DEBUG
  // -------------------------------------------------

  if (DEBUG) {

    Serial.print(
      "GPS: "
    );


    Serial.print(
      latitudActual,
      6
    );


    Serial.print(
      ","
    );


    Serial.println(
      longitudActual,
      6
    );
  }
}


// =====================================================
// CAN - INICIAR
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


  if (
    err != ESP_OK
  ) {

    Serial.print(
      "Error instalando CAN: "
    );


    Serial.println(
      esp_err_to_name(err)
    );


    return false;
  }


  err =
    twai_start();


  if (
    err != ESP_OK
  ) {

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
// CAN - LEER CONSUMO
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

        (
          (uint16_t)mensaje.data[0]
          << 8
        )

        |

        mensaje.data[1];


      consumoActual =
        valor / 100.0;


      consumoValido =
        true;


      if (DEBUG) {

        Serial.print(
          "Consumo: "
        );


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

    WiFi.status()
    != WL_CONNECTED

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


    // -------------------------------------------------
    // CLIENT ID ESTABLE
    // -------------------------------------------------
    //
    // Cada ESP32 tendrá un Client ID
    // único basado en su MAC.
    //

    String clientId =
      "DriveCost-";

    clientId +=
      WiFi.macAddress();


    // Quitamos ':' de la MAC

    clientId.replace(
      ":",
      ""
    );


    // -------------------------------------------------
    // CONEXIÓN MQTT CON LAST WILL
    // -------------------------------------------------

    bool conectado =

      mqttClient.connect(

        clientId.c_str(),

        MQTT_USER,

        MQTT_PASSWORD,

        // Topic LWT
        MQTT_STATUS_TOPIC,

        // QoS
        MQTT_WILL_QOS,

        // Retained
        MQTT_WILL_RETAIN,

        // Mensaje Last Will
        MQTT_WILL_MESSAGE
      );


    if (
      conectado
    ) {

      Serial.println(
        " conectado"
      );


      // ===============================================
      // PUBLICAR ONLINE
      // ===============================================
      //
      // También retained.
      //
      // Esto sustituye cualquier
      // "offline" anterior almacenado.
      //

      bool resultadoEstado =

        mqttClient.publish(

          MQTT_STATUS_TOPIC,

          MQTT_ONLINE_MESSAGE,

          true
        );


      if (
        resultadoEstado
      ) {

        Serial.println(
          "MQTT STATUS -> ONLINE"
        );

      } else {

        Serial.println(
          "ERROR publicando estado ONLINE"
        );
      }


    } else {

      Serial.print(
        " ERROR MQTT: "
      );


      Serial.println(
        mqttClient.state()
      );


      delay(2000);
    }
  }
}


// =====================================================
// MQTT - ENVIAR TELEMETRÍA
// =====================================================

void enviarMQTT() {

  // -------------------------------------------------
  // Necesitamos una posición válida
  // -------------------------------------------------
  //
  // Inicialmente tenemos la ubicación
  // por defecto.
  //

  if (
    !gpsValido
  ) {

    return;
  }


  char payload[200];


  // -------------------------------------------------
  // CREAR JSON
  // -------------------------------------------------

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


  // =================================================
  // PUBLICAR TELEMETRÍA
  // =================================================
  //
  // El TRUE activa RETAIN.
  //
  // Esto hace que Mosquitto conserve
  // siempre la última telemetría.
  //

  bool resultado =

    mqttClient.publish(

      MQTT_TOPIC,

      payload,

      true
    );


  if (
    resultado
  ) {

    if (DEBUG) {

      Serial.print(
        "MQTT -> "
      );


      Serial.println(
        payload
      );
    }


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

  Serial.begin(
    115200
  );


  delay(
    1000
  );


  Serial.println();

  Serial.println(
    "=== DriveCost ==="
  );


  // =================================================
  // GPS
  // =================================================

  GPSSerial.begin(

    9600,

    SERIAL_8N1,

    GPS_RX_PIN,

    GPS_TX_PIN
  );


  // =================================================
  // CAN
  // =================================================

  if (
    !iniciarCAN()
  ) {

    Serial.println(
      "No se pudo iniciar CAN"
    );
  }


  // =================================================
  // WIFI
  // =================================================

  conectarWiFi();


  // =================================================
  // MQTT
  // =================================================

  mqttClient.setServer(

    MQTT_SERVER,

    MQTT_PORT

  );


  // =================================================
  // MQTT KEEP ALIVE
  // =================================================
  //
  // Si Mosquitto deja de recibir tráfico
  // durante este periodo, detectará antes
  // que el ESP32 ha desaparecido.
  //

  mqttClient.setKeepAlive(
    10
  );


  conectarMQTT();
}


// =====================================================
// LOOP
// =====================================================

void loop() {


  // =================================================
  // MANTENER WIFI
  // =================================================

  if (

    WiFi.status()
    != WL_CONNECTED

  ) {

    conectarWiFi();
  }


  // =================================================
  // MANTENER MQTT
  // =================================================

  if (

    !mqttClient.connected()

  ) {

    conectarMQTT();
  }


  // PubSubClient necesita ejecutar loop()
  // constantemente para mantener
  // viva la conexión.

  mqttClient.loop();


  // =================================================
  // LEER GPS
  // =================================================

  while (

    GPSSerial.available()

  ) {


    char c =
      GPSSerial.read();


    if (
      c == '\n'
    ) {

      linea[pos] =
        '\0';


      if (
        DEBUG
      ) {

        Serial.println(
          linea
        );
      }


      // -----------------------------------------------
      // GPGGA o GNGGA
      // -----------------------------------------------

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

        linea[pos++] =
          c;


      } else {

        // Evitar overflow

        pos = 0;
      }
    }
  }


  // =================================================
  // LEER CAN
  // =================================================

  leerCAN();


  // =================================================
  // ENVIAR MQTT CADA SEGUNDO
  // =================================================

  unsigned long ahora =
    millis();


  if (

    ahora -
    ultimoEnvioMQTT

    >=

    INTERVALO_MQTT

  ) {

    ultimoEnvioMQTT =
      ahora;


    enviarMQTT();
  }
}