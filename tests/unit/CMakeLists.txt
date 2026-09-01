add_executable(rillnet_unit_tests
  version_test.cpp
)

target_link_libraries(rillnet_unit_tests
  PRIVATE
    rillnet::rillnet
    GTest::gmock_main
)

rillnet_apply_project_warnings(rillnet_unit_tests)

include(GoogleTest)
gtest_discover_tests(rillnet_unit_tests)