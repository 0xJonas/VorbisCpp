set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
find_package(GTest CONFIG REQUIRED)
add_subdirectory(rapidcheck)

include(GoogleTest)

add_executable(VorbisCppTest
	testCRC.cpp
	../src/util.cpp
)
target_include_directories(VorbisCppTest PUBLIC ../src)
target_link_libraries(VorbisCppTest rapidcheck rapidcheck_gtest GTest::gtest GTest::gtest_main)

gtest_discover_tests(VorbisCppTest)