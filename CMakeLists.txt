cmake_minimum_required(VERSION 3.22)

project(DiscrGen
  LANGUAGES CXX
  VERSION 1.0.0
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Fetch FTXUI --------------------------------------------------------------
include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/arthursonzogni/ftxui.git
  GIT_TAG        v6.1.9
  GIT_PROGRESS   TRUE
  GIT_SHALLOW    TRUE
  EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(ftxui)
# ------------------------------------------------------------------------------

add_library(discrgen_core src/boolean_formula.cpp)
target_include_directories(discrgen_core PUBLIC src)

add_executable(DiscrGen src/main.cpp)
target_link_libraries(DiscrGen PRIVATE discrgen_core)

target_link_libraries(DiscrGen
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
)

if(EMSCRIPTEN)
  target_link_options(DiscrGen PRIVATE
    "-sASYNCIFY=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sEXIT_RUNTIME=0"
  )

  set_target_properties(DiscrGen PROPERTIES
    OUTPUT_NAME index
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/docs"
  )
endif()

enable_testing()

add_executable(boolean_formula_tests tests/boolean_formula_tests.cpp)
target_link_libraries(boolean_formula_tests PRIVATE discrgen_core)

add_test(NAME boolean_formula_tests COMMAND boolean_formula_tests)
