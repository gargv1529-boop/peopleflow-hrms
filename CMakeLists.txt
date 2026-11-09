cmake_minimum_required(VERSION 3.12)
project(PeopleFlowHRMS VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(hrms_core src/HRMS.cpp)
target_include_directories(hrms_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(peopleflow src/main.cpp)
target_link_libraries(peopleflow PRIVATE hrms_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(hrms_tests tests/test_hrms.cpp)
    target_link_libraries(hrms_tests PRIVATE hrms_core)
    add_test(NAME HRMSCoreTests COMMAND hrms_tests)
endif()

# Optional browser-based GUI, served directly by a C++17 HTTP server.
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)
add_executable(peopleflow_web src/web_server.cpp src/Database.cpp)
target_link_libraries(peopleflow_web PRIVATE hrms_core SQLite::SQLite3 OpenSSL::Crypto)
if(WIN32)
    target_link_libraries(peopleflow_web PRIVATE ws2_32)
endif()
