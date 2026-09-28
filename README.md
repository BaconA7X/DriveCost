# DriveCost

**DriveCost** es un sistema de telemetría para vehículos diseñado para obtener datos del coche mediante CAN/OBD-II, combinarlos con información de posición GPS y enviarlos mediante MQTT para su posterior procesamiento, almacenamiento y visualización.

El objetivo principal es disponer de una plataforma capaz de relacionar el **consumo de combustible** con la **posición del vehículo**, permitiendo posteriormente analizar rutas, consumo, eficiencia y coste de los desplazamientos.

## Arquitectura

```text
                         ┌─────────────┐
                         │     GPS     │
                         │    NEO-6M   │
                         └──────┬──────┘
                                │ UART
                                ▼
┌─────────────┐          ┌──────────────┐
│             │   CAN    │              │
│   Arduino   │═════════►│   ESP32-P4   │
│    Nano     │          │              │
│      +      │          └──────┬───────┘
│   MCP2515   │                 │
│             │              Wi-Fi
└──────▲──────┘                 │
       │                        │ MQTT
       │ CAN / OBD-II           ▼
       │                 ┌──────────────┐
┌──────┴──────┐          │  Mosquitto   │
│   Vehículo  │          │ MQTT Broker  │
└─────────────┘          └──────┬───────┘
                                │
                    ┌───────────┴───────────┐
                    │                       │
                    ▼                       ▼
                Node-RED                 Python
                    │
                    ▼
          Procesamiento / Dashboard
```

## Funcionamiento

DriveCost está dividido en tres bloques principales:

1. **Adquisición CAN**
   
   Un Arduino Nano conectado a un MCP2515 se encarga de trabajar con el bus CAN y obtener los datos relacionados con el vehículo.

2. **GPS y gateway**
   
   El ESP32-P4 recibe la posición desde un módulo GPS y los datos CAN mediante un transceptor SN65HVD230.

   El ESP32 combina ambos datos y genera mensajes de telemetría.

3. **Backend MQTT**
   
   El ESP32 se conecta mediante Wi-Fi a un broker Eclipse Mosquitto y publica periódicamente la telemetría utilizando MQTT.

## Hardware

El proyecto utiliza principalmente:

| Componente | Función |
|---|---|
| Arduino Nano | Nodo CAN |
| MCP2515 | Controlador CAN del Arduino |
| ESP32-P4 | Gateway principal |
| SN65HVD230 | Transceptor CAN del ESP32 |
| GPS NEO-6M | Posicionamiento |
| OBD-II | Acceso al vehículo |
| PC / servidor | Broker MQTT y procesamiento |

## Comunicación CAN

El enlace entre el Arduino Nano y el ESP32 utiliza CAN.

Configuración actual:

```text
Bitrate: 500 kbit/s
```

La conexión física es:

```text
Arduino Nano                     ESP32-P4
     │                               │
     │ SPI                           │ TWAI
     ▼                               ▼
  MCP2515                       SN65HVD230
     │                               │
     │ CANH ═══════════════════ CANH │
     │ CANL ═══════════════════ CANL │
     │ GND  ─────────────────── GND  │
```

El bus debe disponer de una resistencia de terminación de **120 Ω en cada extremo**.

Con el sistema apagado:

```text
CANH ↔ CANL ≈ 60 Ω
```

indica que existen dos terminaciones de 120 Ω correctamente conectadas.

## Protocolo CAN interno

Actualmente DriveCost utiliza el identificador:

```text
0x100
```

para transmitir el consumo hacia el ESP32.

El consumo se codifica utilizando dos bytes:

```text
Byte 0 → parte alta
Byte 1 → parte baja
```

El valor se multiplica por `100` antes de enviarlo.

Por ejemplo:

```text
6.42 L/100km

6.42 × 100 = 642
```

El ESP32 reconstruye el valor mediante:

```cpp
uint16_t valor =
    ((uint16_t)mensaje.data[0] << 8) |
    mensaje.data[1];

float consumo = valor / 100.0;
```

## GPS

El ESP32 recibe los datos GPS mediante UART.

Configuración:

```text
Baudrate: 9600
Formato:  8N1
```

Se procesan las tramas NMEA:

```text
$GPGGA
$GNGGA
```

De ellas se obtiene:

```text
Latitud
Longitud
Calidad del FIX
```

Las coordenadas NMEA se convierten a grados decimales antes de ser enviadas.

Ejemplo:

```text
40.416775,-3.703790
```

Si el GPS no dispone de un FIX válido, las coordenadas no se consideran válidas para la telemetría.

## MQTT

El ESP32 actúa como cliente MQTT.

El broker utilizado es:

**Eclipse Mosquitto**

Puerto actual:

```text
1883
```

Topic:

```text
drivecost/telemetry
```

La conexión requiere autenticación mediante usuario y contraseña.

Las conexiones anónimas están deshabilitadas en el broker.

## Formato de telemetría

DriveCost utiliza JSON para transmitir los datos.

Ejemplo:

```json
{
  "latitud": 40.416775,
  "longitud": -3.703790,
  "consumo": 6.42
}
```

Las unidades utilizadas son:

| Campo | Unidad |
|---|---|
| `latitud` | grados decimales |
| `longitud` | grados decimales |
| `consumo` | L/100 km |

El ESP32 publica periódicamente estos mensajes en:

```text
drivecost/telemetry
```

## Flujo completo de datos

El recorrido de un dato dentro de DriveCost es:

```text
Vehículo
   │
   │ CAN / OBD-II
   ▼
Arduino Nano
   │
   │ SPI
   ▼
MCP2515
   │
   │ CAN
   ▼
SN65HVD230
   │
   │ TWAI
   ▼
ESP32-P4 ◄──────── GPS
   │
   │
   │ JSON
   │
   │ Wi-Fi / MQTT
   ▼
Mosquitto
   │
   ├────────► Node-RED
   │
   ├────────► Python
   │
   └────────► Otros clientes MQTT
```

## Estructura del proyecto

Una posible estructura del repositorio es:

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

Cada componente dispone de su propio README con información específica sobre instalación, conexiones y configuración.

## Puesta en marcha

El orden recomendado para iniciar DriveCost es:

### 1. Iniciar Mosquitto

```bash
sudo systemctl start mosquitto
```

Comprobar:

```bash
sudo systemctl status mosquitto
```

### 2. Escuchar la telemetría

Para comprobar directamente los mensajes MQTT:

```bash
mosquitto_sub \
    -h localhost \
    -p 1883 \
    -u drivecost \
    -P 'TU_PASSWORD' \
    -t "drivecost/telemetry" \
    -v
```

### 3. Encender el nodo CAN

Alimentar el Arduino Nano y comprobar que el MCP2515 se inicializa correctamente.

### 4. Encender el ESP32

El ESP32 debería:

```text
1. Inicializar CAN
2. Inicializar GPS
3. Conectarse al Wi-Fi
4. Conectarse al broker MQTT
5. Esperar un FIX GPS válido
6. Recibir datos CAN
7. Publicar la telemetría
```

Una ejecución normal puede mostrar:

```text
=== DriveCost ===

CAN iniciado a 500 kbps

WiFi conectado
IP ESP32: 192.168.1.50

Conectando a MQTT... conectado

GPS: 40.416775,-3.703790
Consumo: 6.42 L/100km

MQTT -> {"latitud":40.416775,"longitud":-3.703790,"consumo":6.42}
```

## Comprobación del broker

Se puede realizar una publicación manual:

```bash
mosquitto_pub \
    -h localhost \
    -p 1883 \
    -u drivecost \
    -P 'TU_PASSWORD' \
    -t "drivecost/telemetry" \
    -m '{"latitud":40.416775,"longitud":-3.703790,"consumo":6.42}'
```

Esto permite probar la parte MQTT independientemente del hardware.

## Diagnóstico CAN

Si existen problemas de comunicación, comprobar primero el bus con un multímetro.

Con el sistema apagado:

```text
CANH ↔ CANL ≈ 60 Ω
```

También comprobar:

```text
CANH ───── CANH
CANL ───── CANL
GND  ───── GND
```

Todos los nodos deben utilizar:

```text
500 kbit/s
```

En el MCP2515 también es necesario configurar correctamente la frecuencia de su cristal (`8 MHz` o `16 MHz`).

## Diagnóstico MQTT

Para observar los logs del broker:

```bash
sudo journalctl -u mosquitto -f
```

Para comprobar si Mosquitto está escuchando en el puerto esperado:

```bash
ss -ltn | grep 1883
```

Para escuchar los mensajes:

```bash
mosquitto_sub \
    -h localhost \
    -u drivecost \
    -P 'TU_PASSWORD' \
    -t "drivecost/#" \
    -v
```

## Seguridad

El broker requiere actualmente autenticación mediante usuario y contraseña:

```text
allow_anonymous false
```

Las credenciales Wi-Fi y MQTT no deben almacenarse en el repositorio público.

Se recomienda utilizar archivos locales ignorados mediante `.gitignore`, variables de entorno durante el proceso de compilación o mecanismos equivalentes para gestionar secretos.

La configuración actual utiliza MQTT en el puerto `1883`, por lo que existe autenticación pero **no cifrado del tráfico**.

Para un despliegue fuera de una red local controlada se recomienda utilizar:

```text
MQTT sobre TLS
Puerto 8883
Certificados
Usuario + contraseña
ACL por dispositivo
```

## Estado del proyecto

Actualmente DriveCost permite:

- Comunicación CAN entre Arduino Nano y ESP32-P4.
- Recepción de consumo mediante CAN.
- Lectura de posición GPS.
- Conversión de coordenadas NMEA.
- Conexión Wi-Fi.
- Conexión MQTT autenticada.
- Publicación de telemetría en JSON.
- Recepción de datos desde clientes MQTT.

## Próximos pasos

La arquitectura permite ampliar DriveCost con funcionalidades como:

- Integración completa con OBD-II.
- Obtención de RPM, velocidad y carga del motor.
- Cálculo de consumo real.
- Almacenamiento histórico de trayectos.
- Integración con Node-RED.
- Dashboard en tiempo real.
- Representación de recorridos sobre un mapa.
- Cálculo del coste de cada trayecto.
- Estadísticas de consumo medio.
- Comparación entre rutas.
- Base de datos de telemetría.
- MQTT sobre TLS.
- Gestión de múltiples vehículos.

## Objetivo final

DriveCost busca convertir los datos disponibles en el vehículo en información útil sobre cada desplazamiento:

```text
CAN + GPS
    │
    ▼
Telemetría
    │
    ▼
Consumo + posición
    │
    ▼
Ruta + eficiencia + coste
```

De esta forma, el sistema puede evolucionar desde un prototipo de adquisición CAN/GPS hasta una plataforma completa de análisis de conducción y costes de desplazamiento.