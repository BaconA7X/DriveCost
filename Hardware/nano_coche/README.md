# DriveCost — Arduino Nano + MCP2515

El Arduino Nano es el nodo encargado de obtener información CAN del vehículo y transmitir los datos necesarios al ESP32-P4.

La comunicación CAN se realiza mediante un controlador **MCP2515**.

## Arquitectura

```text
Vehículo / OBD-II
       │
       │ CAN
       ▼
┌──────────────┐
│ Arduino Nano │
│      +       │
│   MCP2515    │
└──────┬───────┘
       │
       │ CAN
       ▼
┌──────────────┐
│ SN65HVD230   │
│      +       │
│  ESP32-P4    │
└──────────────┘
```

## Conexión Arduino Nano ↔ MCP2515

```text
Arduino Nano          MCP2515
─────────────────────────────
5V        ─────────── VCC
GND       ─────────── GND
D10       ─────────── CS
D11       ─────────── SI / MOSI
D12       ─────────── SO / MISO
D13       ─────────── SCK
```

El pin `INT` no es necesario para la transmisión utilizada actualmente.

## SPI

En el Arduino Nano clásico:

```text
D10 → CS
D11 → MOSI
D12 → MISO
D13 → SCK
```

## Frecuencia del MCP2515

Es importante comprobar la frecuencia del cristal del módulo.

Normalmente estará marcada físicamente como:

```text
8.000
```

o:

```text
16.000
```

La configuración del firmware debe coincidir.

Para 8 MHz:

```cpp
CAN.begin(
    MCP_ANY,
    CAN_500KBPS,
    MCP_8MHZ
);
```

Para 16 MHz:

```cpp
CAN.begin(
    MCP_ANY,
    CAN_500KBPS,
    MCP_16MHZ
);
```

Una frecuencia incorrecta puede hacer que el MCP2515 se inicialice pero sea incapaz de comunicarse correctamente con otros nodos CAN.

## Bus CAN

Conectar:

```text
MCP2515               ESP32/SN65HVD230
──────────────────────────────────────
CANH ═════════════════ CANH
CANL ═════════════════ CANL
GND  ───────────────── GND
```

Todos los nodos deben compartir una referencia de masa.

## Velocidad

DriveCost utiliza:

```text
500 kbit/s
```

Todos los nodos del bus deben utilizar la misma velocidad.

## Terminación

CAN requiere una resistencia de:

```text
120 Ω
```

en cada extremo físico del bus.

Con las dos terminaciones instaladas:

```text
120 Ω                         120 Ω
 │                              │
CANH ═════════════════════════ CANH
CANL ═════════════════════════ CANL
```

Con todo apagado, medir entre CANH y CANL debería dar aproximadamente:

```text
60 Ω
```

## Trama de consumo

DriveCost utiliza el identificador:

```text
0x100
```

El consumo se codifica utilizando los primeros dos bytes.

Ejemplo para:

```text
6.42 L/100km
```

Primero se multiplica por 100:

```text
6.42 × 100 = 642
```

Se transmite como entero de 16 bits:

```cpp
uint16_t valor = consumo * 100;

datos[0] = (valor >> 8) & 0xFF;
datos[1] = valor & 0xFF;
```

El ESP32 reconstruye posteriormente el valor.

## Dependencias

El firmware utiliza:

```cpp
#include <SPI.h>
#include <mcp_can.h>
```

Instalar:

```text
MCP_CAN_lib
```

de Cory J. Fowler.

## Diagnóstico

Si aparece:

```text
ERROR enviando CAN
```

comprobar:

```text
MCP2515 VCC ↔ GND        → aproximadamente 5 V
Nano GND ↔ MCP2515 GND   → continuidad
CANH ↔ CANL              → aproximadamente 60 Ω
```

También comprobar:

- Misma velocidad CAN en ambos nodos.
- Frecuencia correcta del cristal MCP2515.
- CANH conectado con CANH.
- CANL conectado con CANL.
- Masa común.
- Existencia de otro nodo activo que pueda proporcionar ACK.