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
    int value; // Просто число для примера

    // Генерация случайного числа
    int generateRandomValue() {
        static std::mt19937 rng(std::random_device{}());
        static std::uniform_int_distribution<int> dist(1, 100);
        return dist(rng);
    }

public:
    // Пустой конструктор
    SharedPtr() {
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
    }
};
