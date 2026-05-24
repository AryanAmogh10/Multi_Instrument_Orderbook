add_executable(velox_unit_tests
    unit/smoke_test.cpp
    unit/types_test.cpp
    unit/order_test.cpp
    unit/orderbook_test.cpp
    unit/matching_test.cpp
    unit/instrument_registry_test.cpp
    unit/engine_test.cpp
    unit/spsc_test.cpp
    unit/codec_test.cpp
    unit/framer_test.cpp
    unit/dispatcher_test.cpp
    integration/sharded_engine_test.cpp
    integration/gateway_e2e_test.cpp
    property/invariants_test.cpp
)
target_link_libraries(velox_unit_tests PRIVATE velox_core GTest::gtest_main)
velox_set_warnings(velox_unit_tests)
velox_apply_sanitizers(velox_unit_tests)

include(GoogleTest)
gtest_discover_tests(velox_unit_tests)
