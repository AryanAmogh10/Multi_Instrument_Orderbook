add_executable(velox_bench orderbook_bench.cpp)
target_link_libraries(velox_bench PRIVATE velox_core benchmark::benchmark)
velox_set_warnings(velox_bench)
