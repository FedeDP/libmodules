cmake_minimum_required(VERSION 3.0)

file(GLOB EASY_SRC Easy/*.c)
file(GLOB MULTICTX_SRC MultiCtx/*.c)
file(GLOB SHAREDSRC_SRC SharedSrc/*.c)
file(GLOB CPP_SRC Cpp/*.cpp)
file(GLOB POLL_SRC Poll/*.c)

include_directories("${PROJECT_SOURCE_DIR}/Lib/public/")

add_executable(Easy ${EASY_SRC})
add_executable(MultiCtx ${MULTICTX_SRC})
add_executable(SharedSrc ${SHAREDSRC_SRC})
add_executable(Cpp ${CPP_SRC})
add_executable(Poll ${POLL_SRC})

add_library("testmod" SHARED testModule.c)

target_link_libraries(Easy ${PROJECT_NAME})
target_link_libraries(MultiCtx ${PROJECT_NAME} pthread)
target_link_libraries(SharedSrc ${PROJECT_NAME})
target_link_libraries(Cpp ${PROJECT_NAME})
target_link_libraries(Poll ${PROJECT_NAME})
