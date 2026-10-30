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
без компенсации при 30° — 100 см. WT901 по θ проходит, по α — на грани.

### Что делать вместо этого (штатный режим)

Держать веху вертикально и вводить точную длину в `Instrument Height` — SW Maps
сама вычтет её и запишет точку на земле. IMU при этом не даёт ничего.

### Если когда-нибудь возвращаться

1. WT901 (TTL/UART, 9-axis — исполнение с магнитометром, индекс с «C») жёстко на веху,
   на 10–15 см от платы, батареи и ground plane антенны.
2. Готовые Roll/Pitch/Yaw из UART модуля; θ берётся из них напрямую. Подтяжка α по
   GNSS-курсу в движении — отдельным проходом, в статике α остаётся с магнитометра.
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

- [x] Фото платы: `board-top.jpg` ✅ в репозитории; `board-bottom.jpg`, `board-pinout.jpg` — плейсхолдеры в HARDWARE.md
- [x] Фото антенны: `media/antenna-bt800d.jpg` — BT-800D, коннектор на кабеле — **TNC**
      (уточнено, добавлено в таблицу антенны HARDWARE.md)
- [x] Замер напряжения на центральном контакте: **4.35 V** — питание LNA присутствует
      (см. §11.4). Фото `antenna-bias-voltage.jpg` всё ещё нужно
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

## 9. Наблюдаемость инициализации K803

Замечание: `sendCmd()` проверял ответы на `Invalid` / `Error!`, но результат уходил
только в `DBG()` — при `DEBUG_USB 0` он исчезал полностью. Если приёмник молчит
(нет контакта TX/RX, завис K803), прошивка молча прожигала ~12 с таймаутов и уходила
в режим моста; о проблеме можно было догадаться только по отсутствию данных.

Два разных сбоя раньше выглядели одинаково:

| Сбой | Что видел пользователь |
|---|---|
| Приёмник молчит целиком | тишину, GGA нет |
| Связь есть, команда отбита | NMEA идёт, точка есть, но RTK никогда не фиксируется |

Второй — опасный «тихий» отказ: отбитый `FIX NONE` оставляет приёмник в режиме базы,
приложение рисует quality 1.

### Сделано

- [x] `gnss_init.h`: `GnssInitResult` (`OK` / `WARN` / `NO_REPLY` / `SKIPPED`) и
      `GnssInitStatus { result, rejected, noReply }`; `gnssInit()` возвращает статус
- [x] `gnss_init.cpp`: `sendCmd()` → `Reply` (`REPLY_NONE` / `REPLY_OK` / `REPLY_REJECTED`)
- [x] `gnss_init.cpp`: **ранний выход** — нет ответа на `LOG VERSION` → остальные команды
      не шлются, мост поднимается за ~2 с вместо ~12 с
- [x] `gnss_init.cpp`: поле `expectReply` у каждой команды — escape hatch,
      `UNLOGALL COM2/COM3`, финальные `INTERFACEMODE`/`SAVECONFIG` не портят счётчик
      `noReply` (молчание на них — норма)
- [x] `main.cpp`: паттерн LED кодирует итог — `LINKED` / `WAITING` / двойная вспышка
      (`WARN`) / тройная (`NO_REPLY`); паттерн отказа **защелкивается** и не сбрасывается
      при подключении телефона
- [x] `main.cpp`: `[SYS]` — `- init WARN (rejected=N, noReply=M)` / `- init NO REPLY`
- [x] `main.cpp`: `[STAT]` — постоянный суффикс `GNSS WARN rejected=N` / `GNSS NO REPLY`,
      чтобы предупреждение не уезжало вместе с boot-логом
- [x] `FLASHING.md`: три примера boot-лога (норма / WARN / NO REPLY), таблица чтения,
      две новые строки в Troubleshooting
- [x] `COMMANDS.md`: `LOG VERSION` помечен как гейт последовательности; Gotchas —
      «Reply parsing» переписан, добавлен «No reply»; раздел Recovery обновлён
- [x] `install/firmware/xiao-c6-ble.bin` — пересобран из `firmware.factory.bin`

Мост при неудачной инициализации **не** блокируется: через downlink (BLE → UART) команды
можно послать из приложения, поэтому правильное поведение — стартовать и предупредить.

---

## 10. Отказоустойчивость: task watchdog + ретраи инициализации

Замечание: зависший `loop()` или замолчавший GNSS-модуль не приводили ни к чему,
что пользователь заметил бы в поле.

### 10.1 Программный watchdog (esp_task_wdt)

Проверено по sdkconfig C6: `CONFIG_ESP_TASK_WDT_INIT=y`, 5 с, `PANIC=y`
(panic → `PRINT_REBOOT`, т.е. перезагрузка есть), **но**:
- `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0` не задан;
- Arduino-core создаёт `loopTask` с `loopTaskWDTEnabled = false`
  (`cores/esp32/main.cpp:111`), а `feedLoopWDT()`/`enableLoopWDT()` ядро
  в `loop()` не вызывает.

Итог: до этого изменения `loop()` под надзором TWDT **не был** — зависание
не детектировалось вообще.

- [x] `config.h`: `USE_TASK_WDT` (1), `TASK_WDT_TIMEOUT_MS` (5000)
- [x] `main.cpp`: `watchdogInit()` — `esp_task_wdt_reconfigure()` +
      `esp_task_wdt_add(NULL)` для подписки `loopTask`
- [x] подписка — в конце `setup()`, **после** `gnssInit()`: инициализация K803
      блокирует на ~12 с и иначе триггерила бы watchdog
- [x] `main.cpp`: `watchdogFeed()` (`esp_task_wdt_reset()`) в начале `loop()`
- [x] `main.cpp`: `esp_reset_reason() == ESP_RST_TASK_WDT` → строка
      `[SYS] previous run ended by watchdog reset (loop() hung)` — без неё
      перезагрузку от WDT в поле не отличить от обычной
- [x] idle task **не** подписываем: при потоке NMEA `loop()` почти каждый проход
      выходит без yield, idle выглядел бы голодным в штатном режиме

Проверено: `pio run` → SUCCESS, RAM 7.5 %, Flash 51.3 %.

### 10.2 Ретраи команд инициализации и порог «приёмник мёртв»

- [x] `gnss_init.cpp`: поле `maxRetries` в `struct Cmd`; повтор только по
      `REPLY_NONE` — ответ `Invalid` доказывает, что UART жив, ретраить бессмысленно
- [x] `maxRetries=2` у `UNLOGALL <port>` и `SET APPSCENE Survey`, у остальных 1;
      при `expectReply=false` ретраев нет (молчание — норма)
- [x] `config.h`: `GNSS_INIT_NO_REPLY_THRESHOLD` (3)
- [x] `gnss_init.cpp`: `noReply >= 3` → `GNSS_INIT_NO_REPLY` (было: любая
      молчаливая команда только повышала счётчик, итог оставался `OK`/`WARN`)
- [x] LED: критический случай остаётся **тройной** вспышкой (`LED_PATTERN_NO_REPLY`),
      а не «быстрым миганием» из замечания — быстрый паттерн (600 мс) уже занят
      под `WAITING` (ждём телефон), два разных состояния стали бы неотличимы
- [x] `main.cpp` не менялся: `NO_REPLY` уже даёт тройную вспышку,
      `[SYS] ... init NO REPLY` и постоянный суффикс `[STAT] ... GNSS NO REPLY`

Все команды последовательности идемпотентны (`LOG ... ONTIME 1` перезаписывает
настройку, `UNLOGALL`/`FIX NONE`/`UNLOCKOUTALL` — сброс), повтор безопасен.

### Не сделано сознательно

- **Нет** отдельного состояния «частично молчит» кроме `NO_REPLY`: три молчаливые
  команды означают, что всё после них не настроено, это именно мёртвая линия.
- **Не** подписываем BLE host-задачу NimBLE: `NimBLEDevice` не отдаёт её
  `TaskHandle_t`, а подписка из её же колбэка меняла бы семантику стека.
  Зависание BLE накрётся watchdog'ом `loop()` — `linkWrite()` вызывается из него.

### Осталось

- [x] `install/firmware/xiao-c6-ble.bin` — пересобран из `firmware.factory.bin`
      (сигнатура `0xE9`, `INSCONTROL`/`SMOOTHEDDR` отсутствуют)
- [x] `FLASHING.md` — пример boot-лога после WDT-перезагрузки, строки в таблицу чтения
      и в Troubleshooting, абзац про ретраи и порог 3
- [x] `COMMANDS.md` — `kCommands[]` показан как `struct Cmd` с `maxRetries`, таблица полей,
      Gotchas: «Silence is retried, rejection is not»


---

## 11. Стендовая проверка на реальном приёмнике

Первый раз за проект K803 подключён к ПК (USB-TTL, `COM13` @115200 8N1, CR+LF) и
проверен по-настоящему. Всё ниже — измерения на живой плате, не предположения из
мануала.

### 11.1 Что за плата

```
<VERSION COM1 0 60.0 UNKNOWN 1930 000151.400 00000000 0000 1114
     GPSCARD "S32352K803" "0947892600000000" "CRDK-803AA-TTT-1" "630SH-B23A5-1" "7.4.6" "2025/Dec/30" "11:40:29"
```

| | |
|---|---|
| Модель | `S32352K803` |
| Серийный | `0947892600000000` |
| Hardware | `CRDK-803AA-TTT-1` |
| Firmware | **7.4.6**, сборка 2025/Dec/30 |
| Портов | физически **3** (COM1–COM3); `LOG COMCONFIG` показывает ещё COM4/COM5 — они внутренние |

### 11.2 `SAVECONFIG` был мёртвым кодом — исправлено

Гипотеза из `COMMANDS.md` подтверждена экспериментом:

| Команда | в режиме `COMPASSNAV` | после `INTERFACEMODE COM1 AUTO AUTO ON` |
|---|---|---|
| `SAVECONFIG` | `OK! Command accepted!` | **тишина** |
| `FOOBARBAZ` (мусорный ключ) | `Error! Invalid key word!` | **тишина** |
| `LOG VERSION` | полный ответ | **тишина** |
| `INTERFACEMODE COMPASS COMPASS ON` | `OK!` | **`OK!` — работает** |

Порт в режиме AUTO не разбирает ASCII вообще (даже мусор не отвергается), но
`INTERFACEMODE` остаётся живым — это не «порт оглох», а «пропускает ровно одну
команду-выход». Значит `SAVECONFIG`, стоявший после `INTERFACEMODE`, до приёмника
не доходил никогда.

- [x] `gnss_init.cpp`: `SAVECONFIG` перенесён **перед** `INTERFACEMODE`
- [x] `gnss_init.cpp`: `expectReply` у `SAVECONFIG` → `true` — теперь в boot-логе видно,
      что конфигурация действительно записалась
- [x] Проверено на железе: `SAVECONFIG` отвечает за **~400 мс** (запись во флеш; повторный
      вызов — 33 мс), укладывается в `GNSS_CMD_WAIT_MS` 600 мс
- [x] Проверено на железе: после `SAVECONFIG` и **холодной перезагрузки** в пассиве сразу
      идёт NMEA, `LOG LOGLIST` содержит все 7 логов — конфигурация пережила power cycle

### 11.3 Вся последовательность проходит на живой плате

Все **17** команд `kCommands[]` приняты: `rejected=0 silent=0`. Escape hatch
(`INTERFACEMODE COMPASS COMPASS ON`) восстанавливает связь из AUTO — проверено.

### 11.4 Антенна: питание есть, сигнала нет

| Проверка | Результат |
|---|---|
| DC на центральном контакте, антенна **отключена** | **4.63 V** — питание LNA есть |
| DC на центральном контакте, антенна **подключена** | **4.37 V** — просадка 0.26 V → LNA потребляет, цепь цела |
| `SET AE 100M` / `OFF` / `25M` + 20 с на каждое | 0 спутников во всех трёх случаях |
| Антенна куполом вверх, 60 с | `$GPGSV,1,1,00` ×60, C/N0 = 0 |
| `ECUTOFF 0` + `BD2ECUTOFF 0`, 5 минут | 0 спутников, quality 0 |
| Замена антенны, провода и пигтейла целиком | 0 спутников — не изменилось ничего |
| Скан спектра L1/L5, с антенной и без | плоский в обоих случаях |

#### Ошибка в первой интерпретации — исправлена

Первый вывод был: «спектр не поднимается ⇒ антенна ничего не даёт». **Он неверен.**
Сигналы GNSS лежат на ~20 дБ **ниже** уровня шума в сканируемой полосе: L1 C/A приходит
около −130 dBm, тогда как тепловой шум в полосе 2 МГц — около −111 dBm. Сканер спектра
**не может показать спутники** — и у рабочей, и у мёртвой антенны картина плоская.
Отдельно: один свип вернул 56 точек вместо запрошенных 100, то есть сравнение профилей
было некорректно вдвойне. Исправлено в `HARDWARE.md` и `COMMANDS.md`.

#### Что остаётся на самом деле

Всё, что могут показать электронные средства, — в норме: питание есть, LNA потребляет,
команды проходят, RF-тракт отвечает. При этом спутников ноль, и **все измерения сделаны
в помещении** (балкон). Ни одного замера под открытым небом не было.

Стеклопакет с низкоэмиссионным покрытием режет L1 на 20–30 дБ; плюс плита балкона и
парапет. Этого достаточно, чтобы приёмник не нашёл ни одного спутника.

- [x] **Вынести антенну под открытое небо** — сделано. Результат: **17 спутников**
      вместо нуля, C/N0 max 38 / медиана 33 dB-Hz, HDOP 0.8, quality 1. Лог —
      `.pio/open-sky-60s.nmea`, подробности в §12.2.1. Причина полного нуля —
      именно отсутствие неба, железо исправно
- [x] Подмена антенны не понадобилась — с новой антенной и на улице сигнал есть
- [ ] Фото `media/antenna-bias-voltage.jpg` (замер под нагрузкой, 4.37 V)
- [ ] Ground plane: без металлической площадки C/N0 держится на 33–38 dB-Hz вместо
      типичных 45–52 — это следующий по важности шаг, важнее любой настройки приёмника

### 11.5 Находки по командам (для `COMMANDS.md`)

| Команда | Наблюдение |
|---|---|
| `LOG SYSCONFIG` | **работает** на 7.4.6 — метку ⚠️ «не в V3.1» снять |
| `LOG LOGLIST` | работает, но **пуст**, пока не задан ни один `LOG` |
| `LOG TRACKSTATA` | отвечает `OK!`, но **не выдаёт ни строки** — «принято» ≠ «лог есть» |
| `LOG SYSRTS` | работает; поля `ANT1 qual`/`ANT1 status` **пустые**, `ANT1 SNR` = 0 — диагностика антенны на этой плате не заполняется |
| `SCANSPECTRUM` | работает, отвечает `ScanBegin:<f>KHz <r>KHz <n>` |
| `LOG SPECTRUM` | принимается, **молчит** — нужен бинарный `SPECTRUMB` |
| `SET AE` | принимается, на BT-800D не влияет (антенна без тактовой электроники) |
| `ECUTOFF` / `BD2ECUTOFF` | принимаются, значение 0–90, заводское 10 |
| `LOG ANTENNA` / `ANTCONFIG` / `HWSTATUS` / `RXSTATUS` | `Error! Invalid key word!` — таких логов в 7.4.6 нет |
| `SET SIGNAL` | есть в V3.1 §2.2.24 (`SET SIGNAL GPS OFF`, `SET SIGNAL L1CA ON/OFF`) — не проверялось |

### 11.6 Boot loop из-за плохого провода

Первый заход на `COM17` дал:

```
CBOOT 7.4.6 BUILD (2025-Feb-13 13:48:00)
SN: 01A0 09478926 2024/01/17 A80 746
Register Code is OK.
Loading...
CBOOT 7.4.6 ...          ← снова, цикл ~1.3 с
```

Причина — **провод**, не питание: после замены кабеля (тот же приёмник, `COM13`)
связь появилась сразу. Симптом стоит внести в Troubleshooting `HARDWARE.md`:
boot loop `CBOOT → Loading...` без перезагрузки ESP32 означает, что до приёмника
не доходит питание **или** линия данных не контачит.

### 11.7 Инструменты

Набор скриптов в `tools/` — проверка приёмника напрямую с ПК, без ESP32:

| Скрипт | Что делает |
|---|---|
| `probe_k803.py` | пассивное прослушивание + `LOG VERSION`, либо произвольные команды |
| `replay_boot.py` | прогоняет ровно `kCommands[]` и выносит вердикт OK/WARN/NO REPLY |
| `test_saveconfig.py` | проверяет, доходит ли `SAVECONFIG` после `INTERFACEMODE AUTO` |
| `test_autoparse.py` | различает «порт не парсит ASCII» и «команда не поддерживается» |
| `test_ae.py` | перебирает `SET AE` и считает спутники |
| `test_ecutoff.py` | снимает маску возвышения и наблюдает `GPGSV` |
| `scan_spectrum.py` | скан спектра L1/L5 через `SCANSPECTRUM` + `SPECTRUMB` |
| `ntrip_to_receiver.py` | льёт RTCM3 из NTRIP-кастера прямо в UART и следит за `GGA` |
| `check_links.py` | проверяет битые ссылки и якоря в markdown |

Учётные данные кастера — в `.ntrip` (формат `host <значение>` построчно), файл в
`.gitignore`. Пароль нигде не печатается.

---

## 12. RTCM-путь проверен вживую: приёмник доходит до FLOAT

Главное, чего в проекте не было ни разу: проверка, что коррекции, пришедшие по UART,
реально попадают в RTK-движок. Проверено без ESP32 и без телефона — RTCM3 из кастера
подавался прямо в `COM17` тем же способом, каким это делает мост.

### 12.1 Поток с кастера

Кастер `pidt.net`, mount **2307** — ближайший к точке замера, базовая линия **15.4 км**.

```
20 с, 12060 байт = 603 B/s,  88 кадров,  CRC-ошибок 0
  1075  GPS MSM5   1 Гц
  1085  GLO MSM5   1 Гц
  1095  GAL MSM5   1 Гц
  1125  BDS MSM5   1 Гц
  1006  ARP+высота 0.2 Гц      1008 / 1033  описание антенны 0.1 Гц
```

MSM5 = код **и полная фаза** — для двойных разностей годится. `1230` (GLONASS
code-phase bias) на 2307 отсутствует, есть на 2304, но там базовая линия 70 км.

### 12.2 Результат 150-секундного прогона

```
qual 1 (SINGLE):     136 эпох
qual 5 (RTK FLOAT):   16 эпох
GGA age = 1.0 s,  ref station = 0004
```

**Цепочка работает:** `INTERFACEMODE COM1 AUTO AUTO ON` → автоопределение `0xD3` →
длина → CRC-24Q → RTK-движок → FLOAT. Это ровно тот участок, за который отвечает
прошивка моста, и он подтверждён на железе.

`FIXED` за 150 с не наступил. Условия: антенна на лавочке без ground plane,
C/N0 29–36 dB-Hz, `RtkSnrThresh 28` — запас 1–8 дБ, сплошная облачность.

### 12.2.1 Замер под открытым небом (60 с, без коррекций)

Тот же приёмник, та же антенна, вынесены на улицу. Полный лог — `.pio/open-sky-60s.nmea`.

```
17 спутников в решении (GGA), HDOP 0.8, quality 1
C/N0: 978 каналов, max 38 dB-Hz, медиана 33 dB-Hz
GST: σ lat 1.03 / σ lon 0.60 / σ alt 0.49 м
```

Что это доказывает и что нет:

| | |
|---|---|
| Балкон и стекло были причиной полного нуля | ✅ да: ноль → 17 спутников |
| Антенна и RF-тракт исправны | ✅ да |
| До «открытого неба» по C/N0 не хватает | ❌ 33–38 dB-Hz против типичных 45–52 — антенна без ground plane, на лавочке |
| `RtkSnrThresh 28` проходит с запасом 5–10 дБ | ✅ да, но запаса мало |

Talker ID виден прямо в логе — приёмник отдаёт **по констелляциям**, а не одним `$GNGSA`:

```
$GPGSV,2,1,06,...   $GPGSV,2,2,06,...
$BDGSV,1,1,03,...   $GLGSV,1,1,04,...   $GAGSV,1,1,04,...
```

То есть GPS-предложения идут с `$GP`, BeiDou с `$BD`, GLONASS с `$GL`, Galileo с `$GA`.
Наш дашборд режет `slice(-3)` и работает с любым префиксом; SW Maps в режиме
Generic NMEA — внешний код, стоит проверить в поле, не теряет ли он не-`GP` строки.

### 12.3 Что осталось выяснить

- [x] `ntrip_to_receiver.py` — добавлен парсер `BESTPOSA` (V3.1 §3.2.5.1):
      `Pos type` (`SINGLE` / `PSRDIFF` / `NARROW_FLOAT` / `NARROW_INT`) и σ
      напрямую из RTK-движка. Лучит NMEA: показывается `SOL_COMPUTED,NARROW_FLOAT`
      с σlat/σlon/σhgt в мм. Также показывается GGA fallback, если BESTPOSA не
      прилетел (например, порт в AUTO и BESTPOSA не настроен)
- [ ] Повторить прогон с `BESTPOSA` на открытом небе — подтвердить, что
      NARROW_FLOAT → NARROW_INT при нормальном ground plane и C/N0 45+
- [ ] Повторить под открытым небом с нормальным ground plane — при C/N0 45+ и
      базовой линии 15 км шансы на `FIXED` радикально выше
- [ ] Починить отчёт в `ntrip_to_receiver.py` — **сделано**: `state["last"]` затирался
      строками GST/GSA, из-за чего печаталось `(no GGA yet)` при шедшем потоке.
      Теперь GGA пишется только из GGA-строк. Повторный прогон для подтверждения
      не удался: кастер стал недоступен по сети (`WinError 10060`), к приёмнику
      отношения не имеет

### 12.4 Побочные находки

| Находка | Значение |
|---|---|
| `RTKSOURCE` | фильтр RTCM по ID базовой станции. Задокументированный пример V3.1 §2.2.20 `RTKSOURCE RTCMV3 ANY` **принимается**. Если стоит конкретный ID, коррекции с другим ID молча отбрасываются — «тихий» отказ. **В `kCommands[]` этой команды нет** — стоит добавить |
| `LOG RTKSOURCE` / `LOG RTKFIXHOLDTIME` / `LOG DGPSTXID` | `Error! Invalid key word!` — читаются только внутри `LOG SYSCONFIG` |
| `SBAS` | выключается **только** `SET SIGNAL SBAS OFF`; `SBASSYS DISABLE` и `SET SBAS OFF` отвергаются, хотя `SBASSYS` есть в оглавлении V3.1 как §2.2.32 |
| `RtkSnrThresh` | 28, при реальных C/N0 29–36. В облаках/под деревом спутники провалятся ниже порога |
| Talker ID | приёмник отдаёт **по констелляциям**: `$GPGSV $BDGSV $GLGSV $GAGSV`, а не один `$GNGSA`. Дашборд использует `slice(-3)` и не сломается; SW Maps в режиме Generic NMEA — внешний код, стоит проверить в поле |
| Скорости вывода | подтверждено: `ONTIME 0.2` → 5.0 Гц, `ONTIME 0.1` → 10.0 Гц. Авторизация на 10 Гц есть |

### 12.5 Новая прошивка приёмника после перепрошивки

```
было:  "630SH-B23A5-1" "7.4.6" "2025/Dec/30"
стало: "630T6-B26A3-1" "7.4.6" "2026/Apr/ 8"
```

Новая сборка добавляет в `LOGLIST` четыре лога ориентации: `GPNTR`, `GPHPR`, `GPHDT`,
`GPTRA` (yaw/pitch/roll/track). Ни SW Maps, ни наш дашборд их не парсят, а UART и BLE
они нагружают. Убраны вручную (`UNLOG`); в прошивке моста их тоже нет — но при
следующем `SAVECONFIG` на чужой плате они вернутся, так что `UNLOGALL` первым делом
в `kCommands[]` остаётся обязательным.

### 12.6 Состояние приёмника на момент паузы

Ровер-конфигурация из `kCommands[]`, `SBAS Disabled`, `PvtObsMode SINGLE`,
`RTKSOURCE RTCMV3 ANY`, 7 NMEA-логов, `SAVECONFIG` не делался — всё живёт в RAM.
Конфигурация после последнего прогона: `LOGLIST` = GGA GST GSA GSV RMC VTG ZDA.

---

## 13. После полевых испытаний: софт

### 13.1 Сделано

- [x] `RTKSOURCE RTCMV3 ANY` — #9 в `kCommands[]` + `replay_boot.py` + `COMMANDS.md`
      (закрыло «тихий» отказ: ID станции в фильтре молча отбрасывает чужие коррекции)
- [x] `BESTPOSA` в `ntrip_to_receiver.py` — парсер ComNav ASCII (`#`-префикс, `;` после
      заголовка, CRC32 вместо XOR), проверен на примерах V3.1 §3.2.5.1; GGA оставлен fallback
- [x] `BESTPOSA` в дашборде — карточка «Engine verdict» (Pos type, Sol stat, σ lat/lon/hgt,
      height, station ID, datum), скрыта пока приёмник не шлёт BESTPOSA; оба сплиттера
      (BLE + WebSocket) ловят `#`-строки; JS-парсер проверен на node
- [x] Дашборд: баннер fix quality теперь рисуется из BESTPOSA, GGA — fallback
      (согласовано с Python-скриптом)
- [x] `SET SIGNAL SBAS OFF` — #10 в `kCommands[]`: ровер корригируется по RTCM, SBAS
      конкурирует с RTCM-путём и даёт quality 2; принимается только такое написание,
      после SAVECONFIG на чужой плате не переживает — поэтому каждый бут
- [x] README — секции «Field test results» и «Bench tools» (таблица 11 скриптов + ссылка
      на tools/README.md)
- [x] `install/firmware/xiao-c6-ble.bin` — пересобран после каждой правки (проверено:
      строки BESTPOSA в дашборде внутри bin присутствуют)

### 13.2 Осталось (нужно железо/поле)

- [ ] Прогнать `replay_boot.py` — подтвердить всю `kCommands[]` живой платой
      (`RTKSOURCE` и `SET SIGNAL SBAS OFF` в составе последовательности ещё не проверялись)
- [ ] Прогон `ntrip_to_receiver.py` с BESTPOSA под открытым небом и ground plane —
      подтвердить переход NARROW_FLOAT → NARROW_INT
- [ ] Проверить на живом приёмнике, отдаёт ли порт в AUTO строки `#BESTPOSA`
      (парсер проверен только на примерах из мануала)

---


