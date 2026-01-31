csv_median_calculator

Лёгкое консольное приложение на C++23 для инкрементального расчёта медианы цен из CSV-файлов с биржевыми данными.
Реализовано с использованием CMake, Boost (Program_options, Accumulators), toml++ и spdlog.

Оглавление

1. Описание

2. Требования

3. Структура проекта

4. Конфигурация (config.toml)

5. Сборка (Windows / Visual Studio 2022, vcpkg)

6. Запуск и примеры

7. Алгоритм медианы и ограничения

8. Отладка — частые проблемы и решения

1. Описание

Приложение читает CSV-файлы из директории input, фильтрует их по маскам имён, сортирует записи по receive_ts и инкрементально рассчитывает медиану цен. В выходной CSV-файл записывается строка только в тот момент, когда медиана меняется.

2. Требования

C++ компилятор, поддерживающий C++23 (MSVC / Visual Studio 2022 в примерах)

CMake >= 3.23

Boost (Program_options, Accumulators) — желательно через vcpkg или системный пакет

Internet (для FetchContent загрузки spdlog и toml++) при первой конфигурации

Рекомендуется Windows 10/11 (инструкции для VS приведены ниже), но код кроссплатформенный (filesystem, std)

3. Структура проекта

csv_median_calculator/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ README.md
├─ src/
│  ├─ main.cpp
│  ├─ config_parser.hpp
│  ├─ csv_reader.hpp
│  └─ median_calculator.hpp
└─ examples/
   ├─ config.toml
   └─ input/
       ├─ level.csv
       └─ trade.csv

После сборки CMake копирует examples/ в выходную директорию рядом с exe (чтобы удобно запускать из build/...).

4. Конфиг (examples/config.toml)

[main]
input = "examples/input"        # обязательный
# output = "examples/output"   # опционально; по умолчанию ./output рядом с exe
filename_mask = ["level","trade"]

Пояснения:

─ input — путь к директории с CSV (обязателен)

─ output — куда записывать median_result.csv (если не указан — ./output)

─ filename_mask — список подстрок, по которым фильтруются имена файлов. Если пустой — читаются все .csv.

5. Сборка

A. Быстрая инструкция (Windows + Visual Studio 2022 + vcpkg)

1. Установите vcpkg и необходимые пакеты:
# из каталога vcpkg
.\vcpkg.exe install boost-program-options boost-accumulators:x64-windows
# spdlog и toml++ подтягиваются FetchContent; при желании можно установить через vcpkg:
.\vcpkg.exe install spdlog:x64-windows tomlplusplus:x64-windows
.\vcpkg.exe integrate install   # опционально — интеграция с VS

2. Конфигурация CMake (в корне проекта):

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="D:/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_BUILD_TYPE=Release

Примечание: (Замените путь к vcpkg на свой.)

3. Сборка:

cmake --build build --config Release

4. Запуск:

# из корня проекта (exe в build\Release)
.\build\Release\csv_median_calculator.exe --config examples/config.toml
# или запустить без аргументов — программа попытается найти examples/config.toml рядом с exe
.\build\Release\csv_median_calculator.exe

B. Без vcpkg (system Boost)

- Установите Boost через системный пакетный менеджер (или вручную) и укажите -DBOOST_ROOT=/path/to/boost и/или -DCMAKE_PREFIX_PATH=/path/to/boost при вызове cmake.

- spdlog и toml++ подтянутся автоматически через FetchContent.

C. CMake Presets

В проекте есть CMakePresets.json. Для конфигурации:

cmake --preset vs2022-x64-debug
cmake --build --preset vs2022-x64-debug --config Debug

6. Запуск и примеры

- Входные CSV — разделитель ;. Ожидаются колонки receive_ts и price. Остальные колонки (exchange_ts, quantity, side и т.д.) игнорируются по логике чтения, но могут присутствовать.

- Output: median_result.csv с заголовком receive_ts;price_median

Пример запуска:

./csv_median_calculator.exe --config ./examples/config.toml

Результат: файл output/median_result.csv (если output не указан) или в директории, заданной в конфиге.

7. Алгоритм медианы и ограничения

Реализован гибридный алгоритм:

- Сначала приложение собирает значения в небольшой буфер (по умолчанию seed_threshold = 64) и вычисляет точную медиану (через nth_element) — это обеспечивает 100% точность для маленьких наборов.

- После достижения порога буфер «перекармливается» в Boost.Accumulators с p_square_quantile (P²) — потоковый алгоритм, который экономит память и поддерживает инкрементальное добавление новых значений.

Причина: Boost.Accumulators реализует P² (оценка квантиля), который экономит память, но для малых выборок может давать приближённую оценку. Гибридный подход сочетает точность и эффективность.

Если вам нужна строго точная медиана при любых объёмах — можно легко заменить на реализацию «две кучи» (точная, O(log n) на вставку) или добавить конфиг-переключатель (опция на будущее).

8. Отладка — частые проблемы и решения

1) Кракозябры (русский текст в консоли)

- Убедитесь, что исходники сохранены в UTF-8 и MSVC компилирует с флагом /utf-8 (в CMakeLists.txt добавлен /utf-8 в target_compile_options для MSVC).

- В коде установлены SetConsoleOutputCP(CP_UTF8); SetConsoleCP(CP_UTF8);.

- Можно также выполнить chcp 65001 перед запуском.

2) VS запускает ALL_BUILD вместо exe

- Правый клик по проекту csv_median_calculator → Set as Startup Project. Или в CMake Targets выбрать target и Set as Startup.

3) FetchContent vs vcpkg — конфликты

Если и FetchContent и vcpkg пытаются предоставить одни и те же библиотеки, могут возникать конфликты целей. Рекомендуется:

- использовать vcpkg для Boost (и опционально для spdlog/toml++) или

- использовать FetchContent для заголовочных библиотек (в проекте spdlog/toml++ загружаются через FetchContent).

При проблемах временно удаляйте FetchContent / используйте find_package(...) для тех библиотек, которые установлены через vcpkg.

4) IntelliSense "не хватает памяти"

- Boost.MPL / Accumulators тяжело нагружают IntelliSense. Включите 64-битную подсистему IntelliSense и/или увеличьте лимит памяти в настройках VS.

5) Ошибки при использовании Boost.Accumulators

Параметр для P² — quantile_probability. Пример инициализации: accumulator_t acc( boost::accumulators::quantile_probability = 0.5 ); 
и извлечение boost::accumulators::p_square_quantile(acc).

