#include <functional>

template <typename>
class Function;

template <typename Ret, typename... Args>
class Function<Ret(Args...)> {
private:
    static const size_t BUFFER_SIZE = 16; 

    void* fptr;

    char small_buffer[BUFFER_SIZE];

    using invoke_ptr_t = Ret(*)(void*, Args...);
    using destroy_ptr_t = void(*)(void*);
    // using copy_ptr_t = size_t*; // TODO

    invoke_ptr_t invoke_ptr;
    destroy_ptr_t destroy_ptr;
    // copy_ptr_t copy_ptr;



private:
    template <typename F>
    static Ret invoker(F* func, Args... args) {
        return std::invoke(*func, std::forward<Args>(args)...);
    }

    template <typename F>
    static void destroyer(F* func) {
        if constexpr (sizeof(F) > BUFFER_SIZE) {
            delete func;
        } else {
            reinterpret_cast<F*>(func) -> ~F();
        }
    }
public:

    // empty constructor
    Function() 
        : invoke_ptr(nullptr)
        , destroy_ptr(nullptr) 
    {

    }
    
    
    // i can't call sizeof from c-style func 
    // so i write requires
    template <typename F>
    requires (
        !std::is_same_v<std::remove_cvref_t<F>, Function<Ret(Args...)>>
     && !std::is_function_v<std::remove_cvref_t<F>>)
    Function(F&& func) 
        : invoke_ptr(reinterpret_cast<invoke_ptr_t>(&invoker<std::remove_cvref_t<F>>))
        , destroy_ptr(reinterpret_cast<destroy_ptr_t>(&destroyer<std::remove_cvref_t<F>>))
    {
        using TrueType = std::remove_cvref_t<F>;
        if constexpr (sizeof(TrueType) > BUFFER_SIZE) {
            fptr = new F(std::forward<F>(func));
        } else {
            new (small_buffer) TrueType(std::forward<F>(func));
            fptr = small_buffer;
        }
    }

    // this is for functions
    template <typename F>
    requires (
        std::is_function_v<std::remove_cvref_t<F>>
    )
    Function(F* func)
        : fptr(reinterpret_cast<void*>(func))
        , invoke_ptr(reinterpret_cast<invoke_ptr_t>(&invoker<std::remove_cvref_t<F>>))
        , destroy_ptr(nullptr) // nothing to destroy 
    {

    }

    Ret operator()(Args... args) const {
        if (!invoke_ptr) {
            throw std::bad_function_call();
        }
        return invoke_ptr(fptr, std::forward<Args>(args)...);
    }

    ~Function() {
        if (destroy_ptr) {
            destroy_ptr(fptr);
        }
        fptr = nullptr;
    }
};

template <typename>
class MoveOnlyFunction {

    // YOUR CODE HERE
};