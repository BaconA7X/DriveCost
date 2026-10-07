## Problema a resolver y motivación

El objetivo del proyecto es desarrollar un sistema distribuido capaz de recibir, procesar y visualizar información asociada a un vehículo. La aplicación, denominada DriveCost, se plantea como un prototipo orientado a validar el funcionamiento de una arquitectura distribuida aplicada al ámbito de la automoción.
En la versión actual, el sistema trabaja con dos datos principales: el consumo del vehículo y su posición GPS. A partir de esta información, y combinándola con los precios oficiales de las estaciones de servicio, DriveCost permite localizar las gasolineras cercanas y calcular una estimación del coste en €/100 km para cada una de ellas.
La motivación del proyecto parte de que la información necesaria se encuentra originalmente en fuentes distintas. El consumo pertenece al vehículo, la posición se obtiene mediante GPS y los precios del combustible proceden de una fuente externa. DriveCost integra estos datos y los presenta de forma conjunta mediante un dashboard.
Para comunicar los distintos componentes se ha utilizado una arquitectura basada en el paradigma publish/subscribe, empleando MQTT como mecanismo principal de comunicación. De esta forma, los datos pueden enviarse al sistema sin que el productor necesite conocer directamente qué componente va a procesarlos o visualizarlos.
En esta primera versión, el comportamiento del vehículo se simula mediante un control manual que permite modificar el consumo. Este enfoque resulta suficiente para comprobar el flujo completo del sistema, desde la generación de los datos hasta su procesamiento y visualización, sin depender todavía de un vehículo real.
Como evolución futura, se plantea sustituir esta simulación por datos obtenidos directamente del vehículo, por ejemplo mediante CAN u OBD-II, y ampliar la arquitectura para admitir varios vehículos de entrada simultáneamente. El diseño de la comunicación MQTT se ha planteado teniendo en cuenta esa posible ampliación, aunque la implementación actual se haya validado con un único vehículo.


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

La telemetría enviada por DriveCost está formada principalmente por el consumo del vehículo y su posición GPS. Se trata de información que se actualiza de forma periódica y cuyo valor disminuye rápidamente cuando ya existe una muestra más reciente.
Por este motivo se ha optado por utilizar QoS 0 (at-most-once) para el envío de telemetría. Este nivel de servicio permite transmitir los mensajes sin esperar confirmaciones, reduciendo así el número de intercambios, el overhead de comunicación y la latencia.
La principal limitación de QoS 0 es que no garantiza la entrega de todos los mensajes. Sin embargo, en este caso se considera aceptable la pérdida puntual de alguna muestra, ya que poco después se generará una nueva lectura de consumo o posición que reflejará un estado más actualizado del vehículo.
De este modo, se prioriza la inmediatez de la información frente a la garantía de entrega de cada una de las muestras individuales, lo que resulta adecuado para el carácter periódico y dinámico de los datos utilizados en el sistema.

## Conclusiones (beneficios y limitaciones)

El desarrollo de DriveCost ha permitido implementar un prototipo funcional basado en una arquitectura distribuida y orientada a eventos. Mediante el uso de MQTT se ha conseguido desacoplar la generación de los datos de su procesamiento y visualización, de forma que los distintos componentes del sistema pueden comunicarse sin depender directamente unos de otros.
Entre los principales beneficios del sistema destaca la posibilidad de combinar en un mismo entorno la información de consumo del vehículo, su posición GPS y los precios de las gasolineras, permitiendo presentar estos datos de forma centralizada mediante un dashboard. Además, el uso de Node-RED ha facilitado la integración entre los distintos componentes y el tratamiento de los mensajes recibidos.
La arquitectura basada en MQTT también permite que el sistema pueda ampliarse en el futuro sin modificar completamente su estructura. Aunque la implementación actual se ha realizado con un único vehículo, el diseño de los topics se ha planteado de forma que sea posible incorporar varios vehículos y nuevos consumidores de información.
Como principal limitación, el sistema desarrollado es actualmente una simulación. El consumo se genera mediante un control manual y no procede directamente de un vehículo real, por lo que todavía no se ha realizado la integración con CAN u OBD-II.
Además, todos los componentes trabajan dentro de una misma red controlada y no se ha implementado una infraestructura completa de seguridad para conexiones desde redes externas. En un despliegue real sería necesario incorporar mecanismos de cifrado, autenticación y control de acceso.
Por último, la solución se ha validado a pequeña escala, con un único vehículo y un único broker MQTT. Por tanto, no se han realizado pruebas de carga ni de tolerancia a fallos que permitan evaluar su comportamiento con un número elevado de vehículos conectados simultáneamente.
En conjunto, DriveCost cumple el objetivo de demostrar el funcionamiento de una arquitectura distribuida para la recepción, procesamiento y visualización de datos relacionados con un vehículo, al mismo tiempo que deja abiertas distintas líneas de mejora para una futura implementación más cercana a un entorno real.


## Trabajo futuro

Como línea principal de trabajo futuro, se plantea sustituir la simulación actual por la obtención de datos directamente desde un vehículo real. Para ello, el consumo podría obtenerse mediante CAN u OBD-II, mientras que la posición seguiría obteniéndose a partir de un sistema GPS.
También se plantea ampliar el sistema para trabajar con varios vehículos de forma simultánea. La organización actual de los topics MQTT se ha diseñado teniendo en cuenta esta posibilidad, por lo que cada vehículo podría publicar sus datos de forma independiente utilizando un identificador propio.
Otra mejora importante sería permitir el funcionamiento del sistema entre redes diferentes, de forma que los vehículos pudieran enviar información a un broker accesible desde Internet. En este caso sería necesario incorporar mecanismos de seguridad como cifrado mediante TLS, autenticación de los dispositivos y control de permisos mediante ACL.
A nivel de infraestructura, también sería conveniente estudiar mecanismos de redundancia y tolerancia a fallos para evitar que el broker MQTT constituya un único punto de fallo. Del mismo modo, podrían realizarse pruebas de carga para comprobar el comportamiento del sistema con un número elevado de vehículos conectados simultáneamente.
Por último, podría incorporarse almacenamiento histórico de los datos recibidos, permitiendo consultar consumos anteriores, recorridos realizados y estadísticas asociadas a cada vehículo. Esto ampliaría el sistema más allá de la visualización en tiempo real y permitiría realizar análisis posteriores sobre la información recogida.
