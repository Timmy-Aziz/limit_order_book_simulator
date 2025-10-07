cmake_minimum_required(VERSION 3.15)
project(LimitOrderBookSimulator VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler-specific options
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /O2")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -march=native")
endif()

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(include)

# Source files
set(LIB_SOURCES
    src/price_level.cpp
    src/order_book.cpp
    src/order_book_simulator.cpp
    src/market_data_simulator.cpp
)

# Create the main library
add_library(lob_lib STATIC ${LIB_SOURCES})
target_link_libraries(lob_lib Threads::Threads)

# Create test executable
add_executable(test_order_book tests/test_order_book.cpp)
target_link_libraries(test_order_book lob_lib Threads::Threads)

# Create benchmark executable
add_executable(benchmark benchmarks/benchmark.cpp)
target_link_libraries(benchmark lob_lib Threads::Threads)

# Create market data simulator executable
add_executable(market_simulator src/market_data_simulator.cpp)
target_link_libraries(market_simulator lob_lib Threads::Threads)

# Create example executable
add_executable(example examples/basic_usage.cpp)
target_link_libraries(example lob_lib Threads::Threads)

# Enable testing
enable_testing()
add_test(NAME OrderBookTests COMMAND test_order_book)

# Install targets
install(TARGETS lob_lib test_order_book benchmark market_simulator example
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

# Install headers
install(DIRECTORY include/ DESTINATION include)

# Package configuration
set(CPACK_PACKAGE_NAME "LimitOrderBookSimulator")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance C++ Limit Order Book Simulator")
set(CPACK_PACKAGE_VENDOR "Timmy Aziz")
set(CPACK_PACKAGE_CONTACT "timmy@example.com")

include(CPack)
