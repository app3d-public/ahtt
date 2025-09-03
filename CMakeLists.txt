cmake_minimum_required(VERSION 3.17)

if(NOT DEFINED ACBT_PROJECT_LOADED)
    include(cmake/project.cmake)
endif()

project(ahtt
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "ALWF HTML Template Compiler")

if(NOT DEFINED ACBT_LOADED)
    include(cmake/utils.cmake)
endif()

set(APP_LIB_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${APP_LIB_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${APP_LIB_DIR})

add_subdirectory(modules/acul)
add_subdirectory(modules/3rd-party/args)

gen_version_file(${CMAKE_CURRENT_BINARY_DIR}/version.h)

add_files(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE acul args)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
)

set(APP_MANIFEST_DEPS
    libacul:${acul_VERSION}
    libacul-simd:${acul-simd_VERSION}
)

gen_manifest_app(${PROJECT_NAME} ${PROJECT_VERSION} ${APP_MANIFEST_DEPS})
gen_app_config()