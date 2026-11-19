#include "session_cases.hpp"
#include <gtest/gtest.h>
class DrivingSessionTest : public testing::TestWithParam<session_tests::Case> {};
TEST_P(DrivingSessionTest, Contract) { EXPECT_NO_THROW(GetParam().run()); }
INSTANTIATE_TEST_SUITE_P(Session, DrivingSessionTest, testing::ValuesIn(session_tests::cases),
    [](const testing::TestParamInfo<session_tests::Case>& info) { return std::string{info.param.name}; });
