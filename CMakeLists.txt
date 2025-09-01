cmake_minimum_required(VERSION 3.17)

if(NOT DEFINED ACBT_PROJECT_LOADED)
    include(cmake/project.cmake)
endif()

project(ahtt
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "ALWF HTML Template Compiler")

set(APP_LIB_DIR ${CMAKE_SOURCE_DIR}/lib/${CMAKE_SYSTEM_NAME})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${APP_LIB_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${APP_LIB_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${APP_LIB_DIR})

if(NOT DEFINED ACBT_LOADED)
    include(cmake/utils.cmake)
endif()

add_subdirectory(modules/acul)

add_files(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE acul)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

set(APP_MANIFEST_DEPS
    libacul:${acul_VERSION}
    libacul-simd:${acul-simd_VERSION}
)

gen_manifest_app(${PROJECT_NAME} ${PROJECT_VERSION} ${APP_MANIFEST_DEPS})
gen_app_config()