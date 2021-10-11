cmake_minimum_required (VERSION 3.10)

project (VorbisCpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(RELEASE_BUILD "Build in Release mode" OFF)
option(ENABLE_BENCHMARKS "Builds the benchmarks" OFF)

enable_testing()
configure_file(src/VorbisCppConfig.in.h src/VorbisCppConfig.h)

add_executable(VorbisCpp
	src/VorbisCpp.cpp
	src/OggStream.h
	src/OggStream.cpp
	src/util.h
	src/util.cpp
)

if(MSVC)
	target_compile_options(VorbisCpp PUBLIC /W4 /WX)
	if(RELEASE_BUILD)
		target_compile_options(VorbisCpp PUBLIC /O2)
	endif()
else()
	target_compile_options(VorbisCpp PUBLIC -Wall -Werror)
	if(RELEASE_BUILD)
		target_compile_options(VorbisCpp PUBLIC -O2)
	endif()
endif()

if(ENABLE_BENCHMARKS)
	add_subdirectory(benchmarks)
endif()

add_subdirectory("test")
