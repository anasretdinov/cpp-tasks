/// TODO: fix problems with  BaseCB constructor (set tp zero) 


#include <memory>
#include <type_traits>
// #include <allocator>
// template<typename U>
// class WeakPtr;

#define private public

struct BaseControlBlock {
    size_t spcount = 0, weakcount = 0;

    BaseControlBlock()
    : spcount(0)
    , weakcount(0) {
    
    } 

    virtual ~BaseControlBlock() = default;
};

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
    , ptr(ptr) {
        cb -> spcount++;
    }


    void delete_helper() {
        // std::cout << " this is delete helper\n";
        if (!cblock) {
            // типа мувнули/что-то еще
            return;
        }
        cblock -> spcount--;

        if (cblock -> spcount == 0) {
            // std::cout << " opa opa\n";
            if (dynamic_cast<FatControlBlock<T>*>(cblock) != nullptr) {
                // std::cout << " destroy inside???\n";
                (dynamic_cast<FatControlBlock<T>*>(cblock) -> obj).destroy_inside();
            } else {
                delete ptr;
            }

            if (cblock -> weakcount == 0) {
                // std::cout << "opa opa\n";
                delete cblock;
            }
        }

        cblock = nullptr;
        ptr = nullptr;
    }
public:
    SharedPtr()
    : cblock(new WeakControlBlock())
    , ptr(nullptr) {
        cblock -> spcount++;
    }

    template<typename Y>
    SharedPtr(Y* ptr) 
    : cblock(new WeakControlBlock())
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
        cblock -> spcount++;
    }

    // Aliasing constructor
    template <typename Y>
    SharedPtr(const SharedPtr<Y>& other, T* ptr) 
    : cblock(other.cblock)
    , ptr(ptr) {
        cblock -> spcount++;
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

        if (this == &other) {
            return *this;
        }
        delete_helper();
        cblock = other.cblock;
        ptr = other.ptr;
        cblock -> spcount++;
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

    // Move-конструктор
    SharedPtr(SharedPtr&& other)
    : cblock(other.cblock)
    , ptr(other.ptr) {
        other.cblock = nullptr;
        other.ptr = nullptr;
    }

    // Копирующий оператор присваивания
    SharedPtr& operator=(const SharedPtr& other) {
        if (this == &other) {
            return *this;
        }
        delete_helper();
        cblock = other.cblock;
        ptr = other.ptr;
        if (cblock) {
            cblock->spcount++;
        }
        return *this;
    }

    // Move-оператор присваивания
    SharedPtr& operator=(SharedPtr&& other) {
        delete_helper();
        cblock = other.cblock;
        ptr = other.ptr;
        other.cblock = nullptr;
        other.ptr = nullptr;
        return *this;
    }


    void random_non_const() {
        (this -> operator*())++;
        std::cout << " random non const\n";
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
        cblock -> spcount++;
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
SharedPtr<T> makeShared(Args&&... args) {
    return SharedPtr<T>(new typename SharedPtr<T>::template FatControlBlock<T>(std::forward<Args>(args)...), nullptr);
}

template <typename T>
class WeakPtr {
private:
    BaseControlBlock* cblock = nullptr;
    T* ptr = nullptr;

    void delete_helper() {
        ptr = nullptr; // не наша забота
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
        // std::cout << " weak operator= called\n";
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
        // std::cout << " weak operator= called\n";
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
        std::cout << " there1\n";
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
        other.cblock = nullptr;
        other.ptr = nullptr;
        return *this;
    }


    bool expired() const noexcept {
        return (!cblock || cblock -> spcount == 0);
    }

    SharedPtr<T> lock() const noexcept {
        if (expired()) {
            return SharedPtr<T>();
        } else {
            return SharedPtr<T>(cblock, ptr);    
        }
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
    SharedPtr<T> shared_from_this() {

    }
};