
#include <memory>
#include <type_traits>

#define private public

template <typename T>
class EnableSharedFromThis;

struct BaseControlBlock {
    size_t spcount = 0, weakcount = 0;

    virtual void delete_inside() {};

    virtual void destroy() {};

    BaseControlBlock()
    : spcount(0)
    , weakcount(0) {
    
    }

    virtual ~BaseControlBlock() = default;
};


template <typename T, typename Deleter, typename Allocator>
struct WeakControlBlock : BaseControlBlock {
    [[no_unique_address]] Deleter d;
    [[no_unique_address]] Allocator a;
    T* ptr;

    WeakControlBlock(T* p, Deleter deleter = Deleter(), Allocator allocator = Allocator()) : d(deleter), a(allocator), ptr(p) {}

    ~WeakControlBlock() override {
        delete_inside();
        
    }

    void delete_inside() override {
        if (!ptr) return;
        d(ptr);
        ptr = nullptr;
    }

    void destroy() override {
        Allocator al = a;
        this -> ~WeakControlBlock();

        using traits = std::allocator_traits<Allocator>;
        using good_alloc_type = typename traits :: template rebind_alloc<WeakControlBlock<T, Deleter, Allocator>>;
        using good_alloc_traits = typename traits :: template rebind_traits<WeakControlBlock<T, Deleter, Allocator>>;
        auto ppt = static_cast<good_alloc_type>(al);
        good_alloc_traits::deallocate(ppt, this, 1);
    }
};


template<typename T, typename Deleter, typename Allocator>
WeakControlBlock<T, Deleter, Allocator>* custom_construct_weak(T* ptr, Deleter d, Allocator a) {
    using traits = std::allocator_traits<Allocator>;
    using good_alloc_type = typename traits :: template rebind_alloc<WeakControlBlock<T, Deleter, Allocator>>;
    using good_alloc_traits = typename traits :: template rebind_traits<WeakControlBlock<T, Deleter, Allocator>>;
    auto ppt = static_cast<good_alloc_type>(a);

    WeakControlBlock<T, Deleter, Allocator>* space = good_alloc_traits::allocate(ppt, 1);
    return new(space) WeakControlBlock<T, Deleter, Allocator>(ptr, d, a);
}

template <typename T, typename Allocator>
struct FatControlBlock : BaseControlBlock {
    template<typename U>
    union DataHolder {
        U val;

        DataHolder() {};
        ~DataHolder() {};

        void destroy_inside() {
            val.~U();
        }
    };

    [[no_unique_address]] Allocator a;
    DataHolder<T> obj;

    template<typename... Args>
    FatControlBlock(Args&&... args) {
        new (&obj.val) T(std::forward<Args>(args)...);
    }

    void delete_inside() override {
        obj.destroy_inside();
    }

    void destroy() override {

        Allocator al = a;
        this -> ~FatControlBlock();

        using traits = std::allocator_traits<Allocator>;
        using good_alloc_type = typename traits :: template rebind_alloc<FatControlBlock<T, Allocator>>;
        using good_alloc_traits = typename traits :: template rebind_traits<FatControlBlock<T, Allocator>>;
        auto ppt = static_cast<good_alloc_type>(al);
        good_alloc_traits::deallocate(ppt, this, 1);

        // this -> ~FatControlBlock();
        // std::allocator<FatControlBlock<T>> al;
        // al.deallocate(this, 1);
    }

    ~FatControlBlock() = default;
};

template <typename T>
void controlblock_delete(BaseControlBlock*& cb) {
    if (cb == nullptr) {
        return;
    }

    cb -> destroy();
    cb = nullptr;
}

template <typename T>
class SharedPtr {
private:
    BaseControlBlock* cblock = nullptr;
    T* ptr = nullptr;

    template<typename U, typename... Args>
    friend SharedPtr<U> makeShared(Args&&... args);

    template<typename U>
    friend class WeakPtr;

    T* get_ptr() {

        return ptr;

        // if (ptr) {
        //     return ptr;
        // }
        // auto cblock_casted = dynamic_cast<FatControlBlock<T>*>(cblock);
        // if (cblock_casted == nullptr) {
        //     return nullptr;
        // } else {
        //     return &(cblock_casted -> obj.val);
        // }
    }
    
    const T* get_ptr() const {
        return ptr;
        // if (ptr) {
        //     return ptr;
        // }
        // auto cblock_casted = dynamic_cast<FatControlBlock<T>*>(cblock);
        // if (cblock_casted == nullptr) {
        //     return nullptr;
        // } else {
        //     return &(cblock_casted -> obj.val);
        // }
    }

    void delete_helper() {
        // std::cout << " big delete helper.\n";

        ptr = nullptr;
        // std::cout << " this is delete helper\n";
        if (!cblock) {
            // типа мувнули/что-то еще
            return;
        }

        if (cblock -> spcount == 1) {
            cblock -> delete_inside();

            if (cblock -> weakcount == 0) {
                controlblock_delete<T>(cblock);
            }
        }
        if (cblock) cblock -> spcount--;
        
    }
public:
    SharedPtr()
    : cblock(nullptr)
    , ptr(nullptr) {
    }

    template<typename Y>
    SharedPtr(Y* ptr) 
    : cblock(custom_construct_weak(static_cast<T*>(ptr), std::default_delete<T>(), std::allocator<T>()))
    , ptr(static_cast<T*>(ptr)) {
        cblock -> spcount++;
        if constexpr (std::is_base_of<EnableSharedFromThis<T>, T>::value) {
            static_cast<EnableSharedFromThis<T>*>(ptr) -> info = *this;
        }
    }

    template<typename Y, typename Deleter>
    SharedPtr(Y* ptr, Deleter d)
    : cblock(custom_construct_weak(static_cast<T*>(ptr), d, std::allocator<T>()))
    , ptr(static_cast<T*>(ptr)) {
        cblock -> spcount++;
        if constexpr (std::is_base_of<EnableSharedFromThis<T>, T>::value) {
            static_cast<EnableSharedFromThis<T>*>(ptr) -> info = *this;
        }
    }

    template<typename Y, typename Deleter, typename Allocator>
    SharedPtr(Y* ptr, Deleter d, Allocator a)
    : cblock(custom_construct_weak(static_cast<T*>(ptr), d, a))
    , ptr(static_cast<T*>(ptr)) {
        cblock -> spcount++;
        if constexpr (std::is_base_of<EnableSharedFromThis<T>, T>::value) {
            static_cast<EnableSharedFromThis<T>*>(ptr) -> info = *this;
        }
    }


    template <typename Y>
    SharedPtr(const SharedPtr<Y>& other) 
    : cblock(other.cblock)
    , ptr(other.ptr) {
        cblock -> spcount++;
        
    }

    SharedPtr(const SharedPtr& other)
    : cblock(other.cblock)
    , ptr(other.ptr) {
        // std::cout << " copy constructor \n";
        // std::cout << cblock << '\n';
        if (cblock) cblock -> spcount++;
    }

    // Aliasing constructor
    template <typename Y>
    SharedPtr(const SharedPtr<Y>& other, T* ptr) 
    : cblock(other.cblock)
    , ptr(ptr) {
        if (cblock) cblock -> spcount++;
    }



    ~SharedPtr() {
        delete_helper();
    }

    template <typename Y>
    SharedPtr(SharedPtr<Y>&& other) 
    : cblock(other.cblock) 
    , ptr(other.ptr) {
        other.cblock = nullptr;
        other.ptr = nullptr;
    }

    template <typename Y>
    SharedPtr& operator=(const SharedPtr<Y>& other) {
        // if (this == &other) {
        //     return *this;
        // }
        delete_helper();
        cblock = other.cblock;
        ptr = other.ptr;
        if (cblock) cblock -> spcount++;
        return *this;
    }

    template <typename Y>
    SharedPtr& operator=(SharedPtr<Y>&& other) {
        // this->swap(other); TODO 
        delete_helper();
        cblock = other.cblock;
        ptr = other.ptr;
        other.cblock = nullptr;
        other.ptr = nullptr;
        return *this;
    }

    SharedPtr(SharedPtr&& other)
    : cblock(other.cblock)
    , ptr(other.ptr) {
        other.cblock = nullptr;
        other.ptr = nullptr;
    }

    SharedPtr& operator=(const SharedPtr& other) {
        if (this == &other) {
            return *this;
        }
        delete_helper();
        cblock = other.cblock;
        ptr = other.ptr;
        if (cblock) cblock -> spcount++;
        return *this;
    }

    SharedPtr& operator=(SharedPtr&& other) {
        delete_helper();
        cblock = other.cblock;
        ptr = other.ptr;
        other.cblock = nullptr;
        other.ptr = nullptr;
        return *this;
    }

    long use_count() const {
        return cblock -> spcount;
    }

    T& operator*() {
        return *get_ptr();
    }

    const T& operator*() const {
        return *get_ptr();
    }

    T* operator->() {
        return get_ptr();
    }

    const T* operator->() const {
        return get_ptr();
    }

    void swap(SharedPtr<T>& other) {
        std::swap(ptr, other.ptr);
        std::swap(cblock, other.cblock);
    }

    template <typename Y>
    void reset(Y* new_obj) {
        delete_helper();
        ptr = static_cast<T*>(new_obj);
        cblock = custom_construct_weak(ptr, std::default_delete<T>(), std::allocator<T>());
        cblock -> spcount++;
    }

    template <typename Y, typename Deleter>
    void reset(Y* new_obj, Deleter d) {
        delete_helper();
        ptr = static_cast<T*>(new_obj);
        cblock = custom_construct_weak(ptr, d, std::allocator<T>());
        cblock -> spcount++;
    }

    template<typename Y, typename Deleter, typename Allocator>
    void reset(Y* new_obj, Deleter d, Allocator a) {
        delete_helper();
        ptr = static_cast<T*>(new_obj);
        cblock = custom_construct_weak(ptr, d, a);
        cblock -> spcount++;
    }

    void reset() {
        // std::cout << " reset calld\n";
        delete_helper();
        cblock = nullptr;
        ptr = nullptr;
    }

    T* get() const {
        return const_cast<T*>(get_ptr());
    }
};

template<typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
    SharedPtr<T> to_return;
    std::allocator<FatControlBlock<T, std::allocator<std::byte>>> al;
    auto mem = al.allocate(1);
    to_return.cblock = new (mem) FatControlBlock<T, std::allocator<std::byte>>(std::forward<Args>(args)...);
    to_return.ptr = &(dynamic_cast<FatControlBlock<T, std::allocator<std::byte>>*>(to_return.cblock) -> obj.val);
    to_return.cblock -> spcount++;

    if constexpr (std::is_base_of<EnableSharedFromThis<T>, T>::value) {
        static_cast<EnableSharedFromThis<T>*>(to_return.ptr) -> info = to_return;
    }
    return to_return;
}

template <typename T>
class WeakPtr {
private:
    BaseControlBlock* cblock = nullptr;
    T* ptr = nullptr;

    void delete_helper() {
        ptr = nullptr;
        if (!cblock) return;
        cblock -> weakcount--;
        if (cblock -> weakcount == 0 && expired()) {
            // тогда надо совсем все сносить
            controlblock_delete<T>(cblock);
            cblock = nullptr;
        }
    }
public:
    WeakPtr() {} // default vals

    template<typename Y>
    WeakPtr(const SharedPtr<Y>& sp) 
    : cblock(sp.cblock)
    , ptr(sp.ptr) {
        cblock -> weakcount++;
    }

    template <typename Y>
    WeakPtr(const WeakPtr<Y>& other)
    : cblock(other.cblock)
    , ptr(other.ptr) {
        cblock -> weakcount++;
    }

    WeakPtr(const WeakPtr& other)
    : cblock(other.cblock)
    , ptr(other.ptr) {
        cblock -> weakcount++;
    }

    template <typename Y>
    WeakPtr& operator=(const WeakPtr<Y>& other) {
        if (this == &other) {
            return *this;
        }
        delete_helper();

        cblock = other.cblock;
        ptr = other.ptr;

        cblock -> weakcount++;
        return *this;
    }


    WeakPtr& operator=(const WeakPtr& other) {
        if (this == &other) {
            return *this;
        }
        delete_helper();

        cblock = other.cblock;
        ptr = other.ptr;

        cblock -> weakcount++;
        return *this;
    }

    template <typename Y>
    WeakPtr(WeakPtr<Y>&& other)
    : cblock(other.cblock) 
    , ptr(other.ptr) {
        other.cblock = nullptr;
        other.ptr = nullptr;
    }

    WeakPtr(WeakPtr&& other)
    : cblock(other.cblock)
    , ptr(other.ptr) {
        // std::cout << " there1\n";
        other.cblock = nullptr;
        other.ptr = nullptr;
    }

    template <typename Y>
    WeakPtr& operator=(WeakPtr<Y>&& other) {
        delete_helper();
        cblock = other.cblock;
        ptr = other.ptr;
        other.cblock = nullptr;
        other.ptr = nullptr;
        return *this;
    }

    WeakPtr& operator=(WeakPtr&& other) {
        delete_helper();
        cblock = other.cblock;
        ptr = other.ptr;
        other.ptr = nullptr;
        other.cblock = nullptr;
        return *this;
    }


    bool expired() const noexcept {
        return (!cblock || cblock -> spcount == 0);
    }

    SharedPtr<T> lock() const noexcept {
        SharedPtr<T> to_return;
        if (!expired()) {
            to_return.cblock = cblock;
            to_return.ptr = ptr;
            to_return.cblock -> spcount++;
        }
        return to_return;
    }

    ~WeakPtr() {
        delete_helper();
    }

    long use_count() const {
        if (!cblock) {
            // допустим, мувнули
            return 0;
        }
        return cblock -> spcount;
    }
};

template <typename T>
class EnableSharedFromThis {
private:
    template <typename U>
    friend class SharedPtr;

    WeakPtr<T> info;
public:
    SharedPtr<T> shared_from_this() const {
        if (info.cblock == nullptr) {
            // это значит, что еще не инициализировали
            throw std::runtime_error("Called EnableSharedFromThis from unmanaged object");
        }
        return info.lock();
    }
};