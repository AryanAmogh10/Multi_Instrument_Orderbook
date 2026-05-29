add_executable(ordbk_unit_tests
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
    unit/option_contract_test.cpp
    unit/option_chain_test.cpp
    unit/expiry_sweeper_test.cpp
    integration/sharded_engine_test.cpp
    integration/gateway_e2e_test.cpp
    integration/options_e2e_test.cpp
    property/invariants_test.cpp
)
target_link_libraries(ordbk_unit_tests PRIVATE ordbk_core GTest::gtest_main)
ordbk_set_warnings(ordbk_unit_tests)
ordbk_apply_sanitizers(ordbk_unit_tests)

include(GoogleTest)
gtest_discover_tests(ordbk_unit_tests)
