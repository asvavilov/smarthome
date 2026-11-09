# smarthome

Домашняя метеостанция и панель умного дома (в разработке).

## Концепция (v1)

Терминал на планшете + беспроводной узел датчиков + Home Assistant.

| Компонент | Роль |
|---|---|
| Acer Iconia Tab W501 (Linux) | сенсорный терминал, дашборд HA, браузер |
| ESP32 DevKit + BME280 | узел датчиков (T/H/P), ESPHome, Wi-Fi |
| Home Assistant (Proxmox VM) | сервер, история, автоматизации, дашборд |
| T-Watch-S3 Plus (LoRa 868 МГц) | опционально: носимый терминал |

## Документация

- [docs/PLAN.md](docs/PLAN.md) — план по этапам и текущий фокус;
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — схема и поток данных;
- [docs/TABLET-LINUX.md](docs/TABLET-LINUX.md) — оживление планшета (этап 0);
- [docs/ESP32-BME280.md](docs/ESP32-BME280.md) — узел датчиков и ESPHome.

## Текущий статус

**Этап 0 — планшет.** Сейчас на нём нерабочая Ubuntu, планируется переустановка Linux.
См. [docs/TABLET-LINUX.md](docs/TABLET-LINUX.md).

## Легаси: первая версия на отдельном экране

Исходный вариант: ESP32 + отдельный дисплей ILI9488 3.5" 480×320, LVGL 8.1 + TFT_eSPI.

- ESP32 DOIT DevKit v1 (`platformio.ini`);
- ILI9488 480×320 + тач XPT2046 (пины в `lib_conf/Setup21_ILI9488.h`);
- BME280;
- прошивка: `src/main.cpp`.

Оставлено как справка и площадка для экспериментов; в v1 не используется.

## fonts

В `resources` расположены исходные шрифты.

- документация: https://docs.lvgl.io/latest/en/html/overview/font.html
- конвертер: https://lvgl.io/tools/fontconverter
