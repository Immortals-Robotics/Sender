cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)

project(sender)

if (${ENABLE_SANITIZERS})
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined)
  add_link_options(-fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined)
endif ()

find_library(RF24 rf24 REQUIRED)
message(STATUS "using RF24 library: ${RF24}")

add_executable(${PROJECT_NAME}
        source/main.cpp
        source/nrf.cpp
        source/udp.cpp)

target_link_libraries(${PROJECT_NAME}
        ${RF24}
        pthread)

install(TARGETS ${PROJECT_NAME})

set(CPACK_GENERATOR ZIP)
set(CPACK_OUTPUT_FILE_PREFIX ${CMAKE_BINARY_DIR}/dist)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
include(CPack)
