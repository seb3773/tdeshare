cmake_minimum_required(VERSION 3.10)
project(tdeshare C CXX)

set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TDESHARE_AGGRESSIVE_FLAGS "Enable aggressive -O2/LTO compile flags" ON)

if(TDESHARE_AGGRESSIVE_FLAGS)
    # Aggressive size optimization flags (-Os, LTO, no-rtti, no-exceptions, gc-sections)
    set(_TDESHARE_C_FLAGS "-Os -flto=auto -fno-semantic-interposition -fno-common -fstrict-aliasing -ffunction-sections -fdata-sections -fno-ident -fno-plt -fomit-frame-pointer -fno-stack-protector -fno-math-errno -fvisibility=hidden -fmerge-all-constants -fno-unwind-tables -fno-asynchronous-unwind-tables -DNDEBUG")
    set(_TDESHARE_CXX_EXTRA "-std=c++03 -fno-threadsafe-statics -fno-exceptions -fvisibility-inlines-hidden -fmerge-all-constants -Wno-deprecated-declarations -fpermissive")
    set(CMAKE_C_FLAGS "${_TDESHARE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${_TDESHARE_C_FLAGS} ${_TDESHARE_CXX_EXTRA}")
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,-O2 -Wl,--gc-sections -Wl,--as-needed -Wl,--build-id=none -Wl,-z,norelro -Wl,--sort-section,alignment -flto=auto -Os -s")
    set(CMAKE_C_FLAGS_DEBUG "")
    set(CMAKE_C_FLAGS_RELEASE "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
    set(CMAKE_CXX_FLAGS_RELEASE "")
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()
else()
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-std=c++03>)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(TQT REQUIRED tqt-mt)

find_program(TQMOC_EXECUTABLE NAMES tqmoc moc-tqt PATHS /opt/trinity/bin /usr/bin)
if (NOT TQMOC_EXECUTABLE)
    message(FATAL_ERROR "tqmoc (or moc-tqt) not found")
endif()

find_program(SSTRIP_EXECUTABLE sstrip PATHS /usr/bin /usr/local/bin)

include_directories(
    ${TQT_INCLUDE_DIRS}
    /usr/include/tqt3
    /usr/include/tqt
    /opt/trinity/include
    /opt/trinity/include/tde
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Custom macro to generate MOC source files
macro(tqt_wrap_cpp out_files)
    foreach(it ${ARGN})
        get_filename_component(name ${it} NAME_WE)
        set(moc_file ${CMAKE_CURRENT_BINARY_DIR}/moc_${name}.cpp)
        add_custom_command(
            OUTPUT ${moc_file}
            COMMAND ${TQMOC_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/src/${it} -o ${moc_file}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/${it}
        )
        list(APPEND ${out_files} ${moc_file})
    endforeach()
endmacro()

set(MOC_HEADERS
    share_dialog.h
    diag_dialog.h
    tdeshare_window.h
    new_user_dialog.h
    add_user_dialog.h
    global_options_dialog.h
)

tqt_wrap_cpp(MOC_SRCS ${MOC_HEADERS})

set(SOURCES
    src/main.cpp
    src/share_info.cpp
    src/fs_permissions.cpp
    src/share_backend.cpp
    src/share_dialog.cpp
    src/diag_dialog.cpp
    src/tdeshare_window.cpp
    src/new_user_dialog.cpp
    src/add_user_dialog.cpp
    src/global_options_dialog.cpp
    ${MOC_SRCS}
)

add_executable(tdeshare ${SOURCES})

target_compile_options(tdeshare PRIVATE ${TQT_CFLAGS_OTHER})
target_link_libraries(tdeshare
    ${TQT_LIBRARIES}
    /opt/trinity/lib/libtdecore.so
    /opt/trinity/lib/libtdeui.so
    /opt/trinity/lib/libDCOP.so
    pthread
)

if(TDESHARE_AGGRESSIVE_FLAGS)
    set_source_files_properties(
        src/main.cpp
        src/share_info.cpp
        src/fs_permissions.cpp
        src/share_backend.cpp
        src/share_dialog.cpp
        src/diag_dialog.cpp
        src/tdeshare_window.cpp
        src/new_user_dialog.cpp
        src/add_user_dialog.cpp
        src/global_options_dialog.cpp
        PROPERTIES COMPILE_FLAGS "-Os"
    )

    if(SSTRIP_EXECUTABLE)
        add_custom_command(TARGET tdeshare POST_BUILD
            COMMAND ${SSTRIP_EXECUTABLE} $<TARGET_FILE:tdeshare>
            COMMENT "Aggressively stripping tdeshare with sstrip"
        )
    endif()
endif()

# Installation rules
install(TARGETS tdeshare DESTINATION /usr/local/bin)
install(FILES data/tdeshare.desktop DESTINATION /usr/share/applications)
install(FILES data/tdeshare_konqueror.desktop DESTINATION /opt/trinity/share/apps/konqueror/servicemenus OPTIONAL)
