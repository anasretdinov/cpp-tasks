#include <memory>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <cstdint>
#include <array>

template<size_t N>
class StackStorage
{
  
  public: 
  inline StackStorage()
  {
  }
  
  // inline StackStorage<N> & operator=(const StackStorage<N> &) = delete;
  // inline StackStorage(const StackStorage<N> &) = delete;
  inline std::byte * get_memory(size_t amount, size_t alignment)
  {
    void * tail_casted = reinterpret_cast<void *>(this->mem.begin() + this->first_free);
    std::byte * first_free_align = reinterpret_cast<std::byte *>(std::align(alignment, amount, tail_casted, this->space_left));
    if(first_free_align == nullptr) {
      throw std::bad_alloc();
    } 
    
    this->space_left = this->space_left - amount;
    this->first_free = (N - this->space_left);
    return first_free_align;
  }
  
  
  private: 
  std::array<std::byte, N> mem;
  size_t first_free;
  size_t space_left;
};


template<typename T, size_t N>
class StackAllocator
{
  
  public: 
  using value_type = T;
  template<typename U, size_t M>
  friend class StackAllocator;
  inline StackAllocator(StackStorage<N> & ss) noexcept
  : storage(&ss)
  {
  }
  
  template<typename U>
  inline StackAllocator(const StackAllocator<U, N> & alloc) noexcept
  : storage(alloc.storage)
  {
  }
  
  inline StackAllocator<T, N> & operator=(const StackAllocator<T, N> & alloc) noexcept = default;
  inline T * allocate(size_t n)
  {
    void * raw_memory = this->storage->get_memory(n * sizeof(T), alignof(T));
    return reinterpret_cast<T *>(raw_memory);
  }
  
  inline void deallocate(T *, size_t) noexcept
  {
  }
  
  template<typename OtherT, size_t OtherN>
  inline bool operator==(const StackAllocator<OtherT, OtherN> & alloc) const noexcept
  {
    return (this->storage == alloc.storage);
  }
  template<typename U>
  struct rebind
  {
    using other = StackAllocator<U, N>;
  };
  
  
  private: 
  StackStorage<N> * storage;
};


template<typename T, typename Allocator = std::allocator<T>>
class List
{
  
  public: 
  using value_type = T;
  using reference = T &;
  using const_reference = const T &;
  using pointer = typename std::allocator_traits<Allocator>::pointer;
  using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
  using alloc_traits = std::allocator_traits<Allocator>;
  
  private: 
  struct BaseNode;
  struct ListNode;
  
  public: 
  friend BaseNode;
  friend ListNode;
  using true_alloc_type = typename alloc_traits::template rebind_alloc<ListNode>;
  using true_alloc_traits = typename alloc_traits::template rebind_traits<ListNode>;
  typename alloc_traits::template rebind_alloc<ListNode> allocator;
  template<bool is_const>
  class base_iterator
  {
    
    public: 
    friend List<T, Allocator>;
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = std::conditional_t<is_const, const T *, T *>;
    using reference = std::conditional_t<is_const, const T &, T &>;
    inline base_iterator(const base_iterator<false> & val)
    : node(val.node)
    {
    }
    
    inline base_iterator<is_const> & operator=(const base_iterator<is_const> &) = default;
    inline reference operator*() const
    {
      return static_cast<true_dereferencable_type>(this->node)->value;
    }
    
    inline pointer operator->() const
    {
      return &(static_cast<true_dereferencable_type>(this->node)->value);
    }
    
    inline bool operator==(const base_iterator<is_const> & it) const
    {
      return (this->node == it.node);
    }
    
    inline base_iterator<is_const> & operator++()
    {
      this->node = this->node->next;
      return *this;
    }
    
    inline base_iterator<is_const> operator++(int)
    {
      base_iterator<is_const> copy = *this /* NRVO variable */;
      this->node = this->node->next;
      return copy;
    }
    
    inline base_iterator<is_const> & operator--()
    {
      this->node = this->node->prev;
      return *this;
    }
    
    inline base_iterator<is_const> operator--(int)
    {
      base_iterator<is_const> copy = *this /* NRVO variable */;
      this->node = this->node->prev;
      return copy;
    }
    
    inline base_iterator<is_const> next() const
    {
      return iterator(this->node->next);
    }
    
    inline base_iterator<is_const> prev() const
    {
      return iterator(this->node->prev);
    }
    
    
    private: 
    using true_type = std::conditional_t<is_const, const BaseNode *, BaseNode *>;
    using true_dereferencable_type = std::conditional_t<is_const, const ListNode *, ListNode *>;
    true_type node;
    inline explicit base_iterator(true_type nd)
    : node(nd)
    {
    }
    
  };
  
  
  public: 
  using iterator = base_iterator<false>;
  using const_iterator = base_iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  
  private: 
  BaseNode root;
  size_t size_;
  struct BaseNode
  {
    BaseNode * prev;
    BaseNode * next;
    inline BaseNode()
    : prev(this)
    , next(this)
    {
    }
    
    // inline BaseNode(const BaseNode &) = delete;
    
    protected: 
    inline void link_between(BaseNode * prev_, BaseNode * next_)
    {
      this->prev = prev_;
      this->next = next_;
      this->prev->next = this;
      this->next->prev = this;
    }
    
  };
  
  struct ListNode : public BaseNode
  {
    T value;
    template<typename ... Args>
    inline ListNode(BaseNode * prev_, BaseNode * next_, const Args &... args)
    : BaseNode()
    , value(args... )
    {
      BaseNode::link_between(prev_, next_);
    }
    
  };
  
  
  public: 
  inline Allocator get_allocator() const
  {
    return Allocator(this->allocator);
  }
  
  
  private: 
  inline void destroy_helper()
  {
    while(this->root.next != &this->root) {
      ListNode * to_kill = static_cast<ListNode *>(this->root.next);
      this->root.next = to_kill->next;
      this->root.next->prev = &this->root;
      typename alloc_traits::template rebind_traits<ListNode>::destroy(this->allocator, to_kill);
      typename alloc_traits::template rebind_traits<ListNode>::deallocate(this->allocator, to_kill, 1);
    }
    
  }
  
  
  public: 
  inline List()
  : List<T, Allocator>(Allocator())
  {
  }
  
  inline explicit List(const Allocator & alloc)
  : allocator(alloc)
  , root()
  {
  }
  
  inline explicit List(size_t n, const Allocator & alloc)
  : allocator(alloc)
  , root()
  {
    if(n == 0) {
      return;
    } 
    
    build_from_equal_element(n);
  }
  
  inline explicit List(size_t n, const T & value, const Allocator & alloc)
  : allocator(alloc)
  , root()
  {
    build_from_equal_element(n, value);
  }
  
  
  private: 
  template<typename ... Args>
  inline void build_from_equal_element(size_t n, const Args &... args)
  {
    this->size_ = n;
    if(n == 0) {
      return;
    } 
    
    ListNode * new_element = nullptr;
    try 
    {
      for(size_t i = 0; i < n; i++) {
        new_element = typename alloc_traits::template rebind_traits<ListNode>::allocate(this->allocator, 1);
        typename alloc_traits::template rebind_traits<ListNode>::construct(this->allocator, new_element, this->root.prev, &this->root, args... );
      }
      
    } catch(...) {
      typename alloc_traits::template rebind_traits<ListNode>::deallocate(this->allocator, new_element, 1);
      this->destroy_helper();
      throw ;
    }
  }
  inline void build_by_other_list(const List<T, Allocator> & other)
  {
    this->size_ = 0;
    const_iterator it = other.cbegin();
    try 
    {
      while(it != other.cend()) {
        this->push_back(*it);
        ++it;
      }
      
    } catch(...) {
      this->destroy_helper();
      throw ;
    }
  }
  
  inline void wise_assignment(const List<T, Allocator> & other)
  {
    while(this->size() > other.size()) {
      this->pop_back();
    }
    
    iterator it = begin();
    const_iterator other_it = other.cbegin();
    while(it != end()) {
      *it = *other_it;
      ++it;
      ++other_it;
    }
    
    while(other_it != other.cend()) {
      this->push_back(*other_it);
      other_it++;
    }
    
  }
  
  
  public: 
  inline List(const List<T, Allocator> & other, const Allocator & alloc)
  : allocator(alloc)
  , root()
  {
    this->build_by_other_list(other);
  }
  
  inline List(const List<T, Allocator> & other)
  : allocator(alloc_traits::select_on_container_copy_construction(other.get_allocator()))
  , root()
  {
    this->build_by_other_list(other);
  }
  
  inline List<T, Allocator> & operator=(const List<T, Allocator> & other)
  {
    if(this == &other) {
      return *this;
    } 
    
    if(this->allocator == other.allocator) {
      this->wise_assignment(other);
    } else {
      this->destroy_helper();
      if constexpr(alloc_traits::propagate_on_container_copy_assignment::value) {
        this->allocator = other.get_allocator();
      } 
      
      this->build_by_other_list(other);
    } 
    
    return *this;
  }
  
  inline ~List() noexcept
  {
    this->destroy_helper();
  }
  
  inline void check_link_safety() const
  {
    const BaseNode * now = this->root.next;
    while(now != &this->root) {
      if(now != (now->next->prev)) {
        throw std::runtime_error(std::runtime_error("check failed :("));
      } 
      
      now = now->next;
    }
    
  }
  
  inline iterator begin()
  {
    return iterator(this->root.next);
  }
  
  inline const_iterator begin() const
  {
    return const_iterator(this->root.next);
  }
  
  inline iterator end()
  {
    return iterator(&this->root);
  }
  
  inline const_iterator end() const
  {
    return const_iterator(&this->root);
  }
  
  inline const_iterator cbegin() const
  {
    return const_iterator(this->root.next);
  }
  
  inline const_iterator cend() const
  {
    return const_iterator(&this->root);
  }
  
  inline reverse_iterator rbegin()
  {
    return reverse_iterator(end());
  }
  
  inline const_reverse_iterator rbegin() const
  {
    return const_reverse_iterator(end());
  }
  
  inline reverse_iterator rend()
  {
    return reverse_iterator(begin());
  }
  
  inline const_reverse_iterator rend() const
  {
    return const_reverse_iterator(begin());
  }
  
  inline const_reverse_iterator crbegin() const
  {
    return const_reverse_iterator(end());
  }
  
  inline const_reverse_iterator crend() const
  {
    return const_reverse_iterator(begin());
  }
  
  inline size_t size() const
  {
    return this->size_;
  }
  
  inline bool empty() const
  {
    return this->size_ == 0;
  }
  
  inline void push_back(const T & val)
  {
    this->insert(this->cend(), val);
  }
  
  inline void push_front(const T & val)
  {
    this->insert(this->cbegin(), val);
  }
  
  inline void pop_back()
  {
    if(this->empty()) {
      throw std::out_of_range(std::out_of_range("pop_back from empty List"));
    } 
    
    this->erase(this->cend().prev());
  }
  
  inline void pop_front()
  {
    if(this->empty()) {
      throw std::out_of_range(std::out_of_range("pop_front from empty List"));
    } 
    
    this->erase(this->cbegin());
  }
  
  inline iterator erase(const_iterator pos)
  {
    const ListNode * current = static_cast<const ListNode *>(pos.node);
    current->next->prev = current->prev;
    current->prev->next = current->next;
    iterator to_return = {current->next} /* NRVO variable */;
    typename alloc_traits::template rebind_traits<ListNode>::destroy(this->allocator, current);
    typename alloc_traits::template rebind_traits<ListNode>::deallocate(this->allocator, const_cast<ListNode *>(current), 1);
    this->size_--;
    return to_return;
  }
  
  inline iterator insert(const_iterator pos, const T & value)
  {
    BaseNode * after_new = const_cast<BaseNode *>(pos.node);
    BaseNode * before_new = after_new->prev;
    ListNode * new_element = typename alloc_traits::template rebind_traits<ListNode>::allocate(this->allocator, 1);
    try 
    {
      typename alloc_traits::template rebind_traits<ListNode>::construct(this->allocator, new_element, before_new, after_new, value);
    } catch(...) {
      typename alloc_traits::template rebind_traits<ListNode>::deallocate(this->allocator, new_element, 1);
      throw ;
    }
    this->size_++;
    return iterator(new_element);
  }
  
};

