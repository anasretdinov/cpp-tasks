#include <algorithm>  // std::swap_ranges
#include <functional>
#include <iostream>
#define private public
template <typename, bool Copyable>
class GenericFunction;

template <typename Ret, typename... Args, bool Copyable>
class GenericFunction<Ret(Args...), Copyable> {
private:
    static const size_t BUFFER_SIZE = 16;

    void* fptr;

    char small_buffer[BUFFER_SIZE];

    using invoke_ptr_t = Ret (*)(void*, Args...);
    using destroy_ptr_t = void (*)(void*);
    using copy_ptr_t = void* (*)(void*, char*);

    struct custom_vtable {
        invoke_ptr_t invoke_ptr;
        destroy_ptr_t destroy_ptr;
        copy_ptr_t copy_ptr;

        custom_vtable()
            : invoke_ptr(nullptr),
              destroy_ptr(nullptr),
              copy_ptr(nullptr) {
        }

        custom_vtable(invoke_ptr_t invoke_ptr, destroy_ptr_t destroy_ptr, copy_ptr_t copy_ptr)
            : invoke_ptr(invoke_ptr),
              destroy_ptr(destroy_ptr),
              copy_ptr(copy_ptr) {
        }

        void clear() {
            invoke_ptr = nullptr;
            destroy_ptr = nullptr;
            copy_ptr = nullptr;
        }
    };

    custom_vtable vt;

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
            reinterpret_cast<F*>(func)->~F();
        }
    }

    template <typename F, bool TreatAsObject>
    static void* copier(F* func, char* target_buffer) requires Copyable {

        if constexpr (TreatAsObject) {
            if constexpr (sizeof(F) > BUFFER_SIZE) {
                return new F(*func);
            } else {
                new (target_buffer) F(*func);
                return target_buffer;
            }
        } else {
            return reinterpret_cast<void*>(func);
        }
    }

    void destroy_helper() {
        if (vt.destroy_ptr) {
            vt.destroy_ptr(fptr);
        }
        fptr = nullptr;
        vt.clear();
    }

    bool is_small() const {
        return fptr == small_buffer;
    }

public:
    // empty constructor
    GenericFunction()
        : fptr(nullptr),
          vt(nullptr, nullptr, nullptr) {
    }

    // i can't call sizeof from c-style func
    // so i write requires
    template <typename F>
    requires(!std::is_same_v<std::remove_cvref_t<F>, GenericFunction> &&
             !std::is_function_v<std::remove_cvref_t<F>> && std::invocable<F, Args...>)
        GenericFunction(F&& func)
        : vt(reinterpret_cast<invoke_ptr_t>(&invoker<std::remove_cvref_t<F>>),
             reinterpret_cast<destroy_ptr_t>(&destroyer<std::remove_cvref_t<F>>), nullptr) {
        if constexpr (Copyable) {
            vt.copy_ptr = reinterpret_cast<copy_ptr_t>(&copier<std::remove_cvref_t<F>, true>);
        }
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
    requires(std::is_function_v<std::remove_cvref_t<F>>&& std::invocable<F, Args...>)
        GenericFunction(F* func)
        : fptr(reinterpret_cast<void*>(func)),
          vt(reinterpret_cast<invoke_ptr_t>(&invoker<std::remove_cvref_t<F>>),
             nullptr,  // nothing to destroy
             nullptr) {
        if constexpr (Copyable) {
            vt.copy_ptr = reinterpret_cast<copy_ptr_t>(&copier<std::remove_cvref_t<F>, false>);
        }
    }

    GenericFunction(const GenericFunction& f) requires Copyable
        : fptr(f.vt.copy_ptr(f.fptr, small_buffer)),
          vt(f.vt) {
    }

    GenericFunction(GenericFunction&& f)
        : GenericFunction() {
        *this = std::forward<GenericFunction&&>(f);
    }

    GenericFunction& operator=(const GenericFunction& f) requires Copyable {
        if (this == &f) {
            return *this;
        }
        destroy_helper();

        fptr = f.vt.copy_ptr(f.fptr, small_buffer);

        vt = f.vt;

        return *this;
    }

    GenericFunction& operator=(GenericFunction&& f) {
        if (this == &f) {
            return *this;
        }
        destroy_helper();

        std::swap(vt, f.vt);

        // then need to do something with buffer
        std::swap_ranges(small_buffer, small_buffer + BUFFER_SIZE, f.small_buffer);

        if (f.is_small()) {
            fptr = small_buffer;
        } else {
            std::swap(fptr, f.fptr);
        }
        f.fptr = nullptr;

        return *this;
    }

    bool operator==(std::nullptr_t) const {
        return !static_cast<bool>(*this);
    }

    bool operator!=(std::nullptr_t) const {
        return static_cast<bool>(*this);
    }

    Ret operator()(Args... args) const {
        if (!vt.invoke_ptr) {
            throw std::bad_function_call();
        }
        return vt.invoke_ptr(fptr, std::forward<Args>(args)...);
    }

    ~GenericFunction() {
        destroy_helper();
    }

    explicit operator bool() const {
        return static_cast<bool>(fptr);
    }
};

template <typename Stuff>
struct Function : GenericFunction<Stuff, true> {
    using GenericFunction<Stuff, true>::GenericFunction;
};

template <typename Stuff>
struct MoveOnlyFunction : GenericFunction<Stuff, false> {
    using GenericFunction<Stuff, false>::GenericFunction;
};

// standard functions
template <typename R, typename... Args>
Function(R (*)(Args...)) -> Function<R(Args...)>;

// stuff for lambdas, class member functions
template <typename>
struct function_traits;

template <typename R, typename C, typename... Args>
struct function_traits<R (C::*)(Args...) const> {
    using signature = R(Args...);
};

template <typename F>
Function(F) -> Function<typename function_traits<decltype(&F::operator())>::signature>;

// standard functions
template <typename R, typename... Args>
MoveOnlyFunction(R (*)(Args...)) -> MoveOnlyFunction<R(Args...)>;

// stuff for lambdas, class member functions
template <typename>
struct move_only_function_traits;

template <typename R, typename C, typename... Args>
struct move_only_function_traits<R (C::*)(Args...) const> {
    using signature = R(Args...);
};

template <typename F>
MoveOnlyFunction(F)
    -> MoveOnlyFunction<typename move_only_function_traits<decltype(&F::operator())>::signature>;
