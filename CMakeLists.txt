cmake_minimum_required(VERSION 4.2.3 FATAL_ERROR)

project(csv_median_calculator LANGUAGES CXX)

# Требуем C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

# --- Find Boost (program_options) ---
# Boost.Program_options обычно поставляется как пакет через vcpkg (не header-only)
find_package(Boost 1.70 REQUIRED COMPONENTS program_options)
if(NOT Boost_FOUND)
  message(FATAL_ERROR "Boost not found. Install Boost (program_options) or use vcpkg.")
endif()

# ----- FetchContent for header-only libs (spdlog, tomlplusplus) -----
include(FetchContent)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.11.0
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
  tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# ----- Sources -----
add_executable(csv_median_calculator
  src/main.cpp
  src/config_parser.hpp
  src/csv_reader.hpp
  src/median_calculator.hpp
)

# Link libraries
target_link_libraries(csv_median_calculator
  PRIVATE
    Boost::program_options
    spdlog::spdlog
    tomlplusplus::tomlplusplus
)

# Копируем папку examples в выходную директорию исполняемого файла после сборки
add_custom_command(TARGET csv_median_calculator POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/examples"
    $<TARGET_FILE_DIR:csv_median_calculator>/examples
  COMMENT "Copy examples/ -> $<TARGET_FILE_DIR:csv_median_calculator>/examples"
)


if(MSVC)
  # /utf-8 заставляет MSVC читать исходные файлы как UTF-8
  target_compile_options(csv_median_calculator PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(csv_median_calculator PRIVATE -Wall -Wextra -Wpedantic)
endif()
