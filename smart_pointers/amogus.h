#include <iostream>
#include <random>

struct BaseControlBlock {
    size_t spcount = 0, weakcount = 0;

    BaseControlBlock()
    : spcount(0)
    , weakcount(0) {
    
    } 

    virtual ~BaseControlBlock() = default;
};


template<typename T>
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

    SharedPtr(BaseControlBlock* cb, T* ptr)
    : cblock(cb)
    , ptr(ptr) {
        cb -> spcount++;
        std::cout << "Private constructor\n";
        std::cerr << "Private constructor\n";
    }

    void delete_helper() {
        std::cout << " delete helper called\n";
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
                std::cout << " allahacbar\n";
                delete cblock;
            }
        }

        cblock = nullptr;
        ptr = nullptr;
    }
private:
    int value; // Просто число для примера

    // Генерация случайного числа
    int generateRandomValue() {
        static std::mt19937 rng(std::random_device{}());
        static std::uniform_int_distribution<int> dist(1, 100);
        return dist(rng);
    }

public:
    SharedPtr()
    : cblock(new WeakControlBlock())
    , ptr(nullptr) {
        cblock -> spcount++;
        value = generateRandomValue();
        std::cout << "Default constructor, random value = " << value << "\n";
    }

    // Конструктор от T*
    explicit SharedPtr(T* ptr) {
        value = static_cast<int>(*ptr);  // предполагаем, что *ptr можно привести к int
        std::cout << "Constructor from T*: value = " << value << "\n";
    }

    // Копирующий конструктор
    SharedPtr(const SharedPtr& other) {
        value = other.value;
        std::cout << "Copy constructor, copied value = " << value << "\n";
    }

    // Перемещающий конструктор
    SharedPtr(SharedPtr&& other) noexcept {
        value = other.value;
        std::cout << "Move constructor, moved value = " << value << "\n";
    }

    // Копирующий оператор присваивания
    SharedPtr& operator=(const SharedPtr& other) {
        if (this != &other) {
            value = other.value;
            std::cout << "Copy assignment, copied value = " << value << "\n";
        }
        return *this;
    }

    // Перемещающий оператор присваивания
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if (this != &other) {
            value = other.value;
            std::cout << "Move assignment, moved value = " << value << "\n";
        }
        return *this;
    }

    // Деструктор
    ~SharedPtr() {
        std::cout << "Destructor, value = " << value << "\n";
        delete_helper();
    }
};
