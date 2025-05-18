#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <memory>
#include <numeric>
#include <vector>

#include "function.h"

int new_called = 0;
int delete_called = 0;

void* operator new(size_t n) {
    ++new_called;
    return std::malloc(n);
}

void operator delete(void* ptr) noexcept {
    ++delete_called;
    std::free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    ++delete_called;
    std::free(ptr);
}

struct Helper {
    int method_1(int z) {
        return x + z;
    }
    int method_2(int z) {
        return y + z;
    }

    int x = 3;
    int y = 30;
};

int sum(int x, int y) {
    return x + y;
}

int multiply(int x, int y) {
    return x * y;
}

template <typename R, typename T>
R template_test_function(T t) {
    return static_cast<R>(t);
}

struct AllocatorGuard {
    AllocatorGuard() {
        new_called = delete_called = 0;
    }

    ~AllocatorGuard() {
        REQUIRE(new_called == 0);
        REQUIRE(delete_called == 0);
    }
};

template <template <typename> typename Func, bool IsMoveOnly>
void test_function() {
    SECTION("Works") {
        AllocatorGuard guard;
        Func<int(int, int)> func = sum;
        REQUIRE(func(5, 10) == 15);
    }

    SECTION("Template") {
        AllocatorGuard guard;
        Func<int(double)> func = template_test_function<double, int>;
        REQUIRE(func(123.456) == 123);
    }

    SECTION("Lambda") {
        AllocatorGuard guard;
        auto lambda = [](int a, int b, int c, int d) { return a * b + c * d; };

        Func<int(int, int, int, int)> func = lambda;
        REQUIRE(func(2, 3, 20, 30) == 606);
    }

    SECTION("Pointer") {
        AllocatorGuard guard;
        auto func_ptr = +[]() { return 10; };
        Func<int()> func = func_ptr;
        REQUIRE(func() == 10);
    }

    SECTION("Fields & Methods") {
        AllocatorGuard guard;
        Helper helper;

        Func<int(Helper&, int)> func = &Helper::method_1;
        REQUIRE(func(helper, 10) == 13);

        Func<int&(Helper&)> attr = &Helper::x;
        REQUIRE(attr(helper) == 3);

        attr(helper) = 10;
        REQUIRE(func(helper, 10) == 20);
    }

    SECTION("Change simple object") {
        AllocatorGuard guard;
        Func<int(int, int)> func = sum;
        REQUIRE(func(5, 10) == 15);
        func = multiply;
        REQUIRE(func(5, 10) == 50);
    }

    SECTION("operator=") {
        AllocatorGuard guard;
        Func<int(int, int)> func_1 = sum;
        Func<int(int, int)> func_2 = multiply;
        REQUIRE(func_1(5, 10) == 15);

        if constexpr (!IsMoveOnly) {
            func_1 = func_2;
            REQUIRE(func_1(5, 10) == 50);
            REQUIRE(func_2(5, 10) == 50);
        }

        func_1 = std::move(func_2);
        REQUIRE(func_1(5, 10) == 50);
        REQUIRE(!func_2);

        REQUIRE(std::move(func_1)(5, 10) == 50);
        REQUIRE(func_1(5, 10) == 50);
    }

    SECTION("Change object") {
        AllocatorGuard guard;
        Helper helper;
        Func<int(Helper&, int)> func = &Helper::method_1;
        Func<int&(Helper&)> attr = &Helper::x;

        REQUIRE(func(helper, 10) == 13);
        attr(helper) = 10;
        REQUIRE(func(helper, 10) == 20);

        func = &Helper::method_2;
        REQUIRE(func(helper, 10) == 40);

        attr = &Helper::y;
        REQUIRE(attr(helper) == 30);

        attr(helper) = 100;
        REQUIRE(func(helper, 10) == 110);

        int x = 25;
        auto lambda = [&x](Helper&) -> int& { return x; };
        attr = lambda;

        attr(helper) = 55;
        REQUIRE(x == 55);
    }

    SECTION("Empty function") {
        AllocatorGuard guard;
        Func<int(int, int)> func;
        REQUIRE(func == nullptr);
        REQUIRE(!func);
        REQUIRE_THROWS_AS(func(1, 2), std::bad_function_call);

        func = multiply;
        REQUIRE(func != nullptr);
        REQUIRE(func);

        auto f = std::move(func);
        REQUIRE(func == nullptr);
        REQUIRE(!func);
    }

    SECTION("Invoke") {
        Func<int(int, int)> func = sum;
        REQUIRE(std::invoke(func, 5, 10) == 15);
    }
/*
    SECTION("SmallObjectsOptimization") {
        AllocatorGuard guard;
        if constexpr (!IsMoveOnly) {
            Func func = sum;
            REQUIRE(func(5, 10) == 15);
        }

        if constexpr (!IsMoveOnly) {
            Func func = [](int x) { return x * x * x; };
            REQUIRE(func(10) == 1000);
        }

        {
            Func<int(int)> func = [&](int x) { return x == 0 ? 1 : x * func(x - 1); };
            REQUIRE(func(5) == 120);
        }
    }

    SECTION("Call changes state") {
        std::vector<int> vec = {1, 2, 3};
        Func<int()> func = [&]() { return std::accumulate(vec.begin(), vec.end(), 0); };

        REQUIRE(func() == 6);
        vec.push_back(4);
        REQUIRE(func() == 10);

        Func<int()> func2 = std::move(func);
        REQUIRE(func2() == 10);
    }

    SECTION("Bind") {
        using std::placeholders::_1;
        using std::placeholders::_2;

        auto lambda = [](int a, int b, int c, int d) { return a * b + c * d; };

        Func<int(int, int)> func = std::bind(lambda, 2, _2, _1, 30);
        REQUIRE(func(20, 3) == 606);
    }

    struct TestStructConst {
        void const_method(int) const {
        }
        void nonconst_method(int) {
        }
    };

    static_assert(std::is_constructible_v<Func<void(TestStructConst&, int)>,
                                          decltype(&TestStructConst::const_method)>);
    static_assert(!std::is_constructible_v<Func<void(const TestStructConst&, int)>,
                                           decltype(&TestStructConst::nonconst_method)>);

    static_assert(std::is_assignable_v<Func<void(TestStructConst&, int)>,
                                       decltype(&TestStructConst::const_method)>);
    static_assert(!std::is_assignable_v<Func<void(const TestStructConst&, int)>,
                                        decltype(&TestStructConst::nonconst_method)>);

    static_assert(std::is_assignable_v<Func<void(TestStructConst&, int)>,
                                       Func<void(const TestStructConst&, int)>>);
    static_assert(!std::is_assignable_v<Func<void(const TestStructConst&, int)>,
                                        Func<void(TestStructConst&, int)>>);

    struct TestStructRvalue {
        void usual_method(int) {
        }
        void rvalue_method(int) && {
        }
        void lvalue_method(int) & {
        }
        void const_lvalue_method(int) const& {
        }
    };

    static_assert(std::is_constructible_v<Func<void(TestStructRvalue&&, int)>,
                                          decltype(&TestStructRvalue::usual_method)>);
    static_assert(std::is_constructible_v<Func<void(TestStructRvalue&&, int)>,
                                          decltype(&TestStructRvalue::rvalue_method)>);
    static_assert(!std::is_constructible_v<Func<void(TestStructRvalue&&, int)>,
                                           decltype(&TestStructRvalue::lvalue_method)>);
    static_assert(std::is_constructible_v<Func<void(TestStructRvalue&&, int)>,
                                          decltype(&TestStructRvalue::const_lvalue_method)>);

    static_assert(
        !std::is_invocable_v<Func<void(TestStructRvalue&&, int)>, TestStructRvalue&, int>);
    static_assert(
        std::is_invocable_v<Func<void(TestStructRvalue&&, int)>, TestStructRvalue&&, int>);
    static_assert(
        !std::is_invocable_v<Func<void(TestStructRvalue&, int)>, TestStructRvalue&&, int>);

    static_assert(std::is_assignable_v<Func<void(TestStructRvalue&&, int)>,
                                       decltype(&TestStructRvalue::usual_method)>);
    static_assert(std::is_assignable_v<Func<void(TestStructRvalue&&, int)>,
                                       decltype(&TestStructRvalue::rvalue_method)>);
    static_assert(!std::is_assignable_v<Func<void(TestStructRvalue&&, int)>,
                                        decltype(&TestStructRvalue::lvalue_method)>);
    static_assert(std::is_assignable_v<Func<void(TestStructRvalue&&, int)>,
                                       decltype(&TestStructRvalue::const_lvalue_method)>);

    static_assert(!std::is_assignable_v<Func<void(TestStructRvalue&&, int)>,
                                        Func<void(TestStructRvalue&, int)>>);
    static_assert(!std::is_assignable_v<Func<void(TestStructRvalue&, int)>,
                                        Func<void(TestStructRvalue&&, int)>>);
    static_assert(std::is_assignable_v<Func<void(TestStructRvalue&&, int)>,
                                       Func<void(const TestStructRvalue&, int)>>);
    static_assert(!std::is_assignable_v<Func<void(const TestStructRvalue&, int)>,
                                        Func<void(TestStructRvalue&&, int)>>);

    if constexpr (IsMoveOnly) {
        SECTION("NotCopyable") {
            struct NotCopyable {
                std::unique_ptr<int> p{new int(5)};
                int operator()(int x) {
                    return x + *p;
                }
            };

            NotCopyable nc;
            Func<int(int)> f = std::move(nc);
            REQUIRE(f(5) == 10);

            Func<int(int)> f2 = std::move(f);
            f = NotCopyable{std::make_unique<int>(7)};
            REQUIRE(f(5) == 12);
            REQUIRE(f2(5) == 10);

            // doesn't work for std::function
            // static_assert(!std::is_constructible_v<Func<int(int)>, NotCopyable>);
            // static_assert(!std::is_assignable_v<Func<int(int)>, NotCopyable&>);
        }
    }
        */
}




TEST_CASE("Function") {
    test_function<Function, false>();
}
/*
TEST_CASE("MoveOnlyFunction") {
    test_function<MoveOnlyFunction, true>();
}
 */   

TEST_CASE("My1") {
}
