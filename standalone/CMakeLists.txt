cmake_minimum_required(VERSION 3.14...3.22)

project(mniStandalone LANGUAGES CXX)

# --- Import tools ----

include(../cmake/tools.cmake)

# ---- Dependencies ----

include(../cmake/CPM.cmake)

CPMAddPackage(NAME mni SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)

# ---- Create standalone executable ----

file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)

add_executable(${PROJECT_NAME} ${sources})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20 OUTPUT_NAME "nuru")

target_link_libraries(${PROJECT_NAME} mni::mni)

# Windows gives the main thread 1 MB of stack by default; use the 8 MB that Linux and macOS have so
# deep (but legitimate) recursion behaves the same everywhere.
if(MSVC)
  target_link_options(${PROJECT_NAME} PRIVATE /STACK:8388608)
elseif(WIN32)
  target_link_options(${PROJECT_NAME} PRIVATE -Wl,--stack,8388608)
endif()
