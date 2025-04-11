#include <memory>
#include <allocator>


template <typename T>
class SharedPtr {
private:
    struct ControlBlock{
        T ptr = nullptr;
        size_t spcount = 0;
        ControlBlock
    };
    ControlBlock* cblock = nullptr;

    friend template<typename U, typename... Args>
    SharedPtr<U> make_shared(Args...&& args);

    template<typemame... Args>
    SharedPtr(Args...&& args) 
    : cblock(new ControlBlock(std::forward(args)...)) {
        
    }
public:
    SharedPtr(const SharedPtr<T>& other) 
    : cblock(other.cblock) {}

    ~SharedPtr() {
        (cblock -> spcount)
    }
};

template<typename T, typename... Args>
SharedPtr<T> make_shared(Args...&& args) {
    return SharedPtr<T>(args);
}

template <typename>
class WeakPtr {

    // YOUR CODE HERE
};

template <typename>
class EnableSharedFromThis {

    // YOUR CODE HERE
};