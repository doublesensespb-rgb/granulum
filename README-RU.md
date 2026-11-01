# GRAIN FORM — справочник владельца

Всё, что нужно, чтобы собирать, менять и выпускать плагин самому или с любым помощником (ИИ или программист).
Версия на момент написания: **0.91**.

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
| Пресеты пользователя | `Документы/GRAIN FORM/Presets/*.gpreset` |

Код полностью твой. Ни я, ни Anthropic не нужны, чтобы его собирать и менять.

**Резервная копия:** держи офлайн-архив (`GRAIN-FORM-0.91-offline.zip`) на компьютере и ещё в одном месте
(флешка, облако). В нём код с историей, JUCE 8.0.10, готовые сборки 0.91 и этот справочник.
GitHub-аккаунт обязательно защити двухфакторной авторизацией: https://github.com/settings/security

---

## 2. Как собрать новую версию

### Вариант А — в облаке GitHub (ничего ставить не надо)
1. Открой https://github.com/doublesensespb-rgb/granulum/actions/workflows/build.yml
2. Справа кнопка **Run workflow** → ветка `main` → **Run workflow**.
3. Через ~15 минут готовые `GRAIN-FORM-Windows.zip` и `GRAIN-FORM-macOS.zip` появятся тут:
   https://github.com/doublesensespb-rgb/granulum/tree/builds
4. Переименуй под версию, например `GRAIN-FORM-0.92-Windows.zip`.

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
0.91 → 0.92 → … → 1.0 (первый релиз). Номер ставится в `CMakeLists.txt`: строка `project(Granulum VERSION 0.9.1)`
(0.92 = `0.9.2`, 1.0 = `1.0.0`).

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
