cmake_minimum_required(VERSION 3.16)

project(ada_ecu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ada_core
    src/config.cpp
    src/event_logger.cpp
    src/r2_mapper.cpp
    src/risk_assessor.cpp
    src/track_store.cpp
    src/warning_builder.cpp
)

target_include_directories(ada_core PUBLIC include)

add_executable(ada_ecu src/main.cpp)
target_link_libraries(ada_ecu PRIVATE ada_core)

enable_testing()

add_executable(ada_core_tests tests/track_store_tests.cpp)
target_link_libraries(ada_core_tests PRIVATE ada_core)
add_test(NAME ada_core_tests COMMAND ada_core_tests)

