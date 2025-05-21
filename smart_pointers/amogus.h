
#include <memory>
#include <type_traits>

#define private public

struct BaseControlBlock {
    size_t spcount = 0, weakcount = 0;

    BaseControlBlock()
    : spcount(0)
    , weakcount(0) {
    
    } 

    virtual ~BaseControlBlock() = default;
};

template<typename T>
struct WeakControlBlock : BaseControlBlock {
    T* managed_ptr = nullptr;
    WeakControlBlock(T* ptr) : managed_ptr(ptr) {}

    template <typename U>
    WeakControlBlock(const WeakControlBlock<U>& other) : managed_ptr(static_cast<T*>(other.managed_ptr)) {}
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
    T* unmanaged = nullptr;

    template<typename U, typename... Args>
    friend SharedPtr<U> make_shared(Args&&... args);

    template<typename U>
    friend class WeakPtr;

    T* get_ptr() {
        if (unmanaged == nullptr) {
            if (dynamic_cast<FatControlBlock<T>*>(cblock) != nullptr) {
                return &(dynamic_cast<FatControlBlock<T>*>(cblock) -> obj.val);
            } else {
                return dynamic_cast<WeakControlBlock<T>*>(cblock) -> managed_ptr;
                // throw std::runtime_error("Something wrong with sptr");
            }
        } else {
            return unmanaged;
        }
    }
    
    const T* get_ptr() const {
        if (unmanaged == nullptr) {
            if (dynamic_cast<FatControlBlock<T>*>(cblock) != nullptr) {
                return &(dynamic_cast<FatControlBlock<T>*>(cblock) -> obj.val);
            } else {
                return dynamic_cast<WeakControlBlock<T>*>(cblock) -> managed_ptr;
                // throw std::runtime_error("Something wrong with sptr");
            }
        } else {
            return unmanaged;
        }
    }


    // constructor for make_shared and WeakPtr::lock
    SharedPtr(BaseControlBlock* cb, T* ptr)
    : cblock(cb)
    , unmanaged(ptr) {
        cb -> spcount++;
    }


    void delete_helper() {
        std::cout << " this is delete helper\n";
        if (!cblock) {
            // типа мувнули/что-то еще
            return;
        }
        cblock -> spcount--;

        if (cblock -> spcount == 0) {
            std::cout << " opa opa\n";
            if (dynamic_cast<FatControlBlock<T>*>(cblock) != nullptr) {
                std::cout << " destroy inside???\n";
                (dynamic_cast<FatControlBlock<T>*>(cblock) -> obj).destroy_inside();
            } else {
                std::cout << " not destroy inside\n";
                std::cout << cblock << '\n';
                std::cout << dynamic_cast<WeakControlBlock<T>*>(cblock) << " hmmm\n";
                delete dynamic_cast<WeakControlBlock<T>*>(cblock) -> managed_ptr;
            }

            if (cblock -> weakcount == 0) {
                std::cout << "opa opa\n";
                delete cblock;
            }
        }

        cblock = nullptr;
        unmanaged = nullptr;
    }
public:
    SharedPtr()
    : cblock(nullptr)
    , unmanaged(nullptr) {}

    template<typename Y>
    SharedPtr(Y* ptr) 
    : cblock(new WeakControlBlock<T>(ptr))
    , unmanaged(nullptr) {
        std::cout << " fuck!\n";
        std::cout << cblock << ' ' << dynamic_cast<WeakControlBlock<T>*>(cblock) << " two ipo\n";
        cblock -> spcount++;
    }
    
    template <typename Y>
    SharedPtr(const SharedPtr<Y>& other) 
    : cblock(other.cblock)
    , unmanaged(other.unmanaged) {
        cblock -> spcount++;
    }

    SharedPtr(const SharedPtr& other)
    : cblock(other.cblock)
    , unmanaged(other.unmanaged) {
        cblock -> spcount++;
    }

    // Aliasing constructor
    template <typename Y>
    SharedPtr(const SharedPtr<Y>& other, T* ptr) 
    : cblock(other.cblock)
    , unmanaged(ptr) {
        std::cout << " aliasing called\n";
        std::cout << other.cblock << " other cblock\n";
        std::cout << dynamic_cast<WeakControlBlock<Y>*>(other.cblock) << " casted\n";
        cblock -> spcount++;
    }



    ~SharedPtr() {
        delete_helper();
    }

    template <typename Y>
    SharedPtr(SharedPtr<Y>&& other) 
    : cblock(other.cblock) 
    , unmanaged(other.unmanaged) {
        other.cblock = nullptr;
        other.unmanaged = nullptr;
    }

    SharedPtr(SharedPtr&& other)
    : cblock(other.cblock)
    , unmanaged(other.unmanaged) {
        other.cblock = nullptr;
        other.unmanaged = nullptr;
    }

    template <typename Y>
    SharedPtr& operator=(const SharedPtr<Y>& other) {
        if (this == &other) {
            return *this;
        }
        delete_helper();
        cblock = other.cblock;
        unmanaged = other.unmanaged;
        cblock -> spcount++;
        return *this;
    }

    SharedPtr& operator=(const SharedPtr& other) {
        if (this == &other) {
            return *this;
        }
        delete_helper();
        cblock = other.cblock;
        unmanaged = other.unmanaged;
        if (cblock) {
            cblock->spcount++;
        }
        return *this;
    }

    template <typename Y>
    SharedPtr& operator=(SharedPtr<Y>&& other) {
        // this->swap(other); TODO 
        delete_helper();
        cblock = other.cblock;
        unmanaged = other.unmanaged;
        other.cblock = nullptr;
        other.unmanaged = nullptr;
        return *this;
    }


    SharedPtr& operator=(SharedPtr&& other) {
        delete_helper();
        cblock = other.cblock;
        unmanaged = other.unmanaged;
        other.cblock = nullptr;
        other.unmanaged = nullptr;
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

    void swap(SharedPtr& other) {
        std::swap(unmanaged, other.unmanaged);
        std::swap(cblock, other.cblock);
    }

    template <typename Y>
    void reset(Y* new_ptr) {
        delete_helper();
        cblock = new WeakControlBlock(static_cast<T*>(new_ptr));
        ptr = 
        cblock -> spcount++;
        unmanaged = nullptr;
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
    T* unmanaged = nullptr;

    void delete_helper() {
        unmanaged = nullptr; // не наша забота
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
    , unmanaged(sp.unmanaged) {
        cblock -> weakcount++;
    }

    template <typename Y>
    WeakPtr(const WeakPtr<Y>& other)
    : cblock(other.cblock)
    , unmanaged(other.unmanaged) {
        cblock -> weakcount++;
    }

    WeakPtr(const WeakPtr& other)
    : cblock(other.cblock)
    , unmanaged(other.unmanaged) {
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
        unmanaged = other.unmanaged;

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
        unmanaged = other.unmanaged;

        cblock -> weakcount++;
        return *this;
    }

    template <typename Y>
    WeakPtr(WeakPtr<Y>&& other)
    : cblock(other.cblock)
    , unmanaged(other.unmanaged) {
        other.cblock = nullptr;
        other.unmanaged = nullptr;
    }

    WeakPtr(WeakPtr&& other)
    : cblock(other.cblock)
    , unmanaged(other.unmanaged) {
        std::cout << " there1\n";
        other.cblock = nullptr;
        other.unmanaged = nullptr;
    }

    template <typename Y>
    WeakPtr& operator=(WeakPtr<Y>&& other) {
        delete_helper();
        cblock = other.cblock;
        unmanaged = other.unmanaged;
        other.cblock = nullptr;
        other.unmanaged = nullptr;
        return *this;
    }

    WeakPtr& operator=(WeakPtr&& other) {
        delete_helper();
        cblock = other.cblock;
        unmanaged = other.unmanaged;
        other.cblock = nullptr;
        other.unmanaged = nullptr;
        return *this;
    }


    bool expired() const noexcept {
        return (!cblock || cblock -> spcount == 0);
    }

    SharedPtr<T> lock() const noexcept {
        if (expired()) {
            return SharedPtr<T>();
        } else {
            return SharedPtr<T>(cblock, unmanaged);    
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