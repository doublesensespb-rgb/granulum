# GRAIN FORM — справочник владельца

Всё, что нужно, чтобы собирать, менять и выпускать плагин самому или с любым помощником (ИИ или программист).
Версия на момент написания: **0.96**.

---

## 1. Что где лежит

| Что | Где |
|---|---|
| Исходный код (главное) | https://github.com/doublesensespb-rgb/granulum (ветка `main`) |
| Готовые сборки последней версии | ветка `builds`: https://github.com/doublesensespb-rgb/granulum/tree/builds |
| Автосборка (облако GitHub) | https://github.com/doublesensespb-rgb/granulum/actions |
| Инструкция для ИИ/программиста | файл `AGENTS.md` в корне репозитория |
| Этот справочник | `README-RU.md` |
| Сборка на своём компьютере | `tools/build-windows.bat` |
| Шумы OSC 3 | `Resources/noises/*.flac` (генератор: `tools/make_noises.py`) |
| Стоковые атмосферы OSC 2 | `Resources/samples/Atmo 1-5.flac` (у пользователя: `Документы/GRAIN FORM/Samples/Atmos`) |
| Шрифт Inter | `Resources/fonts` (лицензия OFL, можно в коммерческом плагине) |
| Пресеты пользователя | `Документы/GRAIN FORM/Presets/*.gpreset` |

Код полностью твой. Ни я, ни Anthropic не нужны, чтобы его собирать и менять.

**Резервная копия:** держи офлайн-архив (`GRAIN-FORM-0.96-offline.zip`) на компьютере и ещё в одном месте
(флешка, облако). В нём код с историей, JUCE 8.0.10, готовые сборки 0.96 и этот справочник.
GitHub-аккаунт обязательно защити двухфакторной авторизацией: https://github.com/settings/security

---

## 2. Как собрать новую версию

### Вариант А — в облаке GitHub (ничего ставить не надо)
1. Открой https://github.com/doublesensespb-rgb/granulum/actions/workflows/build.yml
2. Справа кнопка **Run workflow** → ветка `main` → в поле **What to build** выбери `all` (или только `windows` / `mac`) → **Run workflow**.
3. Через ~15 минут готовые `GRAIN-FORM-Windows.zip` и `GRAIN-FORM-macOS.zip` появятся тут:
   https://github.com/doublesensespb-rgb/granulum/tree/builds
4. Переименуй под версию, например `GRAIN-FORM-0.97-Windows.zip`.
5. Если Mac-сборка «отменилась» (cancelled) — это занятые серверы GitHub, не ошибка. Запусти ещё раз с `mac`.

Репозиторий приватный: GitHub даёт бесплатные минуты сборки каждый месяц (Windows и Mac считаются дороже Linux). Одна сборка GRAIN FORM — примерно час «облачных» минут, этого хватает на ~30 сборок в месяц на бесплатном аккаунте.
Тарифы: https://docs.github.com/en/billing/managing-billing-for-your-products/managing-billing-for-github-actions/about-billing-for-github-actions

### Вариант Б — на своём Windows-компьютере (работает без интернета)
Один раз установить (пока интернет есть):
- Visual Studio 2022 Community — https://visualstudio.microsoft.com/vs/community/ — при установке отметить **«Desktop development with C++»** («Разработка классических приложений на C++»).
- CMake — https://cmake.org/download/ — при установке отметить **«Add CMake to the system PATH»**.
- Git — https://git-scm.com/download/win (нужен только чтобы забирать/отправлять код на GitHub).

Потом каждый раз: распаковать офлайн-архив (или папку с кодом), положить папку `JUCE` рядом с `CMakeLists.txt`
(в офлайн-архиве она уже на месте) и дважды кликнуть `tools\build-windows.bat`.
Готовый плагин появится в папке `out`.

Mac-версию можно собрать только на Mac (Xcode) или через вариант А.

---

## 3. Как менять плагин без меня

### С другим ИИ-агентом
Подойдёт любой агент, который умеет работать с кодом и GitHub: Claude Code (https://claude.com/claude-code),
OpenAI Codex, Cursor, Kimi и т.п. Первое сообщение агенту:

> Это мой VST-плагин GRAIN FORM: https://github.com/doublesensespb-rgb/granulum
> Сначала полностью прочитай AGENTS.md и следуй правилам оттуда.
> Я не программист: делай всё сам, сначала согласуй со мной правки текстом, покажи картинку дизайна до сборки
> и не запускай сборку без моей команды. Отвечай по-русски.
> Задача: <что хочешь изменить>

### С программистом
Это обычный проект на **C++17 + JUCE 8 + CMake**. Ищи «JUCE developer» — например на форуме JUCE
(https://forum.juce.com), KVR (https://www.kvraudio.com/forum/), Upwork, Kwork/FL.ru. Дай ему ссылку на репозиторий
и попроси прочитать `AGENTS.md`.

### Три вещи, которые нельзя менять (иначе сломаются проекты пользователей)
1. Коды плагина в `CMakeLists.txt`: `PLUGIN_MANUFACTURER_CODE Thmt`, `PLUGIN_CODE GrFm`, `BUNDLE_ID`, `PRODUCT_NAME`.
2. Названия параметров (`ParamID` в `Source/PluginProcessor.h`). Удалить функцию можно, параметр — нет.
3. Формат пресетов (`.gpreset`) — только добавлять новое, не переименовывать старое.

Обязательно скажи это любому помощнику (в `AGENTS.md` это уже написано).

### Как называть версии
0.96 → 0.97 → … → 1.0 (первый релиз). Номер ставится в `CMakeLists.txt`: строка `project(Granulum VERSION 0.9.6)`
(0.97 = `0.9.7`, 1.0 = `1.0.0`).

### Что в плагине сейчас (0.96)
- Дизайн GRAPHITE: графит, медный акцент, шрифт Inter. LFO — стальной синий, шалфей, слоновая кость.
- OSC 1 ADDITIVE (гармоники, аккорды, точки SELECT), OSC 2 SAMPLE (меню SOUNDS, свой WAV), OSC 3 NOISE (PITCH).
- Грануляр: BLOOM, SMEAR, SCATTER, SHIMMER, DRIFT, CHIME + SIZE, SPEED, POSITION, DENSITY, MIX; RANDOM с характерами.
- Луп: OFF / LOOP / PING-PONG / ONE-SHOT, флажки на экране.
- Фильтры: LP12, LP24, HP, BP, PEAK, COMB, VOWEL + окошко фильтра. Дисторшн: FOLD, TUBE, HARD CLIP, BITCRUSH. Дилей, реверб.
- 3 LFO со своими точками (до 16), INTO GRAINS, одна ручка RATE; ADSR; UNDO; тугие ручки (Shift — точнее).

### Идеи на будущее (обсуждали)
RENDER VARIATIONS (пачка WAV-вариаций для игр), LOCK (зёрна в тональности), MUTATE и MORPH, разбор любого звука на
гармоники, версия-эффект, ручка SPLIT для главной фишки, переключатель ECO.

---

## 4. Чек-лист к релизу

**Юридическое**
- [ ] Лицензия JUCE: бесплатно (Starter), пока доход с плагинов до $20 000 в год; выше — Indie ($40/мес или $800 навсегда, до $300 000/год).
      https://juce.com/get-juce/ · условия: https://forum.juce.com/t/amendments-to-the-juce-end-user-licence-agreement-for-juce-8/61265
- [ ] VST3 — с октября 2025 под лицензией MIT, ничего подписывать не нужно. Логотип «VST» на сайте — по правилам Steinberg.
      https://sonicstate.com/news/2025/10/30/vst-3-now-available-under-mit-license
- [ ] Проверить, что имя GRAIN FORM не занято (поиск на KVR, Plugin Boutique, в реестре товарных знаков: https://branddb.wipo.int).
- [ ] EULA (лицензионное соглашение покупателя).

**Mac**
- [ ] Apple Developer Program ($99/год): https://developer.apple.com/programs/ — нужен для подписи и нотаризации,
      иначе macOS ругается при установке. После регистрации подпись встраивается в автосборку.

**Windows**
- [ ] Установщик (Inno Setup: https://jrsoftware.org/isinfo.php) вместо zip.
- [ ] (по желанию) подпись кода — убирает предупреждения SmartScreen.

**Качество**
- [ ] Проверка pluginval: https://github.com/Tracktion/pluginval (Windows и Mac), auval для Logic.
- [ ] Проверить в FL Studio, Ableton, Reaper, Bitwig, Logic.
- [ ] 20–50 заводских пресетов.

**Продажа**
- [ ] Площадка: свой сайт + Gumroad (https://gumroad.com) / Lemon Squeezy (https://www.lemonsqueezy.com),
      или магазины Plugin Boutique (https://www.pluginboutique.com), KVR Marketplace (https://www.kvraudio.com/marketplace).
- [ ] Защита от копирования: без защиты / ключи активации / ключи площадки — решить.
- [ ] Страница плагина, видео-демо, аудиопримеры, почта для поддержки.

---

## 5. Если что-то пошло не так

- **FL показывает старую версию** — удалить старый `GRAIN FORM.vst3` из `C:\Program Files\Common Files\VST3`,
  положить новый, в FL: Options → Manage plugins → Find more plugins (или Rescan).
- **Сборка на GitHub упала** — Actions → открыть красный запуск → прочитать ошибку → передать текст ИИ-агенту или программисту.
- **Пропал доступ к GitHub** — всё есть в офлайн-архиве: создай новый репозиторий и загрузи туда папку с кодом
  (в ней уже есть вся история: папка `.git`), поправь адрес в справочнике.
