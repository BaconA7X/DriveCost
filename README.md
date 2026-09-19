# DriveCost

Sistema de monitorización del consumo de combustible de un vehículo en tiempo real mediante OBD-II, ESP32, GPS y MQTT.

DriveCost obtiene datos del vehículo, calcula el consumo instantáneo y permite expresarlo no solo en L/100 km, sino también en €/100 km utilizando precios actualizados de combustible.

Además, el sistema podrá localizar gasolineras cercanas, consultar sus precios y mostrar su ubicación sobre un mapa.

---

## Descripción

DriveCost es un sistema IoT orientado a vehículos que obtiene información directamente desde el bus CAN del coche mediante el conector OBD-II.

Un ESP32 actúa como nodo de adquisición y se encarga de:

- Leer información OBD-II mediante CAN.
- Obtener la posición GPS del vehículo.
- Calcular o transmitir los datos necesarios para obtener el consumo.
- Enviar la telemetría mediante MQTT.

Posteriormente, un backend recibe los datos y los combina con información externa sobre precios de combustible y localización de gasolineras.

---

## Objetivos

Los principales objetivos del proyecto son:

- Obtener datos del vehículo en tiempo real.
- Leer velocidad, RPM y parámetros relacionados con el consumo.
- Calcular el consumo en L/100 km.
- Convertir el consumo a €/100 km.
- Obtener la posición GPS del vehículo.
- Consultar precios actualizados de gasolineras cercanas.
- Mostrar gasolineras sobre un mapa.
- Permitir la navegación hasta una gasolinera seleccionada.
- Transmitir los datos mediante MQTT.

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
              ┌────┴─────┐
              │          │
             CAN        GPS
              │        NEO-6M
              │
              └────┬─────┘
                   │
                 Wi-Fi
                   │
                  MQTT
                   │
                   ▼
                Backend
          ┌────────┼─────────┐
          │        │         │
      Consumo   Precios     Mapa
      €/100km  gasolina     / rutas