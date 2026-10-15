cmake_minimum_required(VERSION 3.24)

# ---- Project ----

# Mohd Nuru Interpreter: a C++ interpreter for the Nuru language
# (https://github.com/NuruProgramming/Nuru)
project(
  mni
  VERSION 0.1.0
  LANGUAGES CXX
)

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# ---- Options ----

option(MNI_ENABLE_HTTP "Build the mtandao (HTTP) module, which needs libcurl" ON)

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info

include(cmake/CPM.cmake)

# PackageProject.cmake will be used to make our target installable
CPMAddPackage("gh:TheLartians/PackageProject.cmake@1.8.0")

CPMAddPackage(
  NAME fmt
  GIT_TAG 10.2.1
  GITHUB_REPOSITORY fmtlib/fmt
  OPTIONS "FMT_INSTALL YES" # create an installable target
)

CPMAddPackage(
  NAME nlohmann_json
  GITHUB_REPOSITORY nlohmann/json
  VERSION 3.12.0
  OPTIONS "JSON_BuildTests OFF" "JSON_Install YES" "JSON_SystemInclude ON"
)

if(MNI_ENABLE_HTTP)
  # cpr wraps libcurl. Linux and macOS use the system libcurl (on Debian/Ubuntu:
  # libcurl4-openssl-dev), which brings its own TLS backend; Windows builds curl from source with
  # Schannel.
  if(WIN32)
    set(MNI_CPR_SYSTEM_CURL OFF)
  else()
    set(MNI_CPR_SYSTEM_CURL ON)
  endif()
  CPMAddPackage(
    NAME cpr
    GITHUB_REPOSITORY libcpr/cpr
    GIT_TAG 1.15.1
    OPTIONS "CPR_USE_SYSTEM_CURL ${MNI_CPR_SYSTEM_CURL}" "CPR_CURL_USE_LIBPSL OFF"
            # cpr defaults to shared libraries on Windows; static means nuru.exe needs no DLLs
            "BUILD_SHARED_LIBS OFF" "CPR_ENABLE_INSTALL OFF" "CPR_BUILD_TESTS OFF"
  )
endif()

# ---- Add source files ----

# Note: globbing sources is considered bad practice as CMake's generators may not detect new files
# automatically. Keep that in mind when changing files, or explicitly mention them here.
file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")

# ---- Create library ----

# Note: for header-only libraries change all PUBLIC flags to INTERFACE and create an interface
# target: add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME} STATIC ${headers} ${sources})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# being a cross-platform target, we enforce standards conformance on MSVC
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

# Link dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt nlohmann_json::nlohmann_json)

target_include_directories(
  ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                         $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
)

# ---- Packaging ----

if(MNI_ENABLE_HTTP)
  # The exported package cannot carry cpr/libcurl yet, so an installed libmni built with HTTP would
  # fail to link in consumers (and CMake refuses to export a target that depends on cpr). There is
  # therefore no export set in this configuration: installing stops with a clear message, and the
  # version header that packageProject would have made is generated here.
  target_link_libraries(${PROJECT_NAME} PRIVATE cpr::cpr)
  # public so tests (and users) can tell whether mtandao is available
  target_compile_definitions(${PROJECT_NAME} PUBLIC MNI_HAVE_HTTP=1)

  # packageProject normally creates this alias
  add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

  configure_file(cmake/version.h.in ${PROJECT_BINARY_DIR}/generated/mni/version.h @ONLY)
  target_include_directories(
    ${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/generated>
  )

  install(
    CODE "message(FATAL_ERROR \"Installing mni with HTTP support is not supported yet. Configure with -DMNI_ENABLE_HTTP=OFF to install the library.\")"
  )
else()
  # this allows users to install and find the library via `find_package()`.

  # the location where the project's version header will be placed should match the project's
  # regular header paths
  string(TOLOWER ${PROJECT_NAME}/version.h VERSION_HEADER_LOCATION)

  packageProject(
    NAME ${PROJECT_NAME}
    VERSION ${PROJECT_VERSION}
    NAMESPACE ${PROJECT_NAME}
    BINARY_DIR ${PROJECT_BINARY_DIR}
    INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include
    INCLUDE_DESTINATION include/${PROJECT_NAME}-${PROJECT_VERSION}
    VERSION_HEADER "${VERSION_HEADER_LOCATION}"
    COMPATIBILITY SameMajorVersion
    DEPENDENCIES "fmt 10.2.1" "nlohmann_json 3.12.0"
  )
endif()
