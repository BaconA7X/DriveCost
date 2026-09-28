# DriveCost — Arduino Nano

El Arduino Nano funciona como **simulador del consumo de combustible** dentro del prototipo DriveCost.

Un potenciómetro permite generar manualmente un valor entre `0` y `20 L/100 km`. El Arduino lee este valor mediante su ADC y lo transmite al ESP32-P4 utilizando CAN mediante un módulo MCP2515.

## Arquitectura

```text
Potenciómetro
     │
     │ tensión analógica
     ▼
Arduino Nano
     │
     │ SPI
     ▼
  MCP2515
     │
     │ CAN 500 kbit/s
     ▼
SN65HVD230
     │
     ▼
 ESP32-P4
```

## Potenciómetro

El potenciómetro se conecta de la siguiente forma:

```text
Potenciómetro          Arduino Nano

Extremo 1 ──────────── 5V
Pin central ────────── A0
Extremo 2 ──────────── GND
```

El pin central proporciona una tensión variable según la posición del potenciómetro.

## Lectura ADC

El Arduino Nano utiliza un ADC de 10 bits.

Por tanto:

```text
0 V  → ADC = 0
5 V  → ADC = 1023
```

El firmware lee:

```cpp
int valorADC = analogRead(A0);
```

y transforma el valor a un consumo simulado:

```cpp
float consumo = valorADC * 20.0 / 1023.0;
```

El rango resultante es:

```text
0.00 - 20.00 L/100 km
```

Ejemplos aproximados:

| ADC | Consumo |
|---:|---:|
| 0 | 0.00 L/100 km |
| 256 | 5.00 L/100 km |
| 512 | 10.01 L/100 km |
| 768 | 15.01 L/100 km |
| 1023 | 20.00 L/100 km |

## MCP2515

El Arduino utiliza un MCP2515 como controlador CAN externo.

### Conexiones

```text
Arduino Nano          MCP2515

5V        ─────────── VCC
GND       ─────────── GND
D10       ─────────── CS
D11       ─────────── SI / MOSI
D12       ─────────── SO / MISO
D13       ─────────── SCK
```

El pin `INT` no es necesario para la transmisión actual.

## Configuración CAN

El bus funciona a:

```text
500 kbit/s
```

El firmware utiliza:

```cpp
CAN.begin(
    MCP_ANY,
    CAN_500KBPS,
    MCP_8MHZ
);
```

La configuración `MCP_8MHZ` debe coincidir con el cristal del módulo MCP2515.

Si el módulo utiliza un cristal de 16 MHz deberá utilizarse:

```cpp
MCP_16MHZ
```

## Codificación del consumo

CAN transporta bytes, por lo que el valor `float` no se transmite directamente.

Primero se multiplica por `100`:

```cpp
uint16_t consumoCAN =
    (uint16_t)(consumo * 100.0);
```

Ejemplo:

```text
Consumo = 5.43 L/100 km

5.43 × 100 = 543
```

El entero de 16 bits se divide en dos bytes:

```cpp
data[0] = (consumoCAN >> 8) & 0xFF;
data[1] = consumoCAN & 0xFF;
```

Para `5.43 L/100 km`:

```text
543 decimal = 0x021F

data[0] = 0x02
data[1] = 0x1F
```

## Trama CAN

La trama enviada tiene:

```text
ID:  0x100
DLC: 2
```

Formato:

```text
             CAN ID 0x100

        Byte 0          Byte 1
      ┌─────────┬─────────────────┐
      │   MSB   │       LSB       │
      └─────────┴─────────────────┘
             consumo × 100
```

El ESP32 puede reconstruirlo mediante:

```cpp
uint16_t valor =
    ((uint16_t)data[0] << 8) |
    data[1];

float consumo = valor / 100.0;
```

## Frecuencia de transmisión

El firmware espera:

```cpp
delay(500);
```

por lo que aproximadamente se transmite una nueva muestra cada:

```text
500 ms
```

equivalente a aproximadamente:

```text
2 muestras/s
```

## Monitor serie

El monitor serie funciona a:

```text
115200 baud
```

Ejemplo:

```text
Iniciando Arduino Nano...
Iniciando MCP2515...
MCP2515 iniciado correctamente
CAN a 500 kbps
Listo

ADC: 278 | Consumo: 5.43
ADC: 278 | Consumo: 5.43 L/100km | CAN: 2 1F
```

Al girar el potenciómetro deben cambiar tanto `ADC` como `Consumo`.

## Dependencias

El firmware utiliza:

```cpp
#include <SPI.h>
#include <mcp_can.h>
```

Es necesario instalar `MCP_CAN_lib`.

## Terminación CAN

El bus CAN debe tener una resistencia de `120 Ω` en cada extremo.

Con ambos extremos conectados y el sistema apagado:

```text
CANH ↔ CANL ≈ 60 Ω
```

## Diagnóstico

Si aparece:

```text
ERROR enviando CAN
```

comprobar:

```text
MCP2515 VCC ↔ GND      ≈ 5 V
Nano GND ↔ MCP GND     continuidad
CANH ↔ CANL            ≈ 60 Ω con ambos nodos conectados
```

También verificar:

- CANH conectado a CANH.
- CANL conectado a CANL.
- GND común.
- Ambos nodos a 500 kbit/s.
- Frecuencia correcta del cristal MCP2515.
- ESP32/SN65HVD230 conectado y funcionando.

## Objetivo del potenciómetro

El potenciómetro **no mide el consumo real del vehículo**.

Su función es permitir probar la cadena:

```text
Valor analógico
      ↓
Arduino
      ↓
CAN
      ↓
ESP32
      ↓
MQTT
```

sin depender todavía de una ECU o de un vehículo real.

Esto facilita el desarrollo y depuración de DriveCost antes de integrar OBD-II.
