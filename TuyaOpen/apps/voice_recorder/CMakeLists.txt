##
# @file CMakeLists.txt
# @brief Voice Recorder Application
#/

# APP_PATH - current CMakeLists.txt directory
set(APP_PATH ${CMAKE_CURRENT_LIST_DIR})

# APP_NAME - get project name from current directory name
get_filename_component(APP_NAME ${APP_PATH} NAME)

# APP_SRC - collect all source files from src directory
aux_source_directory(${APP_PATH}/src APP_SRC)
aux_source_directory(${APP_PATH}/src/font APP_SRC)

# APP_INC
set(APP_INC 
    ${APP_PATH}/include
)

# APP_OPTIONS
set(APP_OPTIONS "-W" "-Wno-maybe-uninitialized")
list(APPEND APP_OPTIONS "-Wall")

########################################
# Target Configure
########################################
add_library(${EXAMPLE_LIB})
message(STATUS "EXAMPLE_LIB:${APP_PATH}")

target_sources(${EXAMPLE_LIB}
    PRIVATE
        ${APP_SRC}
    )

target_include_directories(${EXAMPLE_LIB}
    PRIVATE
        ${APP_INC}
    )

target_compile_options(${EXAMPLE_LIB}
    PRIVATE
        ${APP_OPTIONS}
    )
