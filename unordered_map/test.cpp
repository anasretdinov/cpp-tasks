#include <algorithm>
#include <cassert>
#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <iterator>
#include <numeric>
#include <ranges>
#include <string>
#include <type_traits>
#include <vector>

#include "unordered_map.h"

// template <typename Key, typename Value, typename Hash = std::hash<Key>,
//           typename EqualTo = std::equal_to<Key>,
//           typename Alloc = std::allocator<std::pair<const Key, Value>>>
// using UnorderedMap = std::unordered_map<Key, Value, Hash, EqualTo, Alloc>;

constexpr size_t operator""_sz(unsigned long long int x) {
    return static_cast<size_t>(x);
}

namespace YandexContest {

// Just a simple SFINAE trick to check CE presence when it's necessary
// Stay tuned, we'll discuss this kind of tricks in our next lectures ;)
template <typename T>
decltype(UnorderedMap<T, T>().cbegin()->second = 0, int()) TestConstIteratorDoesntAllowModification(
    T) {
    assert(false);
}
template <typename... FakeArgs>
void TestConstIteratorDoesntAllowModification(FakeArgs...) {
}

struct VerySpecialType {
    int x = 0;
    explicit VerySpecialType(int x)
        : x(x) {
    }
    VerySpecialType(const VerySpecialType&) = delete;
    VerySpecialType& operator=(const VerySpecialType&) = delete;

    VerySpecialType(VerySpecialType&&) = default;
    VerySpecialType& operator=(VerySpecialType&&) = default;
};

struct NeitherDefaultNorCopyConstructible {
    VerySpecialType x;

    NeitherDefaultNorCopyConstructible() = delete;
    NeitherDefaultNorCopyConstructible(const NeitherDefaultNorCopyConstructible&) = delete;
    NeitherDefaultNorCopyConstructible& operator=(const NeitherDefaultNorCopyConstructible&) =
        delete;

    NeitherDefaultNorCopyConstructible(VerySpecialType&& x)
        : x(std::move(x)) {
    }
    NeitherDefaultNorCopyConstructible(NeitherDefaultNorCopyConstructible&&) = default;
    NeitherDefaultNorCopyConstructible& operator=(NeitherDefaultNorCopyConstructible&&) = default;

    bool operator==(const NeitherDefaultNorCopyConstructible& other) const {
        return x.x == other.x.x;
    }
};

}  // namespace YandexContest

namespace std {
template <>
struct hash<YandexContest::NeitherDefaultNorCopyConstructible> {
    size_t operator()(const YandexContest::NeitherDefaultNorCopyConstructible& x) const {
        return hash<int>()(x.x.x);
    }
};
}  // namespace std

namespace YandexContest {

template <typename T>
struct MyHash {
    size_t operator()(const T& p) const {
        return std::hash<int>()(p.second / p.first);
    }
};

template <typename T>
struct MyEqual {
    bool operator()(const T& x, const T& y) const {
        return y.second / y.first == x.second / x.first;
    }
};

struct OneMoreStrangeStruct {
    int first;
    int second;
};

bool operator==(const OneMoreStrangeStruct&, const OneMoreStrangeStruct&) = delete;

TEST_CASE("Basic") {
    SECTION("Simple") {
        UnorderedMap<std::string, int> m;

        m["aaaaa"] = 5;
        m["bbb"] = 6;
        m.at("bbb") = 7;
        REQUIRE(m.size() == 2_sz);

        REQUIRE(m["aaaaa"] == 5);
        REQUIRE(m["bbb"] == 7);
        REQUIRE(m["ccc"] == 0);

        REQUIRE(m.size() == 3_sz);

        REQUIRE_THROWS(m.at("xxxxxxxx"));
        auto it = m.find("dddd");
        REQUIRE(it == m.end());

        it = m.find("bbb");
        REQUIRE(it->second == 7);
        ++it->second;
        REQUIRE(it->second == 8);

        for (auto& item : m) {
            --item.second;
        }
        REQUIRE(m.at("aaaaa") == 4);

        {
            auto mm = m;
            m = std::move(mm);
        }

        auto res = m.emplace("abcde", 2);
        REQUIRE(res.second);
    }

    SECTION("Iterators") {
        UnorderedMap<double, std::string> m;

        std::vector<double> keys = {0.4, 0.3, -8.32, 7.5, 10.0, 0.0};
        std::vector<std::string> values = {
            "Summer has come and passed",     "The innocent can never last",
            "Wake me up when September ends", "Like my fathers come to pass",
            "Seven years has gone so fast",   "Wake me up when September ends",
        };

        m.reserve(1'000'000);

        for (int i = 0; i < 6; ++i) {
            m.insert({keys[i], values[i]});
        }

        auto beg = m.cbegin();
        std::string s = beg->second;
        auto it = m.begin();
        ++it;
        m.erase(it++);
        it = m.begin();
        m.erase(++it);

        REQUIRE(beg->second == s);
        REQUIRE(m.size() == 4_sz);

        UnorderedMap<double, std::string> mm;
        std::vector<std::pair<const double, std::string>> elements = {
            {3.0, values[0]}, {5.0, values[1]}, {-10.0, values[2]}, {35.7, values[3]}};
        mm.insert(elements.cbegin(), elements.cend());
        s = mm.begin()->second;

        m.insert(mm.begin(), mm.end());
        REQUIRE(mm.size() == 4_sz);
        REQUIRE(mm.begin()->second == s);

        m.reserve(1'000'000);
        REQUIRE(m.size() == 8_sz);
        for (int i = 0; i < 10000; ++i) {
            int64_t h = 0;
            for (auto it = m.cbegin(); it != m.cend(); ++it) {
                h += static_cast<int>(it->first) + static_cast<int>((it->second)[0]);
            }
            std::ignore = h;
        }

        it = m.begin();
        ++it;
        s = it->second;
        for (double d = 100.0; d < 10100.0; d += 0.1) {
            m.emplace(d, "a");
        }
        REQUIRE(it->second == s);

        auto dist = std::distance(it, m.end());
        auto sz = m.size();
        m.erase(it, m.end());
        REQUIRE(sz - dist == m.size());

        for (double d = 200.0; d < 10200.0; d += 0.35) {
            auto it = m.find(d);
            if (it != m.end()) {
                m.erase(it);
            }
        }
    }

    SECTION("No Redundant Copies") {
        UnorderedMap<NeitherDefaultNorCopyConstructible, NeitherDefaultNorCopyConstructible> m;
        m.reserve(10);
        m.emplace(VerySpecialType(0), VerySpecialType(0));
        m.reserve(1'000'000);
        std::pair<NeitherDefaultNorCopyConstructible, NeitherDefaultNorCopyConstructible> p{
            VerySpecialType(1), VerySpecialType(1)};

        m.insert(std::move(p));

        REQUIRE(m.size() == 2_sz);

        m.at(VerySpecialType(1)) = VerySpecialType(0);

        {
            auto mm = std::move(m);
            m = std::move(mm);
        }
        m.erase(m.begin());
        m.erase(m.begin());
        REQUIRE(m.size() == 0_sz);
    }

    SECTION("Custom Hash & Compare") {
        UnorderedMap<std::pair<int, int>, char, MyHash<std::pair<int, int>>,
                     MyEqual<std::pair<int, int>>>
            m;

        m.insert({{1, 2}, 0});
        m.insert({{2, 4}, 1});
        REQUIRE(m.size() == 1_sz);

        m[{3, 6}] = 3;
        REQUIRE(m.at({4, 8}) == 3);

        UnorderedMap<OneMoreStrangeStruct, int, MyHash<OneMoreStrangeStruct>,
                     MyEqual<OneMoreStrangeStruct>>
            mm;
        mm[{1, 2}] = 3;
        REQUIRE(mm.at({5, 10}) == 3);

        mm.emplace(OneMoreStrangeStruct{3, 9}, 2);
        REQUIRE(mm.size() == 2_sz);
        mm.reserve(1'000);
        mm.erase(mm.begin());
        mm.erase(mm.begin());
        for (int k = 1; k < 100; ++k) {
            for (int i = 1; i < 10; ++i) {
                mm.insert({{i, k * i}, 0});
            }
        }
        std::string ans;
        std::string myans;
        for (auto it = mm.cbegin(); it != mm.cend(); ++it) {
            ans += std::to_string(it->second);
            myans += '0';
        }
        REQUIRE(ans == myans);
    }
}

// Finally, some tricky fixtures to test custom allocator.
// Done by professional, don't try to repeat
class Chaste {
private:
    int x_ = 0;
    Chaste() = default;
    Chaste(int x)
        : x_(x) {
    }

    // Nobody can construct me except this guy
    template <typename T>
    friend struct TheChosenOne;

public:
    Chaste(const Chaste&) = default;
    Chaste(Chaste&&) = default;

    bool operator==(const Chaste& other) const {
        return x_ == other.x_;
    }
};

}  // namespace YandexContest

namespace std {
template <>
struct hash<YandexContest::Chaste> {
    size_t operator()(const YandexContest::Chaste& x) const noexcept {
        return std::hash<int>()(reinterpret_cast<const int&>(x));
    }
};
}  // namespace std

namespace YandexContest {

template <typename T>
struct TheChosenOne : public std::allocator<T> {
    TheChosenOne() {
    }

    template <typename U>
    TheChosenOne(const TheChosenOne<U>&) {
    }

    template <typename... Args>
    void construct(T* p, Args&&... args) const {
        new (p) T(std::forward<Args>(args)...);
    }

    void construct(std::pair<const Chaste, Chaste>* p, int a, int b) const {
        new (p) std::pair<const Chaste, Chaste>(Chaste(a), Chaste(b));
    }

    void destroy(T* p) const {
        p->~T();
    }

    template <typename U>
    struct rebind {
        using other = TheChosenOne<U>;
    };
};

TEST_CASE("Special allocator tests") {
    // This container mustn't construct or destroy any objects without using
    // TheChosenOne allocator
    UnorderedMap<Chaste, Chaste, std::hash<Chaste>, std::equal_to<Chaste>,
                 TheChosenOne<std::pair<const Chaste, Chaste>>>
        m;

    m.emplace(0, 0);

    {
        auto mm = m;
        mm.reserve(1'000);
        mm.erase(mm.begin());
    }

    for (int i = 0; i < 1'000'000; ++i) {
        m.emplace(i, i);
    }

    for (int i = 0; i < 500'000; ++i) {
        auto it = m.begin();
        ++it, ++it;
        m.erase(m.begin(), it);
    }
}
}  // namespace YandexContest

namespace Additional {

struct Data {
    int data;

    auto operator<=>(const Data&) const = default;
};

struct Trivial : Data {};
constexpr Trivial operator""_tr(unsigned long long int x) {
    return Trivial{static_cast<int>(x)};
}

struct NonTrivial : Data {
    NonTrivial() = default;
    NonTrivial(int x) {
        data = x;
    }
};
NonTrivial operator""_ntr(unsigned long long int x) {
    return NonTrivial{static_cast<int>(x)};
}

struct NotDefaultConstructible {
    int data;

    NotDefaultConstructible() = delete;
    NotDefaultConstructible(int input)
        : data(input) {
    }

    auto operator<=>(const NotDefaultConstructible&) const = default;
};

struct NeitherDefaultNorCopyConstructible {
    int data;
    const bool moved = false;

    NeitherDefaultNorCopyConstructible() = delete;
    NeitherDefaultNorCopyConstructible(const NeitherDefaultNorCopyConstructible&) = delete;
    NeitherDefaultNorCopyConstructible(NeitherDefaultNorCopyConstructible&& other)
        : data(other.data),
          moved(true) {
    }
    NeitherDefaultNorCopyConstructible(int input)
        : data(input),
          moved(false) {
    }

    auto operator<=>(const NeitherDefaultNorCopyConstructible&) const = default;
};

}  // namespace Additional

namespace std {
template <>
struct hash<Additional::NeitherDefaultNorCopyConstructible> {
    size_t operator()(const Additional::NeitherDefaultNorCopyConstructible& x) const noexcept {
        return std::hash<int>()(x.data);
    }
};
}  // namespace std

namespace Additional {

template <typename T>
struct NotPropagatedCountingAllocator {
    size_t allocates_counter = 0;
    size_t deallocates_counter = 0;

    using propagate_on_container_move_assignment = std::false_type;
    using value_type = T;

    NotPropagatedCountingAllocator() = default;

    template <typename U>
    NotPropagatedCountingAllocator(const NotPropagatedCountingAllocator<U>& other)
        : allocates_counter(other.allocates_counter),
          deallocates_counter(other.deallocates_counter) {
    }

    template <typename U>
    NotPropagatedCountingAllocator(NotPropagatedCountingAllocator<U>&& other)
        : allocates_counter(other.allocates_counter),
          deallocates_counter(other.deallocates_counter) {
        other.allocates_counter = 0;
        other.deallocates_counter = 0;
    }

    bool operator==(const NotPropagatedCountingAllocator<T>&) const {
        return false;
    }

    T* allocate(size_t n) {
        ++allocates_counter;
        return std::allocator<T>().allocate(n);
    }

    void deallocate(T* pointer, size_t n) {
        ++deallocates_counter;
        return std::allocator<T>().deallocate(pointer, n);
    }
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

const size_t kSmallSize = 17;
const size_t kMediumSize = 100;
const size_t kBigSize = 10'000;

namespace ranges = std::ranges;

template <typename Value>
auto make_small_map() {
    UnorderedMap<int, Value> map;
    for (int i = 0; i < static_cast<int>(kSmallSize); ++i) {
        map.emplace(i, Value{i});
    }
    return map;
}

bool maps_equal(const auto& left, const auto& right) {
    return ranges::all_of(left,
                          [&right](const auto& pr) { return right.at(pr.first) == pr.second; });
}

TEST_CASE("Basic iterator concept check") {
    using container_type = UnorderedMap<int, int>;
    using iterator = container_type::iterator;
    using const_iterator = container_type::const_iterator;

    // See https://en.cppreference.com/w/cpp/iterator/input_iterator for more information
    // Also check type aliases and methods from https://en.cppreference.com/w/cpp/named_req/InputIterator
    static_assert(std::input_iterator<iterator>, "Map iterator must satisfy concept 'input_iterator'");
    static_assert(std::input_iterator<const_iterator>, "Map const_iterator must satisfy concept 'input_iterator'");

    // See https://en.cppreference.com/w/cpp/iterator/forward_iterator for more information
    // Also check type aliases and methods from https://en.cppreference.com/w/cpp/named_req/ForwardIterator
    static_assert(std::forward_iterator<iterator>, "Map iterator must satisfy concept 'forward_iterator'");
    static_assert(std::forward_iterator<const_iterator>, "Map const_iterator must satisfy concept 'forward_iterator'");
}

TEST_CASE("Construction & Assignment") {
    SECTION("Default") {
        UnorderedMap<int, int> defaulted;
        REQUIRE(defaulted.size() == 0_sz);
        UnorderedMap<int, NotDefaultConstructible> without_default;
        REQUIRE(without_default.size() == 0_sz);
    }

    SECTION("Copy & Move") {
        {
            auto map = make_small_map<Trivial>();
            auto copy = map;
            REQUIRE(maps_equal(copy, map));
            auto move_copy = std::move(map);
            REQUIRE(maps_equal(copy, move_copy));
            REQUIRE(map.size() == 0_sz);
            REQUIRE(map.empty());
        }
        {
            auto map = make_small_map<NonTrivial>();
            auto copy = map;
            REQUIRE(maps_equal(copy, map));
            auto move_copy = std::move(map);
            REQUIRE(maps_equal(copy, move_copy));
            REQUIRE(map.size() == 0_sz);
            REQUIRE(map.empty());
        }
    }

    SECTION("Assignment") {
        auto map = make_small_map<Trivial>();
        REQUIRE(map.size() == kSmallSize);
        REQUIRE_FALSE(map.empty());
        UnorderedMap<int, Trivial> map2;
        REQUIRE(map2.size() == 0_sz);
        REQUIRE(map2.empty());

        map2 = map;
        REQUIRE(maps_equal(map, map2));
        map2 = std::move(map);
        REQUIRE(map.size() == 0_sz);
        REQUIRE(map2.size() == kSmallSize);
    }

    SECTION("Multiple self-assignment") {
        UnorderedMap<int, Trivial> map{kBigSize};
        constexpr size_t kIterCount = 10'000'000;
        for (size_t _ = 0; _ < kIterCount; ++_) {
            AssignToSelf(map);
        }
    }

    SECTION("Swap") {
        auto map = make_small_map<Trivial>();
        decltype(map) another;
        auto it = map.find(1);
        auto address = &(*it);
        REQUIRE(it->second == 1_tr);
        map.swap(another);
        REQUIRE(it->second == 1_tr);
        REQUIRE(address->second == 1_tr);
    }
}

TEST_CASE("Modifications") {
    SECTION("Emplace") {
        UnorderedMap<int, NonTrivial> map;
        auto [place, did_insert] = map.emplace(1, 1_ntr);
        REQUIRE(map.at(1) == 1_ntr);
        REQUIRE(place == map.begin());
        REQUIRE(did_insert);

        auto [new_place, new_did_insert] = map.emplace(2, 2_ntr);
        // update place as it could be invalidated by rehash
        place = map.find(1);
        REQUIRE(place != new_place);
        REQUIRE(new_did_insert);
        REQUIRE(map.at(2) == 2_ntr);
        REQUIRE(map.at(1) == 1_ntr);

        auto [old_place, reinsert] = map.emplace(1, 3_ntr);
        REQUIRE_FALSE(reinsert);
        REQUIRE(old_place == place);
        REQUIRE(map.at(1) == 1_ntr);
        REQUIRE(map.at(2) == 2_ntr);
    }

    SECTION("Emplace move") {
        UnorderedMap<std::string, std::string> moving_map;
        std::string a = "a";
        std::string b = "b";
        std::string c = "c";

        moving_map.emplace(a, a);
        REQUIRE(a == "a");

        moving_map.emplace(std::move(b), a);
        REQUIRE(a == "a");
        REQUIRE(b.empty());

        moving_map.emplace(std::move(c), std::move(a));
        REQUIRE(a.empty());
        REQUIRE(c.empty());
        REQUIRE(moving_map.size() == 3_sz);
        REQUIRE(moving_map.at("a") == "a");
        REQUIRE(moving_map.at("b") == "a");
        REQUIRE(moving_map.at("c") == "a");
    }

    SECTION("Insert NonTrivial") {
        UnorderedMap<int, NonTrivial> map;

        auto [place, did_insert] = map.insert({1, 1_ntr});
        REQUIRE(map.at(1) == 1_ntr);
        REQUIRE(place == map.begin());
        REQUIRE(did_insert);

        auto [new_place, new_did_insert] = map.insert({2, 2_ntr});
        place = map.find(1);
        REQUIRE_FALSE(place == new_place);
        REQUIRE(new_did_insert);
        REQUIRE(map.at(2) == 2_ntr);
        REQUIRE(map.at(1) == 1_ntr);

        auto [old_place, reinsert] = map.insert({1, 3_ntr});
        REQUIRE_FALSE(reinsert);
        REQUIRE(old_place == place);
        REQUIRE(map.at(1) == 1_ntr);
        REQUIRE(map.at(2) == 2_ntr);
    }

    SECTION("Insert Move") {
        UnorderedMap<std::string, std::string> moving_map;
        using node = std::pair<std::string, std::string>;

        node a{"a", "a"};
        node b{"b", "b"};

        moving_map.insert(a);
        REQUIRE(a.first == "a");
        REQUIRE(moving_map.size() == 1_sz);

        moving_map.insert(std::move(b));
        REQUIRE(b.first.empty());
        REQUIRE(moving_map.size() == 2_sz);

        REQUIRE(moving_map.at("a") == "a");
        REQUIRE(moving_map.at("b") == "b");
    }

    SECTION("Insert Range") {
        UnorderedMap<int, NonTrivial> map;
        std::vector<std::pair<int, NonTrivial>> range;
        for (int i = 0; i < static_cast<int>(kMediumSize); ++i) {
            range.emplace_back(i, NonTrivial{i});
        }
        map.insert(range.begin(), range.end());
        std::vector<int> indices(kSmallSize);
        std::iota(indices.begin(), indices.end(), 0);
        REQUIRE(std::all_of(indices.begin(), indices.end(),
                            [&](int item) { return map.at(item) == NonTrivial{item}; }));
    }

    SECTION("Move Insert Range") {
        UnorderedMap<int, std::string> map;
        std::vector<int> indices(kSmallSize);
        std::iota(indices.begin(), indices.end(), 0);
        std::vector<std::pair<int, std::string>> storage;
        std::transform(indices.begin(), indices.end(), std::back_inserter(storage), [](int idx) {
            return std::pair<const int, std::string>{idx, std::to_string(idx)};
        });
        map.insert(storage.begin(), storage.end());
        REQUIRE(ranges::all_of(storage,
                               [](auto& pr) { return std::to_string(pr.first) == pr.second; }));

        map = UnorderedMap<int, std::string>();
        map.insert(std::move_iterator(storage.begin()), std::move_iterator(storage.end()));
        REQUIRE(storage.size() == kSmallSize);
        REQUIRE(ranges::all_of(storage, [&](auto& p) {
            bool equal = p.second.empty();
            CHECK(equal);
            return equal;
        }));
    }
}

TEST_CASE("Access") {
    SECTION("At and []") {
        std::vector<int> indices(kSmallSize);
        std::iota(indices.begin(), indices.end(), 0);
        std::vector<std::pair<int, std::string>> range;
        std::transform(indices.begin(), indices.end(), std::back_inserter(range), [](int idx) {
            return std::pair<const int, std::string>{idx, std::to_string(idx)};
        });
        std::iota(indices.begin(), indices.end(), 0);

        UnorderedMap<int, std::string> map;
        map.insert(range.begin(), range.end());
        for (int idx : indices) {
            REQUIRE(std::to_string(idx) == map.at(idx));
            REQUIRE(std::to_string(idx) == map[idx]);
        }

        REQUIRE_THROWS(map.at(-1) = "abacaba");
        map[-1] = "abacaba";
        REQUIRE(map.at(-1) == "abacaba");
        map.at(-1) = "qwerty";
        REQUIRE(map[-1] == "qwerty");
    }

    SECTION("Move []") {
        std::vector<int> indices(kSmallSize);
        std::iota(indices.begin(), indices.end(), 0);
        std::vector<std::string> storage;
        std::transform(indices.begin(), indices.end(), std::back_inserter(storage),
                       [](int idx) { return std::to_string(idx); });

        UnorderedMap<std::string, std::string> map;
        map[std::move(storage[0])] = std::move(storage[1]);
        REQUIRE(storage[0].empty());
        REQUIRE(storage[1].empty());
        map[std::move(storage[2])] = storage[3];
        REQUIRE(storage[2].empty());
        REQUIRE(storage[3] == "3");
        map[storage[3]] = std::move(storage[4]);
        REQUIRE(storage[3] == "3");
        REQUIRE(storage[4].empty());
    }

    SECTION("Find") {
        auto map = make_small_map<Trivial>();
        auto existing = map.find(1);
        REQUIRE(existing->second == 1_tr);
        auto non_existing = map.find(-1);
        REQUIRE(non_existing == map.end());
    }

    SECTION("Bucket borders check") {
        UnorderedMap<int, int, decltype([](int element) -> size_t {
                         return static_cast<size_t>(abs(element) % 10);
                     })>
            map;
        map.emplace(1, 1);
        map.emplace(11, 11);
        REQUIRE(map.find(1) != map.end());
        REQUIRE(map.find(11) != map.end());
        REQUIRE(map.find(11) != map.find(1));
    }
}

TEST_CASE("Misc") {
    SECTION("Load factor") {
        auto map = make_small_map<Trivial>();
        auto max_val = std::max_element(map.begin(), map.end(), [](auto& left, auto& right) {
                           return left.first < right.first;
                       })->first;
        REQUIRE(map.load_factor() > 0.0f);
        auto new_load_factor = map.load_factor() / 2.0f;
        map.max_load_factor(new_load_factor);
        for (auto i = max_val + 1; i < max_val + 1 + static_cast<int>(kMediumSize); ++i) {
            auto [_, inserted] = map.emplace(i, Trivial{i});
            REQUIRE(inserted);
            REQUIRE(map.load_factor() > 0.0f);
            REQUIRE(map.load_factor() <= new_load_factor);
        }
    }
}

};  // namespace Additional