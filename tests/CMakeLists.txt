cmake_minimum_required(VERSION 3.16)
project(doorbell_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable testing support
enable_testing()
add_compile_definitions(UNIT_TESTING)

# Fetch Unity framework
include(FetchContent)
FetchContent_Declare(
  unity
  GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
  GIT_TAG        v2.5.2
)
FetchContent_MakeAvailable(unity)

# Include paths
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  ${CMAKE_CURRENT_SOURCE_DIR}/../firmware_esp32s3zero/main
)

# Shared Mock Source
set(MOCK_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_esp.c)

# 1. Doorbell Logic Test
add_executable(test_doorbell_logic
  test_doorbell_logic.c
  ${MOCK_SRCS}
  ${CMAKE_CURRENT_SOURCE_DIR}/../firmware_esp32s3zero/main/doorbell_logic.c
)
target_link_libraries(test_doorbell_logic unity)
add_test(NAME test_doorbell_logic COMMAND test_doorbell_logic)

# 2. Config Manager Test
add_executable(test_config_manager
  test_config_manager.c
  ${MOCK_SRCS}
  ${CMAKE_CURRENT_SOURCE_DIR}/../firmware_esp32s3zero/main/config_manager.c
)
target_link_libraries(test_config_manager unity)
add_test(NAME test_config_manager COMMAND test_config_manager)
