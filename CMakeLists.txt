cmake_minimum_required(VERSION 3.20)
project(bank)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(libpqxx REQUIRED CONFIG)
find_package(Crow REQUIRED CONFIG)
find_package(jwt-cpp REQUIRED CONFIG)
find_package(OpenSSL REQUIRED)
add_executable(main 
src/main.cpp
src/db_pool.h
src/models.h
src/course.h
)
target_include_directories(main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(main PRIVATE
 nlohmann_json::nlohmann_json
 cpr::cpr
 libpqxx::pqxx
 Crow::Crow
 jwt-cpp::jwt-cpp
 OpenSSL::SSL
 OpenSSL::Crypto
)
if(WIN32)
     target_link_libraries(main PRIVATE ws2_32 mswsock)
endif()