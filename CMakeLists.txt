cmake_minimum_required(VERSION 3.24)

# Force x86 before project definition
set(CMAKE_GENERATOR_PLATFORM "Win32" CACHE STRING "" FORCE)
set(VCPKG_TARGET_ARCHITECTURE x86)

project(wowmapview)

# Basic project settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Source files
set(SOURCES 
    wowmapview.cpp 
    areadb.cpp 
    dbcfile.cpp 
    font.cpp 
    frustum.cpp 
    liquid.cpp 
    maptile.cpp 
    menu.cpp 
    model.cpp 
    mpq_libmpq.cpp 
    particle.cpp 
    shaders.cpp 
    sky.cpp 
    test.cpp 
    video.cpp 
    wmo.cpp 
    world.cpp
    database/Database.cpp
    database/DbField.cpp
    database/QueryObject.cpp
    database/QueryResult.cpp
    nodes.cpp
    objects/WorldObject.cpp
    gui_manager.cpp
)

set(HEADERS
    animated.h
    appstate.h
    areadb.h
    dbcfile.h
    font.h
    frustum.h
    liquid.h
    manager.h
    maptile.h
    matrix.h
    menu.h
    model.h
    modelheaders.h
    mpq.h
    mpq_libmpq.h
    particle.h
    quaternion.h
    shaders.h
    sky.h
    test.h
    vec3d.h
    video.h
    wmo.h
    world.h
    wowmapview.h
    database/Database.h
    database/DbField.h
    database/QueryObjects.h
    database/QueryResult.h
    database/SafeQueue.h
    defines/ByteBuffer.h
    defines/Common.h
    defines/DBCFile.h
    nodes.h
    objects/WorldObject.h
    gui_manager.h
)

# Platform-specific settings
if(WIN32)
    set(SDL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/SDL/include")
    set(SDL_LIBRARY_DIR "${CMAKE_SOURCE_DIR}/SDL/lib")
    set(MYSQL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/MySQL/include")
    set(MYSQL_LIBRARY_DIR "${CMAKE_SOURCE_DIR}/MySQL/lib")
endif()

# ImGui
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl2.cpp
	imgui_impl_sdl1.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC 
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/backends
    ${SDL_INCLUDE_DIR}
)

# Configure zlib
set(ZLIB_PUBLIC_HDRS
    ${CMAKE_SOURCE_DIR}/zlib/zconf.h
    ${CMAKE_SOURCE_DIR}/zlib/zlib.h
)
set(ZLIB_PRIVATE_HDRS
    ${CMAKE_SOURCE_DIR}/zlib/crc32.h
    ${CMAKE_SOURCE_DIR}/zlib/deflate.h
    ${CMAKE_SOURCE_DIR}/zlib/inffast.h
    ${CMAKE_SOURCE_DIR}/zlib/inffixed.h
    ${CMAKE_SOURCE_DIR}/zlib/inflate.h
    ${CMAKE_SOURCE_DIR}/zlib/inftrees.h
    ${CMAKE_SOURCE_DIR}/zlib/trees.h
    ${CMAKE_SOURCE_DIR}/zlib/zutil.h
)
file(GLOB zlibfiles zlib/*.c)
add_library(zlib SHARED ${zlibfiles} ${ZLIB_PUBLIC_HDRS} ${ZLIB_PRIVATE_HDRS})
set_target_properties(zlib PROPERTIES DEFINE_SYMBOL ZLIB_DLL)
set_target_properties(zlib PROPERTIES SOVERSION 1)

set(bzipfiles
        ${CMAKE_SOURCE_DIR}/bzip2/compress.c
        ${CMAKE_SOURCE_DIR}/bzip2/crctable.c
        ${CMAKE_SOURCE_DIR}/bzip2/decompress.c
        ${CMAKE_SOURCE_DIR}/bzip2/huffman.c
        ${CMAKE_SOURCE_DIR}/bzip2/randtable.c
        ${CMAKE_SOURCE_DIR}/bzip2/blocksort.c
        ${CMAKE_SOURCE_DIR}/bzip2/bzlib.c
)

add_library(bzip2 SHARED ${bzipfiles})

# Configure libmpq
file(GLOB mpqfiles libmpq/*.c libmpq/*.h *.h)
add_library(libmpq STATIC ${mpqfiles})
target_include_directories(libmpq PRIVATE zlib bzip2)
target_link_libraries(libmpq PRIVATE zlib bzip2)

# Main executable
add_executable(wowmapview WIN32 ${SOURCES} ${HEADERS})

# Set executable properties
set_target_properties(wowmapview PROPERTIES
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Include directories
target_include_directories(wowmapview PRIVATE
    ${SDL_INCLUDE_DIR}
    ${MYSQL_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    zlib
    bzip2
    libmpq
)

# Find required packages
find_package(OpenGL REQUIRED)

# Link libraries
target_link_libraries(wowmapview PRIVATE
    ${CMAKE_SOURCE_DIR}/SDL/lib/SDL.lib
    ${CMAKE_SOURCE_DIR}/SDL/lib/SDLmain.lib
    ${CMAKE_SOURCE_DIR}/MySQL/lib/libmysql.lib
    zlib
    bzip2
    libmpq
    opengl32
    glu32
    imgui
)

if(MSVC)
    target_compile_options(libmpq PRIVATE /wd4103)
    target_include_directories(libmpq PUBLIC ${CMAKE_SOURCE_DIR}/libmpq/win)
endif()

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_SOURCE_DIR}/fonts/arial.info"
        "${CMAKE_SOURCE_DIR}/fonts/arial.tga"
        "${CMAKE_SOURCE_DIR}/SDL/lib/SDL.dll"
        "${CMAKE_SOURCE_DIR}/MySQL/lib/libmysql.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMENT "Copying required files to output directory"
)

# Create batch files after directories are created
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "wowmapview -w -p" > "$<TARGET_FILE_DIR:${PROJECT_NAME}>/windowed_with_patch.cmd"
    COMMAND ${CMAKE_COMMAND} -E echo "wowmapview -w -np" > "$<TARGET_FILE_DIR:${PROJECT_NAME}>/windowed_no_patch.cmd"
    COMMENT "Creating batch files"
)

# Create convenience batch files
file(WRITE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/windowed_with_patch.cmd" "wowmapview -w -p")
file(WRITE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/windowed_no_patch.cmd" "wowmapview -w -np")
