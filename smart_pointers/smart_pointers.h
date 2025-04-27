
#include <memory>
#include <type_traits>

template <typename T>
class EnableSharedFromThis;

template <typename T>
class SharedPtr;

template <typename T>
class WeakPtr;

struct BaseControlBlock {
    size_t spcount = 0, weakcount = 0;

    virtual void delete_inside(){};

    virtual void destroy(){};

    BaseControlBlock()
        : spcount(0),
          weakcount(0) {
    }

    virtual ~BaseControlBlock() = default;
};

template <typename T, typename Deleter, typename Allocator>
struct WeakControlBlock : BaseControlBlock {
    [[no_unique_address]] Deleter d;
    [[no_unique_address]] Allocator a;
    T* ptr;

    WeakControlBlock(T* p, Deleter deleter = Deleter(), Allocator allocator = Allocator())
        : d(deleter),
          a(allocator),
          ptr(p) {
    }

    ~WeakControlBlock() override {
        delete_inside();
    }

    void delete_inside() override {
        if (!ptr) {
            return;
        }
        d(ptr);
        ptr = nullptr;
    }

    void destroy() override {
        Allocator al = a;
        this->~WeakControlBlock();

        using traits = std::allocator_traits<Allocator>;
        using good_alloc_type =
            typename traits ::template rebind_alloc<WeakControlBlock<T, Deleter, Allocator>>;
        using good_alloc_traits =
            typename traits ::template rebind_traits<WeakControlBlock<T, Deleter, Allocator>>;
        auto ppt = static_cast<good_alloc_type>(al);
        good_alloc_traits::deallocate(ppt, this, 1);
    }
};

template <typename T, typename Deleter, typename Allocator>
WeakControlBlock<T, Deleter, Allocator>* custom_construct_weak(T* ptr, Deleter d, Allocator a) {
    using traits = std::allocator_traits<Allocator>;
    using good_alloc_type =
        typename traits ::template rebind_alloc<WeakControlBlock<T, Deleter, Allocator>>;
    using good_alloc_traits =
        typename traits ::template rebind_traits<WeakControlBlock<T, Deleter, Allocator>>;
    auto ppt = static_cast<good_alloc_type>(a);

    WeakControlBlock<T, Deleter, Allocator>* space = good_alloc_traits::allocate(ppt, 1);
    return new (space) WeakControlBlock<T, Deleter, Allocator>(ptr, d, a);
}

template <typename T, typename Allocator>
struct FatControlBlock : BaseControlBlock {
    template <typename U>
    union DataHolder {
        U val;

        DataHolder(){};
        ~DataHolder(){};

        void destroy_inside() {
            val.~U();
        }
    };

    [[no_unique_address]] Allocator a;
    DataHolder<T> obj;

    template <typename... Args>
    FatControlBlock(Allocator alloc, Args&&... args)
        : a(alloc) {
        new (&obj.val) T(std::forward<Args>(args)...);
    }

    void delete_inside() override {
        obj.destroy_inside();
    }

    void destroy() override {

        Allocator al = a;
        using traits = std::allocator_traits<Allocator>;
        using good_alloc_type =
            typename traits ::template rebind_alloc<FatControlBlock<T, Allocator>>;
        using good_alloc_traits =
            typename traits ::template rebind_traits<FatControlBlock<T, Allocator>>;
        auto ppt = static_cast<good_alloc_type>(al);
        good_alloc_traits::destroy(ppt, this);
        good_alloc_traits::deallocate(ppt, this, 1);
    }

    ~FatControlBlock() = default;
};

template <typename T>
void controlblock_delete(BaseControlBlock*& cb) {
    if (cb == nullptr) {
        return;
    }

    cb->destroy();
    cb = nullptr;
}

template <typename T>
class SharedPtr {
private:
    BaseControlBlock* cblock_ = nullptr;
    T* ptr_ = nullptr;

    template <typename U>
    friend class SharedPtr;

    template <typename U, typename Alloc, typename... Args>
    friend SharedPtr<U> allocateShared(Alloc, Args&&... args);

    template <typename U>
    friend class WeakPtr;

    T* get_ptr() {
        return ptr_;
    }

    const T* get_ptr() const {
        return ptr_;
    }

    void delete_helper() {
        ptr_ = nullptr;
        if (!cblock_) {
            // типа мувнули/что-то еще
            return;
        }

        if (cblock_->spcount == 1) {
            cblock_->delete_inside();

            if (cblock_->weakcount == 0) {
                controlblock_delete<T>(cblock_);
            }
        }
        if (cblock_) {
            cblock_->spcount--;
        }
    }

public:
    SharedPtr()
        : cblock_(nullptr),
          ptr_(nullptr) {
    }

    template <typename Y>
    SharedPtr(Y* ptr)
        : cblock_(custom_construct_weak(static_cast<T*>(ptr), std::default_delete<T>(),
                                       std::allocator<T>())),
          ptr_(static_cast<T*>(ptr)) {
         cblock_->spcount++;
        if constexpr (std::is_base_of<EnableSharedFromThis<T>, T>::value) {
            static_cast<EnableSharedFromThis<T>*>(ptr)->info_ = *this;
        }
    }

    template <typename Y, typename Deleter>
    SharedPtr(Y* ptr, Deleter d)
        : cblock_(custom_construct_weak(static_cast<T*>(ptr), d, std::allocator<T>())),
          ptr_(static_cast<T*>(ptr)) {
        cblock_->spcount++;
        if constexpr (std::is_base_of<EnableSharedFromThis<T>, T>::value) {
            static_cast<EnableSharedFromThis<T>*>(ptr)->info_ = *this;
        }
    }

    template <typename Y, typename Deleter, typename Allocator>
    SharedPtr(Y* ptr, Deleter d, Allocator a)
        : cblock_(custom_construct_weak(static_cast<T*>(ptr), d, a)),
          ptr_(static_cast<T*>(ptr)) {
        cblock_->spcount++;
        if constexpr (std::is_base_of<EnableSharedFromThis<T>, T>::value) {
            static_cast<EnableSharedFromThis<T>*>(ptr)->info_ = *this;
        }
    }

    template <typename Y>
    SharedPtr(const SharedPtr<Y>& other)
        : cblock_(other.cblock_),
          ptr_(other.ptr_) {
         cblock_->spcount++;
    }

    SharedPtr(const SharedPtr& other)
        : cblock_(other.cblock_),
          ptr_(other.ptr_) {
        if (cblock_) {
             cblock_->spcount++;
        }
    }

    // Aliasing constructor
    template <typename Y>
    SharedPtr(const SharedPtr<Y>& other, T* ptr)
        : cblock_(other.cblock_),
          ptr_(ptr) {
        if (cblock_) {
             cblock_->spcount++;
        }
    }

    ~SharedPtr() {
        delete_helper();
    }

    template <typename Y>
    SharedPtr(SharedPtr<Y>&& other)
        : cblock_(other.cblock_),
          ptr_(other.ptr_) {
        other.cblock_ = nullptr;
        other.ptr_ = nullptr;
    }

    template <typename Y>
    SharedPtr& operator=(const SharedPtr<Y>& other) {
        delete_helper();
        cblock_ = other.cblock_;
        ptr_ = other.ptr_;
        if (cblock_) {
             cblock_->spcount++;
        }
        return *this;
    }

    template <typename Y>
    SharedPtr& operator=(SharedPtr<Y>&& other) {
        // this->swap(other); TODO
        delete_helper();
        cblock_ = other.cblock_;
        ptr_ = other.ptr_;
        other.cblock_ = nullptr;
        other.ptr_ = nullptr;
        return *this;
    }

    SharedPtr(SharedPtr&& other)
        : cblock_(other.cblock_),
          ptr_(other.ptr_) {
        other.cblock_ = nullptr;
        other.ptr_ = nullptr;
    }

    SharedPtr& operator=(const SharedPtr& other) {
        if (this == &other) {
            return *this;
        }
        delete_helper();
        cblock_ = other.cblock_;
        ptr_ = other.ptr_;
        if (cblock_) {
             cblock_->spcount++;
        }
        return *this;
    }

    SharedPtr& operator=(SharedPtr&& other) {
        delete_helper();
        cblock_ = other.cblock_;
        ptr_ = other.ptr_;
        other.cblock_ = nullptr;
        other.ptr_ = nullptr;
        return *this;
    }

    long use_count() const {
        return  cblock_->spcount;
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
        std::swap(ptr_, other.ptr_);
        std::swap(cblock_, other.cblock_);
    }

    template <typename Y>
    void reset(Y* new_obj) {
        delete_helper();
        ptr_ = static_cast<T*>(new_obj);
        cblock_ = custom_construct_weak(ptr_, std::default_delete<T>(), std::allocator<T>());
         cblock_->spcount++;
    }

    template <typename Y, typename Deleter>
    void reset(Y* new_obj, Deleter d) {
        delete_helper();
        ptr_ = static_cast<T*>(new_obj);
        cblock_ = custom_construct_weak(ptr_, d, std::allocator<T>());
         cblock_->spcount++;
    }

    template <typename Y, typename Deleter, typename Allocator>
    void reset(Y* new_obj, Deleter d, Allocator a) {
        delete_helper();
        ptr_ = static_cast<T*>(new_obj);
        cblock_ = custom_construct_weak(ptr_, d, a);
         cblock_->spcount++;
    }

    void reset() {
        delete_helper();
        cblock_ = nullptr;
        ptr_ = nullptr;
    }

    T* get() const {
        return const_cast<T*>(get_ptr());
    }
};

template <typename T, typename Allocator, typename... Args>
SharedPtr<T> allocateShared(Allocator allocator, Args&&... args) {
    SharedPtr<T> to_return;

    using traits = std::allocator_traits<Allocator>;
    using good_alloc_type = typename traits ::template rebind_alloc<FatControlBlock<T, Allocator>>;
    using good_alloc_traits =
        typename traits ::template rebind_traits<FatControlBlock<T, Allocator>>;
    auto ppt = static_cast<good_alloc_type>(allocator);
    FatControlBlock<T, Allocator>* mem = good_alloc_traits::allocate(ppt, 1);
    to_return.cblock_ = mem;
    good_alloc_traits::construct(ppt, mem, allocator, std::forward<Args>(args)...);
    to_return.ptr_ = &(dynamic_cast<FatControlBlock<T, Allocator>*>(to_return.cblock_)->obj.val);
    to_return. cblock_->spcount++;

    if constexpr (std::is_base_of<EnableSharedFromThis<T>, T>::value) {
        static_cast<EnableSharedFromThis<T>*>(to_return.ptr_)->info_ = to_return;
    }
    return to_return;
}

template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
    return allocateShared<T>(std::allocator<std::byte>(), std::forward<Args>(args)...);
}

template <typename T>
class WeakPtr {
private:
    BaseControlBlock* cblock_ = nullptr;
    T* ptr_ = nullptr;

    template <typename U>
    friend class WeakPtr;

    template <typename U>
    friend class EnableSharedFromThis;

    void delete_helper() {
        ptr_ = nullptr;
        if (!cblock_) {
            return;
        }
        cblock_->weakcount--;
        if (cblock_->weakcount == 0 && expired()) {
            // тогда надо совсем все сносить
            controlblock_delete<T>(cblock_);
            cblock_ = nullptr;
        }
    }

public:
    WeakPtr() {
    }  // default vals

    template <typename Y>
    WeakPtr(const SharedPtr<Y>& sp)
        : cblock_(sp.cblock_),
          ptr_(sp.ptr_) {
        cblock_->weakcount++;
    }

    template <typename Y>
    WeakPtr(const WeakPtr<Y>& other)
        : cblock_(other.cblock_),
          ptr_(other.ptr_) {
        cblock_->weakcount++;
    }

    WeakPtr(const WeakPtr& other)
        : cblock_(other.cblock_),
          ptr_(other.ptr_) {
        cblock_->weakcount++;
    }

    template <typename Y>
    WeakPtr& operator=(const WeakPtr<Y>& other) {
        if (this == &other) {
            return *this;
        }
        delete_helper();

        cblock_ = other.cblock_;
        ptr_ = other.ptr_;

        cblock_->weakcount++;
        return *this;
    }

    WeakPtr& operator=(const WeakPtr& other) {
        if (this == &other) {
            return *this;
        }
        delete_helper();

        cblock_ = other.cblock_;
        ptr_ = other.ptr_;

        cblock_->weakcount++;
        return *this;
    }

    template <typename Y>
    WeakPtr(WeakPtr<Y>&& other)
        : cblock_(other.cblock_),
          ptr_(other.ptr_) {
        other.cblock_ = nullptr;
        other.ptr_ = nullptr;
    }

    WeakPtr(WeakPtr&& other)
        : cblock_(other.cblock_),
          ptr_(other.ptr_) {
        other.cblock_ = nullptr;
        other.ptr_ = nullptr;
    }

    template <typename Y>
    WeakPtr& operator=(WeakPtr<Y>&& other) {
        delete_helper();
        cblock_ = other.cblock_;
        ptr_ = other.ptr_;
        other.cblock_ = nullptr;
        other.ptr_ = nullptr;
        return *this;
    }

    WeakPtr& operator=(WeakPtr&& other) {
        delete_helper();
        cblock_ = other.cblock_;
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;
        other.cblock_ = nullptr;
        return *this;
    }

    bool expired() const noexcept {
        return (!cblock_ ||  cblock_->spcount == 0);
    }

    SharedPtr<T> lock() const noexcept {
        SharedPtr<T> to_return;
        if (!expired()) {
            to_return.cblock_ = cblock_;
            to_return.ptr_ = ptr_;
            to_return. cblock_->spcount++;
        }
        return to_return;
    }

    ~WeakPtr() {
        delete_helper();
    }

    long use_count() const {
        if (!cblock_) {
            // допустим, мувнули
            return 0;
        }
        return  cblock_->spcount;
    }
};

template <typename T>
class EnableSharedFromThis {
private:
    template <typename U>
    friend class SharedPtr;

    template <typename U, typename Alloc, typename... Args>
    friend SharedPtr<U> allocateShared(Alloc, Args&&...);
    WeakPtr<T> info_;

public:
    SharedPtr<T> shared_from_this() const {
        if (info_.cblock_ == nullptr) {
            // это значит, что еще не инициализировали
            throw std::runtime_error("Called EnableSharedFromThis from unmanaged object");
        }
        return info_.lock();
    }
};