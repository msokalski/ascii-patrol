set(CMAKE_SYSTEM_NAME DOS)
set(CMAKE_GENERATOR "Watcom WMake")

if(NOT DEFINED ENV{WATCOM})
  message(FATAL_ERROR "WATCOM environment variable not set. Try running dos.ps1 to set it if on Windows.")
endif()

file(TO_CMAKE_PATH "$ENV{WATCOM}" watcom_root)

set(CMAKE_SYSROOT ${watcom_root})

find_program(CMAKE_CXX_COMPILER
NAMES wcl386
HINTS ${watcom_root}/binl64 ${watcom_root}/binl ${watcom_root}/binnt64 ${watcom_root}/binnt
NO_DEFAULT_PATH
REQUIRED
)
