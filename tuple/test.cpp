#include <catch2/catch_test_macros.hpp>
#include <string>
#include <type_traits>
#include <vector>

#include "tuple.h"

// #include <tuple>

// template <typename... Types>
// using Tuple = std::tuple<Types...>;

// template <typename... Types>
// constexpr auto makeTuple(Types&&... args) {
//     return std::make_tuple(std::forward<Types>(args)...);
// }

// template <typename... Tuples>
// constexpr auto tupleCat(Tuples&&... args) {
//     return std::tuple_cat(std::forward<Tuples>(args)...);
// }

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

struct NeitherDefaultNorCopyConstructible {
    double d;

    NeitherDefaultNorCopyConstructible() = delete;
    NeitherDefaultNorCopyConstructible(double d)
        : d(d) {
    }

    NeitherDefaultNorCopyConstructible(const NeitherDefaultNorCopyConstructible&) = delete;
    NeitherDefaultNorCopyConstructible(NeitherDefaultNorCopyConstructible&&) = default;

    NeitherDefaultNorCopyConstructible& operator=(const NeitherDefaultNorCopyConstructible&) =
        delete;
    NeitherDefaultNorCopyConstructible& operator=(NeitherDefaultNorCopyConstructible&&) = default;
};

struct NotDefaultConstructible {
    int c;
    NotDefaultConstructible() = delete;
    NotDefaultConstructible(int c)
        : c(c) {
    }
};

struct NotCopyableOnlyMovable {
    NotCopyableOnlyMovable(const NotCopyableOnlyMovable&) = delete;
    NotCopyableOnlyMovable& operator=(const NotCopyableOnlyMovable&) = delete;
    NotCopyableOnlyMovable(NotCopyableOnlyMovable&&) = default;
    NotCopyableOnlyMovable& operator=(NotCopyableOnlyMovable&&) = default;
};

struct Accountant {
    inline static int constructed = 0;
    inline static int destructed = 0;
    inline static int copy_constructed = 0;
    inline static int move_constructed = 0;
    inline static int copy_assigned = 0;
    inline static int move_assigned = 0;

    Accountant() {
        ++constructed;
    }
    Accountant(const Accountant&) {
        ++constructed;
        ++copy_constructed;
    }
    Accountant(Accountant&&) {
        ++constructed;
        ++move_constructed;
    }
    Accountant& operator=(const Accountant&) {
        ++copy_assigned;
        return *this;
    }
    Accountant& operator=(Accountant&&) {
        ++move_assigned;
        return *this;
    }
    ~Accountant() {
        ++destructed;
    }
};

template <typename T>
class Debug {
    Debug() = delete;
};

template <typename T>
constexpr bool explicit_test() {
    return requires(Tuple<int, T> t) {
        t = {11, 22};
    };
}

struct ExplicitDefault {
    explicit ExplicitDefault() {
    }
};

template <typename T>
constexpr bool explicit_default_test() {
    return requires(Tuple<int, T> t) {
        t = {{}, {}};
    };
}

template <typename T>
constexpr bool explicit_conversion_test() {
    return requires(Tuple<int, T> t) {
        t = Tuple<double, int>(3.14, 0);
    };
}

TEST_CASE("Explicit") {
    static_assert(explicit_test<int>());
    static_assert(!explicit_test<std::vector<int>>());

    static_assert(explicit_default_test<int>());
    static_assert(!explicit_default_test<ExplicitDefault>());

    static_assert(explicit_conversion_test<int>());
    static_assert(!explicit_conversion_test<std::vector<int>>());
}

// Unfortunately doesn't work with std::get
/*
template <typename T>
constexpr int get_another_test(T& (*ptr)(Tuple<int, double, char>&)
        = static_cast<T& (*)(Tuple<int, double, char>&)> ( &std::get<T, int, double, char>)) {
    return 0;
}

template <typename T, int = get_another_test<std::string>()>
constexpr bool test(int) {
    return false;
}

template <typename T>
constexpr bool test(...) {
    return true;
}

static_assert(test<std::string>(0));
*/

TEST_CASE("DefaultConstructible") {

    static_assert(std::is_default_constructible_v<Tuple<int, int>>);
    static_assert(!std::is_default_constructible_v<Tuple<NotDefaultConstructible, std::string>>);
    static_assert(!std::is_default_constructible_v<Tuple<int, NotDefaultConstructible>>);

    {
        Tuple<int, int> t1;
        REQUIRE(get<0>(t1) == 0);
        REQUIRE(get<1>(t1) == 0);
    }

    {
        Tuple<Accountant, Accountant> t2;
        REQUIRE(Accountant::constructed == 2);
    }
    REQUIRE(Accountant::destructed == 2);
    Accountant::constructed = Accountant::destructed = 0;
}

template <typename T, typename... Args>
constexpr bool IsGettable() {
    return requires(Tuple<Args...> t) {
        get<T>(t);
    };
}

TEST_CASE("Constructible") {
    new_called = delete_called = 0;
    Accountant::copy_constructed = Accountant::move_constructed = Accountant::destructed = 0;
    Accountant::copy_assigned = Accountant::move_assigned = 0;

    static_assert(std::is_constructible_v<Tuple<int, double, char>, int, double, char>);
    static_assert(!std::is_constructible_v<Tuple<NotCopyableOnlyMovable, int>,
                                           const NotCopyableOnlyMovable&, int>);
    static_assert(
        std::is_constructible_v<Tuple<NotCopyableOnlyMovable, int>, NotCopyableOnlyMovable&&, int>);

    static_assert(std::is_constructible_v<Tuple<std::string, int>, const char*, int>);

    static_assert(std::is_constructible_v<Tuple<int&, char&>, int&, char&>);
    static_assert(std::is_constructible_v<Tuple<int&, const char&>, int&, char&>);
    static_assert(!std::is_constructible_v<Tuple<int&, char&>, int&, const char&>);
    static_assert(std::is_constructible_v<Tuple<int&, const char&>, int&, const char&>);
    static_assert(std::is_constructible_v<Tuple<int&, const char&>, int&, char>);

    static_assert(std::is_constructible_v<Tuple<int&&, char&>, int, char&>);
    static_assert(std::is_constructible_v<Tuple<int&&, char&>, int&&, char&>);
    static_assert(!std::is_constructible_v<Tuple<int&, char&&>, int&, char&>);
    static_assert(std::is_constructible_v<Tuple<int&, const char&>, int&, char&&>);

    // Basic
    {
        {
            Accountant a;
            Tuple<Accountant, int> t(a, 5);
            REQUIRE(Accountant::copy_constructed == 1);

            Tuple t2 = t;
            REQUIRE(Accountant::copy_constructed == 2);

            get<1>(t) = 7;
            REQUIRE(get<1>(t) == 7);
            REQUIRE(get<1>(t2) == 5);

            t = t2;
            REQUIRE(Accountant::copy_assigned == 1);
            REQUIRE(Accountant::copy_constructed == 2);
            REQUIRE(get<1>(t) == 5);
            REQUIRE(get<1>(t2) == 5);

            t2 = std::move(t);
            REQUIRE(Accountant::move_assigned == 1);
            REQUIRE(Accountant::copy_assigned == 1);
            REQUIRE(Accountant::move_constructed == 0);
            REQUIRE(Accountant::copy_constructed == 2);

            const auto& t3 = t;
            t2 = std::move(t3);
            REQUIRE(Accountant::move_assigned == 1);
            REQUIRE(Accountant::copy_assigned == 2);
        }
        REQUIRE(Accountant::destructed == 3);
        Accountant::copy_constructed = Accountant::destructed = 0;
        Accountant::copy_assigned = Accountant::move_assigned = 0;
    }

    // Basic refs
    {
        int x = 5;
        double d = 3.14;
        Tuple<int&, double&> t{x, d};

        int y = 6;
        double e = 2.72;
        Tuple<int&, double&> t2{y, e};

        t = t2;  // x changed to 6
        REQUIRE(get<0>(t) == 6);
        REQUIRE(x == 6);

        y = 7;
        REQUIRE(get<0>(t) == 6);
        REQUIRE(get<0>(t2) == 7);

        get<0>(t) = 8;  // x changed to 8
        REQUIRE(x == 8);
        REQUIRE(y == 7);
        REQUIRE(get<0>(t2) == 7);
    }

    // Construction with lvalue refs
    {
        {
            Accountant a;
            int x = 5;
            Tuple<Accountant&, const int&> t(a, x);
            REQUIRE(Accountant::copy_constructed == 0);
            REQUIRE(get<1>(t) == 5);

            x = 7;
            REQUIRE(get<1>(t) == 7);

            Tuple<Accountant, const int> t2 = t;
            REQUIRE(Accountant::copy_constructed == 1);

            static_assert(!std::is_assignable_v<decltype(t2), decltype(t2)>);
            static_assert(!std::is_assignable_v<decltype(t2), decltype(t)>);
            static_assert(!std::is_assignable_v<decltype(get<1>(t)), int>);
            REQUIRE(get<1>(t2) == 7);

            Tuple<Accountant, int> t3 = std::move(t);
            REQUIRE(Accountant::move_constructed == 0);
            REQUIRE(Accountant::copy_constructed == 2);
            REQUIRE(get<1>(t3) == 7);

            Tuple<Accountant&, const int> t4{a, x};
            Tuple<Accountant, int> t5 = std::move(t4);
            REQUIRE(Accountant::move_constructed == 0);
            REQUIRE(Accountant::copy_constructed == 3);

            Tuple<Accountant, int> t6 = std::move(t2);
            REQUIRE(Accountant::move_constructed == 1);
            REQUIRE(Accountant::copy_constructed == 3);

            Tuple<Accountant, const int&> t7{a, x};
            Tuple<Accountant, int> t8 = std::move(t7);
            REQUIRE(Accountant::move_constructed == 2);
            REQUIRE(Accountant::copy_constructed == 4);

            static_assert(!std::is_copy_assignable_v<decltype(t7)>);
            static_assert(!std::is_move_assignable_v<decltype(t7)>);

            t3 = std::move(t2);
            REQUIRE(Accountant::move_assigned == 1);
            REQUIRE(Accountant::copy_assigned == 0);

            get<1>(t3) = 8;

            t3 = std::move(t4);
            REQUIRE(Accountant::move_assigned == 1);
            REQUIRE(Accountant::copy_assigned == 1);
            REQUIRE(get<1>(t4) == 7);
        }
        REQUIRE(Accountant::destructed == 7);
        Accountant::copy_constructed = Accountant::move_constructed = Accountant::destructed = 0;
        Accountant::copy_assigned = Accountant::move_assigned = 0;
    }

    static_assert(!std::is_default_constructible_v<Tuple<Accountant, int&>>);
    static_assert(std::is_copy_constructible_v<Tuple<Accountant, int&>>);
    static_assert(!std::is_copy_constructible_v<Tuple<Accountant, int&&>>);

    // Construction with rvalue refs
    {
        {
            Accountant a;
            int x = 5;
            Tuple<Accountant, int&&> t(std::move(a), std::move(x));
            REQUIRE(Accountant::move_constructed == 1);
            REQUIRE(Accountant::copy_constructed == 0);
            REQUIRE(get<1>(t) == 5);

            x = 7;
            REQUIRE(get<1>(t) == 7);

            get<1>(t) = 8;
            REQUIRE(x == 8);

            Tuple t2 = std::move(t);
            REQUIRE(Accountant::copy_constructed == 0);
            REQUIRE(Accountant::move_constructed == 2);

            REQUIRE(get<1>(t2) == 8);

            Tuple<Accountant&&, int&> t3{std::move(a), x};
            Tuple<Accountant, int> t4 = t3;
            REQUIRE(Accountant::copy_constructed == 1);
            REQUIRE(Accountant::move_constructed == 2);

            Tuple<Accountant, int> t5 = std::move(t3);
            REQUIRE(Accountant::copy_constructed == 1);
            REQUIRE(Accountant::move_constructed == 3);

            x = 15;
            t4 = t3;
            REQUIRE(Accountant::copy_assigned == 1);
            REQUIRE(Accountant::move_assigned == 0);
            REQUIRE(get<1>(t4) == 15);

            t5 = std::move(t3);
            REQUIRE(Accountant::copy_assigned == 1);
            REQUIRE(Accountant::move_assigned == 1);
            REQUIRE(get<1>(t5) == 15);

            get<int>(t4) = 22;

            t3 = t4;
            REQUIRE(Accountant::copy_assigned == 2);
            REQUIRE(Accountant::move_assigned == 1);
            REQUIRE(x == 22);

            get<int>(t5) = 33;

            t3 = std::move(t5);
            REQUIRE(Accountant::copy_assigned == 2);
            REQUIRE(Accountant::move_assigned == 2);
            REQUIRE(x == 33);
        }
        REQUIRE(Accountant::destructed == 5);
    }

    // MoveOnly
    {
        int x = 5;
        Tuple<const int, int&, NeitherDefaultNorCopyConstructible> t(1, x, 3.14);

        static_assert(!std::is_assignable_v<decltype(get<0>(t)), int>);
        static_assert(
            !std::is_assignable_v<decltype(get<0>(t)), NeitherDefaultNorCopyConstructible>);

        get<1>(t) = 7;
        REQUIRE(x == 7);

        Tuple<int&, NeitherDefaultNorCopyConstructible> tt(x, 3.14);
        auto t2 = std::move(tt);
        std::ignore = t2;
    }

    static_assert(!std::is_copy_constructible_v<Tuple<int&, NeitherDefaultNorCopyConstructible>>);
    static_assert(std::is_move_constructible_v<Tuple<int&, NeitherDefaultNorCopyConstructible>>);

    // Ref check
    {
        int x = 5;
        NotDefaultConstructible ndc{1};

        Tuple<NotDefaultConstructible&, int&> tr{ndc, x};

        Tuple<NotDefaultConstructible, int> t2 = tr;

        x = 3;
        REQUIRE(get<1>(tr) == 3);
        REQUIRE(get<1>(t2) == 5);
    }

    static_assert(!std::is_constructible_v<Tuple<std::string&, int&>, Tuple<std::string, int>>);
    static_assert(!std::is_constructible_v<Tuple<std::string&&, int&&>, Tuple<std::string&, int&>>);
    static_assert(!std::is_constructible_v<Tuple<std::string&, int&&>, Tuple<std::string&, int&>>);

    static_assert(
        std::is_constructible_v<Tuple<const std::string&, int&>, Tuple<std::string&, int&>>);
    static_assert(
        !std::is_constructible_v<Tuple<std::string&, int&>, Tuple<const std::string&, int&>>);

    REQUIRE(new_called == 0);
    REQUIRE(delete_called == 0);

    // From pair
    {
        std::pair<int, double> p{1, 3.14};
        Tuple t = p;
        get<int>(t) = 2;
        get<double>(t) = 2.72;

        int x = 1;
        double d = 3.14;
        std::pair<int&, double&> p2{x, d};
        Tuple t2 = p2;
        get<int&>(t2) = 2;
        REQUIRE(x == 2);

        Tuple<int, double> t3 = p2;
        get<int>(t3) = 3;
        REQUIRE(x == 2);

        Tuple<int&&, double&&> t4 = std::move(p);
        get<int&&>(t4) = 5;
        REQUIRE(p.first == 5);
    }

    // Two refs on the same object
    {
        int x = 1;
        Tuple<int, int&, const int&, int&&> t{x, x, x, std::move(x)};

        ++get<int>(t);
        ++get<int&>(t);
        ++get<int&&>(t);
        REQUIRE(get<const int&>(t) == 3);
        REQUIRE(get<int>(t) == 2);
        REQUIRE(get<3>(t) == 3);
        REQUIRE(x == 3);
    }

    // Get with modifiers
    {
        int x = 3;
        int y = 4;
        Tuple<int, int64_t, int&, const int&, int&&> tuple(1, 2, x, x, std::move(y));

        static_assert(std::is_same_v<decltype(get<int&&>(tuple)), int&>);

        static_assert(std::is_same_v<decltype(get<int>(std::move(tuple))), int&&>);
        static_assert(std::is_same_v<decltype(get<int&>(std::move(tuple))), int&>);
        static_assert(std::is_same_v<decltype(get<const int&>(std::move(tuple))), const int&>);

        const auto& const_tuple = tuple;

        static_assert(std::is_same_v<decltype(get<int>(std::move(const_tuple))), const int&&>);
        static_assert(std::is_same_v<decltype(get<int&>(std::move(const_tuple))), int&>);
        static_assert(
            std::is_same_v<decltype(get<const int&>(std::move(const_tuple))), const int&>);
        static_assert(std::is_same_v<decltype(get<int&&>(std::move(const_tuple))), int&&>);
    }

    // Get by type
    {
        Tuple<int> first(12);
        auto second = first;
        REQUIRE(get<0>(first) == get<0>(second));

        second = Tuple<int>(14);
        REQUIRE(get<0>(second) == 14);

        second = first;

        Tuple<double> third{3.14};
        second = third;

        REQUIRE(get<int>(second) == 3);

        first = 1;
        REQUIRE(get<int>(first) == 1);

        // For none or multiple choice there is a compilation error, preferably via SFINAE or
        // constraints
        static_assert(!IsGettable<int, double>());
        static_assert(!IsGettable<int, int, int>());
    }
}

TEST_CASE("Helpers") {
    Accountant::copy_constructed = Accountant::move_constructed = Accountant::destructed = 0;
    Accountant::copy_assigned = Accountant::move_assigned = 0;

    SECTION("MakeTuple") {
        std::string test("test");
        Tuple tuple =
            makeTuple(5, test, std::vector<int>(2, 5), NeitherDefaultNorCopyConstructible(1));

        get<1>(tuple)[1] = 'i';
        REQUIRE(get<1>(tuple) == "tist");
        REQUIRE(test == "test");

        get<2>(tuple)[1] = 2;
        REQUIRE(get<2>(tuple)[1] == 2);
    }

    // SECTION("Tie") {
    //     int value = 5;
    //     std::string test("test");

    //     tie(value, test) = {3, std::string("amogus")};
    //     REQUIRE(value == 3);
    //     REQUIRE(test == "amogus");

    //     static_assert(!std::is_assignable_v<decltype(tie<const int>(std::declval<int>())), int>);
    // }

    SECTION("ForwardAsTuple") {
        int value = 1;
        NeitherDefaultNorCopyConstructible checker(2);
        auto t = forwardAsTuple(value, checker, std::move(checker));
        static_assert(std::is_same_v<decltype(t), Tuple<int&, NeitherDefaultNorCopyConstructible&,
                                                        NeitherDefaultNorCopyConstructible&&>>);
    }

    SECTION("TupleCat") {
        Tuple<Accountant, Accountant, int> t1;

        Tuple<Accountant, Accountant, int, Accountant> t2;

        Tuple<Accountant, Accountant, Accountant> t3;

        auto cat = tupleCat(t1, t2, std::move(t3));

        REQUIRE(Accountant::move_constructed == 3);
        REQUIRE(Accountant::copy_constructed == 5);
    }
}
