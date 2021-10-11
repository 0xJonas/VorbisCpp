find_package(benchmark CONFIG REQUIRED)

add_executable(VorbisCppBenchmark
	../src/util.cpp
)
target_include_directories(VorbisCppBenchmark PUBLIC ../src)
if(MSVC)
	target_compile_options(VorbisCpp PUBLIC /W4 /WX)
	target_compile_options(VorbisCpp PUBLIC /O2)
else()
	target_compile_options(VorbisCpp PUBLIC -Wall -Werror)
	target_compile_options(VorbisCpp PUBLIC -O2)
endif()
target_link_libraries(VorbisCppBenchmark PRIVATE benchmark::benchmark benchmark::benchmark_main)
