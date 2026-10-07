# DriveCost — Node-RED

Node-RED actúa como **consumidor y procesador** de la telemetría de DriveCost.

Sus funciones son:

- Suscribirse al topic `drivecost/telemetry` del broker Mosquitto.
- Validar la posición GPS y el consumo recibidos.
- Descargar los precios oficiales de carburantes de España.
- Calcular las 3 gasolineras más cercanas al vehículo y su coste en €/100 km.
- Mostrar el resultado en un dashboard y en un mapa.

## Arquitectura

```text
ESP32-P4
   │
   │ MQTT  drivecost/telemetry
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
```

Así el orden en que llegue cada dato no importa.

## Nodos principales

| Nodo | Función |
|---|---|
| `Ubicación MQTT` | Suscripción a `drivecost/telemetry` |
| `Guardar GPS + consumo` | Valida el JSON y guarda posición y consumo |
| `API precios carburantes` | Descarga estaciones y precios del Ministerio |
| `Guardar precios en caché` | Guarda la lista de estaciones en memoria |
| `Guardar combustible` | Guarda el combustible elegido en el dashboard |
| `Calcular TOP 3` | Calcula distancias, selecciona y calcula costes |
| `Preparar Worldmap` | Genera los marcadores del mapa |
| `Dashboard gasolineras` | Tarjetas, selector de combustible y mapa incrustado |

El nodo `TEST MQTT DriveCost` inyecta un mensaje de ejemplo directamente en el flow, sin pasar por el broker.

## Mensaje de telemetría

El flow espera el mismo JSON que publica el ESP32:

```json
{
  "latitud": 40.416775,
  "longitud": -3.703790,
  "consumo": 6.42
}
```

Se descartan los mensajes que no sean JSON válido, con coordenadas fuera de rango o con consumo no numérico o negativo.

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

## Configuración MQTT

| Parámetro | Valor | Motivo |
|---|---|---|
| Topic | `drivecost/telemetry` | Topic único de telemetría |
| QoS de suscripción | 0 | Cada muestra sustituye a la anterior |
| Clean session | Sí | Con QoS 0 no hay mensajes que encolar |
| Retained | No | Entregaría una posición antigua como actual |
| Keep alive | 60 s | Valor por defecto del nodo |
| Versión | MQTT 3.1.1 | La que usan Mosquitto y `PubSubClient` |

El QoS efectivo es el **menor** entre el de publicación y el de suscripción. El ESP32 publica con QoS 0, ya que `PubSubClient` no permite otro nivel.

Las credenciales no se exportan con el flow: hay que introducirlas en el broker `BROKER MQTT` (usuario `drivecost`, contraseña `TU_PASSWORD`).

## Dashboard

```text
Dashboard: http://localhost:1880/dashboard/gasolineras
Mapa:      http://localhost:1880/gasolineras-map
```

Muestra el consumo actual, un selector de combustible, tres tarjetas con precio, coste, distancia y dirección, y un mapa con el vehículo y las gasolineras numeradas.

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
