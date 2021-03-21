cmake_minimum_required (VERSION 3.3.2)

project(libmodule VERSION 6.0.0 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(GNUInstallDirs)

find_package (Threads)

# Find sources
file(GLOB SOURCES Lib/*.c Lib/structs/*.c Lib/utils/*.c)

set(PKG_DEPS "-ldl ${CMAKE_THREAD_LIBS_INIT}")

# Useful macro to properly set pkg-config script deps
macro(set_deps name)
    string(REPLACE ";" " " PKG_DIRS "${PKG_DIRS} ${${name}_LIBRARY_DIRS}")  # -L flags
    string(REPLACE ";" " " PKG_DEPS "${PKG_DEPS} ${${name}_LDFLAGS}")       # -l flags
    string(REPLACE ";" " " PKG_INCS "${PKG_INCS} ${${name}_CFLAGS}")        # -I flags
endmacro()

# Options
option(WITH_FS "build ${PROJECT_NAME} with libfuse support" OFF)
if (WITH_FS)
    find_package(PkgConfig)
    pkg_check_modules(FUSE REQUIRED fuse3)
    add_compile_definitions(WITH_FS)
    set(SOURCES ${SOURCES} Lib/fs/fs_priv.c)
    set_deps(FUSE)
endif()

option(BUILD_TESTS "build ${PROJECT_NAME} tests" OFF)
if(BUILD_TESTS)
    find_package(Cmocka)
    if (CMOCKA_FOUND)
        enable_testing()
        add_subdirectory(tests)
        message(STATUS "Tests building enabled.")
    else()
        message(WARNING "Missing cmocka.")
    endif()
endif()

option(BUILD_SAMPLES "build ${PROJECT_NAME} examples" OFF)
if(BUILD_SAMPLES)
     add_subdirectory(Samples)
     message(STATUS "Examples building enabled.")
endif()

option(BUILD_DOCS "build ${PROJECT_NAME} docs" OFF)
if(BUILD_DOCS)
    find_package(Sphinx)
    if(SPHINX_FOUND)
        add_subdirectory(docs)
        message(STATUS "Docs building enabled.")
    else()
        message(WARNING "Missing sphinx.")
    endif()
endif()

option(STATIC_MODULE "build ${PROJECT_NAME} as static library" OFF)
if(STATIC_MODULE)
    set(LIBRARY_TYPE STATIC)
else()
    set(LIBRARY_TYPE SHARED)
endif()

# Set requested poll plugin
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    option(WITH_LIBURING "build ${PROJECT_NAME} with liburing support" OFF)
    option(WITH_LIBKQUEUE "build ${PROJECT_NAME} with libkqueue support" OFF)
    add_compile_definitions(_GNU_SOURCE)
else()
    include(CheckFunctionExists)
    check_function_exists(kqueue KQUEUE_IN_LIBC)
    if(NOT KQUEUE_IN_LIBC)
        set(WITH_LIBKQUEUE true)
    else()
        set(WITH_KQUEUE true)
    endif()
endif()

if(WITH_LIBURING)
    find_package(PkgConfig)
    pkg_check_modules(URING REQUIRED liburing)
    set(SOURCES ${SOURCES} Lib/poll/uring_priv.c Lib/poll/cmn_linux.c)
    set(POLL_PLUGIN "liburing")
    set_deps(URING)
elseif(WITH_LIBKQUEUE)
    find_package(PkgConfig)
    pkg_check_modules(KQUEUE REQUIRED libkqueue)
    set(SOURCES ${SOURCES} Lib/poll/kqueue_priv.c)
    set(POLL_PLUGIN "libkqueue")
    set_deps(KQUEUE)
elseif(WITH_KQUEUE)
    set(SOURCES ${SOURCES} Lib/poll/kqueue_priv.c)
    set(POLL_PLUGIN "kqueue")
else()
    set(SOURCES ${SOURCES} Lib/poll/epoll_priv.c Lib/poll/cmn_linux.c)
    set(POLL_PLUGIN "epoll")
endif()

# Some pretty printings
MESSAGE(STATUS "Building ${PROJECT_NAME}-${PROJECT_VERSION}")
MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
MESSAGE(STATUS "Library Type: ${LIBRARY_TYPE}")
MESSAGE(STATUS "Poll plugin: ${POLL_PLUGIN}")
MESSAGE(STATUS "Target OS: ${CMAKE_SYSTEM_NAME}")

# Configure files
configure_file(Extra/libmodule.pc.in libmodule.pc @ONLY)
configure_file(Lib/public/module/cmn.h.in ${PROJECT_SOURCE_DIR}/Lib/public/module/cmn.h @ONLY)
if(WITH_FS)
    configure_file(Lib/public/module/fs.h.in ${PROJECT_SOURCE_DIR}/Lib/public/module/fs.h @ONLY)
endif()

file(GLOB PUBLIC_H Lib/public/module/*.h)

add_library(${PROJECT_NAME} ${LIBRARY_TYPE} ${SOURCES})

set_target_properties(
    ${PROJECT_NAME} PROPERTIES 
    PREFIX ""
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${PUBLIC_H}"
)

target_include_directories(${PROJECT_NAME} PRIVATE ${URING_INCLUDE_DIRS} ${KQUEUE_INCLUDE_DIRS} ${FUSE_INCLUDE_DIRS} Lib/ Lib/fs/ Lib/poll/ Lib/public/module/)
target_link_libraries(${PROJECT_NAME} ${URING_LIBRARIES} ${KQUEUE_LIBRARIES} ${FUSE_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} dl)
target_link_directories(${PROJECT_NAME} PUBLIC ${URING_LIBRARY_DIRS} ${KQUEUE_LIBRARY_DIRS} ${FUSE_LIBRARY_DIRS})

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/module
)

install(FILES ${CMAKE_BINARY_DIR}/libmodule.pc
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

install(FILES LICENSE
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/licenses/${PROJECT_NAME})

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -std=c11 -fvisibility=hidden -Wtype-limits -Wstrict-overflow -fno-strict-aliasing -Wformat -Wformat-security -fsanitize=undefined")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -fvisibility=hidden")

# Packaging support
# THANKS to libkqueue CMakeLists.txt for packaging support :)
SET(CPACK_SET_DESTDIR "on")
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

# Metadata common to all packaging systems
set(CPACK_PACKAGE_CONTACT "Federico Di Pierro <nierro92@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION "Libmodule offers a small and simple implementation of an actor library, aiming at letting developers easily create modular C projects in a way which is both simple and elegant.")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Small and simple implementation of an actor library for C.")

# RPM Specific configuration
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_URL "https://github.com/FedeDP/libmodule")
set(CPACK_RPM_PACKAGE_GROUP "System Environment/Libraries")
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "${CMAKE_INSTALL_DATAROOTDIR}/licenses ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig")
if(WITH_FS)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "fuse3-libs")
endif()
if(WITH_LIBURING)
    set(CPACK_RPM_PACKAGE_REQUIRES "liburing")
elseif(WITH_LIBKQUEUE)
    set(CPACK_RPM_PACKAGE_REQUIRES "libkqueue")
endif()
set(CPACK_RPM_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION})
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

# DEB Specific configuration
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/FedeDP/libmodule")
set(CPACK_DEBIAN_PACKAGE_SECTION "libs")
if(WITH_FS)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libfuse3-3")
endif()
if(WITH_LIBURING)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "liburing")
elseif(WITH_LIBKQUEUE)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libkqueue0")
endif()
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

include(CPack)
