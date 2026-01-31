# csv_median_calculator

Консольное приложение на **C++23** для инкрементального расчёта медианы цен из CSV-файлов с биржевыми данными.  
Реализовано с использованием **CMake**, **Boost** (Program_options, Accumulators), `toml++` и `spdlog`. Проект читает CSV, сортирует записи по `receive_ts`, вычисляет медиану по мере поступления новых значений и записывает в выходной CSV только те строки, в которых медиана изменилась.

---

## Содержание

- Краткое описание  
- Требования  
- Структура проекта  
- Формат конфигурации (config.toml)  
- Пример входных CSV  
- Сборка  
- Запуск  
- Алгоритм медианы  
- Поведение и ограничения  
- Отладка  
- Дальнейшие улучшения  

---

## Краткое описание

Программа:
- считывает CSV-файлы из директории;
- фильтрует их по маскам имён;
- сортирует записи по `receive_ts`;
- инкрементально вычисляет медиану цены;
- записывает результат только при изменении медианы.

---

## Требования

- C++23 (MSVC / GCC / Clang)
- CMake >= 3.23
- Boost (program_options, accumulators)
- toml++
- spdlog

---

## Структура проекта

```
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
```

---

## Формат конфигурации (config.toml)

```toml
[main]
input = "examples/input"
# output = "examples/output"
filename_mask = ["level", "trade"]
```

---

## Пример входных CSV

```csv
receive_ts;price
1716810808593627;68480.0
```

---

## Сборка (Windows + VS2022)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake

cmake --build build --config Release
```

---

## Запуск

```powershell
build\Release\csv_median_calculator.exe --config examples/config.toml
```

---

## Алгоритм медианы

Используется гибридный подход:
- точная медиана для малых выборок;
- потоковая оценка медианы через Boost.Accumulators (P²) для больших потоков данных.

---

## Поведение и ограничения

- CSV должен содержать `receive_ts` и `price`
- Разделитель `;`
- Для очень больших файлов возможна доработка потоковой обработки

---

## Отладка

- Для корректного отображения русского текста используйте UTF-8 и `chcp 65001`
- В Visual Studio назначьте `csv_median_calculator` стартовым проектом

---

## Дальнейшие улучшения

- Выбор алгоритма медианы через config
- Unit-тесты
- CI (GitHub Actions)
