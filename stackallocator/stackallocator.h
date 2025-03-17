#include <memory>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <cstdint>
#include <array>
template <size_t N>
class StackStorage {
public:

    StackStorage() {}

    StackStorage& operator = (const StackStorage&) = delete;

    StackStorage(const StackStorage&) = delete;

    std::byte* get_memory(size_t amount , size_t alignment) {
        // std::cout << (tail - mem.begin()) << " bytes taken\n";
        // std::cout << "called get mem " << amount << ' ' << alignment << '\n';
        void* tailcast = reinterpret_cast<void*>(tail);
        // assert(amount % alignment == 0);
        size_t space = N - (tail - mem.begin());
        std::byte* allocated_start = reinterpret_cast<std::byte*>(custom_align(alignment, amount, tailcast, space));
        if (!allocated_start) {
            throw std::bad_alloc();
        }
        tail = allocated_start + amount;
        return allocated_start;
    }
private:
    std::array<std::byte, N> mem;
    std::byte* tail = mem.begin();

    void* custom_align(size_t alignment, size_t size, void*& ptr, size_t& space) {
        // std::align results in UB if alignment is not a power of 2. 
        // Since it is not true in most cases, i use custom alignment. 
        uintptr_t address = reinterpret_cast<uintptr_t>(ptr);


        size_t shift = (address % alignment == 0 ? 0 : alignment - (address % alignment));
        
        if (space < shift + size) return nullptr;
        
        ptr = reinterpret_cast<void*>(address + shift);
        space -= shift;
        return ptr;
    }
};

template <typename T, size_t N>
class StackAllocator {
public:
    using value_type = T;

    template <typename U, size_t M>
    friend class StackAllocator;

    StackAllocator(StackStorage<N>& ss) noexcept : storage(&ss) {};

    template<typename U>
    StackAllocator(const StackAllocator<U, N>& alloc) noexcept : storage(alloc.storage) {
        // std::cout << " Constructed allocator of " << typeid(T).name() << '\n';
    }

    StackAllocator& operator = (const StackAllocator& alloc) noexcept = default;
    
    T* allocate(size_t n) noexcept {
        void* raw_memory = storage -> get_memory(n * sizeof(T), sizeof(T));
        return reinterpret_cast<T*>(raw_memory);
    }

    void deallocate(T*, size_t) noexcept {} // Dumb version does nothing

    bool operator == (const StackAllocator& alloc) const noexcept {
        return (storage == alloc.storage);
    }



    template<typename U>
    struct rebind{
        using other = StackAllocator<U, N>;
    };
    

private:
    StackStorage<N>* storage = nullptr;
};
 

template <typename T, typename Allocator = std::allocator<T>>
class List {
public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = typename std::allocator_traits<Allocator>::pointer;
    using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
    using alloc_traits = typename std::allocator_traits<Allocator>;

private:
    class BaseNode;
    class ListNode;
    class FakeNode;
public:
    friend class BaseNode;
    friend class ListNode;
    friend class FakeNode;
    using true_alloc_type = typename alloc_traits::template rebind_alloc<ListNode>;
    using true_alloc_traits = typename alloc_traits::template rebind_traits<ListNode>;
    true_alloc_type allocator;

    template <bool is_const>
    class base_iterator {
        public:
            friend class List;
            using iterator_category = std::bidirectional_iterator_tag;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using pointer = typename std::conditional<is_const, const T*, T*>::type;
            using reference = typename std::conditional<is_const, const T&, T&>::type;
    
            base_iterator(const base_iterator<false> &val) : node(val.get_node()) {}

            base_iterator& operator=(const base_iterator&) = default;

            reference operator *() const { return node->get_reference(); }

            pointer operator -> () const { return node->get_ptr(); };

            bool operator == (const base_iterator& it) const {
                return (node == it.node);
            }

            base_iterator& operator ++ () {
                node = node->get_next();
                return *this;
            }

            base_iterator operator ++ (int) {
                base_iterator copy = *this;
                node = node->get_next();
                return copy;
            }

            base_iterator& operator -- () {
                node = node->get_prev();
                return *this;
            };

            base_iterator operator -- (int) {
                base_iterator copy = *this;
                node = node->get_next();
                return copy;
            }
            
            base_iterator next() const {
                base_iterator value = *this;
                ++value;
                return value;
            }

            base_iterator prev() const {
                base_iterator value = *this;
                --value;
                return value;
            }
        private:
            using true_type = std::conditional<is_const, const BaseNode*, BaseNode*>::type;
            true_type node;

            true_type get_node() const {
                return node;
            }

            explicit base_iterator(true_type nd) : node(nd) {}
        /// TODO
    };
public:
    using iterator = base_iterator<false>;
    using const_iterator = base_iterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
private:
    FakeNode tail;
    BaseNode* head = &tail;

    size_t size_ = 0;

    class BaseNode {
    public:
        friend class List;

        virtual T* get_ptr() = 0;

        virtual const T* get_ptr() const = 0;

        virtual const T get_value() const = 0;

        T& get_reference() {
            T* ptr = get_ptr();
            if (ptr == nullptr) {
                throw std::runtime_error("Trying to do some shit. \n");
            }
            return *ptr;
        }

        const T& get_reference() const {
            return *get_ptr();
        }
        
        virtual BaseNode* get_next() const = 0;

        ListNode* get_prev() const {
            return prev;
        };

    protected:
        ListNode *prev = nullptr;

        BaseNode(ListNode* prev_ = nullptr) : prev(prev_) {}
    };

    class ListNode : public BaseNode {
    public:
        friend class List;
        ListNode(ListNode* prev_, BaseNode* next_, const T& val)
        : BaseNode(prev_)
        , next(next_) 
        , value(val) {
            // std::cout << " ListNode created " << prev_ << ' ' << next_ << '\n';
            if (BaseNode::prev != nullptr) {
                BaseNode::prev->next = this;
            }
            if (next != nullptr) {
                next->prev = this;
            }
        }

        T* get_ptr() {
            return &value;
        }

        const T* get_ptr() const {
            return &value;
        }

        const T get_value() const {
            return value;
        }

        BaseNode* get_next() const {
            return next;
        }
        
    private:
        BaseNode *next = nullptr; // It can be FakeNode, so BaseNode
        T value;
    };

    class FakeNode : public BaseNode {
    public:
        friend class List;
        using BaseNode::BaseNode;

        T* get_ptr() {
            return nullptr;
        }
        
        const T* get_ptr() const {
            return nullptr;
        }

        const T get_value() const {
            throw std::out_of_range("Dereferenced .end() of List");
        }

        BaseNode* get_next() const {
            throw std::out_of_range(
                std::string("Trying to reach out of List\n") + 
                std::string("Debug logs:\n") + logs
            );
        }
        std::string logs = "Logs start\n";
    };
public:
    Allocator get_allocator() const {
        return Allocator(allocator); // вроде бы корректно?? 
    }
private:
    

    void destroy_helper() {
        std::cerr << "CALLED DESTROY HELPER";
        // Удаляет все от head до tail если все указатели правильны 
        // И спасает от копипасты 
        while (head != static_cast<BaseNode*>(&tail)) {
            ListNode* to_kill = static_cast<ListNode*>(head);
            head = head->get_next();
            true_alloc_traits::destroy(allocator, to_kill);
            true_alloc_traits::deallocate(allocator, to_kill, 1);
        }
    }


public:
    List() : List(Allocator()) {}

    explicit List(const Allocator& alloc) : allocator(alloc) {}

    explicit List(size_t n, const Allocator& alloc = Allocator()) : allocator(alloc) {
        build_from_equal_element(n, T());
    }

    explicit List(size_t n, const T& value, const Allocator& alloc = Allocator()) : allocator(alloc) {
        build_from_equal_element(n, value);
    }
private:

    void build_from_equal_element(size_t n, const T& value) {
        /*
            Requirements:
            1. *this has {head == tail} (it is empty)
            2. this->allocator is defined correctly at the moment            
        */
        size_ = n;
        if (n == 0) {
            return; // По стандарту это UB, но по моему представлению - empty list 
        }
        ListNode* new_head = nullptr;
        try {
            for (size_t i = 0; i < n; i++) {
                new_head = true_alloc_traits::allocate(allocator, 1);
                true_alloc_traits::construct(allocator, new_head, nullptr, head, value); /// TODO: move semantics for T() ? 
                head = new_head;
            }
        } catch (...) { 
            // полагаю что исключение в T(), т.к. остальные noexcept 
            true_alloc_traits::deallocate(allocator, new_head, 1);
            destroy_helper();
            throw;
        }
    };

    template<typename OtherAllocator>
    void build_by_other_list(const List<T, OtherAllocator>& other) {
        /*
            Requirements:
            1. *this has head == tail (it is empty)
            2. this->allocator is defined correctly at the moment
        */
        size_ = other.size_;
        typename List<T, OtherAllocator>::reverse_iterator it = other.rbegin();

        ListNode* new_head = nullptr;
        try {
            while (it != other.rend()) {
                new_head = true_alloc_traits::allocate(allocator, 1);
                true_alloc_traits::construct(allocator, new_head, nullptr, head, *it);
                head = new_head;
                it++;
            }
        } catch (...) {
            true_alloc_traits::deallocate(allocator, new_head, 1);
            destroy_helper();
            throw;
        }
    }

public:
    template<typename OtherAllocator>
    List(const List<T, OtherAllocator>& other, const Allocator& alloc) : allocator(alloc) {
        build_by_other_list(other);
    }

    template<typename OtherAllocator>
    List(const List<T, OtherAllocator>& other) : allocator(
        std::allocator_traits<Allocator>::
            select_on_container_copy_construction
                (other.get_allocator())
    ) {
        build_by_other_list(other);
    }

    template<typename OtherAllocator>
    List& operator=(const List<T, OtherAllocator>& other) {
        destroy_helper();
        /* TODO : необязательная оптимизация - переиспользование памяти в 
         случае, если итератор равен */
        if constexpr (
            std::allocator_traits<typename List<T, OtherAllocator>::allocator_type>::
            propagate_on_container_copy_assignment::
            value
        ) {
            allocator = other.get_allocator();
        }
        build_by_other_list(other);
        return *this;
    }

    ~List() noexcept {
        destroy_helper();
    }
    
    void check_link_safety() const {
        const BaseNode* now = head;
        while (now != static_cast<const BaseNode*>(&tail)) {
            std::cout << now << " there are we  now \n";
            std::cout << now -> get_next() << " the next\n";
            if (now == (now->get_next())->get_prev()) {
                std::cout << "OK!\n";
            } else {
                throw std::runtime_error("check failed :(");
            }
            now = now->get_next();
        }
        std::cout << " Check completed!\n";
    }
    
    iterator begin() {
        return iterator(head);
    }

    const_iterator begin() const {
        return const_iterator(head);
    }

    iterator end() {
        return iterator(&tail);
    }

    const_iterator end() const {
        return const_iterator(&tail);
    }

    const_iterator cbegin() const {
        return const_iterator(head);
    }

    const_iterator cend() const {
        return const_iterator(const_cast<FakeNode*>(&tail));
    }

    reverse_iterator rbegin() {
        return reverse_iterator(end());  // Reverse begins at end()
    }

    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(end());
    }

    reverse_iterator rend() {
        return reverse_iterator(begin());  // Reverse ends at begin()
    }

    const_reverse_iterator rend() const {
        return const_reverse_iterator(begin());
    }

    const_reverse_iterator crbegin() const {
        return const_reverse_iterator(end());  // crbegin() is like rbegin() but returns a const iterator
    }

    const_reverse_iterator crend() const {
        return const_reverse_iterator(begin());
    }

    size_t size() const {
        return size_;
    }

    bool empty() const {
        return size_ == 0;
    }

    void push_back(const T& val) {
        insert(cend(), val);
    };

    void push_front(const T& val) {
        insert(cbegin(), val);
    }

    void pop_back() {
        if (empty()) {
            throw std::out_of_range("pop_back from empty List");
        }
        erase(cend().prev());
    }

    void pop_front() {
        if (empty()) {
            throw std::out_of_range("pop_front from empty List");
        }
        erase(cbegin());
    };

    iterator erase(const_iterator pos) {
        BaseNode *current = const_cast<BaseNode*>(pos.get_node());
        BaseNode *nxt = current->get_next();
        ListNode *prv = current->get_prev();
        if (prv != nullptr) {
            prv->next = nxt;
        }
        nxt->prev = prv; // nxt точно существует - мб fake, но существует
        if (pos == cbegin()) {
            head = nxt;
        }
        true_alloc_traits::destroy(allocator, current);
        true_alloc_traits::deallocate(allocator, static_cast<ListNode*>(current), 1);


        size_--;
        return iterator(nxt);
    }
    
    iterator insert(const_iterator pos, const T& value) {
        // std::cout << " called insert! " << value << '\n';
        BaseNode* current = const_cast<BaseNode*>(pos.get_node());
        ListNode* prv = current->get_prev();
        ListNode* new_element = true_alloc_traits::allocate(allocator, 1);
        try {
            true_alloc_traits::construct(allocator, new_element, prv, current, value);
        } catch (...) {
            true_alloc_traits::deallocate(allocator, new_element, 1);
            throw;
        }
        // std::cout << " after construction: " << tail.get_prev() << '\n';
        // std::cout << (pos == cbegin()) << " comp result\n";

        if (pos == cbegin()) {
            head = new_element;
        }
        // std::cout << head << ' ' << &tail << '\n';
        size_++;
        return iterator(new_element);
    }
};