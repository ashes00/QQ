cmake_minimum_required(VERSION 3.16)

# Parse app name + version from src/version.h (single source of truth —
# never hardcode these here or anywhere else; see docs/STATUS.md Versioning Rules).
file(STRINGS "${CMAKE_SOURCE_DIR}/src/version.h" VERSION_H_CONTENT)
foreach(LINE ${VERSION_H_CONTENT})
    if(LINE MATCHES "define QQ_APP_NAME \"([^\"]+)\"")
        set(APP_NAME_STRING "${CMAKE_MATCH_1}")
    endif()
    if(LINE MATCHES "define QQ_VERSION \"([^\"]+)\"")
        set(APP_VERSION_STRING "${CMAKE_MATCH_1}")
    endif()
endforeach()

project(${APP_NAME_STRING} VERSION ${APP_VERSION_STRING} LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSON_C REQUIRED json-c)

find_library(READLINE_LIBRARY readline REQUIRED)

add_executable(${PROJECT_NAME}
    src/main.c
    src/config.c
    src/settings.c
    src/ollama.c
    src/utils.c
    src/interactive.c
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "${APP_NAME_STRING}_v${APP_VERSION_STRING}"
)

target_include_directories(${PROJECT_NAME} PRIVATE ${JSON_C_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE
    CURL::libcurl
    ${JSON_C_LIBRARIES}
    ${READLINE_LIBRARY}
    Threads::Threads
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
