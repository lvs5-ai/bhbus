cmake_minimum_required(VERSION 3.10)
project(BH-BUS C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)
find_library(CJSON_LIB cjson REQUIRED)
find_library(M_LIB m REQUIRED)

include_directories(include)
include_directories(/usr/include/cjson)

add_executable(bus_stop_monitor
    src/bus_stop_monitor.c
    src/eta_predictor.c
    src/http_client.c
)

target_link_libraries(bus_stop_monitor PRIVATE
    CURL::libcurl
    ${CJSON_LIB}
    ${M_LIB}
)