#include "multiplication.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Signature") {
    INFO("Do not change Multiply signature");
    STATIC_CHECK(std::is_same_v<decltype(&Multiply), int64_t (*)(int, int)>);
}

TEST_CASE("Simple") {
    CHECK(Multiply(2, 3) == 6);
}

TEST_CASE("Advanced") {
    CHECK(Multiply(999'999'993, -100'000'019) == -100'000'018'299'999'867);
}
