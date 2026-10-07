# DriveCost

DriveCost es un prototipo de sistema de telemetría vehicular que combina datos de consumo con posición GPS y los transmite mediante MQTT para su posterior procesamiento, almacenamiento y visualización.

En el estado actual del proyecto, el consumo de combustible **se simula mediante un potenciómetro conectado a un Arduino Nano**. El valor generado se transmite mediante CAN a un ESP32-P4, que lo combina con las coordenadas obtenidas de un GPS NEO-6M y publica la telemetría mediante MQTT.

El proyecto permite validar toda la cadena de adquisición y comunicación antes de sustituir el consumo simulado por datos reales procedentes del vehículo.

## Instalación

![Instalación](hardware.jpeg)

## Esquema de conexiones

![Esquema de conexiones](esquematico%20hardware.png)

## Arquitectura

```text
                  POTENCIÓMETRO
                       │
                       │ ADC
                       ▼
                ┌──────────────┐
                │ Arduino Nano │
                └──────┬───────┘
                       │ SPI
                       ▼
                  ┌─────────┐
                  │ MCP2515 │
                  └────┬────┘
                       │
                       │ CAN 500 kbit/s
                       │
                       ▼
                ┌─────────────┐
                │ SN65HVD230  │
                └──────┬──────┘
                       │ TWAI
                       ▼
GPS NEO-6M ─────────► ESP32-P4
    UART                 │
                         │ Wi-Fi
                         │
                         │ MQTT
                         ▼
                  ┌─────────────┐
                  │  Mosquitto  │
                  │ MQTT Broker │
                  └──────┬──────┘
                         │
                  drivecost/telemetry
                         │
              ┌──────────┴──────────┐
              ▼                     ▼
           Node-RED               Python
```

## Funcionamiento

DriveCost está formado actualmente por tres bloques principales.

### Simulador de consumo

Un Arduino Nano lee mediante su ADC un potenciómetro conectado al pin `A0`.

El valor ADC se encuentra entre:

```text
0 - 1023
```

y se transforma a un consumo simulado entre:

```text
0 - 20 L/100 km
```

mediante:

```cpp
float consumo = valorADC * 20.0 / 1023.0;
```

Por ejemplo:

```text
ADC = 0      → 0.00 L/100 km
ADC ≈ 512    → 10.01 L/100 km
ADC = 1023   → 20.00 L/100 km
```

Este mecanismo permite probar el sistema modificando manualmente el consumo mediante el potenciómetro.

### Comunicación CAN

El Arduino Nano utiliza un controlador MCP2515 para transmitir el consumo al ESP32-P4.

La red funciona a:

```text
500 kbit/s
```

La trama utilizada es:

```text
CAN ID: 0x100
DLC:    2 bytes
```

Antes de transmitirlo, el consumo se multiplica por `100`.

Por ejemplo:

```text
5.43 L/100 km
      ↓
543
      ↓
0x021F
      ↓
02 1F
```

El formato de la trama es:

```text
ID 0x100

Byte 0                 Byte 1
┌────────────┬───────────────┐
│ MSB        │ LSB           │
└────────────┴───────────────┘
          consumo × 100
```

El ESP32 reconstruye el valor mediante:

```cpp
uint16_t valor =
    ((uint16_t)mensaje.data[0] << 8) |
    mensaje.data[1];

float consumo = valor / 100.0;
```

### Gateway ESP32-P4

El ESP32-P4 centraliza la información.

Recibe:

- Consumo simulado mediante CAN.
- Latitud mediante GPS.
- Longitud mediante GPS.

Posteriormente genera un mensaje JSON y lo publica mediante MQTT.

## Hardware

| Componente | Función |
|---|---|
| Arduino Nano | Lectura y generación del consumo simulado |
| Potenciómetro | Simulación del consumo instantáneo |
| MCP2515 | Controlador CAN del Arduino Nano |
| SN65HVD230 | Transceptor CAN del ESP32 |
| ESP32-P4 | Gateway CAN/GPS/MQTT |
| GPS NEO-6M | Obtención de posición |
| PC/servidor | Broker MQTT |

## Potenciómetro

El potenciómetro se conecta al Arduino Nano:

```text
Potenciómetro            Arduino Nano

Extremo 1 ────────────── 5V
Central   ────────────── A0
Extremo 2 ────────────── GND
```

El terminal central proporciona una tensión variable entre aproximadamente `0 V` y `5 V`.

El ADC de 10 bits del Nano convierte esta tensión en:

```text
0 - 1023
```

## Bus CAN

La conexión entre los nodos es:

```text
Arduino Nano
     │
     │ SPI
     ▼
  MCP2515
     │
     │ CANH/CANL
     ▼
SN65HVD230
     │
     │ TX/RX
     ▼
 ESP32-P4
```

Todos los nodos deben compartir GND.

El bus utiliza:

```text
500 kbit/s
```

y debe disponer de una resistencia de terminación de `120 Ω` en cada extremo.

Con el sistema apagado:

```text
CANH ↔ CANL ≈ 60 Ω
```

indica que las dos terminaciones de 120 Ω están conectadas.

## GPS

El GPS NEO-6M está conectado al ESP32 mediante UART.

Configuración:

```text
9600 baud
8N1
```

El firmware procesa:

```text
$GPGGA
$GNGGA
```

y extrae la latitud y longitud.

Las coordenadas NMEA se convierten posteriormente a grados decimales.

## MQTT

El ESP32 se conecta mediante Wi-Fi a un broker Eclipse Mosquitto.

Configuración actual:

```text
Protocolo: MQTT
Puerto:    1883
Topic:     drivecost/telemetry
```

El broker requiere autenticación mediante usuario y contraseña.

Las conexiones anónimas están deshabilitadas.

## Formato de telemetría

El ESP32 publica mensajes JSON con la siguiente estructura:

```json
{
  "latitud": 40.416775,
  "longitud": -3.703790,
  "consumo": 6.42
}
```

Las unidades son:

| Campo | Descripción | Unidad |
|---|---|---|
| `latitud` | Latitud obtenida del GPS | grados decimales |
| `longitud` | Longitud obtenida del GPS | grados decimales |
| `consumo` | Consumo simulado por el potenciómetro | L/100 km |

## Flujo de datos

```text
Potenciómetro
     │
     │ 0-5 V
     ▼
ADC Arduino Nano
     │
     │ 0-1023
     ▼
Conversión a L/100 km
     │
     │ consumo × 100
     ▼
CAN ID 0x100
     │
     ▼
ESP32-P4 ◄──────── GPS NEO-6M
     │
     │ JSON
     ▼
Wi-Fi
     │
     │ MQTT
     ▼
Mosquitto
     │
     ▼
drivecost/telemetry
```

## Estructura del proyecto

```text
DriveCost/
│
├── README.md
│
├── hardware/
│   │
│   ├── arduino-nano/
│   │   ├── README.md
│   │   └── src/
│   │
│   └── esp32/
│       ├── README.md
│       └── src/
│
├── mqtt-broker/
│   ├── README.md
│   └── drivecost.conf
│
├── node-red/
│   └── README.md
│
└── scripts/
    └── mqtt_receiver.py
```

## Puesta en marcha

El orden recomendado es:

1. Iniciar Mosquitto.
2. Iniciar un cliente MQTT para observar la telemetría.
3. Encender el Arduino Nano.
4. Encender el ESP32-P4.
5. Esperar a que el GPS obtenga FIX.
6. Girar el potenciómetro para modificar el consumo.
7. Comprobar los mensajes MQTT.

Para escuchar la telemetría:

```bash
mosquitto_sub \
    -h localhost \
    -p 1883 \
    -u drivecost \
    -P 'TU_PASSWORD' \
    -t "drivecost/telemetry" \
    -v
```

Se deberían recibir mensajes similares a:

```text
drivecost/telemetry {"latitud":40.416775,"longitud":-3.703790,"consumo":4.32}
drivecost/telemetry {"latitud":40.416781,"longitud":-3.703795,"consumo":8.57}
drivecost/telemetry {"latitud":40.416790,"longitud":-3.703801,"consumo":12.14}
```

Al girar el potenciómetro debe cambiar el campo `consumo`.

## Estado actual

Actualmente DriveCost permite:

- Simular un consumo entre `0` y `20 L/100 km`.
- Leer el potenciómetro mediante el ADC del Arduino Nano.
- Transmitir el consumo mediante CAN.
- Comunicación CAN a 500 kbit/s.
- Recepción CAN mediante el ESP32-P4.
- Obtención de latitud y longitud mediante GPS.
- Conversión de coordenadas NMEA.
- Conexión Wi-Fi.
- Autenticación MQTT.
- Publicación de telemetría mediante JSON.

## Limitaciones actuales

El valor de consumo utilizado actualmente **no procede del vehículo**.

El potenciómetro funciona como generador de datos para validar:

```text
Sensor → Arduino → CAN → ESP32 → MQTT → Backend
```

Por tanto, los valores de `L/100 km` publicados son valores simulados y no deben interpretarse como medidas reales del consumo del vehículo.

## Evolución prevista

Una vez validada toda la infraestructura, el generador mediante potenciómetro puede sustituirse por adquisición real de datos del vehículo.

La arquitectura permite evolucionar hacia:

```text
OBD-II / ECU
     │
     ▼
Datos reales del vehículo
     │
     ▼
CAN
     │
     ▼
ESP32 + GPS
     │
     ▼
MQTT
     │
     ▼
Análisis de trayectos
```

Entre las futuras funcionalidades se encuentran:

- Obtención de datos reales mediante OBD-II.
- Velocidad del vehículo.
- RPM.
- Carga del motor.
- Cálculo de consumo real.
- Almacenamiento histórico.
- Node-RED.
- Dashboard en tiempo real.
- Representación GPS sobre mapa.
- Cálculo del coste de cada trayecto.
- Estadísticas de consumo.
- MQTT sobre TLS.

## Objetivo

La versión actual de DriveCost funciona como banco de pruebas de toda la infraestructura de telemetría:

```text
Consumo simulado + GPS
          │
          ▼
         CAN
          │
          ▼
       ESP32-P4
          │
          ▼
         MQTT
          │
          ▼
      Procesamiento
```

Esto permite validar cada etapa de comunicación antes de integrar DriveCost con datos reales del vehículo.




## Semántica de producción y consumo

La telemetría producida por DriveCost consiste principalmente en datos frecuentes cuyo valor disminuye rápidamente con el tiempo. Una muestra concreta de velocidad o RPM tiene poca utilidad varios segundos después de haberse producido, especialmente si ya existe una lectura más reciente.
Por este motivo se ha optado por utilizar QoS 0 (at-most-once) para la telemetría periódica.
Con QoS 0, el productor envía el mensaje sin requerir confirmación de recepción. Esto reduce el número de mensajes intercambiados, el overhead del protocolo y la latencia.
Existe la posibilidad de perder alguna muestra ante un problema de red. Sin embargo, para esta aplicación esta pérdida puntual resulta aceptable porque pocos instantes después se genera una nueva muestra.


