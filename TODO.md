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
      → **отменено в пункте 6** (на обычной K803 tilt compensation нет, DR не нужен)
- [x] `config.h`: «Lite» убрано, пины обновлены под XIAO, SPP-секция удалена
- [x] `platformio.ini`: убраны env `esp32dev_spp` и `esp32dev_ble`, только `xiao_esp32c6_ble`
- [x] `README.md`: «K803 Lite» → «K803», добавлен IMU, убран DevKitC/SPP
- [x] `HARDWARE.md`: убран «Lite», добавлена секция «K803 vs K803 Lite», убран DevKitC wiring
- [x] `FLASHING.md`: убраны DevKitC инструкции, только XIAO + platformio
- [x] `src/link_spp.cpp`: удалён (не нужен)
- [x] `install/index.html`: убрана DevKitC секция, «K803 Lite» → «K803»
- [x] `src/link.h`: обновлён комментарий, убран SPP

- [x] `COMMANDS.md`: переписан под полную K803 — убраны `RTKOBSMODE`/`RTKDYNAMICS`/
      `INTERFACEMODE SAVECONFIG`, добавлены `SET APPSCENE`, IMU/DR (§4.5 V3.1),
      `LOG SYSCONFIG`, RTCM relay, lockout, PPS. Все ссылки — на разделы V3.1
- [x] `web/rtk-monitor.html`: проверено — упоминаний Lite/SPP/DevKitC/hmd83 нет
- ⚠️ **Не менять** физический разъём/пины до проверки конкретной платы —
      у полной K803 несущая плата и harness могут отличаться от Lite

---

## 3. Замечания по коду

Отдельный проход после пунктов 1–2. Предварительно:

- [x] `gnss_init.cpp:sendCmd()` — `resp[512]` → `resp[1024]`, LOG VERSION
      на полной K803 может быть длиннее
- [x] `gnss_init.cpp` — `containsNoCase("ERROR")` → `isRejected("Error!")`,
      ложный REJECTED на `ERRORCODE 0` исправлен
- [x] `gnss_init.cpp` — `INTERFACEMODE SAVECONFIG` → `SAVECONFIG` (пункт 2)
- [x] `gnss_init.cpp` — `UNLOGALL COM2/COM3` захардкожены, а `LOG ...` идут через
      `GNSS_CMD_PORT`: при переносе на COM2 последовательность рассинхронизируется
      → не баг: UNLOGALL для COM2/COM3 — безопасные no-op на любом порту
- [x] `main.cpp:pumpUplink()` — `millis()` при переполнении: `(millis() - lastByte)`
      корректно, но `if (!linkReady())` сбрасывает `len` и теряет накопленный хвост
      → не баг: drain при отсутствии телефона — обязательная защита от
      переполнения UART-буфера
- [x] `main.cpp:pumpDownlink()` — не проверяется `linkReady()`, байты из
      stream buffer могут уйти в UART после отключения телефона
      → решено: `xStreamBufferReset(downlink)` в `onDisconnect`
- [x] `link_ble.cpp` — `downlink` создаётся без проверки на `nullptr`
      → добавлены проверки в `onWrite` и `linkRead`
- [x] `link_ble.cpp:onWrite()` — `xStreamBufferSend(..., 0)`: при переполнении
      буфера RTCM-пакет теряется целиком, без какого-либо признака
      → можно оставить: non-blocking send — ожидаемое поведение, пакет
      потеряется только если буфер переполнился до drain'а; в штатном режиме
      буфер не переполняется
- [x] `link_spp.cpp:linkWrite()` — `SerialBT.write()` может вернуть частичную
      запись; данные теряются без ретрая
      → удалён в пункте 2 (не нужен)
- [x] `.gitignore` — проверить, что `ComNav_OEM_Board_Reference_Manual_V3.1.pdf`
      стоит коммитить (крупный бинарный файл в репозитории)
      → ✅ уже коммитится, не в .gitignore

---

## 4. Мелочи

- [x] `README.md` — раздел «Credits»: в текущем README такой секции нет,
      упоминаний оригинального автора и Claude тоже нет — нечего убирать
- [x] `LICENSE` — MIT требует сохранить исходный копирайт, поэтому строка
      `Hussein Daj` оставлена, добавлена вторая: `Copyright (c) 2026 AlksSAV`
- [x] Медиа: `media/*.jpg` с метаданными (гео, время) — **пропущено по решению**
- [x] `README.md` — поле `Instrument Height` и заметка про SW Maps: проверено,
      актуально — SW Maps вычитает высоту прибора и пишет точку на земле
- [x] `README.md` — ссылка на дашборд: `hmd83.github.io/...` → относительный путь
      `web/rtk-monitor.html`
- [x] `FLASHING.md` — пример `LOG VERSION`: `"K803Lite"` → `"K803"`

---

## 5. Tilt compensation — только как идея, под неё ничего не менять

Проверено по ComNav OEM Board Reference Manual V3.1:

- §4.6 Tilt Measurement (`insrtk rodlength`, `insrtk leverarmarp`, `insrtk leverarmgnss`,
  `insrtk installangle`, `insrtk fastinit`, `insrtk init 1`) и логи §3.4.8
  (`INSRTKCOMPENSATE` ID 2366 и др.) заявлены для другой платы.
  На нашей K803 их **нет**.
- `insrtk rodlength <mm>` — это ровно «длина вехи от острия до ARP», 0–20000 мм.
  Но команда относится к другой плате, на нашей не появится.
- ANTEX/модели фазового центра в прошивке приёмника нет вообще: `leverarmgnss` —
  это постоянный механический вектор ±500 мм, а не калибровка антенны.

Что остаётся на обычной K803: `LOG IMUDATAB` (ID 2264, сырые гир/акц, 100 Гц) —
заявлен для K803. Магнитометра в приёмнике нет.

### Почему не делаем

Компенсация наклона: `tip = PC − L·(sinθ·cosα, sinθ·sinα, cosθ)`.

- `θ` (наклон от вертикали) — из акселерометра, решается.
- `α` (азимут плоскости наклона в ENU) — узкое место. Магнитометр рядом с
  ground plane / ESP32 / батареей даёт 5–15° и нестабильную figure-8;
  курс от `GPVTG` есть только в движении; честное решение — EKF GNSS+IMU,
  это отдельный проект.

Бюджет ошибки при L = 2 м: 0.3° → 1 см, 1° → 3.5 см, 3° → 10 см;
без компенсации при 30° — 100 см. BNO085 по θ проходит, по α — на грани.

### Что делать вместо этого (штатный режим)

Держать веху вертикально и вводить точную длину в `Instrument Height` — SW Maps
сама вычтет её и запишет точку на земле. IMU при этом не даёт ничего.

### Если когда-нибудь возвращаться

1. Внешний магнитометр (BNO055 / BMM150 / QMC5883L) вынести шлейфом на 10–15 см от платы.
2. Комплементарный фильтр на ESP32-C6, источник θ — акселерометр.
3. Результат — подмена GGA в прошивке (lat/lon/alt = остриё), `Instrument Height = 0`
   в приложении; RTCM не трогать, RTK-фиксация не страдает.
4. Сырые θ/α/статус — отдельным `$PTNL,TILT`, чтобы в логе было видно, что позиция
   вычисленная, а не сырая.

---

## 6. Убрать INS/DR из загрузочной последовательности

- [x] `gnss_init.cpp`: убраны `INSCONTROL ENABLE` и `SET SMOOTHEDDR ON`

Причина: tilt compensation из V3.1 §4.6 на этой плате недоступна,
а DR для ровера-вехи с остановками точности не добавляет. Включённый INS местами
переводит GGA в quality `6` (GNSS/INS integrated) вместо честного `4` (RTK FIXED),
и GIS-приложение принимает DR-позицию как валидную. Отменяет пункт §2
«добавлены `INSCONTROL ENABLE` и `SET SMOOTHEDDR ON` для IMU».

- [x] `COMMANDS.md`: §«IMU / Dead-reckoning» — сняты ✅ с `INSCONTROL ENABLE` и
      `SET SMOOTHEDDR ON`, раздел переименован в «(not used)», добавлены `LOG IMUDATAB`,
      объяснение «почему не на вехе» и оговорка, что tilt compensation — это §4.6 другой платы
- [x] `HARDWARE.md`: переписан целиком — см. пункт 7
- [x] `HARDWARE.md`: bench test, шаг 4 — ожидание «две строки REJECTED» убрано
- [x] `README.md`: строка «an IMU (dead-reckoning)» убрана вместе с переписыванием «What it is»

---

## 7. Документация под отдельную OEM-плату K803

Исходный проект описывал разборку купола ECOVACS GOAT. У нас **отдельная OEM-плата K803**
(только K803, без QD-02 и без DC-DC модулей), питание то же (5 V/GND + TX/RX), антенна —
внешняя **Beitian BT-800D**, корпус — печатный, пока не сделан.

- [x] `HARDWARE.md` — переписан: удалены «What is inside the dome» и «Teardown»;
      добавлены «The board» + «What the K803 does not do», «Connections», «Antenna»
      (BT-800D: ARP=BAM, смещения фазового центра из реестра NGS, проверка питания LNA),
      «Housing» (TBD, только требования)
- [x] `README.md` — «What it is» про OEM-плату и Beitian BT-800D, таблица железа,
      предупреждение про антенну, описание HARDWARE.md в таблице доков
- [x] `COMMANDS.md` — убраны «mower», «QD-02 we dropped» и ссылки на источник, которого
      нет в репозитории: `comnav.md` и «FW 7.4.6». Метка 🟡 заменена на 🔷/⚠️,
      формат PPSCONTROL исправлен по V3.1 §2.2.15, `LOG SATVISA` заменён на
      `LOG SATXYZA`, `LOG SYSCONFIG` помечен как отсутствующий в V3.1,
      нумерация последовательности пересчитана (16 команд, ~12 с)
- [x] `install/index.html` — шапка: без ECOVACS; «Disconnect the GNSS harness» → «receiver wiring»
- [x] `FLASHING.md` — якоря на переименованные разделы HARDWARE.md, «harness» → «receiver wiring»,
      время инициализации 15 с → 12 с
- [x] `src/config.h` — комментарий про время инициализации (14 с → 12 с)
- [x] Медиа: удалены `listings-secondhand.jpg`, `rover-field.jpeg`,
      `teardown-board-harness.jpeg`, `pinout-k803-harness.jpeg` (последние два — фото платы
      апстрима: K803**Lite** + QinNav QD-02 + LTD-D751S, к нашей плате не относятся).
      Остались только скриншоты SW Maps.
- [x] Проверены все ссылки и якоря между `.md` и `.html` — 0 битых

### Осталось

- [ ] Фото платы: `board-top.jpg`, `board-bottom.jpg`, `board-pinout.jpg` — плейсхолдеры в HARDWARE.md
- [x] Фото антенны: `media/antenna-bt800d.jpg` — BT-800D, коннектор на кабеле — **TNC**
      (уточнено, добавлено в таблицу антенны HARDWARE.md)
- [ ] Замер напряжения на центральном контакте: `antenna-bias-voltage.jpg` — отложено,
      замер потом
- [x] Данные BT-800D взяты из реестра NGS, код `BEIBT800D`: диапазоны частот, ARP=BAM,
      смещения фазового центра L1 = 0.6 / 1.2 / 73.7 мм (север / восток / вверх)
- [ ] Раздел «Housing» — расписать после печати корпуса
- [x] `install/firmware/xiao-c6-ble.bin` — пересобран из `firmware.factory.bin`
      (см. §8): в новом бинарнике `INSCONTROL`/`SMOOTHEDDR` отсутствуют,
      `SET APPSCENE Survey` / `SAVECONFIG` / `UNLOGALL` / `LOG VERSION` присутствуют

---

## 8. Починка esptool в PlatformIO

Симптом: сборка падала на этапе создания `firmware.bin`:
`ModuleNotFoundError: No module named 'esptool'` из `penv\Scripts\esptool.exe`.

Причина: пакет `tool-esptoolpy` в `~/.platformio/packages/` был повреждён — в нём
остались только `package.json` и `tools.json`, исходники `esptool/` отсутствовали.
При этом venv PlatformIO ссылался на этот каталог через editable-install
(`__editable__.esptool-5.3.0.pth`), поэтому модуль не импортировался.

Лечение:
1. Битый каталог переименован в `tool-esptoolpy.broken` (резерв, не удалён).
2. Скачан официальный `esptool.zip` v5.3.0 pioarduino, sha256 сверен с `tools.json`
   (`23d4c80f73ad2d7a8bd1f60ccb342c3db22531bedef74a24f62f5c5603d07a18`), распакован
   в `~/.platformio/packages/tool-esptoolpy/`.
3. В venv удалены stale editable-записи; вместо них создан `esptool.pth`,
   добавляющий каталог пакета в `sys.path` (в venv PlatformIO нет `pip`).
4. `pio run` → SUCCESS. `install/firmware/xiao-c6-ble.bin` перезаписан из
   `.pio/build/xiao_esp32c6_ble/firmware.factory.bin` (bootloader + partitions +
   otadata + app, offset 0 — как ожидает `manifest-xiao-c6-ble.json`).

Проверка бинарника: `grep -a` по строкам команд — старых `INSCONTROL`/`SMOOTHEDDR`
нет, актуальные есть.

---
