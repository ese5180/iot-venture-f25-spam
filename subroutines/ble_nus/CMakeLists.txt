cmake_minimum_required(VERSION 3.20.0)

# # Force CMake to use the NCS-provided Python (has required deps like pykwalify)
# if(DEFINED ENV{ZEPHYR_PYTHON_EXECUTABLE})
#   set(Python3_EXECUTABLE $ENV{ZEPHYR_PYTHON_EXECUTABLE}
#       CACHE FILEPATH "Python interpreter for Zephyr" FORCE)
# endif()

# # Keep Zephyr's user cache inside the project tree (avoids write issues outside sandbox)
# if(NOT DEFINED USER_CACHE_DIR)
#   set(USER_CACHE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.cache"
#       CACHE PATH "Local Zephyr user cache" FORCE)
#   file(MAKE_DIRECTORY "${USER_CACHE_DIR}/ToolchainCapabilityDatabase")
# endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ble_nus)

target_sources(app PRIVATE
  src/main.c
  src/ble_nus.c
)


target_include_directories(app PRIVATE include)