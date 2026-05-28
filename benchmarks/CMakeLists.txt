add_executable(velox_bench orderbook_bench.cpp)
target_link_libraries(velox_bench PRIVATE velox_core benchmark::benchmark)
velox_set_warnings(velox_bench)

# Standalone latency-percentile profiler (no Google Benchmark dependency).
add_executable(velox_latency latency_profile.cpp)
target_link_libraries(velox_latency PRIVATE velox_core)
velox_set_warnings(velox_latency)
