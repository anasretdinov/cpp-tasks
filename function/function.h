#include <algorithm>  // std::swap_ranges
#include <functional>
#include <iostream>
template <typename, bool Copyable>
class GenericFunction;

template <typename Ret, typename... Args, bool Copyable>
class GenericFunction<Ret(Args...), Copyable> {
private:
    static const size_t kBufferSize = 16;

    void* fptr_;

    char small_buffer_[kBufferSize];

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

    custom_vtable vt_;

private:
    template <typename F>
    static Ret invoker(void* func, Args... args) {
        return std::invoke(*reinterpret_cast<F*>(func), std::forward<Args>(args)...);
    }

    template <typename F>
    static void destroyer(void* func) {
        if constexpr (sizeof(F) > kBufferSize) {
            delete reinterpret_cast<F*>(func);
        } else {
            reinterpret_cast<F*>(func)->~F();
        }
    }

    template <typename F, bool TreatAsObject>
    static void* copier(void* func, char* target_buffer)
        requires Copyable
    {

        if constexpr (TreatAsObject) {
            if constexpr (sizeof(F) > kBufferSize) {
                return new F(*reinterpret_cast<F*>(func));
            } else {
                new (target_buffer) F(*reinterpret_cast<F*>(func));
                return target_buffer;
            }
        } else {
            return func;
        }
    }

    void destroy_helper() {
        if (vt_.destroy_ptr) {
            vt_.destroy_ptr(fptr_);
        }
        fptr_ = nullptr;
        vt_.clear();
    }

    bool is_small() const {
        return fptr_ == small_buffer_;
    }

public:
    // empty constructor
    GenericFunction()
        : fptr_(nullptr),
          vt_(nullptr, nullptr, nullptr) {
    }

    // i can't call sizeof from c-style func
    // so i write requires
    template <typename F>
        requires(!std::is_same_v<std::remove_cvref_t<F>, GenericFunction> &&
                 !std::is_function_v<std::remove_cvref_t<F>> && std::invocable<F, Args...>)
    GenericFunction(F&& func)
        : vt_(reinterpret_cast<invoke_ptr_t>(&invoker<std::remove_cvref_t<F>>),
              reinterpret_cast<destroy_ptr_t>(&destroyer<std::remove_cvref_t<F>>), nullptr) {
        if constexpr (Copyable) {
            vt_.copy_ptr = reinterpret_cast<copy_ptr_t>(&copier<std::remove_cvref_t<F>, true>);
        }
        using TrueType = std::remove_cvref_t<F>;
        if constexpr (sizeof(TrueType) > kBufferSize) {
            fptr_ = new F(std::forward<F>(func));
        } else {
            new (small_buffer_) TrueType(std::forward<F>(func));
            fptr_ = small_buffer_;
        }
    }

    // this is for functions
    template <typename F>
        requires(std::is_function_v<std::remove_cvref_t<F>>&& std::invocable<F, Args...>)
    GenericFunction(F* func)
        : fptr_(reinterpret_cast<void*>(func)),
          vt_(reinterpret_cast<invoke_ptr_t>(&invoker<std::remove_cvref_t<F>>),
             nullptr,  // nothing to destroy
             nullptr) {
        if constexpr (Copyable) {
            vt_.copy_ptr = reinterpret_cast<copy_ptr_t>(&copier<std::remove_cvref_t<F>, false>);
        }
    }

    GenericFunction(const GenericFunction& f)
        requires Copyable
        : fptr_(f.vt_.copy_ptr(f.fptr_, small_buffer_)),
          vt_(f.vt_) {
    }

    GenericFunction(GenericFunction&& f)
        : GenericFunction() {
        *this = std::forward<GenericFunction&&>(f);
    }

    GenericFunction& operator=(const GenericFunction& f)
        requires Copyable
    {
        if (this == &f) {
            return *this;
        }
        destroy_helper();

        fptr_ = f.vt_.copy_ptr(f.fptr_, small_buffer_);

        vt_ = f.vt_;

        return *this;
    }

    GenericFunction& operator=(GenericFunction&& f) {
        if (this == &f) {
            return *this;
        }
        destroy_helper();

        std::swap(vt_, f.vt_);

        // then need to do something with buffer
        std::swap_ranges(small_buffer_, small_buffer_ + kBufferSize, f.small_buffer_);

        if (f.is_small()) {
            fptr_ = small_buffer_;
        } else {
            std::swap(fptr_, f.fptr_);
        }
        f.fptr_ = nullptr;

        return *this;
    }

    bool operator==(std::nullptr_t) const {
        return !static_cast<bool>(*this);
    }

    bool operator!=(std::nullptr_t) const {
        return static_cast<bool>(*this);
    }

    Ret operator()(Args... args) const {
        if (!vt_.invoke_ptr) {
            throw std::bad_function_call();
        }
        return vt_.invoke_ptr(fptr_, std::forward<Args>(args)...);
    }

    ~GenericFunction() {
        destroy_helper();
    }

    explicit operator bool() const {
        return static_cast<bool>(fptr_);
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
