# DriveCost — MQTT Broker

DriveCost utiliza **Eclipse Mosquitto** como broker MQTT para recibir la telemetría enviada por el ESP32.

La arquitectura es:

```text
ESP32-P4
   │
   │ Wi-Fi
   │
   │ MQTT
   │ usuario + contraseña
   ▼
┌─────────────────┐
│    Mosquitto    │
│      :1883      │
└────────┬────────┘
         │
         ├── Node-RED
         ├── Python
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

## Topic

La telemetría se publica en:

```text
drivecost/telemetry
```

## Payload

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

## Probar autenticación

Una conexión sin credenciales debería ser rechazada:

```bash
mosquitto_sub \
    -h localhost \
    -p 1883 \
    -t "drivecost/telemetry"
```

Con autenticación:

```bash
mosquitto_sub \
    -h localhost \
    -p 1883 \
    -u drivecost \
    -P 'SistemasDistribuidos2026' \
    -t "drivecost/telemetry" \
    -v
```

Cuando el ESP32 publique datos deberían aparecer mensajes similares a:

```text
drivecost/telemetry {"latitud":40.416775,"longitud":-3.703790,"consumo":6.42}
drivecost/telemetry {"latitud":40.416781,"longitud":-3.703795,"consumo":6.38}
```

## Publicación manual

También puede comprobarse el broker publicando manualmente:

```bash
mosquitto_pub \
    -h localhost \
    -p 1883 \
    -u drivecost \
    -P 'SistemasDistribuidos2026' \
    -t "drivecost/telemetry" \
    -m '{"latitud":40.416775,"longitud":-3.703790,"consumo":6.42}'
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
