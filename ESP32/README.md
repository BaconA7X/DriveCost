# DriveCost - Hardware

Este directorio contiene la parte de adquisición de datos de **DriveCost**.

El sistema utiliza un **ESP32** para leer información del vehículo mediante **OBD-II sobre CAN**, obtener la posición mediante un módulo **GPS NEO-6M** y enviar posteriormente la telemetría mediante **MQTT** para su procesamiento en **Node-RED**.

---

## Arquitectura

```text
                    VEHÍCULO
                       │
                    OBD-II
                       │
                CAN-H / CAN-L
                       │
                 SN65HVD230
                       │
                     ESP32
                ┌──────┴──────┐
                │             │
               CAN           GPS
                │          NEO-6M
                │             │
                └──────┬──────┘
                       │
                     Wi-Fi
                       │
                      MQTT
                       │
                       ▼
                    Node-RED
```

---

## Componentes

- ESP32
- Transceptor CAN SN65HVD230
- Conector OBD-II
- Módulo GPS NEO-6M
- Power bank USB
- Cables Dupont
- Breadboard
- Cable USB de datos

---

# Esquema de conexiones

## OBD-II

Para acceder al bus CAN del vehículo se utilizan los siguientes pines del conector OBD-II:

```text
              CONECTOR OBD-II

        1   2   3   4   5   6   7   8
       ---------------------------------
      \ 9  10  11  12  13  14  15  16 /
       ---------------------------------

Pin 5  -> GND
Pin 6  -> CAN High
Pin 14 -> CAN Low
```

El pin 16 proporciona alimentación de batería del vehículo, pero no se utiliza en el prototipo inicial.

El ESP32 se alimenta mediante USB desde una batería externa.

---

## ESP32 + SN65HVD230 + OBD-II

```text
ESP32                    SN65HVD230                  OBD-II

3V3  ------------------> VCC

GND  ------------------> GND ----------------------> Pin 5

GPIO 5 ----------------> TXD

GPIO 4 <---------------- RXD


                          CANH ----------------------> Pin 6

                          CANL ----------------------> Pin 14
```

### Pinout CAN

| Elemento | Pin |
|---|---|
| ESP32 CAN TX | GPIO 5 |
| ESP32 CAN RX | GPIO 4 |
| SN65HVD230 VCC | 3.3 V |
| SN65HVD230 GND | GND |
| OBD-II CAN High | Pin 6 |
| OBD-II CAN Low | Pin 14 |
| OBD-II GND | Pin 5 |

El ESP32 incorpora un controlador CAN/TWAI, pero necesita un transceptor físico como el **SN65HVD230** para conectarse al bus CAN del vehículo.

> No añadir una resistencia de terminación de 120 Ω al conectarse directamente al bus CAN del coche. El vehículo ya dispone de terminación en el bus.

---

## ESP32 + GPS NEO-6M

El módulo GPS se comunica con el ESP32 mediante UART.

```text
NEO-6M                    ESP32

VCC  -------------------> 5V

GND  -------------------> GND

TX   -------------------> GPIO 16

RX   <------------------- GPIO 17
```

### Pinout GPS

| NEO-6M | ESP32 |
|---|---|
| VCC | 5V |
| GND | GND |
| TX | GPIO 16 |
| RX | GPIO 17 |

La conexión RX del GPS no es estrictamente necesaria si únicamente se quiere recibir información de posición.

---

# Esquema completo

```text
                           OBD-II
                       ┌─────────────┐
                       │             │
               Pin 6 CAN-H     Pin 14 CAN-L
                       │             │
                       ▼             ▼
                   ┌────────────────────┐
                   │    SN65HVD230      │
                   │                    │
                   │ CANH          CANL │
                   │                    │
                   │ TXD            RXD │
                   └──┬──────────────┬──┘
                      │              │
                      │              │
                  GPIO 5          GPIO 4
                      │              │
                ┌─────┴──────────────┴─────┐
                │          ESP32           │
                │                          │
                │ GPIO 16          GPIO 17 │
                └─────┬──────────────┬─────┘
                      │              │
                      │              │
                    RX GPS         TX GPS
                      │              │
                   ┌──┴──────────────┴──┐
                   │      NEO-6M        │
                   │                    │
                   │   GPS / GNSS       │
                   └────────────────────┘
```

Todas las masas deben estar conectadas entre sí:

```text
GND ESP32
   │
   ├── GND SN65HVD230
   │
   ├── GND NEO-6M
   │
   └── OBD-II Pin 5
```

---

# Pines utilizados en el ESP32

| Función | GPIO |
|---|---:|
| CAN TX | GPIO 5 |
| CAN RX | GPIO 4 |
| GPS RX | GPIO 16 |
| GPS TX | GPIO 17 |

Estos pines pueden modificarse posteriormente desde el código.

---

# Comunicación OBD-II

DriveCost utiliza OBD-II sobre CAN.

Las peticiones OBD-II se envían habitualmente mediante:

```text
CAN ID: 0x7DF
```

Las respuestas de las ECU suelen encontrarse entre:

```text
0x7E8 - 0x7EF
```

Ejemplo de petición de velocidad:

```text
ID: 0x7DF

02 01 0D 00 00 00 00 00
```

---

# PIDs utilizados

| PID | Parámetro | Unidad |
|---|---|---|
| `0x0C` | RPM | rpm |
| `0x0D` | Velocidad | km/h |
| `0x10` | Mass Air Flow | g/s |
| `0x5E` | Engine Fuel Rate | L/h |

---

# Cálculo de consumo

Si el vehículo soporta el PID `0x5E`, se obtiene el caudal de combustible en L/h.

El consumo instantáneo puede calcularse mediante:

```text
Consumo (L/100 km) =
(Fuel Rate (L/h) / Velocidad (km/h)) × 100
```

Ejemplo:

```text
Fuel Rate = 5.4 L/h

Velocidad = 90 km/h

Consumo = (5.4 / 90) × 100

Consumo = 6.0 L/100 km
```

Cuando el vehículo está detenido, el consumo se expresa en L/h.

---

# GPS

El módulo NEO-6M proporciona:

- Latitud
- Longitud
- Velocidad GPS
- Fecha
- Hora

Ejemplo de datos:

```json
{
  "latitude": 40.4168,
  "longitude": -3.7038,
  "gps_speed_kmh": 72.4
}
```

---

# Alimentación

Durante el desarrollo inicial el ESP32 se alimenta mediante una batería externa USB.

```text
Power Bank
    │
   USB
    │
    ▼
  ESP32
```

De esta forma no es necesario utilizar los 12 V disponibles en el pin 16 del conector OBD-II.

---

# MQTT

El ESP32 enviará la telemetría del vehículo mediante MQTT.

Topic principal previsto:

```text
drivecost/telemetry
```

Ejemplo de mensaje:

```json
{
  "speed_kmh": 82,
  "rpm": 2250,
  "fuel_rate_l_h": 5.4,
  "consumption_l_100km": 6.58,
  "latitude": 40.4168,
  "longitude": -3.7038
}
```

---

# Node-RED

Node-RED actuará como consumidor y procesador de la telemetría recibida desde MQTT.

Sus funciones principales serán:

- Recibir los datos enviados por el ESP32.
- Procesar la telemetría.
- Calcular valores derivados.
- Integrar el precio del combustible.
- Mostrar los datos en una interfaz.
- Trabajar con la posición GPS.
- Integrar la información de gasolineras y mapas.

Flujo general:

```text
ESP32
  │
  ▼
MQTT Broker
  │
  ▼
Node-RED
  │
  ├── Consumo
  ├── GPS
  ├── Precio combustible
  └── Interfaz
```

---

# Flujo de datos

```text
OBD-II
   │
   ▼
CAN / SN65HVD230
   │
   ▼
 ESP32 <----- NEO-6M
   │
   ▼
 Wi-Fi
   │
   ▼
 MQTT
   │
   ▼
Node-RED
```

---

# Orden de pruebas

1. Comprobar alimentación del ESP32.
2. Comprobar comunicación con el GPS.
3. Mostrar latitud y longitud por Serial.
4. Inicializar CAN/TWAI.
5. Comprobar recepción de tráfico CAN.
6. Leer velocidad mediante PID `0x0D`.
7. Leer RPM mediante PID `0x0C`.
8. Probar PID `0x5E`.
9. Calcular consumo en L/100 km.
10. Enviar los datos mediante MQTT.
11. Recibir los datos desde Node-RED.

---

# Seguridad

Al trabajar con el bus CAN de un vehículo:

- Realizar las primeras pruebas con el vehículo detenido.
- Verificar correctamente CAN-H y CAN-L.
- Compartir masa entre ESP32, transceptor CAN, GPS y OBD-II.
- No conectar directamente los 12 V del OBD-II al ESP32.
- Evitar transmitir tramas CAN arbitrarias.
- Comenzar con lectura y peticiones OBD-II estándar.
- No modificar parámetros de las ECU del vehículo.

---
