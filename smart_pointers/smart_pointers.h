#include <memory>
// #include <allocator>


template <typename T>
class SharedPtr {
private:
    struct BaseControlBlock {
        size_t spcount = 0, weakcount = 0;

        BaseControlBlock()
        : spcount(1)
        , weakcount(0) {
        
        } 

        virtual ~BaseControlBlock() = default;
    };

    struct WeakControlBlock : BaseControlBlock {
        
    };

    template<typename U>
    struct FatControlBlock : BaseControlBlock {
        U obj;

        template<typename... Args>
        FatControlBlock(Args&&... args) : obj(std::forward<Args>(args)...) {}

        ~FatControlBlock() = default;
    };

    BaseControlBlock* cblock = nullptr;
    T* ptr = nullptr;

    template<typename U, typename... Args>
    friend SharedPtr<U> make_shared(Args&&... args);

    template<typename U>
    friend class WeakPtr<U>;
public:
    SharedPtr()
    : cblock(new BaseControlBlock())
    . ptr(nullptr) {}

    SharedPtr(T* ptr) 
    : cblock(new WeakControlBlock())
    , ptr(ptr) {}

    SharedPtr(const SharedPtr& other) 
    : cblock(other.cblock)
    , ptr(other.ptr) {
        cblock -> spcount++;
    }

    ~SharedPtr() {
        
        

    }

    SharedPtr(SharedPtr&& other) 
    : cblock(other.cblock) 
    , ptr(other.ptr) {
        other.cblock = nullptr;
        other.ptr = nullptr;
    }

    SharedPtr& operator=(const SharedPtr& other) {
        if (*this == other) {
            return *this;
        }
        cblock = other.cblock;
        ptr = other.ptr;
        cblock -> spcount++;
        return *this;
    }

    SharedPtr& operator=(SharedPtr&& other) {
        cblock = other.cblock;
        ptr = other.ptr;
        other.cblock = nullptr;
        return *this;
    }

    long use_count() const {
        return cblock -> spcount;
    }

    T& operator*() {
        return *ptr;
    }

    const T& operator*() const {
        return *ptr;
    }

    T* operator->() {
        return ptr;
    }

    const T* operator->() const {
        return ptr;
    }
};

template<typename T, typename... Args>
SharedPtr<T> make_shared(Args&&... args) {
    return SharedPtr<T>(new SharedPtr<T>::FatControlBlock<T>(std::forward<Args>(args)...));
}

template <typename T>
class WeakPtr {
private:
    SharedPtr<T>::BaseControlBlock cblock = nullptr;
    T* ptr = nullptr;
public:
    WeakPtr(const SharedPtr& sp) 
    : cblock(sp.cblock)
    . ptr(sp.ptr) {
        cblock -> weakcount++;
    }

    bool expired() const noexcept {
        return (!cblock || cblock -> spcount == 0);
    }

    SharedPtr<T> lock() const noexcept {
        if (expired()) {

        } else {
            return SharedPtr<T>();
        }
    }
    // YOUR CODE HERE
};

template <typename>
class EnableSharedFromThis {

    // YOUR CODE HERE
};