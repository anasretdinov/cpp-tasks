#include <cassert>
#include <catch2/catch_test_macros.hpp>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "variant.h"

// template <typename... Args>
// using Variant = std::variant<Args...>;

// using std::get;
// using std::holds_alternative;
// using std::visit;

TEST_CASE("Basic") {
    Variant<int, std::string, double> v = 5;

    static_assert(std::is_assignable_v<decltype(v), float>);
    static_assert(!std::is_assignable_v<decltype(v), std::vector<int>>);

    REQUIRE(get<int>(v) == 5);

    v = "abc";

    REQUIRE(get<std::string>(v) == "abc");

    v = "cde";
    REQUIRE(get<std::string>(v) == "cde");

    v = 5.0;
    REQUIRE(get<double>(v) == 5.0);

    static_assert(!std::is_assignable_v<Variant<float, int>, double>);

    const auto& cv = v;
    static_assert(!std::is_assignable_v<decltype(get<double>(cv)), double>);

    static_assert(std::is_rvalue_reference_v<decltype(get<double>(std::move(v)))>);
    static_assert(std::is_lvalue_reference_v<decltype(get<double>(v))>);
}

TEST_CASE("OverloadingSelection") {
    Variant<int*, char*, std::vector<char>, const int*, bool> v = true;

    REQUIRE(holds_alternative<bool>(v));

    v = std::vector<char>();

    get<std::vector<char>>(v).push_back('x');
    get<std::vector<char>>(v).push_back('y');
    get<std::vector<char>>(v).push_back('z');

    REQUIRE(holds_alternative<std::vector<char>>(v));

    REQUIRE(get<std::vector<char>>(v).size() == 3);

    char c = 'a';
    v = &c;

    REQUIRE(holds_alternative<char*>(v));

    *get<char*>(v) = 'b';

    REQUIRE(*get<char*>(v) == 'b');

    REQUIRE_THROWS(get<int*>(v));

    const int x = 1;
    v = &x;

    REQUIRE((!std::is_assignable_v<decltype(*get<const int*>(v)), int>));
    REQUIRE((std::is_assignable_v<decltype(get<const int*>(v)), int*>));

    try {
        get<int*>(v);
        REQUIRE(false);
    } catch (...) {
        // ok
    }

    const int y = 2;
    get<const int*>(v) = &y;
    REQUIRE(*get<const int*>(v) == 2);

    REQUIRE(!holds_alternative<int*>(v));
    REQUIRE(holds_alternative<const int*>(v));

    int z = 3;

    get<const int*>(v) = &z;
    REQUIRE(!holds_alternative<int*>(v));
    REQUIRE(holds_alternative<const int*>(v));
    REQUIRE(*get<const int*>(v) == 3);

    v = &z;

    REQUIRE(holds_alternative<int*>(v));
    REQUIRE(!holds_alternative<const int*>(v));

    REQUIRE(*get<int*>(v) == 3);

    REQUIRE((!std::is_assignable_v<decltype(get<int*>(v)), const int*>));

    REQUIRE_THROWS(get<const int*>(v));
}

TEST_CASE("CopyMoveConstructorsAssignments") {
    Variant<std::string, char, std::vector<int>> v = "abcdefgh";

    auto vv = v;

    REQUIRE(get<std::string>(vv).size() == 8);
    REQUIRE(get<std::string>(v).size() == 8);

    {
        auto vvv = std::move(v);

        REQUIRE(get<std::string>(v).size() == 0);
        v.emplace<std::vector<int>>({1, 2, 3});

        REQUIRE(get<std::string>(vv).size() == 8);
    }

    v = std::move(vv);
    REQUIRE(get<std::string>(v).size() == 8);

    REQUIRE(get<std::string>(vv).size() == 0);

    vv = 'a';

    REQUIRE(holds_alternative<char>(vv));
    REQUIRE(holds_alternative<std::string>(v));

    get<0>(v).resize(3);
    get<0>(v)[0] = 'b';
    REQUIRE(get<std::string>(v) == "bbc");

    {
        Variant<int, const std::string> vvv = std::move(get<0>(v));

        std::vector<int> vec = {1, 2, 3, 4, 5};

        v = vec;
        REQUIRE(get<2>(v).size() == 5);
        REQUIRE(vec.size() == 5);

        vec[1] = 0;
        REQUIRE(get<std::vector<int>>(v)[1] == 2);

        vvv.emplace<int>(1);
        REQUIRE(holds_alternative<int>(vvv));
    }
}

TEST_CASE("VariantWithConstType") {
    int& (*get_ptr)(Variant<int, double>&) = &get<int, int, double>;

    static_assert(!std::is_invocable_v<decltype(get_ptr), Variant<const int, double>>);

    const int& (*get_const_ptr)(Variant<const int, double>&) = &get<const int, const int, double>;

    static_assert(!std::is_invocable_v<decltype(get_const_ptr), Variant<int, double>>);

    const int& (Variant<const int, double>::*method_const_ptr)(const int&) =
        &Variant<const int, double>::emplace<const int, const int&>;

    static_assert(
        !std::is_invocable_v<decltype(method_const_ptr), Variant<int, double>&, const int&>);

    int& (Variant<int, double>::*method_ptr)(const int&) =
        &Variant<int, double>::emplace<int, const int&>;

    static_assert(
        !std::is_invocable_v<decltype(method_ptr), Variant<const int, double>&, const int&>);

    Variant<const int, /*int,*/ std::string, const std::string, double> v = 1;

    // static_assert(!std::is_assignable_v<decltype(v), int>);

    REQUIRE(holds_alternative<const int>(v));

    v.emplace<std::string>("abcde");

    get<1>(v).resize(1);

    REQUIRE(!holds_alternative<const std::string>(v));

    v.emplace<0>(5);

    REQUIRE(get<0>(v) == 5);
}

struct VerySpecialType {};

template <typename... Ts>
struct Overload : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overload(Ts...) -> Overload<Ts...>;

TEST_CASE("Visit") {
    // Unfortunatelly, long long has no alias in libc++ as int64_t and ssize_t both are aliases for
    // long.
    // NOLINTNEXTLINE
    using long_long_alias = long long;

    std::vector<Variant<char, ssize_t, float, int, double, long_long_alias>> vec_variant = {
        5, '2', 5.4, 100ll, 2011l, 3.5f, 2017};

    auto visitor = Overload{
        [](char) { return 1; },
        [](int) { return 2; },
        [](unsigned int) { return 3; },
        [](ssize_t) { return 4; },
        [](long_long_alias) { return 5; },
        [](auto) { return 6; },
    };

    std::string result;
    for (auto v : vec_variant) {
        result += std::to_string(visit(visitor, v));
    }

    REQUIRE(result == "2165462");

    std::vector<Variant<std::vector<int>, double, std::string>> vec_variant2 = {
        1.5, std::vector<int>{1, 2, 3, 4, 5}, "Hello "};

    result.clear();
    auto display_me = Overload{
        [&result](std::vector<int>& my_vec) {
            for (auto v : my_vec) {
                result += std::to_string(v);
            }
        },
        [&result](auto& arg) {
            if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, std::string>) {
                result += "string";
            } else if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, double>) {
                result += "double";
            }
        },
    };

    for (auto v : vec_variant2) {
        visit(display_me, v);
    }
    REQUIRE(result == "double12345string");
}

struct OneShot {
    OneShot() = default;
    OneShot(const OneShot&) = delete;
    OneShot(OneShot&&) = default;
    OneShot& operator=(const OneShot&) = delete;
    OneShot& operator=(OneShot&&) = default;

    int operator()(auto x) && {
        if constexpr (std::is_same_v<decltype(x), int>) {
            return x * x;
        } else {
            return 42;
        }
    }
};

TEST_CASE("VisitOrGetRvalue") {
    std::vector<Variant<int, std::string>> vec = {"abc", 7};

    std::string result;
    result += std::to_string(visit(OneShot(), vec[0]));
    result += std::to_string(visit(OneShot(), vec[1]));

    REQUIRE(result == "4249");

    auto& var_string = vec[0];
    auto new_string = get<std::string>(std::move(var_string));

    REQUIRE(get<std::string>(var_string).size() == 0);
    REQUIRE(new_string.size() == 3);

    new_string.clear();
    var_string = "cde";

    new_string = get<1>(std::move(var_string));

    REQUIRE(get<1>(var_string).size() == 0);
    REQUIRE(new_string.size() == 3);
}

TEST_CASE("MultipleVisit") {
    // example taken from here https://www.cppstories.com/2018/09/visit-variants/
    struct LightItem {};
    struct HeavyItem {};
    struct FragileItem {};

    Variant<LightItem, HeavyItem> basic_pack1;
    Variant<LightItem, HeavyItem, FragileItem> basic_pack2;

    {
        auto visitor = Overload{[](LightItem&, LightItem&) { return "2 light items"; },
                                [](LightItem&, HeavyItem&) { return "light & heavy items"; },
                                [](HeavyItem&, LightItem&) { return "heavy & light items"; },
                                [](HeavyItem&, HeavyItem&) { return "2 heavy items"; },
                                [](auto, auto) { return "another"; }};

        std::string result = visit(visitor, basic_pack1, basic_pack2);
        REQUIRE(result == "2 light items");

        basic_pack1 = HeavyItem();

        result = visit(visitor, basic_pack1, basic_pack2);

        REQUIRE(result == "heavy & light items");

        basic_pack2 = FragileItem();
        result = visit(visitor, basic_pack1, basic_pack2);

        REQUIRE(result == "another");
    }

    {
        auto visitor = Overload{[](HeavyItem&, FragileItem&) { return "both lvalue"; },
                                [](HeavyItem&&, FragileItem&) { return "rvalue and lvalue"; },
                                [](const HeavyItem&, const FragileItem&) { return "both const"; },
                                [](auto&&, auto&&) { return "another"; }};

        std::string result = visit(visitor, basic_pack1, basic_pack2);

        REQUIRE(result == "both lvalue");

        result = visit(visitor, std::move(basic_pack1), basic_pack2);
        REQUIRE(result == "rvalue and lvalue");

        result = visit(visitor, std::move(basic_pack1), std::move(basic_pack2));
        REQUIRE(result == "another");

        const auto& const_pack1 = basic_pack1;
        const auto& const_pack2 = basic_pack2;

        result = visit(visitor, const_pack1, const_pack2);
        REQUIRE(result == "both const");

        result = visit(visitor, basic_pack1, const_pack2);
        REQUIRE(result == "another");
    }
}

static_assert(
    !std::is_base_of_v<std::variant<VerySpecialType, int>, Variant<VerySpecialType, int>>);