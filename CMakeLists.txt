cmake_minimum_required (VERSION 3.10)

project (VorbisCpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

enable_testing()
configure_file(src/VorbisCppConfig.in.h src/VorbisCppConfig.h)

add_executable(VorbisCpp
	src/VorbisCpp.cpp
	src/OggStream.h
	src/OggStream.cpp
	src/util.h
	src/util.cpp
)
if (MSVC)
	target_compile_options(VorbisCpp PUBLIC /W4 /WX)
else()
	target_compile_options(VorbisCpp PUBLIC -Wall -Werror)
endif()
add_subdirectory("test")
