#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

using mem_type = unsigned char;

template <size_t N>
class alignas(std::max_align_t) StackStorage {
public:
    StackStorage() {
    }

    StackStorage& operator=(const StackStorage&) = delete;

    StackStorage(const StackStorage&) = delete;

    mem_type* get_memory(size_t amount, size_t alignment) {
        mem_type* first_free_align =
            custom_align(alignment, amount, mem_ + first_free_, space_left_);

        if (first_free_align == nullptr) {
            throw std::bad_alloc();
        }

        space_left_ -= amount;
        first_free_ = N - space_left_;
        return first_free_align;
    }

private:
    mem_type mem_[N];

    size_t first_free_ = 0;
    size_t space_left_ = N;

    mem_type* custom_align(size_t alignment, size_t size, mem_type* ptr, size_t space) {
        std::uintptr_t numerical = reinterpret_cast<std::uintptr_t>(ptr);

        size_t shift = (alignment - (numerical % alignment)) % alignment;

        if (space < size + shift) {
            return nullptr;
        }
        ptr += shift;
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

    StackAllocator(StackStorage<N>& ss) noexcept
        : storage_(&ss) {};

    template <typename U>
    StackAllocator(const StackAllocator<U, N>& alloc) noexcept
        : storage_(alloc.storage_) {
    }

    // StackAllocator& operator=(const StackAllocator& alloc) noexcept = default;

    T* allocate(size_t n) {
        mem_type* raw_memory = storage_->get_memory(n * kSize, kAlignment);
        return reinterpret_cast<T*>(raw_memory);
    }

    void deallocate(T*, size_t) noexcept {
    }  // Dumb version does nothing

    template <typename OtherT>
    bool operator==(const StackAllocator<OtherT, N>& alloc) const noexcept {
        return (storage_ == alloc.storage_);
    }

    template <typename U>
    struct rebind {
        using other = StackAllocator<U, N>;
    };

private:
    StackStorage<N>* storage_ = nullptr;
    static constexpr size_t kAlignment = alignof(T);
    static constexpr size_t kSize = sizeof(T);
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
    using node_alloc_type = typename alloc_traits::template rebind_alloc<ListNode>;
    using node_alloc_traits = typename alloc_traits::template rebind_traits<ListNode>;
    node_alloc_type allocator;

    template <bool is_const>
    class BaseIterator {
    public:
        friend class List;
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = typename std::conditional_t<is_const, const T*, T*>;
        using reference = typename std::conditional_t<is_const, const T&, T&>;

        BaseIterator(const BaseIterator<false>& val)
            : node_(val.node_) {
        }

        // BaseIterator& operator=(const BaseIterator&) = default;

        BaseIterator& operator=(const BaseIterator<false>& val) {
            node_ = val.node_;
            return *this;
        }

        reference operator*() const {
            return static_cast<node_dereferencable_type>(node_)->value;
        }

        pointer operator->() const {
            return &(this->operator*());
        }

        bool operator==(const BaseIterator& it) const {
            return (node_ == it.node_);
        }

        BaseIterator& operator++() {
            node_ = node_->next;
            return *this;
        }

        BaseIterator operator++(int) {
            BaseIterator copy = *this;
            node_ = node_->next;
            return copy;
        }

        BaseIterator& operator--() {
            node_ = node_->prev;
            return *this;
        };

        BaseIterator operator--(int) {
            BaseIterator copy = *this;
            node_ = node_->prev;
            return copy;
        }

        BaseIterator next() const {
            return iterator(node_->next);
        }

        BaseIterator prev() const {
            return iterator(node_->prev);
        }

    private:
        using node_type = typename std::conditional_t<is_const, const BaseNode*, BaseNode*>;
        using node_dereferencable_type =
            typename std::conditional_t<is_const, const ListNode*, ListNode*>;

        node_type node_;

        explicit BaseIterator(node_type nd)
            : node_(nd) {
        }
        /// TODO
    };

public:
    using iterator = BaseIterator<false>;
    using const_iterator = BaseIterator<true>;

    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:
    BaseNode root_;

    size_t size_ = 0;

    struct BaseNode {
        BaseNode *prev = nullptr, *next = nullptr;
        BaseNode()
            : prev(this),
              next(this) {
        }

        BaseNode(const BaseNode&) = delete;

    protected:
        void link_between(BaseNode* prv, BaseNode* nxt) {
            prev = prv;
            next = nxt;
            prev->next = this;
            next->prev = this;
        }
    };

    struct ListNode : BaseNode {
        T value;

        template <typename... Args>
        ListNode(BaseNode* prv, BaseNode* nxt, const Args&... args)
            : BaseNode(),
              value(args...) {
            BaseNode::link_between(prv, nxt);
        }
    };

public:
    Allocator get_allocator() const {
        return Allocator(allocator);  // вроде бы корректно??
    }

private:
    void clear() {
        // Удаляет все, если все указатели правильны
        // И спасает от копипасты
        while (root_.next != &root_) {
            ListNode* to_kill = static_cast<ListNode*>(root_.next);
            root_.next = to_kill->next;
            root_.next->prev = &root_;
            node_alloc_traits::destroy(allocator, to_kill);
            node_alloc_traits::deallocate(allocator, to_kill, 1);
        }
    }

public:
    List()
        : List(Allocator()) {
    }

    explicit List(const Allocator& alloc)
        : allocator(alloc),
          root_() {
    }

    explicit List(size_t n, const Allocator& alloc = Allocator())
        : allocator(alloc),
          root_() {
        if (n == 0) {
            return;
        }
        build_from_equal_element(n);
    }

    explicit List(size_t n, const T& value, const Allocator& alloc = Allocator())
        : allocator(alloc),
          root_() {
        build_from_equal_element(n, value);
    }

private:
    template <typename... Args>
    void build_from_equal_element(size_t n, const Args&... args) {
        /*
            Requirements:
            1. *this is empty
            2. this->allocator is defined correctly at the moment
        */
        size_ = n;
        if (n == 0) {
            return;  // По стандарту это UB, но по моему представлению - empty list
        }
        ListNode* new_element = nullptr;
        try {
            for (size_t i = 0; i < n; i++) {
                new_element = node_alloc_traits::allocate(allocator, 1);
                node_alloc_traits::construct(allocator, new_element, root_.prev, &root_,
                                             args...);  /// TODO: move semantics for T() ?
            }
        } catch (...) {
            // полагаю что исключение в T(), т.к. остальные noexcept
            node_alloc_traits::deallocate(allocator, new_element, 1);
            clear();
            throw;
        }
    }

    void build_by_other_list(const List& other) {
        /*
            Requirements:
            1. *this is empty
            2. this->allocator is defined correctly at the moment
        */
        size_ = 0;
        const_iterator it = other.cbegin();
        try {
            while (it != other.cend()) {
                push_back(*it);
                ++it;
            }
        } catch (...) {
            clear();
            throw;
        }
    }

    void wise_assignment(const List& other) {
        while (size() > other.size()) {
            pop_back();
        }

        iterator it = begin();
        const_iterator other_it = other.cbegin();

        while (it != end()) {
            *it = *other_it;
            ++it;
            ++other_it;
        }

        while (other_it != other.cend()) {
            push_back(*other_it);
            other_it++;
        }
    }

public:
    List(const List& other, const Allocator& alloc)
        : allocator(alloc),
          root_() {
        build_by_other_list(other);
    }

    List(const List& other)
        : allocator(alloc_traits::select_on_container_copy_construction(other.get_allocator())),
          root_() {
        build_by_other_list(other);
    }

    List& operator=(const List& other) {
        if (this == &other) {
            return *this;
        }

        if (allocator == other.allocator) {
            // реюзаем
            wise_assignment(other);
        } else {
            clear();
            if constexpr (alloc_traits::propagate_on_container_copy_assignment::value) {
                allocator = other.get_allocator();
            }
            build_by_other_list(other);
        }
        return *this;
    }

    ~List() noexcept {
        clear();
    }

    void check_link_safety() const {
        const BaseNode* now = root_.next;
        while (now != &root_) {
            if (now != (now->next->prev)) {
                throw std::runtime_error("check failed :(");
            }
            now = now->next;
        }
    }

    iterator begin() {
        return iterator(root_.next);
    }

    const_iterator begin() const {
        return const_iterator(root_.next);
    }

    iterator end() {
        return iterator(&root_);
    }

    const_iterator end() const {
        return const_iterator(&root_);
    }

    const_iterator cbegin() const {
        return const_iterator(root_.next);
    }

    const_iterator cend() const {
        return const_iterator(&root_);
    }

    reverse_iterator rbegin() {
        return reverse_iterator(end());
    }

    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(end());
    }

    reverse_iterator rend() {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rend() const {
        return const_reverse_iterator(begin());
    }

    const_reverse_iterator crbegin() const {
        return const_reverse_iterator(end());
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

    template <typename... Args>
    void emplace_back(const Args&... args) {
        ListNode* place = node_alloc_traits::allocate(allocator, 1);

        try {
            node_alloc_traits::construct(allocator, place, root_.prev, root_, args...);
        } catch (...) {
            node_alloc_traits::deallocate(allocator, place, 1);
            throw;
        }
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
        const ListNode* current = static_cast<const ListNode*>(pos.node_);

        current->next->prev = current->prev;
        current->prev->next = current->next;

        iterator to_return{current->next};

        node_alloc_traits::destroy(allocator, current);
        node_alloc_traits::deallocate(allocator, const_cast<ListNode*>(current), 1);

        size_--;
        return to_return;
    }

    iterator insert(const_iterator pos, const T& value) {
        BaseNode* after_new = const_cast<BaseNode*>(pos.node_);
        BaseNode* before_new = after_new->prev;
        ListNode* new_element = node_alloc_traits::allocate(allocator, 1);
        try {
            node_alloc_traits::construct(allocator, new_element, before_new, after_new, value);
        } catch (...) {
            node_alloc_traits::deallocate(allocator, new_element, 1);
            throw;
        }

        size_++;
        return iterator(new_element);
    }
};