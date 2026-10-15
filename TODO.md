# TODO

Форк под российские условия и K803 (полная версия, с IMU).
Задачи выполняются по одной.

---

## 1. Убрать европейские ссылки и локальные привязки

Найдено: 8 мест в `README.md`, 1 в `HARDWARE.md`, ссылки на `hmd83.github.io`.
Просто удаляю неприменимое, не адаптирую под Россию.

- [x] `README.md` — шапка: `hmd83.github.io/...` → относительные пути `install/`, `web/rtk-monitor.html`
- [x] `README.md` — убрать цену `40–90 €`
- [x] `README.md` — убрать `under 100 €`
- [x] `README.md` — обезличить «Test 1 — Bühl, 22 Aug 2026»
- [x] `README.md` — убрать «this part of Germany is 47–50 m»
- [x] `README.md` — убрать таблицу стоимости в € (столбец Cost удалён)
- [x] `README.md` — убрать NTRIP: `Centipede-RTK` / `RTK2go`
- [x] `HARDWARE.md` — убрать `for 50 €`
- [x] `FLASHING.md` — `hmd83` → `AlksSAV` (git clone, installer, dashboard)
- [x] `install/index.html` — `hmd83` → `AlksSAV` (GitHub, clone, footer)
- [x] Проверить `web/rtk-monitor.html` — ссылки на Google Maps и HTTPS-контекст допустимы
- [x] Медиафайлы не трогать

**Не трогаем:** ссылки на Seeed Studio, NimBLE-Arduino, pioarduino, ESP Web Tools,
Silicon Labs / WCH драйверы — это инструменты и комплектующие.

---

## 2. K803 Lite → K803 (полная версия, с IMU)

Проверено по ComNav OEM Board Reference Manual V3.1:
- **RTKOBSMODE** и **RTKDYNAMICS** — НЕ НАЙДЕНЫ в V3.1 (были в Lite)
- Вместо `RTKDYNAMICS FOOT` → `SET APPSCENE Survey` (Survey/Robot/Car/Uav/Air/Space)
- `INTERFACEMODE SAVECONFIG` → по V3.2.22 корректно: `SAVECONFIG`
- Полная K803 имеет IMU: `INSCONTROL enable`, `SET SMOOTHEDDR on/off`, `SET DRCONFIG`
- Плата только **XIAO ESP32C6** — убраны DevKitC/SPP builds из platformio.ini и доков

- [x] `gnss_init.cpp`: `RTKOBSMODE 0` → `SET APPSCENE Survey` ✅
- [x] `gnss_init.cpp`: `RTKDYNAMICS FOOT` → `SET APPSCENE Survey` ✅
- [x] `gnss_init.cpp`: `INTERFACEMODE SAVECONFIG` → `SAVECONFIG` ✅
- [x] `gnss_init.cpp`: добавлены `INSCONTROL ENABLE` и `SET SMOOTHEDDR ON` для IMU
- [x] `config.h`: «Lite» убрано, пины обновлены под XIAO, SPP-секция удалена
- [x] `platformio.ini`: убраны env `esp32dev_spp` и `esp32dev_ble`, только `xiao_esp32c6_ble`
- [x] `README.md`: «K803 Lite» → «K803», добавлен IMU, убран DevKitC/SPP
- [x] `HARDWARE.md`: убран «Lite», добавлена секция «K803 vs K803 Lite», убран DevKitC wiring
- [x] `FLASHING.md`: убраны DevKitC инструкции, только XIAO + platformio
- [x] `src/link_spp.cpp`: удалён (не нужен)
- [x] `install/index.html`: убрана DevKitC секция, «K803 Lite» → «K803»
- [x] `src/link.h`: обновлён комментарий, убран SPP

- [ ] `COMMANDS.md`: обновить команды, убрать непроверенные, добавить IMU-команды
- [ ] `web/rtk-monitor.html`: проверить, нет ли упоминаний Lite/SPP/DevKitC
- ⚠️ **Не менять** физический разъём/пины до проверки конкретной платы —
      у полной K803 несущая плата и harness могут отличаться от Lite

---

## 3. Замечания по коду

Отдельный проход после пунктов 1–2. Предварительно:

- [ ] `gnss_init.cpp:sendCmd()` — `resp[512]`: `LOG VERSION` на полной K803
      может быть длиннее; проверить переполнение (символы молча теряются)
- [ ] `gnss_init.cpp` — `containsNoCase()` ищет `ERROR` где угодно, в т.ч. внутри
      `ERRORCODE 0`; возможен ложный `REJECTED`
- [ ] `gnss_init.cpp` — `INTERFACEMODE SAVECONFIG` как последняя строка: по
      справочнику корректный вид — `SAVECONFIG` ( COMMANDS.md сам это отмечает)
- [ ] `gnss_init.cpp` — `UNLOGALL COM2/COM3` захардкожены, а `LOG ...` идут через
      `GNSS_CMD_PORT`: при переносе на COM2 последовательность рассинхронизируется
- [ ] `main.cpp:pumpUplink()` — `millis()` при переполнении: `(millis() - lastByte)`
      корректно, но `if (!linkReady())` сбрасывает `len` и теряет накопленный хвост
- [ ] `main.cpp:pumpDownlink()` — не проверяется `linkReady()`, байты из
      stream buffer могут уйти в UART после отключения телефона
- [ ] `link_ble.cpp` — `downlink` создаётся без проверки на `nullptr`
- [ ] `link_ble.cpp:onWrite()` — `xStreamBufferSend(..., 0)`: при переполнении
      буфера RTCM-пакет теряется целиком, без какого-либо признака
- [ ] `link_spp.cpp:linkWrite()` — `SerialBT.write()` может вернуть частичную
      запись; данные теряются без ретрая
- [ ] `.gitignore` — проверить, что `ComNav_OEM_Board_Reference_Manual_V3.1.pdf`
      стоит коммитить (крупный бинарный файл в репозитории)

---

## 4. Мелочи

- [ ] `README.md` — раздел «Credits»: убрать/переписать упоминание оригинального
      автора и Claude, если форк ведётся независимо
- [ ] `LICENSE` — проверить, нужна ли строка автора upstream
- [ ] Медиа: `media/*.jpg` с метаданными (гео, время) — в форке уже убрали
      тестовые точки, но сами фото могут содержать EXIF
- [ ] `README.md` — поле `Instrument Height` и заметка про SW Maps: проверить
      актуальность для используемой версии приложения
