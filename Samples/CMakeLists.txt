cmake_minimum_required(VERSION 3.0)

file(GLOB EASY_SRC Easy/*.c)
file(GLOB SHAREDSRC_SRC SharedSrc/*.c)
file(GLOB POLL_SRC Poll/*.c)
file(GLOB TASK_SRC Task/*.c)
file(GLOB THPOOL_SRC Thpool/*.c)

include_directories("${PROJECT_SOURCE_DIR}/Lib/public/")

add_executable(Easy ${EASY_SRC})
add_executable(SharedSrc ${SHAREDSRC_SRC})
add_executable(Poll ${POLL_SRC})
add_executable(Task ${TASK_SRC})
add_executable(Thpool ${THPOOL_SRC})

add_library("testmod" SHARED testModule.c)

target_link_libraries(Easy ${PROJECT_NAME})
target_link_libraries(SharedSrc ${PROJECT_NAME})
target_link_libraries(Poll ${PROJECT_NAME})
target_link_libraries(Task ${PROJECT_NAME})
target_link_libraries(Thpool ${PROJECT_NAME})
