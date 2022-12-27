cmake_minimum_required(VERSION 3.14)

project(ASCIIpatrol LANGUAGES CXX)

option(sound "play sound (Unix, Windows)" true)
option(web "build WASM using emscripten")

# --- auto-ignore build directory
if(NOT EXISTS ${PROJECT_BINARY_DIR}/.gitignore)
  file(WRITE ${PROJECT_BINARY_DIR}/.gitignore "*")
endif()

# --- external libraries
if(web)
  find_path(EMSCRIPTEN_INCLUDE_DIR NAMES emscripten.h REQUIRED)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL OpenWatcom)
  message(STATUS "OpenWatcom: $ENV{WATCOM}
  Host: ${CMAKE_HOST_SYSTEM_NAME} ${CMAKE_HOST_SYSTEM_VERSION}
  Target: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}
  ")
elseif(UNIX)
  find_package(X11 REQUIRED)
  find_package(Threads REQUIRED)
  find_library(XI_LIBRARY NAMES Xi REQUIRED)
  find_path(XI_INCLUDE_DIR NAMES X11/extensions/XInput2.h REQUIRED)

  if(sound)
    find_library(PULSEAUDIO_LIBRARY NAMES pulse REQUIRED)
    find_path(PULSEAUDIO_INCLUDE_DIR NAMES pulse/pulseaudio.h REQUIRED)
  endif()
endif()

# --- main program

add_executable(asciipat
manual.cpp mo3.cpp unmo3.cpp stb_vorbis.cpp
conf.cpp gameover.cpp inter.cpp
twister.cpp game.cpp temp.cpp menu.cpp assets.cpp
)

if(CMAKE_CXX_COMPILER_ID MATCHES "(Clang|GNU)")
  target_compile_options(asciipat PRIVATE -Wno-multichar)
endif()

set_target_properties(asciipat PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS true)

install(TARGETS asciipat)

if(web)
  target_sources(asciipat PRIVATE spec_web.cpp)
	target_include_directories(asciipat PRIVATE ${EMSCRIPTEN_INCLUDE_DIR})
elseif(CMAKE_CXX_COMPILER_ID STREQUAL OpenWatcom)
  target_sources(asciipat PRIVATE spec_dos.cpp)
  target_compile_options(asciipat PRIVATE -bt=dos -bcl=dos)
elseif(UNIX)
  target_sources(asciipat PRIVATE spec_nix.cpp
  $<IF:$<BOOL:${sound}>,pulse_audio.cpp,sound_stub.cpp>
  )
  target_link_libraries(asciipat PRIVATE X11::X11 ${XI_LIBRARY} ${PULSEAUDIO_LIBRARY} Threads::Threads)
  target_include_directories(asciipat PRIVATE ${XI_INCLUDE_DIR} ${PULSEAUDIO_INCLUDE_DIR})
elseif(WIN32)
  target_compile_definitions(asciipat PRIVATE
  "$<$<BOOL:${MSVC}>:NOMINMAX;_CRT_SECURE_NO_WARNINGS>"
  )
  target_sources(asciipat PRIVATE spec_win.cpp
  $<IF:$<BOOL:${sound}>,win_sound.cpp,sound_stub.cpp>
  )
  target_link_libraries(asciipat PRIVATE winmm "$<$<BOOL:${sound}>:dsound;gdi32;dxguid>")
else()
  message(FATAL_ERROR "Unknown platform / cross-compile configuration.
  CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}
  CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}
  ")
endif()
