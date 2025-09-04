cmake_minimum_required(VERSION 3.17)

if(NOT DEFINED ACBT_PROJECT_LOADED)
    include(cmake/project.cmake)
endif()

project(ahtt
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "ALWF HTML Template Transpiler")

if(NOT DEFINED ACBT_LOADED)
    include(cmake/utils.cmake)
endif()

if (NOT TARGET acul)
    add_subdirectory(modules/acul)
endif()
add_subdirectory(modules/3rd-party/args)

gen_version_file(${CMAKE_CURRENT_BINARY_DIR}/version.h)

add_files(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_executable(${PROJECT_NAME} ${AHTT_SRC})
target_compile_options(${PROJECT_NAME} PRIVATE -march=native)

target_link_libraries(${PROJECT_NAME} PRIVATE acul args)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)