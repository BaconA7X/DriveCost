# DriveCost — Node-RED

Node-RED actúa como **consumidor y procesador** de la telemetría de DriveCost.

Sus funciones son:

- Suscribirse a `drivecost/telemetry` para recibir la posición y el consumo.
- Suscribirse a `drivecost/status` para conocer si el vehículo está conectado.
- Validar los datos recibidos.
- Descargar los precios oficiales de carburantes de España.
- Calcular las 3 gasolineras más cercanas al vehículo y su coste en €/100 km.
- Mostrar el resultado en un dashboard y en un mapa.

## Arquitectura

```text
ESP32-P4
   │
   │ MQTT
   ├── drivecost/telemetry   (posición + consumo)
   └── drivecost/status      (online / offline, LWT)
   ▼
┌─────────────┐
│  Mosquitto  │
└──────┬──────┘
       │
       ▼
┌────────────────┐        API precios carburantes
│    Node-RED    │◄────── (descarga cada 15 min)
└───────┬────────┘
        │
   ┌────┴─────┐
   ▼          ▼
Dashboard   Mapa
```

Las entradas del flow no se pasan los datos entre sí. Cada una guarda su información en el **flow context** y dispara el cálculo, que lee el estado completo:

```text
Telemetría MQTT ──► gps, consumo ────┐
Precios (15 min) ─► estaciones ──────┤
Selector (dashboard) ─► combustible ─┼──► Calcular TOP 3 ──► Dashboard + Mapa
Mapa abierto ─► redibujar ───────────┘

Estado MQTT ──► Validar estado ──► LED de conexión
```

Así el orden en que llegue cada dato no importa. El estado del vehículo sigue un camino independiente y no interviene en el cálculo.

## Nodos principales

| Nodo | Función |
|---|---|
| `Ubicación MQTT` | Suscripción a `drivecost/telemetry` |
| `Guardar GPS + consumo` | Valida el JSON y guarda posición y consumo |
| `Estado vehículo (LWT)` | Suscripción a `drivecost/status` |
| `Validar estado` | Acepta solo `online` / `offline` |
| `LED estado vehículo` | Indicador de conexión en el dashboard |
| `API precios carburantes` | Descarga estaciones y precios del Ministerio |
| `Guardar precios en caché` | Guarda la lista de estaciones en memoria |
| `Guardar combustible` | Guarda el combustible elegido en el dashboard |
| `Calcular TOP 3` | Calcula distancias, selecciona y calcula costes |
| `Preparar Worldmap` | Genera los marcadores del mapa |
| `Dashboard gasolineras` | Tarjetas, selector de combustible y mapa incrustado |

El nodo `TEST MQTT DriveCost` inyecta un mensaje de ejemplo directamente en el flow, sin pasar por el broker.

## Mensajes

Telemetría, en `drivecost/telemetry`:

```json
{
  "latitud": 40.416775,
  "longitud": -3.703790,
  "consumo": 6.42
}
```

Se descartan los mensajes que no sean JSON válido, con coordenadas fuera de rango o con consumo no numérico o negativo.

Estado, en `drivecost/status`:

```json
{"estado":"online"}
```

```json
{"estado":"offline"}
```

Se descarta cualquier otro valor.

## Decisiones MQTT

| Topic | QoS suscripción | Retenido | Motivo |
|---|---|---|---|
| `drivecost/telemetry` | 0 | Sí | Flujo continuo: cada muestra sustituye a la anterior |
| `drivecost/status` | 1 | Sí | Estado: no debe perderse y debe conocerse al conectarse |

### QoS

- **Telemetría con QoS 0 (at-most-once).** Llega una muestra nueva cada segundo, así que perder una no tiene efecto. QoS 1 solo añadiría confirmaciones y posibles duplicados.
- **Estado con QoS 1 (at-least-once).** Un `offline` perdido dejaría el LED en verde con el vehículo caído. Un duplicado no tiene efecto, porque solo sobrescribe el mismo valor.
- **QoS 2 no se usa.** Su intercambio de 4 mensajes solo compensa cuando un duplicado tiene consecuencias, y aquí no las tiene.

El QoS efectivo es el **menor** entre el de publicación y el de suscripción. El ESP32 publica con QoS 0, ya que `PubSubClient` no permite otro nivel. El `offline` llega con QoS 1 porque lo publica el broker.

### Mensajes retenidos

Los dos topics son retenidos, de modo que Node-RED recibe el último valor nada más suscribirse:

- **Telemetría:** al desplegar el flow o reiniciar Node-RED, el dashboard muestra la última posición conocida sin esperar a la siguiente muestra.
- **Estado:** el LED muestra el estado real desde el primer momento.

La telemetría retenida puede ser antigua. El estado retenido resuelve esa ambigüedad: si el LED está en rojo, los datos mostrados son los últimos conocidos y no los actuales.

### Last Will and Testament

El `offline` lo publica el **broker**, no Node-RED. El ESP32 lo registra como Last Will al conectarse, y Mosquitto lo publica si la conexión se corta sin DISCONNECT (desconexión, pérdida de Wi-Fi). Con el keep alive de `10 s` del ESP32, se detecta en unos `15 s`.

Node-RED solo tiene que suscribirse al topic. No necesita sondear al vehículo ni medir tiempos.

### Sesión

| Parámetro | Valor | Motivo |
|---|---|---|
| Clean session | Sí | Los retenidos ya entregan el último estado; una sesión persistente no aportaría nada |
| Keep alive | 60 s | Valor por defecto del nodo |
| Versión | MQTT 3.1.1 | La que usan Mosquitto y `PubSubClient` |

Las credenciales no se exportan con el flow: hay que introducirlas en el broker `BROKER MQTT` (usuario `drivecost`, contraseña `TU_PASSWORD`).

## Cálculo del TOP 3

1. Se calcula la distancia a cada estación con la fórmula de **Haversine**.
2. Se ordenan por distancia y se seleccionan las 3 más cercanas.
3. Se comprueba si cada una vende el combustible elegido.
4. Si el combustible se mide en €/L, se calcula el coste:

```text
coste (€/100 km) = consumo (L/100 km) × precio (€/L)
```

Para los combustibles en €/kg (GNC, GNL, hidrógeno) no se calcula el coste, porque el consumo llega en L/100 km.

El TOP 3 se calcula **solo por distancia**. Si una estación no publica precio para el combustible elegido, se muestra como no disponible.

## Dashboard

```text
Dashboard: http://localhost:1880/dashboard/gasolineras
Mapa:      http://localhost:1880/gasolineras-map
```

Muestra:

- Un LED de conexión del vehículo: verde (conectado), rojo (desconectado) o gris (sin datos).
- El consumo actual y un selector de combustible.
- Tres tarjetas con precio, coste, distancia y dirección.
- Un mapa con el vehículo y las gasolineras numeradas.

## Dependencias

```text
@flowfuse/node-red-dashboard      1.31.0
node-red-contrib-web-worldmap     5.8.1
```

`@flowfuse/node-red-dashboard` es Dashboard 2.0, distinto del antiguo `node-red-dashboard`.

## Datos de precios

API REST oficial de precios de carburantes del Ministerio:

```text
https://energia.serviciosmin.gob.es/ServiciosRestCarburantes/PreciosCarburantes/EstacionesTerrestres/
```

Se descarga al desplegar el flow y cada 15 minutos.
