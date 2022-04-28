cmake_minimum_required(VERSION 3.14)

project(ASCIIpatrol LANGUAGES CXX)

option(web "build WASM using emscripten")

# --- auto-ignore build directory
if(NOT EXISTS ${PROJECT_BINARY_DIR}/.gitignore)
  file(WRITE ${PROJECT_BINARY_DIR}/.gitignore "*")
endif()

# --- external libraries
find_package(Threads)

if(web)
  find_path(EMSCRIPTEN_INCLUDE_DIR NAMES emscripten.h REQUIRED)
elseif(UNIX)
  find_package(X11 REQUIRED)
  find_library(XI_LIBRARY NAMES Xi REQUIRED)
  find_path(XI_INCLUDE_DIR NAMES X11/extensions/XInput2.h REQUIRED)
  find_library(PULSEAUDIO_LIBRARY NAMES pulse REQUIRED)
  find_path(PULSEAUDIO_INCLUDE_DIR NAMES pulse/pulseaudio.h REQUIRED)
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

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set_target_properties(asciipat PROPERTIES CXX_STANDARD 98)
# necessary for modern compilers that default to C++11 or newer such as Clang

install(TARGETS asciipat)

if(web)
  target_compile_definitions(asciipat PRIVATE WEB)
  target_sources(asciipat PRIVATE spec_web.cpp)
	target_include_directories(asciipat PRIVATE ${EMSCRIPTEN_INCLUDE_DIR})
elseif(UNIX)
  target_compile_definitions(asciipat PRIVATE NIX)
  target_sources(asciipat PRIVATE spec_nix.cpp)
  target_link_libraries(asciipat PRIVATE X11::X11 ${XI_LIBRARY} ${PULSEAUDIO_LIBRARY})
  target_include_directories(asciipat PRIVATE ${XI_INCLUDE_DIR} ${PULSEAUDIO_INCLUDE_DIR})
elseif(WIN32)
  target_compile_definitions(asciipat PRIVATE WIN
  "$<$<BOOL:${MSVC}>:NOMINMAX;_CRT_SECURE_NO_WARNINGS>"
  )
  target_sources(asciipat PRIVATE spec_win.cpp)
  target_link_libraries(asciipat PRIVATE dsound winmm gdi32 dxguid)
else()
  message(FATAL_ERROR "DOS is not yet handled.")
endif()

if(Threads_FOUND)
  target_link_libraries(asciipat PRIVATE Threads::Threads)
endif()
