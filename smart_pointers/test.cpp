// #include <algorithm>
#include <catch2/catch_test_macros.hpp>
// #include <vector>
#include <iostream>


#include "smart_pointers.h"

// #include <memory>

// template <typename T>
// using SharedPtr = std::shared_ptr<T>;

// template <typename T>
// using WeakPtr = std::weak_ptr<T>;

// template <typename T>
// using EnableSharedFromThis = std::enable_shared_from_this<T>;

// template <typename T, typename... Args>
// SharedPtr<T> makeShared(Args&&... args) {
//     return std::make_shared<T>(std::forward<Args>(args)...);
// }

// template <typename T, typename Alloc, typename... Args>
// SharedPtr<T> allocateShared(const Alloc& alloc, Args&&... args) {
//     return std::allocate_shared<T>(std::forward<const Alloc>(alloc),
//     std::forward<Args>(args)...);
// }



struct Base {
    virtual ~Base() {
    }
};

struct Derived : public Base {};

template <int Tag>
struct TaggedDestructionCounter {
    inline static int destroyed = 0;

    static int GetTag() {
        return Tag;
    }

    ~TaggedDestructionCounter() {
        ++destroyed;
    }
};

TEST_CASE("SharedPtr") {
    using std::vector;

    auto first_ptr = SharedPtr<vector<int>>(new vector<int>(1'000'000));
    (*first_ptr)[0] = 1;

    vector<int>& vec = *first_ptr;
    auto second_ptr = SharedPtr<vector<int>>(new vector<int>(vec));
    (*second_ptr)[0] = 2;

    SECTION("Swaps") {
        // for (int i = 0; i < 1'000'000; ++i) {
        //     first_ptr.swap(second_ptr);
        // }
        first_ptr->swap(*second_ptr);

        REQUIRE(first_ptr->front() == 2);
        REQUIRE(second_ptr->front() == 1);

        REQUIRE(first_ptr.use_count() == 1);
        REQUIRE(second_ptr.use_count() == 1);

        for (int i = 0; i < 10; ++i) {
            auto third_ptr = SharedPtr<vector<int>>(new vector<int>(vec));
            auto fourth_ptr = second_ptr;
            // REQUIRE(second_ptr.use_count() == 2);
            REQUIRE(fourth_ptr.use_count() == 2);

            fourth_ptr.swap(third_ptr);
            REQUIRE(second_ptr.use_count() == 2);
        }

        REQUIRE(second_ptr.use_count() == 1);
    }

    SECTION("Multiple users") {
        vector<SharedPtr<vector<int>>> ptrs(10, SharedPtr<vector<int>>(first_ptr));
        for (int i = 0; i < 100'000; ++i) {
            ptrs.push_back(ptrs.back());
            ptrs.push_back(SharedPtr<vector<int>>(ptrs.back()));
        }
        REQUIRE(first_ptr.use_count() == 1 + 10 + 200'000);
    }

    SECTION("Reset") {
        first_ptr.reset(new vector<int>());
        second_ptr.reset();
        // std::cout << second_ptr.cblock << ' ' << second_ptr.ptr << " datas\n";
        // std::cout << second_ptr.get_ptr() << " thats get ptr\n";
        SharedPtr<vector<int>>().swap(first_ptr);
        // std::cout << second_ptr.get_ptr() << " thats get ptr\n";
        std::cout << second_ptr.get() << " gett\n";
        REQUIRE(second_ptr.get() == nullptr);
        REQUIRE(second_ptr.get() == nullptr);
    }

    SECTION("Workflow") {
        for (int k = 0; k < 2; ++k) {
            vector<SharedPtr<int>> ptrs;
            for (int i = 0; i < 100'000; ++i) {
                int* p = new int(rand() % 99'999);
                ptrs.push_back(SharedPtr<int>(p));
            }
            std::sort(ptrs.begin(), ptrs.end(), [](auto&& x, auto&& y) { return *x < *y; });
            for (int i = 0; i + 1 < 100'000; ++i) {
                REQUIRE(*(ptrs[i]) <= *(ptrs[i + 1]));
            }
            while (!ptrs.empty()) {
                ptrs.pop_back();
            }
        }
    }

    SECTION("Constness") {
        const SharedPtr<int> sp(new int(42));
        REQUIRE(sp.use_count() == 1);
        REQUIRE(*sp.get() == 42);
        REQUIRE(*sp == 42);
    }

    SECTION("Aliasing constructor") {
        std::cout << std::string(75, '=') << '\n';

        SharedPtr<TaggedDestructionCounter<0>> initial(new TaggedDestructionCounter<0>{});
        TaggedDestructionCounter<1>* unmanaged = new TaggedDestructionCounter<1>{};
        SharedPtr<TaggedDestructionCounter<1>> counter_holder(initial, unmanaged);
        std::cout << std::string(75, '=') << '\n';

        REQUIRE(initial.use_count() == 2);
        REQUIRE(initial->GetTag() == 0);
        REQUIRE(counter_holder.use_count() == 2);
        REQUIRE(counter_holder->GetTag() == 1);
        std::cout << std::string(75, '=') << '\n';

        initial.reset();
        std::cout << std::string(75, '=') << '\n';
        REQUIRE(TaggedDestructionCounter<0>::destroyed == 0);
        REQUIRE(TaggedDestructionCounter<1>::destroyed == 0);
        REQUIRE(counter_holder.use_count() == 1);
        std::cout << std::string(75, '=') << '\n';

        counter_holder.reset();
        std::cout << std::string(75, '=') << '\n';
        REQUIRE(TaggedDestructionCounter<0>::destroyed == 1);
        REQUIRE(TaggedDestructionCounter<1>::destroyed == 0);

        delete unmanaged;
    }
}

struct Node;

struct Next {
    SharedPtr<Node> shared;
    WeakPtr<Node> weak;
    Next(const SharedPtr<Node>& shared)
        : shared(shared) {
    }
    Next(const WeakPtr<Node>& weak)
        : weak(weak) {
    }
};

struct Node {
    static int constructed;
    static int destructed;

    int value;
    Next next;
    Node(int value)
        : value(value),
          next(SharedPtr<Node>()) {
        ++constructed;
    }
    Node(int value, const SharedPtr<Node>& next)
        : value(value),
          next(next) {
        ++constructed;
    }
    Node(int value, const WeakPtr<Node>& next)
        : value(value),
          next(next) {
        ++constructed;
    }
    ~Node() {
        ++destructed;
    }
};

int Node::constructed = 0;
int Node::destructed = 0;

SharedPtr<Node> getCyclePtr(int cycle_size) {
    SharedPtr<Node> head(new Node(0));

    SharedPtr<Node> prev(head);

    for (int i = 1; i < cycle_size; ++i) {
        SharedPtr<Node> current(new Node(i));
        prev->next.shared = current;
        prev = current;
    }
    prev->next.weak = head;
    return head;
}

TEST_CASE("WeakPtr") {
    auto sp = SharedPtr<int>(new int(23));
    WeakPtr<int> weak = sp;

    SECTION("Expired") {
        CHECK_FALSE(weak.expired());

        {
            auto shared = SharedPtr<int>(new int(42));
            weak = shared;
            REQUIRE(weak.use_count() == 1);
            REQUIRE(!weak.expired());
            // std::cout << weak.use_count() << " uc\n";
        }
        // std::cout << weak.cblock << " cblock ptr\n";
        // std::cout << weak.use_count() << " uc\n";
        REQUIRE(weak.use_count() == 0);
        REQUIRE(weak.expired());
    }

    SECTION("Interaction") {
        auto wp = weak;
        REQUIRE(weak.use_count() == 1);
        REQUIRE(wp.use_count() == 1);
        auto wwp = std::move(weak);
        REQUIRE(wwp.use_count() == 1);
        REQUIRE(weak.use_count() == 0);
        REQUIRE(weak.expired());
        // std::cout << dynamic_cast<WeakControlBlock<int>*>(sp.cblock) << "progrev \n";
        auto ssp = wwp.lock();
        REQUIRE(sp.use_count() == 2);
        REQUIRE(ssp.get() == sp.get());

        sp = ssp;
        ssp = sp;
        REQUIRE(ssp.use_count() == 2);

        sp = std::move(ssp);
        REQUIRE(sp.use_count() == 1);

        ssp.reset();  // nothing should happen
        sp.reset();
    }

    SECTION("Stress") {
        unsigned int useless_value = 0;
        for (int i = 0; i < 100'000; ++i) {
            SharedPtr<Node> head = getCyclePtr(8);
            SharedPtr<Node> next_head = head->next.shared;
            REQUIRE(next_head.use_count() == 2);
            useless_value += 19'937 * i * next_head.use_count();

            head.reset();
            REQUIRE(next_head.use_count() == 1);
        }
        std::ignore = useless_value;

        REQUIRE(Node::constructed == 800'000);
        REQUIRE(Node::destructed == 800'000);
    }

    SECTION("Inheritance") {
        SharedPtr<Derived> dsp(new Derived());

        SharedPtr<Base> bsp = dsp;

        WeakPtr<Derived> wdsp = dsp;
        WeakPtr<Base> wbsp = dsp;
        WeakPtr<Base> wwbsp = wdsp;

        // std::cout << dynamic_cast<WeakControlBlock<Base>*>(bsp.cblock) -> ptr << " ???\n";
        // std::cout << dynamic_cast<WeakControlBlock<Derived>*>(wdsp.cblock) -> ptr << " ???\n";
        // std::cout << dynamic_cast<WeakControlBlock<Base>*>(wbsp.cblock) -> ptr << " ???\n";
        // std::cout << dynamic_cast<WeakControlBlock<Base>*>(wwbsp.cblock) -> ptr << " ???\n";


        REQUIRE(dsp.use_count() == 2);

        bsp = std::move(dsp);
        REQUIRE(bsp.use_count() == 1);

        bsp.reset();
        std::cout << " blyat \n";

        REQUIRE(wdsp.expired());
        REQUIRE(wbsp.expired());
        REQUIRE(wwbsp.expired());
    }

    SECTION("Constness") {
        SharedPtr<int> sp(new int(42));
        const WeakPtr<int> wp(sp);
        REQUIRE(!wp.expired());
        auto ssp = wp.lock();
        REQUIRE(ssp.get() != nullptr);
        REQUIRE(ssp.get() == sp.get());
    }
}

struct NeitherDefaultNorCopyConstructible {
    NeitherDefaultNorCopyConstructible() = delete;
    NeitherDefaultNorCopyConstructible(const NeitherDefaultNorCopyConstructible&) = delete;
    NeitherDefaultNorCopyConstructible& operator=(const NeitherDefaultNorCopyConstructible&) =
        delete;

    NeitherDefaultNorCopyConstructible(NeitherDefaultNorCopyConstructible&&) = default;
    NeitherDefaultNorCopyConstructible& operator=(NeitherDefaultNorCopyConstructible&&) = default;

    explicit NeitherDefaultNorCopyConstructible(int x)
        : x(x) {
    }
    int x;
};

struct Accountant {
    static int constructed;
    static int destructed;

    Accountant() {
        ++constructed;
    }
    Accountant(const Accountant&) {
        ++constructed;
    }
    ~Accountant() {
        ++destructed;
    }
};

int Accountant::constructed = 0;
int Accountant::destructed = 0;

int allocated = 0;
int deallocated = 0;

int allocate_called = 0;
int deallocate_called = 0;

int new_called = 0;
int delete_called = 0;

int construct_called = 0;
int destroy_called = 0;

struct VerySpecialType {};

void* operator new(size_t n) {
    ++new_called;
    return std::malloc(n);
}

void operator delete(void* ptr) noexcept {
    ++delete_called;
    std::free(ptr);
}

void* operator new(size_t n, VerySpecialType) {
    return std::malloc(n);
}

void operator delete(void* ptr, VerySpecialType) {
    std::free(ptr);
}

// to prevent compiler warnings
void operator delete(void* ptr, size_t) noexcept {
    ++delete_called;
    std::free(ptr);
}

template <typename T>
struct MyAllocator {
    using value_type = T;

    MyAllocator() = default;

    template <typename U>
    MyAllocator(const MyAllocator<U>&) {
    }

    T* allocate(size_t n) {
        ++allocate_called;
        allocated += n * sizeof(T);
        return static_cast<T*>(::operator new(n * sizeof(T), VerySpecialType()));
    }

    void deallocate(T* p, size_t n) {
        ++deallocate_called;
        deallocated += n * sizeof(T);
        ::operator delete(static_cast<void*>(p), VerySpecialType());
    }

    template <typename U, typename... Args>
    void construct(U* ptr, Args&&... args) {
        ++construct_called;
        ::new (static_cast<void*>(ptr)) U(std::forward<Args>(args)...);
    }

    template <typename U>
    void destroy(U* ptr) {
        ++destroy_called;
        ptr->~U();
    }
};

void InitCounters() {
    allocated = 0;
    deallocated = 0;
    allocate_called = 0;
    deallocate_called = 0;
    construct_called = 0;
    destroy_called = 0;
}

TEST_CASE("[Allocate|Make]Shared") {
    Accountant::constructed = 0;
    Accountant::destructed = 0;

    InitCounters();

    SECTION("NeitherDefaultNorCopyConstructible") {
        new_called = 0;
        delete_called = 0;

        {
            auto sp = makeShared<NeitherDefaultNorCopyConstructible>(
                NeitherDefaultNorCopyConstructible(0));
            REQUIRE(new_called == 1);

            WeakPtr<NeitherDefaultNorCopyConstructible> wp = sp;
            REQUIRE(new_called == 1);

            auto ssp = sp;
            sp.reset();
            REQUIRE(new_called == 1);
            std::cout << " that guy\n";
            REQUIRE(!wp.expired());
            ssp.reset();
            std::cout << delete_called << " after see\n";
            REQUIRE(new_called == 1);

            REQUIRE(wp.expired());
        }

        REQUIRE(new_called == 1);
        REQUIRE(delete_called == 1);
    }

    SECTION("Accountant") {
        new_called = 0;
        delete_called = 0;

        {
            auto sp = makeShared<Accountant>();
            REQUIRE(Accountant::constructed == 1);
            REQUIRE(new_called == 1);
            WeakPtr<Accountant> wp = sp;
            auto ssp = sp;
            sp.reset();
            REQUIRE(Accountant::constructed == 1);
            REQUIRE(Accountant::destructed == 0);

            CHECK_FALSE(wp.expired());
            ssp.reset();
            REQUIRE(Accountant::destructed == 1);
        }

        REQUIRE(new_called == 1);
        REQUIRE(delete_called == 1);
    }
/*
    SECTION("AllocatedShared, MoveOnly") {
        new_called = 0;
        delete_called = 0;

        {
            MyAllocator<NeitherDefaultNorCopyConstructible> alloc;
            auto sp = allocateShared<NeitherDefaultNorCopyConstructible>(
                alloc, NeitherDefaultNorCopyConstructible(0));
            int count = allocated;
            REQUIRE(allocated > 0);
            REQUIRE(allocate_called == 1);

            WeakPtr<NeitherDefaultNorCopyConstructible> wp = sp;
            auto ssp = sp;
            sp.reset();
            REQUIRE(count == allocated);
            REQUIRE(deallocated == 0);

            REQUIRE(!wp.expired());
            ssp.reset();
            REQUIRE(count == allocated);
        }

        REQUIRE(allocated == deallocated);

        REQUIRE(allocate_called == 1);
        REQUIRE(deallocate_called == 1);
        REQUIRE(construct_called == 1);
        REQUIRE(destroy_called == 1);
    }

    SECTION("AllocateShared, Accountant") {
        new_called = 0;
        delete_called = 0;

        {
            MyAllocator<Accountant> alloc;
            auto sp = allocateShared<Accountant>(alloc);
            int count = allocated;
            REQUIRE(allocated > 0);
            REQUIRE(allocate_called == 1);
            REQUIRE(Accountant::constructed == 1);

            WeakPtr<Accountant> wp = sp;
            auto ssp = sp;
            sp.reset();
            REQUIRE(count == allocated);
            REQUIRE(deallocated == 0);
            REQUIRE(Accountant::constructed == 1);
            REQUIRE(Accountant::destructed == 0);

            REQUIRE(!wp.expired());
            ssp.reset();
            REQUIRE(count == allocated);
        }

        REQUIRE(allocated == deallocated);

        REQUIRE(Accountant::constructed == 1);
        REQUIRE(Accountant::destructed == 1);

        REQUIRE(allocate_called == 1);
        REQUIRE(deallocate_called == 1);
        REQUIRE(construct_called == 1);
        REQUIRE(destroy_called == 1);

        REQUIRE(new_called == 0);
        REQUIRE(delete_called == 0);
    }
    */
}

struct Enabled : public EnableSharedFromThis<Enabled> {
    SharedPtr<Enabled> get_shared() {
        return shared_from_this();
    }
};

TEST_CASE("EnableSharedFromThis") {
    SECTION("Manual managment") {
        Enabled e;
        REQUIRE_THROWS(e.get_shared());
    }

    SECTION("Works") {
        auto esp = makeShared<Enabled>();

        auto& e = *esp;
        auto sp = e.get_shared();

        REQUIRE(sp.use_count() == 2);

        esp.reset();
        REQUIRE(sp.use_count() == 1);

        sp.reset();
    }

    SECTION("Manual allocation") {
        SharedPtr<Enabled> esp(new Enabled());
        std::cout << " hm\n";
        auto& e = *esp;
        auto sp = e.get_shared();
        std::cout << " hm\n";

        REQUIRE(sp.use_count() == 2);
        std::cout << " hm\n";
        std::cout << sp.cblock -> spcount << ' ' << sp.cblock -> weakcount << '\n';
        esp.reset();
        std::cout << " hm\n";
        std::cout << sp.cblock -> spcount << ' ' << sp.cblock -> weakcount << '\n';

        REQUIRE(sp.use_count() == 1);
        std::cout << " hm\n";

        sp.reset();
        std::cout << " hm\n";

    }
}

int mother_created = 0;
int mother_destroyed = 0;
int son_created = 0;
int son_destroyed = 0;

struct Mother {
    Mother() {
        ++mother_created;
    }
    virtual ~Mother() {
        ++mother_destroyed;
    }
};

struct Son : public Mother {
    Son() {
        ++son_created;
    }
    virtual ~Son() {
        ++son_destroyed;
    }
};

TEST_CASE("InheritanceDestroy") {
    mother_created = 0;
    mother_destroyed = 0;
    son_created = 0;
    son_destroyed = 0;

    InitCounters();

    SECTION("Basic") {
        {
            SharedPtr<Son> sp(new Son());

            SharedPtr<Mother> mp(new Mother());

            mp = sp;

            sp.reset(new Son());
        }
        REQUIRE(son_created == 2);
        REQUIRE(son_destroyed == 2);
        REQUIRE(mother_created == 3);
        REQUIRE(mother_destroyed == 3);
    }
/*
    SECTION("Custom alloc") {
        {
            MyAllocator<Son> alloc;
            auto sp = allocateShared<Son>(alloc);

            SharedPtr<Mother> mp = sp;

            sp.reset(new Son());
        }
        REQUIRE(son_created == 2);
        REQUIRE(son_destroyed == 2);
        REQUIRE(mother_created == 2);
        REQUIRE(mother_destroyed == 2);

        REQUIRE(allocated == deallocated);
        REQUIRE(allocate_called == 1);
        REQUIRE(deallocate_called == 1);
        REQUIRE(construct_called == 1);
        REQUIRE(destroy_called == 1);
    }
*/
}

int custom_deleter_called = 0;

struct MyDeleter {
    template <typename T>
    void operator()(T*) {
        std::cout << " AIAIAO IFJGFNGFNGF\n";
        ++custom_deleter_called;
    }
};

TEST_CASE("CustomDeleter") {
    MyDeleter deleter;
    int x = 0;

    new_called = 0;
    delete_called = 0;

    {
        SharedPtr<int> sp(&x, deleter);

        auto ssp = std::move(sp);

        auto sssp = ssp;

        ssp = makeShared<int>(5);
    }
    REQUIRE(custom_deleter_called == 1);

    // 1 for ControlBlock in sp and 1 for makeShared
    REQUIRE(new_called == 2);
    REQUIRE(delete_called == 2);

    new_called = 0;
    delete_called = 0;

    InitCounters();

    custom_deleter_called = 0;

    Accountant::constructed = 0;
    Accountant::destructed = 0;

    Accountant acc;
    {
        MyAllocator<Accountant> alloc;
        MyDeleter deleter;

        SharedPtr<Accountant> sp(&acc, deleter, alloc);
        std::cout << Accountant::destructed << " count\n";
        auto ssp = std::move(sp);
        std::cout << Accountant::destructed << " count\n";
        auto sssp = ssp;
        std::cout << Accountant::destructed << " count\n";

        ssp = makeShared<Accountant>();
        std::cout << Accountant::destructed << " count\n";
    }

    REQUIRE(new_called == 1);  // for makeShared
    REQUIRE(delete_called == 1);
    REQUIRE(allocate_called == 1);
    REQUIRE(deallocate_called == 1);
    REQUIRE(allocated == deallocated);

    REQUIRE(Accountant::constructed == 2);
    REQUIRE(Accountant::destructed == 1);

    REQUIRE(construct_called == 0);
    REQUIRE(destroy_called == 0);
    REQUIRE(custom_deleter_called == 1);
}



TEST_CASE("Custom1") {
    SharedPtr<int> p1 = makeShared<int>(5);

    SharedPtr<int> p2 = p1;

    std::cout << p2.use_count() << " ???\n";

    SharedPtr<int> p3(p1);
    std::cout << (p3.use_count());
    std::cout << (*p3);
    p1 = p2;
    std::cout << (*p1);
}

TEST_CASE("Custom2") {
    using std::vector;

    auto first_ptr = SharedPtr<vector<int>>(new vector<int>(1'000'000));
    (*first_ptr)[0] = 1;

    vector<int>& vec = *first_ptr;
    auto second_ptr = SharedPtr<vector<int>>(new vector<int>(vec));
    (*second_ptr)[0] = 2;

    SECTION("Swaps") {
        // for (int i = 0; i < 1'000'000; ++i) {
        //     first_ptr.swap(second_ptr);
        // }
        first_ptr->swap(*second_ptr);

        REQUIRE(first_ptr->front() == 2);
        REQUIRE(second_ptr->front() == 1);

        REQUIRE(first_ptr.use_count() == 1);
        REQUIRE(second_ptr.use_count() == 1);

        for (int i = 0; i < 10; ++i) {
            auto third_ptr = SharedPtr<vector<int>>(new vector<int>(vec));
            auto fourth_ptr = second_ptr;
            // REQUIRE(second_ptr.use_count() == 2);
            REQUIRE(fourth_ptr.use_count() == 2);

            fourth_ptr.swap(third_ptr);
            REQUIRE(second_ptr.use_count() == 2);
        }

        REQUIRE(second_ptr.use_count() == 1);
    }

    SECTION("Multiple users") {
        vector<SharedPtr<vector<int>>> ptrs(10, SharedPtr<vector<int>>(first_ptr));
        for (int i = 0; i < 100'000; ++i) {
            ptrs.push_back(ptrs.back());
            ptrs.push_back(SharedPtr<vector<int>>(ptrs.back()));
        }
        REQUIRE(first_ptr.use_count() == 1 + 10 + 200'000);
    }
}

TEST_CASE("Custom3") {
    SharedPtr<int> p1(new int(5));

    SharedPtr<int> p2 = p1;
    // std::cout << *p2 << ' ' << p2.use_count() << '\n';

    // REQUIRE(p2.use_count() == 2);
}