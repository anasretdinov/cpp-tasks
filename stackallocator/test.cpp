#include <sys/resource.h>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstddef>
#include <deque>
#include <iostream>
#include <iterator>
#include <list>
#include <memory>
#include <sstream>
#include <vector>

#include "stackallocator.h"

// template <typename T, typename Alloc = std::allocator<T>>
// using List = std::list<T, Alloc>;

// template <typename T, size_t>
// using StackAllocator = std::allocator<T>;

constexpr size_t kStorageSize = 120'000'000;
StackStorage<kStorageSize> static_storage;

constexpr rlim_t kStackSize = 250 * 1024 * 1024;  // min stack size = 16 MB

namespace {
struct StackProvider {
    StackProvider() {
        struct rlimit rl;
        int result;

        result = getrlimit(RLIMIT_STACK, &rl);
        if (result != 0) {
            std::cerr << "Failed to get current stack size\n";
            std::abort();
        }

        std::cerr << "Limit: " << rl.rlim_cur << '\n';

        if (rl.rlim_cur < kStackSize) {
            rl.rlim_cur = kStackSize;
            result = setrlimit(RLIMIT_STACK, &rl);
            if (result != 0) {
                std::cerr << "Failed to set bigger stack size\n";
                std::abort();
            }
            std::cerr << "Stack size is successfully set to " << kStackSize << '\n';
        } else {
            std::cerr << "Stack size is left at " << rl.rlim_cur << '\n';
        }
    }
} stack_provider;
}  // namespace

template <typename T>
struct IotaIterator {
    T current;

    const T& operator*() const {
        return current;
    }

    const T* operator->() const {
        return &current;
    }

    IotaIterator& operator++() {
        ++current;
        return *this;
    }

    IotaIterator operator++(int) {
        auto copy = current;
        operator++();
        return {copy};
    }

    auto operator<=>(const IotaIterator&) const = default;
};

template <typename T>
struct ReversedIotaIterator {
    T current;

    const T& operator*() const {
        return current;
    }

    const T* operator->() const {
        return &current;
    }

    ReversedIotaIterator& operator++() {
        --current;
        return *this;
    }

    ReversedIotaIterator operator++(int) {
        auto copy = current;
        operator++();
        return {copy};
    }

    auto operator<=>(const ReversedIotaIterator&) const = default;
};

struct CustomAllocatorException {
    const char* const message;

    CustomAllocatorException(const char* const message)
        : message(message) {
    }

    const char* What() const noexcept {
        return message;
    }
};

template <typename T>
struct ExceptionalAllocator {
    using value_type = T;

    size_t time_to_exception;
    std::allocator<T> actual_allocator;

    ExceptionalAllocator(size_t time)
        : time_to_exception(time) {
    }
    template <typename U>
    ExceptionalAllocator(const ExceptionalAllocator<U>& other)
        : time_to_exception(other.time_to_exception) {
    }

    T* allocate(size_t n) {
        if (time_to_exception == 0) {
            throw CustomAllocatorException("this is exceptional");
        }
        if (time_to_exception < n) {
            time_to_exception = 0;
        } else {
            time_to_exception -= n;
        }
        return actual_allocator.allocate(n);
    }

    void deallocate(T* pointer, size_t n) {
        return actual_allocator.deallocate(pointer, n);
    }

    bool operator==(const ExceptionalAllocator& other) const = default;
};

struct Fragile {
    Fragile(int durability, int data)
        : durability(durability),
          data(data) {
    }
    ~Fragile() = default;

    // for std::swap
    Fragile(Fragile&& other)
        : Fragile() {
        *this = other;
    }

    Fragile(const Fragile& other)
        : Fragile() {
        *this = other;
    }

    Fragile& operator=(const Fragile& other) {
        durability = other.durability - 1;
        data = other.data;
        if (durability <= 0) {
            throw 2;
        }
        return *this;
    }

    Fragile& operator=(Fragile&& other) {
        return *this = other;
    }

    int durability = -1;
    int data = -1;

private:
    Fragile() {
    }
};

struct Explosive {
    struct Safeguard {};

    inline static bool exploded = false;

    Explosive()
        : should_explode_(true) {
        throw 1;
    }

    Explosive(Safeguard)
        : should_explode_(false) {
    }

    Explosive(const Explosive&)
        : should_explode_(true) {
        throw 2;
    }

    Explosive& operator=(const Explosive&) {
        return *this;
    }

    ~Explosive() {
        exploded |= should_explode_;
    }

private:
    const bool should_explode_;
};

struct DefaultConstructible {
    DefaultConstructible()
        : data(kDefaultData) {
    }

    int data = kDefaultData;
    inline static const int kDefaultData = 117;
};

struct NotDefaultConstructible {
    NotDefaultConstructible() = delete;
    NotDefaultConstructible(int input)
        : data(input) {
    }
    int data;

    auto operator<=>(const NotDefaultConstructible&) const = default;
};

struct CountedException : public std::exception {};

template <int WhenThrow>
struct Counted {
    inline static int counter = 0;

    Counted() {
        ++counter;
        if (counter == WhenThrow) {
            --counter;
            throw CountedException();
        }
    }

    Counted(const Counted&)
        : Counted() {
    }

    ~Counted() {
        --counter;
    }
};

constexpr size_t kSmallSize = 17;
constexpr size_t kMediumSize = 100;
constexpr size_t kBigSize = 10'000;
constexpr int kNontrivialInt = 42;

template <typename Iter, typename T>
struct CheckIter {
    using Traits = std::iterator_traits<Iter>;

    static_assert(
        std::is_same_v<std::remove_cv_t<typename Traits::value_type>, std::remove_cv_t<T>>);
    static_assert(std::is_same_v<typename Traits::pointer, T*>);
    static_assert(std::is_same_v<typename Traits::reference, T&>);
    static_assert(
        std::is_same_v<typename Traits::iterator_category, std::bidirectional_iterator_tag>);

    static_assert(std::is_same_v<decltype(std::declval<Iter>()++), Iter>);
    static_assert(std::is_same_v<decltype(++std::declval<Iter>()), Iter&>);

    static_assert(std::is_same_v<decltype(std::declval<Iter>()--), Iter>);
    static_assert(std::is_same_v<decltype(--std::declval<Iter>()), Iter&>);

    static_assert(std::is_same_v<decltype(*std::declval<Iter>()), T&>);

    static_assert(std::is_same_v<decltype(std::declval<Iter>() == std::declval<Iter>()), bool>);
    static_assert(std::is_same_v<decltype(std::declval<Iter>() != std::declval<Iter>()), bool>);
};

template <typename T>
void AssignToSelf(T& value) {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
    value = value;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
}

template <typename T, typename Alloc>
void CheckContent(List<T, Alloc>& list, std::initializer_list<T> content) {
    REQUIRE(list.size() == content.size());
    std::vector<T> container{content};
    REQUIRE(std::equal(container.begin(), container.end(), list.begin()));
}

TEST_CASE("Sanity") {
    static_assert(!(std::is_assignable_v<List<int>, std::list<int>> ||
                    std::is_assignable_v<std::list<int>, List<int>>));
}

TEST_CASE("Constructors & Assignment") {
    SECTION("Assertions") {
        using T1 = int;
        using T2 = NotDefaultConstructible;

        static_assert(std::is_default_constructible_v<List<T1>>, "should have default constructor");
        static_assert(std::is_default_constructible_v<List<T2>>, "should have default constructor");
        static_assert(std::is_copy_constructible_v<List<T1>>, "should have copy constructor");
        static_assert(std::is_copy_constructible_v<List<T2>>, "should have copy constructor");
        static_assert(std::is_constructible_v<List<T1>, size_t, const T1&>,
                      "should have constructor from size_t and const T&");
        static_assert(std::is_constructible_v<List<T2>, size_t, const T2&>,
                      "should have constructor from size_t and const T&");

        static_assert(std::is_copy_assignable_v<List<T1>>, "should have assignment operator");
        static_assert(std::is_copy_assignable_v<List<T2>>, "should have assignment operator");
    }

    SECTION("Default") {
        List<int> defaulted;
        REQUIRE(defaulted.empty());
        List<NotDefaultConstructible> without_default;
        REQUIRE(without_default.empty());
    }

    SECTION("Copy") {
        List<NotDefaultConstructible> without_default;
        List<NotDefaultConstructible> copy = without_default;
        REQUIRE(copy.empty());
    }

    SECTION("Size") {
        size_t size = kSmallSize;
        int value = kNontrivialInt;

        List<int> simple(size);
        REQUIRE(((simple.size() == size_t(size)) &&
                 std::all_of(simple.begin(), simple.end(), [](int item) { return item == 0; })));

        List<NotDefaultConstructible> less_simple(size, value);
        REQUIRE(((less_simple.size() == size_t(size)) &&
                 std::all_of(less_simple.begin(), less_simple.end(),
                             [&](const auto& item) { return item.data == value; })));

        List<DefaultConstructible> default_constructor(size);
        REQUIRE(std::all_of(
            default_constructor.begin(), default_constructor.end(),
            [](const auto& item) { return item.data == DefaultConstructible::kDefaultData; }));
    }

    SECTION("Assignment") {
        List<int> first(kSmallSize, kNontrivialInt);
        const auto second_size = kSmallSize - 1;
        List<int> second(kSmallSize - 1, kNontrivialInt - 1);

        first = second;
        REQUIRE(((first.size() == second.size()) && (first.size() == second_size) &&
                 std::equal(first.begin(), first.end(), second.begin())));

        AssignToSelf(second);
        REQUIRE(((first.size() == second.size()) && (first.size() == second_size) &&
                 std::equal(first.begin(), first.end(), second.begin())));
    }

    SECTION("Multiple self-assignment") {
        List<int> list(kBigSize, kNontrivialInt);
        constexpr size_t kIterCount = 10'000'000;
        for (size_t _ = 0; _ < kIterCount; ++_) {
            AssignToSelf(list);
        }
    }
}

TEST_CASE("Modification") {
    SECTION("Push/Pop") {
        List<int> list;

        list.push_back(3);
        list.push_back(4);
        list.push_front(2);
        list.push_back(5);
        list.push_front(1);

        REQUIRE(list.size() == 5);
        REQUIRE(std::equal(list.begin(), list.end(), IotaIterator<int>{1}));

        std::reverse(list.begin(), list.end());

        REQUIRE(list.size() == 5);
        REQUIRE(std::equal(list.begin(), list.end(), ReversedIotaIterator<int>{5}));

        list.pop_front();
        list.pop_back();
        list.pop_back();

        CheckContent(list, {4, 3});
    }

    SECTION("Insert/Erase") {
        List<int> list;

        list.push_back(1);
        list.push_back(2);
        list.push_back(3);
        list.push_back(4);
        list.push_back(5);

        list.insert(list.cbegin(), 6);
        list.insert(list.cend(), 7);

        auto it_b = list.cbegin();
        std::advance(it_b, 3);
        list.insert(it_b, 8);

        CheckContent(list, {6, 1, 2, 8, 3, 4, 5, 7});

        list.erase(list.cbegin());
        list.erase(std::prev(list.cend()));

        auto it_e = list.cend();
        std::advance(it_e, -3);
        list.erase(it_e);

        CheckContent(list, {1, 2, 8, 4, 5});
    }

    SECTION("Iterator increment/decrement") {
        List<int> l;
        l.push_back(3);
        l.push_back(4);
        l.push_back(7);

        auto it = l.begin();
        REQUIRE(it++ == l.begin());
        REQUIRE(*it == 4);
        REQUIRE(--it == l.begin());

        it = l.end();
        REQUIRE(it-- == l.end());
        REQUIRE(*it == 7);
        REQUIRE(++it == l.end());
    }

    SECTION("Erase and iterator change") {
        List<int> l;
        auto it = l.end();
        for (size_t i = 0; i < 10; ++i) {
            it = l.insert(it, i);
        }
        REQUIRE(*l.begin() == 9);
        REQUIRE(*l.rbegin() == 0);

        for (size_t i = 0; i < 10; ++i) {
            l.erase(it++);
        }
        REQUIRE(l.empty());
    }

    SECTION("Exceptions") {
        try {
            List<Counted<kSmallSize>> big_list(kMediumSize);
        } catch (CountedException& e) {
            REQUIRE(Counted<kSmallSize>::counter == 0);
        }

        try {
            List<Explosive> explosive_list(kMediumSize);

            // Will not be executed if exception is rethrown
            REQUIRE(false);
        } catch (...) {
        }

        try {
            List<Explosive> explosive_list;
        } catch (...) {
            // No objects should have been created
            REQUIRE(false);
        }
        REQUIRE(Explosive::exploded == false);

        try {
            List<Explosive> guarded;
            auto safe = Explosive(Explosive::Safeguard{});
            guarded.push_back(safe);
        } catch (...) {
        }

        // Destructor should not be called for an object with not finished constructor. The only
        // destructor called is for explosive with the safeguard
        REQUIRE(Explosive::exploded == false);
    }
}

TEST_CASE("Iterator") {
    SECTION("Checks") {
        std::ignore = CheckIter<typename List<int>::iterator, int>{};
        std::ignore = CheckIter<typename List<int>::const_iterator, const int>{};
        std::ignore = CheckIter<typename List<int>::reverse_iterator, int>{};
        std::ignore = CheckIter<typename List<int>::const_reverse_iterator, const int>{};

        static_assert(std::is_same_v<decltype(List<int>().begin()), List<int>::iterator>);
        static_assert(std::is_same_v<decltype(List<int>().cbegin()), List<int>::const_iterator>);

        static_assert(!std::is_assignable_v<List<int>::iterator, List<int>::const_iterator>);
        static_assert(std::is_assignable_v<List<int>::const_iterator, List<int>::iterator>);
    }
}

TEST_CASE("Allocator") {
    SECTION("Exceptional") {
        using DataT = size_t;
        using Alloc = ExceptionalAllocator<DataT>;

        auto exceptional_list = List<DataT, Alloc>(Alloc(kSmallSize));
        auto& list = exceptional_list;
        for (size_t i = 0; i < kSmallSize; ++i) {
            list.push_back(i);
        }
        // Exactly kSmallSize allocations should have occured, no exceptions

        auto try_modify = [&](auto callback, size_t expected_size) {
            try {
                callback();

                // Never will be executed because of exceptions
                REQUIRE(false);
            } catch (CustomAllocatorException& e) {
                REQUIRE(list.size() == expected_size);
                REQUIRE(std::equal(list.begin(), list.end(), IotaIterator<size_t>{0}));
            }
        };

        try_modify([&] { list.push_back({}); }, kSmallSize);
        try_modify([&] { list.push_front({}); }, kSmallSize);
        try_modify(
            [&] {
                auto iter = list.begin();
                for (size_t i = 0; i < kSmallSize / 2; ++i) {
                    ++iter;
                }
                list.insert(iter, 0);
            },
            kSmallSize);

        while (!list.empty()) {
            list.pop_back();
        }

        try_modify([&] { list.push_back({}); }, 0);
        try_modify([&] { list.push_front({}); }, 0);
    }

    SECTION("StackAllocator") {
        using DataT = size_t;
        using Alloc = StackAllocator<DataT, kBigSize>;

        auto big_storage = StackStorage<kBigSize>();
        auto big_list = List<DataT, Alloc>(Alloc(big_storage));
        auto big_stl_list = std::list<DataT, Alloc>(Alloc(big_storage));
        for (size_t i = 0; i < kMediumSize; ++i) {
            big_list.push_back(i);
            big_stl_list.push_front(i);
        }

        std::reverse(big_list.begin(), big_list.end());
        REQUIRE(std::equal(big_list.begin(), big_list.end(), big_stl_list.begin()));
        REQUIRE(std::equal(big_list.rbegin(), big_list.rend(), IotaIterator<DataT>{DataT(0)}));
    }

    SECTION("Memory limits") {
        using DataT = size_t;
        constexpr size_t kBytesCount = kSmallSize * (sizeof(DataT) + sizeof(void*) + sizeof(void*));
        using Alloc = StackAllocator<DataT, kBytesCount>;

        auto small_storage = StackStorage<kBytesCount>();
        auto small_list = List<DataT, Alloc>(Alloc(small_storage));
        for (size_t i = 0; i < kSmallSize; ++i) {
            small_list.push_front(i);
        }

        try {
            small_list.push_back({});

            // Should fail if not enough memory
            REQUIRE(false);
        } catch (std::bad_alloc& e) {
        } catch (...) {
            // bad_alloc is expected when not enough memory
            REQUIRE(false);
        }
        REQUIRE(small_list.size() == kSmallSize);
        REQUIRE(std::equal(small_list.rbegin(), small_list.rend(), IotaIterator<DataT>{0}));

        try {
            small_list.push_front({});

            // Should fail if not enough memory
            REQUIRE(false);
        } catch (std::bad_alloc& e) {
        } catch (...) {
            // bad_alloc is expected when not enough memory
            REQUIRE(false);
        }
        REQUIRE(small_list.size() == kSmallSize);
        REQUIRE(std::equal(small_list.rbegin(), small_list.rend(), IotaIterator<DataT>{0}));

        // no allocations for empty list
        auto empty_list = List<DataT, Alloc>(Alloc(small_storage));
        try {
            auto new_list = List<DataT, Alloc>(Alloc(small_storage));
            new_list.push_back({});

            REQUIRE(false);
        } catch (std::bad_alloc& e) {
        } catch (...) {
            // bad_alloc is expected when not enough memory
            REQUIRE(false);
        }

        // All allocated data is still valid
        REQUIRE(small_list.size() == kSmallSize);
        REQUIRE(std::equal(small_list.rbegin(), small_list.rend(), IotaIterator<DataT>{0}));
    }
}

namespace by_mesyarik {

template <typename Alloc = std::allocator<int>>
void BasicListTest(Alloc alloc = Alloc()) {
    List<int, Alloc> lst(alloc);

    REQUIRE(lst.empty());

    lst.push_back(3);
    lst.push_back(4);
    lst.push_front(2);
    lst.push_back(5);
    lst.push_front(1);

    std::reverse(lst.begin(), lst.end());
    // now lst is 5 4 3 2 1

    REQUIRE(lst.size() == 5);

    std::string s;
    for (int x : lst) {
        s += std::to_string(x);
    }
    REQUIRE(s == "54321");
    // std::cerr << " check 1.1 ok, list contains 5 4 3 2 1" << std::endl;

    auto cit = lst.cbegin();
    std::advance(cit, 3);

    lst.insert(cit, 6);
    lst.insert(cit, 7);

    std::advance(cit, -3);
    lst.insert(cit, 8);
    lst.insert(cit, 9);
    // now lst is 5 4 8 9 3 6 7 2 1

    REQUIRE(lst.size() == 9);

    s.clear();
    for (int x : lst) {
        s += std::to_string(x);
    }
    REQUIRE(s == "548936721");
    // std::cerr << " check 1.2 ok, list contains 5 4 8 9 3 6 7 2 1" << std::endl;

    lst.erase(lst.cbegin());
    lst.erase(cit);

    lst.pop_front();
    lst.pop_back();

    const auto copy = lst;
    REQUIRE(lst.size() == 5);
    REQUIRE(copy.size() == 5);
    // now both lists are 8 9 6 7 2

    s.clear();
    for (int x : lst) {
        s += std::to_string(x);
    }
    REQUIRE(s == "89672");
    // std::cerr << " check 1.3 ok, list contains 8 9 6 7 2" << std::endl;

    auto rit = lst.rbegin();
    ++rit;
    lst.erase(rit.base());
    REQUIRE(lst.size() == 4);

    rit = lst.rbegin();
    *rit = 3;

    // now lst: 8 9 6 3, copy: 8 9 6 7 2
    s.clear();
    for (int x : lst) {
        s += std::to_string(x);
    }
    REQUIRE(s == "8963");

    REQUIRE(copy.size() == 5);

    s.clear();
    for (int x : copy) {
        s += std::to_string(x);
    }
    REQUIRE(s == "89672");

    // std::cerr << " check 1.4 ok, list contains 8 9 6 3, another list is still 8 9 6 7 2" <<
    // std::endl;

    typename List<int, Alloc>::const_reverse_iterator crit = rit;
    crit = copy.rbegin();
    REQUIRE(*crit == 2);

    cit = crit.base();
    std::advance(cit, -2);
    REQUIRE(*cit == 7);
}

struct VerySpecialType {
    int x = 0;
    explicit VerySpecialType(int x)
        : x(x) {
    }
};

struct NotDefaultConstructible {
    NotDefaultConstructible() = delete;
    NotDefaultConstructible(VerySpecialType x)
        : x(x) {
    }
    VerySpecialType x;
};

struct Accountant {
    // Some field of strange size
    char arr[40];

    static size_t ctor_calls;
    static size_t dtor_calls;

    static void Reset() {
        ctor_calls = 0;
        dtor_calls = 0;
    }

    Accountant() {
        ++ctor_calls;
    }
    Accountant(const Accountant&) {
        ++ctor_calls;
    }

    Accountant& operator=(const Accountant&) {
        // Actually, when it comes to assign one list to another,
        // list can use element-wise assignment instead of destroying nodes and creating new ones
        ++ctor_calls;
        ++dtor_calls;
        return *this;
    }

    Accountant(Accountant&&) = delete;
    Accountant& operator=(Accountant&&) = delete;

    ~Accountant() {
        ++dtor_calls;
    }
};

size_t Accountant::ctor_calls = 0;
size_t Accountant::dtor_calls = 0;

template <typename Alloc = std::allocator<NotDefaultConstructible>>
void TestNotDefaultConstructible(Alloc alloc = Alloc()) {
    List<NotDefaultConstructible, Alloc> lst(alloc);
    REQUIRE(lst.empty());

    lst.push_back(VerySpecialType(0));
    REQUIRE(lst.size() == 1);
    REQUIRE(!lst.empty());

    lst.pop_front();
    REQUIRE(lst.empty());
}

template <typename Alloc = std::allocator<Accountant>>
void TestAccountant(Alloc alloc = Alloc()) {
    Accountant::Reset();

    {
        List<Accountant, Alloc> lst(5, alloc);
        REQUIRE(lst.size() == 5);
        REQUIRE(Accountant::ctor_calls == 5);

        List<Accountant, Alloc> another = lst;
        REQUIRE(another.size() == 5);
        REQUIRE(Accountant::ctor_calls == 10);
        REQUIRE(Accountant::dtor_calls == 0);

        another.pop_back();
        another.pop_front();
        REQUIRE(Accountant::dtor_calls == 2);

        lst = another;  // dtor_calls += 5, ctor_calls += 3
        REQUIRE(another.size() == 3);
        REQUIRE(lst.size() == 3);

        REQUIRE(Accountant::ctor_calls == 13);
        REQUIRE(Accountant::dtor_calls == 7);

    }  // dtor_calls += 6

    REQUIRE(Accountant::ctor_calls == 13);
    REQUIRE(Accountant::dtor_calls == 13);
}

struct ThrowingAccountant : public Accountant {
    static bool need_throw;

    int value = 0;

    ThrowingAccountant(int value = 0)
        : Accountant(),
          value(value) {
        if (need_throw && ctor_calls % 5 == 4) {
            throw std::string("Ahahahaha you have been cocknut");
        }
    }

    ThrowingAccountant(const ThrowingAccountant& other)
        : Accountant(),
          value(other.value) {
        if (need_throw && ctor_calls % 5 == 4) {
            throw std::string("Ahahahaha you have been cocknut");
        }
    }

    ThrowingAccountant& operator=(const ThrowingAccountant& other) {
        value = other.value;
        ++ctor_calls;
        ++dtor_calls;
        if (need_throw && ctor_calls % 5 == 4) {
            throw std::string("Ahahahaha you have been cocknut");
        }
        return *this;
    }
};

bool ThrowingAccountant::need_throw = false;

void TestExceptionSafety() {
    Accountant::Reset();

    ThrowingAccountant::need_throw = true;

    try {
        List<ThrowingAccountant> lst(8);
    } catch (...) {
        REQUIRE(Accountant::ctor_calls == 4);
        REQUIRE(Accountant::dtor_calls == 4);
    }

    ThrowingAccountant::need_throw = false;
    List<ThrowingAccountant> lst(8);

    List<ThrowingAccountant> lst2;
    for (int i = 0; i < 13; ++i) {
        lst2.push_back(i);
    }

    Accountant::Reset();
    ThrowingAccountant::need_throw = true;

    try {
        auto lst3 = lst2;
    } catch (...) {
        REQUIRE(Accountant::ctor_calls == 4);
        REQUIRE(Accountant::dtor_calls == 4);
    }

    Accountant::Reset();

    try {
        lst = lst2;
    } catch (...) {
        REQUIRE(Accountant::ctor_calls == 4);
        REQUIRE(Accountant::dtor_calls == 4);

        // Actually it may not be 8 (although de facto it is), but the only thing we can demand here
        // is the abscence of memory leaks
        //
        // REQUIRE(lst.size() == 8);
    }
}

void TestAlignment() {

    StackStorage<200'000> storage;

    StackAllocator<char, 200'000> charalloc(storage);

    StackAllocator<int, 200'000> intalloc(charalloc);

    auto* pchar = charalloc.allocate(3);

    auto* pint = intalloc.allocate(1);

    REQUIRE((void*)pchar != (void*)pint);

    REQUIRE(reinterpret_cast<uintptr_t>(pint) % sizeof(int) == 0);

    charalloc.deallocate(pchar, 3);

    pchar = charalloc.allocate(555);

    intalloc.deallocate(pint, 1);

    StackAllocator<long double, 200'000> ldalloc(charalloc);

    auto* pld = ldalloc.allocate(25);

    REQUIRE(reinterpret_cast<uintptr_t>(pld) % sizeof(long double) == 0);

    charalloc.deallocate(pchar, 555);
    ldalloc.deallocate(pld, 25);
}

template <typename T, bool PropagateOnConstruct, bool PropagateOnAssign>
struct WhimsicalAllocator : public std::allocator<T> {
    std::shared_ptr<int> number;

    auto select_on_container_copy_construction() const {
        return PropagateOnConstruct
                   ? WhimsicalAllocator<T, PropagateOnConstruct, PropagateOnAssign>()
                   : *this;
    }

    struct propagate_on_container_copy_assignment
        : std::conditional_t<PropagateOnAssign, std::true_type, std::false_type> {};

    template <typename U>
    struct rebind {
        using other = WhimsicalAllocator<U, PropagateOnConstruct, PropagateOnAssign>;
    };

    WhimsicalAllocator()
        : number(std::make_shared<int>(counter)) {
        ++counter;
    }

    template <typename U>
    WhimsicalAllocator(
        const WhimsicalAllocator<U, PropagateOnConstruct, PropagateOnAssign>& another)
        : number(another.number) {
    }

    template <typename U>
    auto& operator=(const WhimsicalAllocator<U, PropagateOnConstruct, PropagateOnAssign>& another) {
        number = another.number;
        return *this;
    }

    template <typename U>
    bool operator==(
        const WhimsicalAllocator<U, PropagateOnConstruct, PropagateOnAssign>& another) const {
        return std::is_same_v<decltype(*this), decltype(another)> && *number == *another.number;
    }

    template <typename U>
    bool operator!=(
        const WhimsicalAllocator<U, PropagateOnConstruct, PropagateOnAssign>& another) const {
        return !(*this == another);
    }

    static size_t counter;
};

template <typename T, bool PropagateOnConstruct, bool PropagateOnAssign>
size_t WhimsicalAllocator<T, PropagateOnConstruct, PropagateOnAssign>::counter = 0;

void TestWhimsicalAllocator() {
    {
        List<int, WhimsicalAllocator<int, true, true>> lst;

        lst.push_back(1);
        lst.push_back(2);

        auto copy = lst;
        REQUIRE(copy.get_allocator() != lst.get_allocator());

        lst = copy;
        REQUIRE(copy.get_allocator() == lst.get_allocator());
    }
    {
        List<int, WhimsicalAllocator<int, false, false>> lst;

        lst.push_back(1);
        lst.push_back(2);

        auto copy = lst;
        REQUIRE(copy.get_allocator() == lst.get_allocator());

        lst = copy;
        REQUIRE(copy.get_allocator() == lst.get_allocator());
    }
    {
        List<int, WhimsicalAllocator<int, true, false>> lst;

        lst.push_back(1);
        lst.push_back(2);

        auto copy = lst;
        REQUIRE(copy.get_allocator() != lst.get_allocator());

        lst = copy;
        REQUIRE(copy.get_allocator() != lst.get_allocator());
    }
}

template <typename Alloc, typename Storage>
void BigTest(Storage& storage) {
    Alloc alloc(storage);

    std::deque<char, Alloc> d(alloc);

    d.push_back(1);
    REQUIRE(d.back() == 1);

    d.resize(2'500'000, 5);
    REQUIRE(d[1'000'000] == 5);

    d.pop_back();
    for (int i = 0; i < 2'000'000; ++i) {
        d.push_back(i % 100);
    }

    REQUIRE(d.size() == 4'499'999);
    REQUIRE(d[4'000'000] == 1);

    for (int i = 0; i < 4'000'000; ++i) {
        d.pop_front();
    }

    REQUIRE(d[400'000] == 1);
}

TEST_CASE("Small tests by mesyarik") {
    SECTION("Basic") {
        BasicListTest<>();

        {
            StackStorage<200'000> storage;
            StackAllocator<int, 200'000> alloc(storage);

            BasicListTest<StackAllocator<int, 200'000>>(alloc);
        }
    }

    SECTION("Accountant") {
        TestAccountant<>();

        {
            StackStorage<200'000> storage;
            StackAllocator<int, 200'000> alloc(storage);

            TestAccountant<StackAllocator<Accountant, 200'000>>(alloc);
        }
    }

    SECTION("Exception safety") {
        TestExceptionSafety();
    }

    SECTION("Alignment") {
        TestAlignment();
    }

    SECTION("NotDefaultConstructible") {
        TestNotDefaultConstructible<>();

        {
            StackStorage<200'000> storage;
            StackAllocator<int, 200'000> alloc(storage);

            TestNotDefaultConstructible<StackAllocator<NotDefaultConstructible, 200'000>>(alloc);
        }
    }

    SECTION("Allocator awareness") {
        TestWhimsicalAllocator();
    }
}

TEST_CASE("Big test By Mesyarik") {
    BigTest<StackAllocator<char, kStorageSize>>(static_storage);
}

template <class List>
int ListPerformanceTest(List&& l) {
    using Clock = std::chrono::high_resolution_clock;

    std::ostringstream oss;

    auto start = Clock::now();

    for (int i = 0; i < 1'000'000; ++i) {
        l.push_back(i);
    }
    auto it = l.begin();
    for (int i = 0; i < 1'000'000; ++i) {
        l.push_front(i);
    }
    oss << *it;

    auto it2 = std::prev(it);
    for (int i = 0; i < 2'000'000; ++i) {
        l.insert(it, i);
        if (i % 534'555 == 0) {
            oss << *it;
        }
    }
    oss << *it;

    for (int i = 0; i < 1'500'000; ++i) {
        l.pop_back();
        if (i % 342'985 == 0) {
            oss << *l.rbegin();
        }
    }
    oss << *l.rbegin();

    for (int i = 0; i < 1'000'000; ++i) {
        l.erase(it2++);
        if (i % 432'098 == 0) {
            oss << *it2;
        }
    }
    oss << *it2;

    for (int i = 0; i < 1'000'000; ++i) {
        l.pop_front();
    }
    oss << *l.begin();

    for (int i = 0; i < 1'000'000; ++i) {
        l.push_back(i);
    }
    oss << *l.rbegin();

    REQUIRE(oss.str() ==
            "00000099999865701331402819710431628058149999904320988641969999991000000999999");

    auto finish = Clock::now();
    return duration_cast<std::chrono::milliseconds>(finish - start).count();
}

template <template <typename, typename> class Container>
void TestPerformance() {

    std::ostringstream oss_first;
    std::ostringstream oss_second;

    int first = 0;
    int second = 0;

    {
        StackStorage<kStorageSize> storage;
        StackAllocator<int, kStorageSize> alloc(storage);

        first = ListPerformanceTest(Container<int, std::allocator<int>>());
        second = ListPerformanceTest(Container<int, StackAllocator<int, kStorageSize>>(alloc));
        std::ignore = first;
        std::ignore = second;
        first = 0, second = 0;
    }

    double mean_stl_alloc = 0.0;
    double mean_stack_alloc = 0.0;

    for (int i = 0; i < 3; ++i) {
        first = ListPerformanceTest(Container<int, std::allocator<int>>());
        mean_stl_alloc += first;
        oss_first << first << " ";

        StackStorage<kStorageSize> storage;
        StackAllocator<int, kStorageSize> alloc(storage);
        second = ListPerformanceTest(Container<int, StackAllocator<int, kStorageSize>>(alloc));
        mean_stack_alloc += second;
        oss_second << second << " ";
    }

    mean_stl_alloc /= 5;
    mean_stack_alloc /= 5;

    std::cerr << " Results with std::allocator: " << oss_first.str()
              << " ms, results with StackAllocator: " << oss_second.str() << " ms " << std::endl;

    REQUIRE(mean_stl_alloc * 0.9 > mean_stack_alloc);
}

TEST_CASE("Benchmark for std::list") {
    TestPerformance<std::list>();
}

TEST_CASE("Benchmark for List") {
    TestPerformance<List>();
}

}  // namespace by_mesyarik
