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
        std::cout << " get mem called \n";
        size_t space = N - (tail - mem.begin());

        void* tail_casted = static_cast<void*>(tail);
        void* res = std::align(alignment, amount, tail_casted, space);
        if (res == nullptr) {
            std::cout << "Bum.\n";
            throw std::bad_alloc();
        }
        std::cout << space - amount << " space left \n";
        tail = static_cast<std::byte*>(res);

        std::byte* to_return = tail;
        tail += amount;
        return to_return;
    }
private:
    std::array<std::byte, N> mem;
    std::byte* tail = mem.begin();

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
        void* raw_memory = storage -> get_memory(n * sizeof(T), alignof(T));
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
    struct BaseNode;
    struct ListNode;
public:
    friend struct BaseNode;
    friend struct ListNode;
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
            using pointer = typename std::conditional_t<is_const, const T*, T*>;
            using reference = typename std::conditional_t<is_const, const T&, T&>;
    
            base_iterator(const base_iterator<false> &val) : node(val.node) {}

            base_iterator& operator=(const base_iterator&) = default;

            reference operator *() const {
                return static_cast<true_dereferencable_type>(node)->value; 
            }

            pointer operator -> () const {
                return &(static_cast<true_dereferencable_type>(node)->value); 
            }

            bool operator == (const base_iterator& it) const {
                return (node == it.node);
            }

            base_iterator& operator ++ () {
                node = node->next;
                return *this;
            }

            base_iterator operator ++ (int) {
                base_iterator copy = *this;
                node = node->next;
                return copy;
            }

            base_iterator& operator -- () {
                node = node->prev;
                return *this;
            };

            base_iterator operator -- (int) {
                base_iterator copy = *this;
                node = node->prev;
                return copy;
            }
            
            base_iterator next() const {
                return iterator(node->next);
            }

            base_iterator prev() const {
                return iterator(node->prev);
            }
        private:
            using true_type = typename std::conditional_t<is_const, const BaseNode*, BaseNode*>;
            using true_dereferencable_type = typename std::conditional_t<is_const, const ListNode*, ListNode*>;

            true_type node;

            explicit base_iterator(true_type nd) : node(nd) {}
        /// TODO
    };
public:
    using iterator = base_iterator<false>;
    using const_iterator = base_iterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
private:
    BaseNode root;

    size_t size_ = 0;

    struct BaseNode {
        BaseNode * prev = nullptr, * next = nullptr;
        BaseNode() : prev(this), next(this) {}

        BaseNode(const BaseNode&) = delete; // без хуйни 

        /* 
        BaseNode(BaseNode * prev_, BaseNode * next_)
        : prev(prev_)
        , next(next_) {
            prev -> next = this;
            next -> prev = this;
        }
        */
    protected:
        void link_between(BaseNode * prev_, BaseNode * next_) {
            prev = prev_;
            next = next_;
            prev -> next = this;
            next -> prev = this;
        }
    };

    struct ListNode : BaseNode {
        T value;

        ListNode(BaseNode * prev_, BaseNode * next_, const T& val) : BaseNode(), value(val) {
            BaseNode::link_between(prev_, next_);
        }
    };

public:
    Allocator get_allocator() const {
        return Allocator(allocator); // вроде бы корректно?? 
    }

private:
    void destroy_helper() {
        // std::cerr << "CALLED DESTROY HELPER\n";
        // std::cerr << "id by last : " << crbegin()->x << '\n';
        // std::cerr << size() << "that's the size\n";
        // Удаляет все, если все указатели правильны 
        // И спасает от копипасты 
        while (root.next != &root) {
            ListNode* to_kill = static_cast<ListNode*>(root.next);
            root.next = to_kill->next;
            root.next->prev = &root;
            true_alloc_traits::destroy(allocator, to_kill);
            true_alloc_traits::deallocate(allocator, to_kill, 1);
        }
    }


public:
    List() : List(Allocator()) {}

    explicit List(const Allocator& alloc) : allocator(alloc), root() {}

    explicit List(size_t n, const Allocator& alloc = Allocator()) : allocator(alloc), root() {
        if (n == 0) {
            return;
        }
        build_from_equal_element(n, T());
    }

    explicit List(size_t n, const T& value, const Allocator& alloc = Allocator()) : allocator(alloc), root() {
        build_from_equal_element(n, value);
    }

private:
    void build_from_equal_element(size_t n, const T& value) {
        /*
            Requirements:
            1. *this is empty
            2. this->allocator is defined correctly at the moment            
        */
        size_ = n;
        if (n == 0) {
            return; // По стандарту это UB, но по моему представлению - empty list 
        }
        ListNode * new_element = nullptr;
        try {
            for (size_t i = 0; i < n; i++) {
                new_element = true_alloc_traits::allocate(allocator, 1);
                true_alloc_traits::construct(allocator, new_element, root.prev, &root, value); /// TODO: move semantics for T() ? 
            }
        } catch (...) { 
            // полагаю что исключение в T(), т.к. остальные noexcept 
            true_alloc_traits::deallocate(allocator, new_element, 1);
            destroy_helper();
            throw;
        }
    };

    template<typename OtherAllocator>
    void build_by_other_list(const List<T, OtherAllocator>& other) {
        /*
            Requirements:
            1. *this is empty
            2. this->allocator is defined correctly at the moment
        */
        size_ = other.size_;
        // std::cerr << " called build by other\n";
        typename List<T, OtherAllocator>::const_iterator it = other.begin();
        ListNode * new_element = nullptr;
        try {
            while (it != other.end()) {
                new_element = true_alloc_traits::allocate(allocator, 1);
                true_alloc_traits::construct(allocator, new_element, root.prev, &root, *it);
                ++it;
            }
        } catch(...) {
            true_alloc_traits::deallocate(allocator, new_element, 1);
            destroy_helper();
            throw;
        }
    }

public:
    template<typename OtherAllocator>
    List(const List<T, OtherAllocator>& other, const Allocator& alloc) : allocator(alloc), root() {
        build_by_other_list(other);
    }

    template<typename OtherAllocator>
    List(const List<T, OtherAllocator>& other) 
    : allocator(
        std::allocator_traits<OtherAllocator>::
            select_on_container_copy_construction
                (other.get_allocator()))
    , root() {
        build_by_other_list(other);
    }

    List(const List& other) 
    : allocator(
        std::allocator_traits<Allocator>::
            select_on_container_copy_construction
                (other.get_allocator()))
    , root() {
        // std::cerr << "Hopa!\n";
        build_by_other_list(other); 
    }

    template<typename OtherAllocator>
    List& operator=(const List<T, OtherAllocator>& other) {
        destroy_helper();
        /* TODO : необязательная оптимизация - переиспользование памяти в 
         случае, если аллокатор равен */
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

    List& operator=(const List& other) {
        if (this == &other) {
            return *this;
        }
        destroy_helper();
        build_by_other_list(other);
        return *this;
    }


    ~List() noexcept {
        destroy_helper();
    }
    
    void check_link_safety() const {
        const BaseNode* now = root.next;
        while (now != &root) {
            if (now != (now->next->prev)) {
                throw std::runtime_error("check failed :(");
            }
            now = now->next;
        }
    }
    
    iterator begin() {
        return iterator(root.next);
    }

    const_iterator begin() const {
        return const_iterator(root.next);
    }

    iterator end() {
        return iterator(&root);
    }

    const_iterator end() const {
        return const_iterator(&root);
    }

    const_iterator cbegin() const {
        return const_iterator(root.next);
    }

    const_iterator cend() const {
        return const_iterator(&root);
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
        const ListNode * current = static_cast<const ListNode*>(pos.node);

        current->next->prev = current->prev;
        current->prev->next=current->next;

        iterator to_return{current->next};

        true_alloc_traits::destroy(allocator, current);
        true_alloc_traits::deallocate(allocator, const_cast<ListNode*>(current), 1);

        size_--;
        return to_return;
    }
    
    iterator insert(const_iterator pos, const T& value) {
        std::cout << " hohoho\n";
        BaseNode * after_new = const_cast<BaseNode*>(pos.node);
        BaseNode * before_new = after_new->prev;
        ListNode * new_element = true_alloc_traits::allocate(allocator, 1);
        std::cout << "hihihi\n";
        try {
            true_alloc_traits::construct(allocator, new_element, before_new, after_new, value);
        } catch (...) {
            true_alloc_traits::deallocate(allocator, new_element, 1);
            throw;
        }

        size_++;
        return iterator(new_element);
    }
};