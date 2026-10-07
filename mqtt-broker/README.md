# DriveCost — MQTT Broker

DriveCost utiliza **Eclipse Mosquitto** como broker MQTT para recibir la telemetría y el estado de conexión enviados por el ESP32.

La arquitectura es:

```text
ESP32-P4
   │
   │ Wi-Fi
   │
   │ MQTT
   │ usuario + contraseña
   │
   ├── drivecost/telemetry   (posición + consumo)
   └── drivecost/status      (online / offline, LWT)
   ▼
┌─────────────────┐
│    Mosquitto    │
│      :1883      │
└────────┬────────┘
         │
         ├── Node-RED
         └── otros consumidores
```

## Instalación

En Ubuntu:

```bash
sudo apt update
sudo apt install mosquitto mosquitto-clients
```

Comprobar:

```bash
sudo systemctl status mosquitto
```

## Crear usuario

Crear el usuario `drivecost`:

```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd drivecost
```

Introducir una contraseña cuando sea solicitada.

Proteger el archivo:

```bash
sudo chown mosquitto:mosquitto /etc/mosquitto/passwd
sudo chmod 600 /etc/mosquitto/passwd
```

No subir `/etc/mosquitto/passwd` ni las contraseñas al repositorio.

## Configuración

Crear:

```bash
sudo nano /etc/mosquitto/conf.d/drivecost.conf
```

Contenido:

```text
listener 1883

allow_anonymous false

password_file /etc/mosquitto/passwd
```

Reiniciar:

```bash
sudo systemctl restart mosquitto
```

Comprobar:

```bash
sudo systemctl status mosquitto
```

El servicio debe aparecer como:

```text
active (running)
```

La configuración por defecto de Mosquitto en Ubuntu incluye `persistence true`, por lo que los mensajes retenidos se conservan aunque se reinicie el broker.

## Topics

| Topic | Publica | QoS | Retenido | Contenido |
|---|---|---|---|---|
| `drivecost/telemetry` | ESP32, cada segundo | 0 | Sí | Posición y consumo |
| `drivecost/status` | ESP32 (`online`) y broker (`offline`) | 0 / 1 | Sí | Estado de conexión del vehículo |

Ambos topics son **retenidos**: un consumidor que se conecte tarde recibe al momento la última telemetría y el último estado. Como la telemetría retenida puede ser antigua, `drivecost/status` indica si el vehículo sigue conectado.

El ESP32 solo puede publicar con QoS 0, ya que la librería `PubSubClient` no permite otro nivel. El mensaje `offline` llega con QoS 1 porque lo publica el broker.

## Payload de telemetría

Topic:

```text
drivecost/telemetry
```

Formato JSON:

```json
{
  "latitud": 40.416775,
  "longitud": -3.703790,
  "consumo": 6.42
}
```

Donde:

```text
latitud  → grados decimales
longitud → grados decimales
consumo  → L/100km
```

## Payload de estado

Topic:

```text
drivecost/status
```

Formato JSON:

```json
{"estado":"online"}
```

```json
{"estado":"offline"}
```

Donde:

```text
online  → lo publica el ESP32 al conectarse
offline → lo publica el broker si el ESP32 desaparece (Last Will)
```

## Last Will and Testament

El ESP32 registra un **Last Will** al conectarse al broker:

```text
Topic:    drivecost/status
Mensaje:  {"estado":"offline"}
QoS:      1
Retenido: sí
```

Funcionamiento:

```text
Al conectar   → el ESP32 registra el Last Will
                y publica {"estado":"online"} (retenido)

Si se cae     → el broker publica {"estado":"offline"}
                tras 1,5 × keep alive

Al reconectar → {"estado":"online"} sobrescribe
                el offline retenido
```

El ESP32 usa un keep alive de `10 s`, por lo que la desconexión se detecta en unos `15 s`.

El Last Will **solo se publica si la conexión se corta sin DISCONNECT**, por ejemplo al desenchufar el ESP32 o al perder la Wi-Fi.

El Client ID es fijo y se obtiene de la MAC del ESP32:

```text
DriveCost-<MAC>
```

Con un Client ID aleatorio, tras un reinicio la conexión antigua seguiría abierta y su `offline` llegaría después del `online` nuevo.

## Probar autenticación

Una conexión sin credenciales debería ser rechazada:

```bash
mosquitto_sub \
    -h localhost \
    -p 1883 \
    -t "drivecost/#"
```

Con autenticación:

```bash
mosquitto_sub \
    -h localhost \
    -p 1883 \
    -u drivecost \
    -P 'TU_PASSWORD' \
    -t "drivecost/#" \
    -v
```

Cuando el ESP32 se conecte deberían aparecer mensajes similares a:

```text
drivecost/status {"estado":"online"}
drivecost/telemetry {"latitud":40.416775,"longitud":-3.703790,"consumo":6.42}
drivecost/telemetry {"latitud":40.416781,"longitud":-3.703795,"consumo":6.38}
```

## Probar el Last Will

Con el `mosquitto_sub` anterior abierto, desconectar la alimentación del ESP32.

Tras unos `15 s` debe aparecer:

```text
drivecost/status {"estado":"offline"}
```

Al volver a conectarlo:

```text
drivecost/status {"estado":"online"}
```

## Publicación manual

También puede comprobarse el broker publicando manualmente.

Telemetría:

```bash
mosquitto_pub \
    -h localhost \
    -p 1883 \
    -u drivecost \
    -P 'TU_PASSWORD' \
    -t "drivecost/telemetry" \
    -m '{"latitud":40.416775,"longitud":-3.703790,"consumo":6.42}'
```

Estado:

```bash
mosquitto_pub \
    -h localhost \
    -p 1883 \
    -u drivecost \
    -P 'TU_PASSWORD' \
    -t "drivecost/status" \
    -r \
    -q 1 \
    -m '{"estado":"offline"}'
```

## Borrar un mensaje retenido

Para eliminar el mensaje retenido de un topic se publica un mensaje vacío con `-r`:

```bash
mosquitto_pub \
    -h localhost \
    -p 1883 \
    -u drivecost \
    -P 'TU_PASSWORD' \
    -t "drivecost/status" \
    -r \
    -n
```

## Obtener IP del broker

El ESP32 necesita la IP del ordenador donde se ejecuta Mosquitto.

En Linux:

```bash
hostname -I
```

Ejemplo:

```text
192.168.1.34
```

En el firmware del ESP32:

```cpp
const char* MQTT_SERVER = "192.168.1.34";
const int MQTT_PORT = 1883;
```

No utilizar `localhost` en el ESP32, ya que `localhost` haría referencia al propio ESP32.

## Firewall

Si el ESP32 no consigue conectarse y Mosquitto funciona localmente, comprobar que el puerto TCP `1883` es accesible desde la red local.

Si UFW está habilitado:

```bash
sudo ufw allow 1883/tcp
```

## Seguridad

La configuración actual proporciona **autenticación**, pero MQTT en el puerto `1883` no cifra el tráfico.

Para un entorno de desarrollo dentro de una red local controlada puede ser suficiente.

Para despliegues reales se recomienda:

```text
MQTT + TLS
Puerto 8883
Usuario + contraseña
ACL por dispositivo
```

También se recomienda limitar al usuario `drivecost` para que únicamente pueda publicar en los topics que necesite.

## Logs

Para diagnosticar problemas:

```bash
sudo journalctl -u mosquitto -f
```

Esto permite observar conexiones, desconexiones y errores de autenticación en tiempo real.
