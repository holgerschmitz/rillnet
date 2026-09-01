add_executable(rillnet_unit_tests
  version_test.cpp
  identifiers_test.cpp
  protocol_version_test.cpp
  frame_flags_test.cpp
  frame_test.cpp
  error_test.cpp
  lifecycle_test.cpp
  status_code_test.cpp
  transport_test.cpp
  tcp_transport_test.cpp
  tcp_client_test.cpp
)

target_link_libraries(rillnet_unit_tests
  PRIVATE
    rillnet::rillnet
    GTest::gmock_main
)

rillnet_apply_project_warnings(rillnet_unit_tests)
rillnet_apply_project_sanitizers(rillnet_unit_tests)

include(GoogleTest)
gtest_discover_tests(rillnet_unit_tests)
