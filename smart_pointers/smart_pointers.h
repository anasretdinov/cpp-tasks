#include <memory>
// #include <allocator>


template <typename T>
class SharedPtr {
private:
    struct ControlBlock{
        T ptr;
        size_t spcount = 0;

        template<typename... Args>
        ControlBlock(Args&&... args) : ptr(std::forward<Args>(args)...), spcount(1) {}
    };
    ControlBlock* cblock = nullptr;

    template<typename U, typename... Args>
    friend SharedPtr<U> make_shared(Args&&... args);

    template<typename... Args>
    SharedPtr(Args&&... args) 
    : cblock(new ControlBlock(std::forward<Args>(args)...)) {}
public:
    SharedPtr(const SharedPtr& other) 
    : cblock(other.cblock) {
        cblock -> spcount++;
    }

    ~SharedPtr() {
        cblock -> spcount--;
        if (cblock -> spcount == 0) {
            delete (cblock);
            cblock = nullptr;
        }
    }

    SharedPtr(SharedPtr&& other) 
    : cblock(other.cblock) {
        other.cblock = nullptr;
    }

    SharedPtr& operator=(const SharedPtr& other) {

        cblock = other.cblock;
        cblock -> spcount++;
        return *this;
    }

    SharedPtr& operator=(SharedPtr&& other) {
        cblock = other.cblock;
        other.cblock = nullptr;
        return *this;
    }

    size_t get_pscount() const {
        return cblock -> spcount;
    }

    T& operator*() {
        return cblock -> ptr;
    }

    const T& operator*() const {
        return cblock -> ptr;
    }

    T* operator->() {
        return &(cblock->ptr);
    }

    const T* operator->() const {
        return &(cblock -> ptr);
    }
};

template<typename T, typename... Args>
SharedPtr<T> make_shared(Args&&... args) {
    return SharedPtr<T>(std::forward<Args>(args)...);
}

template <typename>
class WeakPtr {

    // YOUR CODE HERE
};

template <typename>
class EnableSharedFromThis {

    // YOUR CODE HERE
};