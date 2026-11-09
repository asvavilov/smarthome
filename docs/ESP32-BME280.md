# ESP32 DevKit + BME280 (ESPHome)

Этап 1. Минимальный узел датчиков: ESP32 читает BME280 и отдаёт данные в Home Assistant.

## Подключение

| BME280 | ESP32 DevKit |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

- Питание датчика — **3.3 В**.
- I2C-адрес: **0x76** (по умолчанию) или **0x77** (перемычка SDO).
- На модуле GY-BME280 обычно уже есть подтяжки — дополнительные не нужны.

## Прошивка (ESPHome)

1. Установить ESPHome: CLI, веб-дашборд или аддон в HA.
2. Создать `secrets.yaml`:

```yaml
wifi_ssid: "ваш_ssid"
wifi_password: "ваш_пароль"
```

3. Конфиг `smarthome-node.yaml`:

```yaml
esphome:
  name: smarthome-node
  friendly_name: "Smarthome Node"

esp32:
  board: esp32dev
  framework:
    type: arduino

logger:
  level: INFO

api:

ota:
  - platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  ap:
    ssid: "smarthome-node Fallback"
    password: "changeme123"

captive_portal:

i2c:
  id: bus_a
  sda: GPIO21
  scl: GPIO22
  scan: true

sensor:
  - platform: bme280_i2c
    i2c_id: bus_a
    address: 0x76
    update_interval: 60s
    temperature:
      name: "BME280 Temperature"
    humidity:
      name: "BME280 Humidity"
    pressure:
      name: "BME280 Pressure"
```

4. Первая прошивка — **по USB**:

```bash
esphome run smarthome-node.yaml
```

Дальше — обновления по воздуху (**OTA**) через Wi-Fi.

## Home Assistant

- В HA добавить интеграцию **ESPHome**.
- Узел обычно обнаруживается сам (mDNS). Если нет — добавить вручную по IP.
- Появятся сущности: температура, влажность, давление.
- Добавить их на дашборд.

**Важно:** VM HA в Proxmox должна быть в **bridged-сети**, иначе автообнаружение не работает.

## Проверка

- В логах ESPHome: `i2c scan` должен показать `0x76`.
- В HA: значения T/H/P обновляются (раз в 60 с).
- Если датчик не найден — проверить питание, SDA/SCL, адрес.

## Замечания

- BME280 измеряет T/H/P; для улицы нужен радиационный щиток.
- Длинный I2C-кабель (≥ 2–3 м) нестабилен — для улицы лучше отдельный Wi-Fi-узел.
- ESP32 питается от USB (5 В), датчик — от 3.3 В платы.
