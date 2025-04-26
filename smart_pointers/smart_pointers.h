
#include <memory>
#include <type_traits>

#define private public

struct BaseControlBlock {
    size_t spcount = 0, weakcount = 0;

    virtual void delete_inside() {}

    BaseControlBlock()
    : spcount(0)
    , weakcount(0) {
    
    } 

    virtual ~BaseControlBlock() = default;
};


template <typename T, typename Deleter>
struct WeakControlBlock : BaseControlBlock {
    [[no_unique_address]] Deleter d;    
    T* ptr;

    WeakControlBlock(T* p, Deleter deleter = Deleter()) : d(deleter), ptr(p) {}

    ~WeakControlBlock() override {
        delete_inside();
    }

    void delete_inside() override {
        if (!ptr) return;
        d(ptr);
        ptr = nullptr;
    }
};


// template <typename T, std::default_delete>
// struct WeakControlBlock : BaseControlBlock {
//     T* ptr;

//     WeakControlBlock(T* p) : ptr(p) {}

//     ~WeakControlBlock() override {
//         delete_inside();
//     }

//     void delete_inside() override {
//         if (!ptr) return;
//         delete ptr;
//         ptr = nullptr;
//     }
// };


// template <typename T, std::default_delete>
// struct WeakControlBlock : BaseControlBlock {
//     T* ptr;

//     WeakControlBlock(T* p) : ptr(p) {}

//     ~WeakControlBlock() override {
//         delete_inside();
//     }

//     void delete_inside() override {
//         if (!ptr) return;
//         delete ptr;
//         ptr = nullptr;
//     }
// };

// template<typename T, typename Alloc = std::allocator> // здесь в alloc придет уже аккуратный тип, прогнанный через rebind 
template <typename T>
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
    DataHolder<T> obj;

    template<typename... Args>
    FatControlBlock(Args&&... args) {
        new (&obj.val) T(std::forward<Args>(args)...);
    }

    void delete_inside() override {
        obj.destroy_inside();
    }

    ~FatControlBlock() = default;
};

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
        if (ptr) {
            return ptr;
        }
        auto cblock_casted = dynamic_cast<FatControlBlock<T>*>(cblock);
        if (cblock_casted == nullptr) {
            return nullptr;
        } else {
            return &(cblock_casted -> obj.val);
        }
    }
    
    const T* get_ptr() const {

        if (ptr) {
            return ptr;
        }
        auto cblock_casted = dynamic_cast<FatControlBlock<T>*>(cblock);
        if (cblock_casted == nullptr) {
            return nullptr;
        } else {
            return &(cblock_casted -> obj.val);
        }
    }

    void delete_helper() {
        ptr = nullptr;
        // std::cout << " this is delete helper\n";
        if (!cblock) {
            // типа мувнули/что-то еще
            return;
        }
        cblock -> spcount--;

        if (cblock -> spcount == 0) {
            cblock -> delete_inside();

            if (cblock -> weakcount == 0) {
                delete cblock; 
            }
        }
    }
public:
    SharedPtr()
    : cblock(nullptr)
    , ptr(nullptr) {
    }

    template<typename Y>
    SharedPtr(Y* ptr) 
    : cblock(new WeakControlBlock<T, std::default_delete<Y>>(static_cast<T*>(ptr)))
    , ptr(static_cast<T*>(ptr)) {
        cblock -> spcount++;
    }

    template<typename Y, typename Deleter>
    SharedPtr(Y* ptr, Deleter d)
    : cblock(new WeakControlBlock<T, Deleter>(static_cast<T*>(ptr), d))
    , ptr(static_cast<T*>(ptr)) {
        cblock -> spcount++;
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
        cblock = new WeakControlBlock<T, std::default_delete<Y>>(ptr);
        cblock -> spcount++;
    }

    template <typename Y, typename Deleter>
    void reset(Y* new_obj, Deleter d) {
        delete_helper();
        ptr = static_cast<T*>(new_obj);
        cblock = new WeakControlBlock(ptr, d);
        cblock -> spcount++;
    }

    void reset() {
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
    to_return.cblock = new FatControlBlock<T>(std::forward<Args>(args)...);
    to_return.ptr = to_return.get_ptr();
    to_return.cblock -> spcount++;
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
            delete cblock;
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
public:
    SharedPtr<T> shared_from_this() {

    }
};