#include <memory>
#include <type_traits>
// #include <allocator>
// template<typename U>
// class WeakPtr;

#define private public

template <typename T>
class SharedPtr {
private:

    template<typename U>
    union DataHolder {
        U val;

        DataHolder() {};
        ~DataHolder() {};

        void destroy_inside() {
            val.~U();
        }
    };

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
        DataHolder<U> obj;

        template<typename... Args>
        FatControlBlock(Args&&... args) {
            new (&obj.val) U(std::forward<Args>(args)...);
        }

        ~FatControlBlock() = default;
    };

    BaseControlBlock* cblock = nullptr;
    T* ptr = nullptr;

    template<typename U, typename... Args>
    friend SharedPtr<U> make_shared(Args&&... args);

    template<typename U>
    friend class WeakPtr;

    T* get_ptr() {
        if (dynamic_cast<FatControlBlock<T>*>(cblock) != nullptr) {
            return &(dynamic_cast<FatControlBlock<T>*>(cblock) -> obj.val);
        } else {
            return ptr;
        }
    }
    
    const T* get_ptr() const {
        if (dynamic_cast<const FatControlBlock<T>*>(cblock) != nullptr) {
            return &(dynamic_cast<const FatControlBlock<T>*>(cblock) -> obj.val);
        } else {
            return ptr;
        }
    }


    // constructor for make_shared and WeakPtr::lock
    SharedPtr(BaseControlBlock* cb, T* ptr)
    : cblock(cb)
    , ptr(ptr) {}


    void delete_helper() {
        if (!cblock) {
            // типа мувнули/что-то еще
            return;
        }
        cblock -> spcount--;

        if (cblock -> spcount == 0) {
            if (dynamic_cast<FatControlBlock<T>*>(cblock) != nullptr) {
                (dynamic_cast<FatControlBlock<T>*>(cblock) -> obj).destroy_inside();
            } else {
                delete ptr;
            }

            if (cblock -> weakcount == 0) {
                delete cblock;
            }
        }

        cblock = nullptr;
        ptr = nullptr;
    }
public:
    SharedPtr()
    : cblock(new WeakControlBlock())
    , ptr(nullptr) {}

    SharedPtr(T* ptr) 
    : cblock(new WeakControlBlock())
    , ptr(ptr) {}
    
    SharedPtr(const SharedPtr& other) 
    : cblock(other.cblock)
    , ptr(other.ptr) {
        cblock -> spcount++;
    }


    ~SharedPtr() {
        delete_helper();
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
        cblock -> spcount++;
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
        cblock = new WeakControlBlock();
        ptr = new_obj;
    }

    void reset() {
        delete_helper();
    }

    T* get() const {
        return const_cast<T*>(get_ptr());
    }
};

template<typename T, typename... Args>
SharedPtr<T> make_shared(Args&&... args) {
    return SharedPtr<T>(new typename SharedPtr<T>::FatControlBlock<T>(std::forward<Args>(args)...), nullptr);
}

template <typename T>
class WeakPtr {
private:
    SharedPtr<T>::BaseControlBlock* cblock = nullptr;
    T* ptr = nullptr;
public:
    WeakPtr() {} // default vals

    WeakPtr(const SharedPtr<T>& sp) 
    : cblock(sp.cblock)
    , ptr(sp.ptr) {
        cblock -> weakcount++;
    }

    bool expired() const noexcept {
        return (!cblock || cblock -> spcount == 0);
    }

    SharedPtr<T> lock() const noexcept {
        if (expired()) {
            return SharedPtr<T>();
        } else {
            cblock -> spcount++;
            return SharedPtr<T>(cblock, ptr);    
        }
    }

    ~WeakPtr() {
        if (!cblock) return;
        cblock -> weakcount--;
        if (cblock -> weakcount == 0 && expired()) {
            // тогда надо совсем все сносить
            delete cblock;
            cblock = nullptr;
        }
    }

    long use_count() const {
        return cblock -> spcount;
    }


};

template <typename>
class EnableSharedFromThis {

    // YOUR CODE HERE
};