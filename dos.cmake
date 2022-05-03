set(CMAKE_SYSTEM_NAME DOS)
set(CMAKE_GENERATOR "Watcom WMake")

if(NOT DEFINED ENV{WATCOM})
  if(WIN32)
    set(scr dos.ps1)
  else()
    set(scr dos.sh)
  endif()
  message(FATAL_ERROR "WATCOM environment variable not set. Try running ${scr} first.")
endif()

file(TO_CMAKE_PATH "$ENV{WATCOM}" watcom_root)

set(CMAKE_SYSROOT ${watcom_root})

if(WIN32)
  set(bin ${watcom_root}/binnt64 ${watcom_root}/binnt)
else()
  set(bin ${watcom_root}/binl64 ${watcom_root}/binl)
endif()

find_program(CMAKE_CXX_COMPILER
NAMES wcl386
HINTS ${bin}
NO_DEFAULT_PATH
REQUIRED
)
