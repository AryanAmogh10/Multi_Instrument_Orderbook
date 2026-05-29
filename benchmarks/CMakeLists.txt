add_executable(ordbk_bench orderbook_bench.cpp)
target_link_libraries(ordbk_bench PRIVATE ordbk_core benchmark::benchmark)
ordbk_set_warnings(ordbk_bench)

# Standalone latency-percentile profiler (no Google Benchmark dependency).
add_executable(ordbk_latency latency_profile.cpp)
target_link_libraries(ordbk_latency PRIVATE ordbk_core)
ordbk_set_warnings(ordbk_latency)
