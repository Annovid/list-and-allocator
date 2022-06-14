#include <iostream>
#include <vector>

template <size_t chunkSize>
struct FixedAllocator {
    static size_t count_of_alloc;
    const size_t count_of_elements = 2048;
    static std::vector<void*> free, given;
    FixedAllocator() {
        ++count_of_alloc;
    }
    FixedAllocator(const FixedAllocator<chunkSize>&) {
        ++count_of_alloc;
    }
    void* allocate() {
        if (free.empty()) {
            void* ptr_ = ::operator new(chunkSize * count_of_elements);
            given.push_back(ptr_);
            for (size_t i = 0; i < count_of_elements; ++i) {
                int8_t* ptr1 = reinterpret_cast<int8_t*>(ptr_);
                int8_t* ptr2 = ptr1 + i * chunkSize;
                void* ptr3 = reinterpret_cast<void*>(ptr2);
                free.push_back(ptr3);
            }
        }
        void *ptr = free.back();
        free.pop_back();
        return ptr;
    }
    void deallocate(void* ptr) {
        free.push_back(ptr);
    }
    FixedAllocator operator=(FixedAllocator) {
        ++count_of_alloc;
        return *this;
    }
    ~FixedAllocator() {
        --count_of_alloc;
        if (count_of_alloc == 0) {
            free.clear();
            while (!given.empty()) {
                ::operator delete(given.back());
                given.pop_back();
            }
        }
    }
};

template <size_t chunkSize>
size_t FixedAllocator<chunkSize>::count_of_alloc = 0;
template <size_t chunkSize>
std::vector<void*> FixedAllocator<chunkSize>::free;
template <size_t chunkSize>
std::vector<void*> FixedAllocator<chunkSize>::given;

template <typename T>
struct FastAllocator {
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using value_type = T;

    FastAllocator() = default;
    template<typename T1>
    FastAllocator(const FastAllocator<T1>&) {}
    FixedAllocator<8> fixed8;
    FixedAllocator<16> fixed16;
    FixedAllocator<24> fixed24;
    FixedAllocator<32> fixed32;
    T* allocate(size_t cnt) {
        switch(cnt * sizeof(T)) {
            case 8:
                return static_cast<T*>(fixed8.allocate());
            case 16:
                return static_cast<T*>(fixed16.allocate());
            case 24:
                return static_cast<T*>(fixed24.allocate());
            case 32:
                return static_cast<T*>(fixed32.allocate());
            default:
                return static_cast<T*>(::operator new(cnt * sizeof(T)));
        }
    }

    void deallocate(T* ptr, size_t cnt) {
        switch(cnt * sizeof(T)) {
            case 8:
                fixed8.deallocate(reinterpret_cast<void*>(ptr));
                break;
            case 16:
                fixed16.deallocate(reinterpret_cast<void*>(ptr));
                break;
            case 24:
                fixed24.deallocate(reinterpret_cast<void*>(ptr));
                break;
            case 32:
                fixed32.deallocate(reinterpret_cast<void*>(ptr));
                break;
            default:
                ::operator delete(ptr);
        }
    }
    template<typename... Args>
    void construct(T* ptr, Args&... args) {
        new (ptr) T(args...);
    }
    void destroy(T* ptr) {
        ptr->~T();
    }
    template<typename U>
    bool operator==(FastAllocator<U>) {
        return true;
    }
    template<typename U>
    bool operator!=(FastAllocator<U>) {
        return false;
    }
};

template <typename T, typename Allocator = std::allocator<T>>
struct List {
    struct Node {
        Node * prev = this;
        Node * next = this;
        T value;
        Node() = default;
        Node(const T& value_): value(value_) {}
        bool operator==(const Node & it) const {
            return prev == it.prev && next == it.next;
        }
        bool operator!=(const Node & it) const {
            return (this == it);
        }
        ~Node() = default;
    };
    typename std::allocator_traits<Allocator>::template rebind_alloc<Node> alloc;
    using AllocTraits = std::allocator_traits<typename std::allocator_traits<Allocator::template rebind_alloc<Node>>;
    Node * fake;
    template <bool IsConst>
    struct common_iterator {
    public:
        using conditional_ptr = typename std::conditional<IsConst, const T*, T*>::type;
        using conditional_node = typename std::conditional<IsConst, const Node*, Node*>::type;
        using conditional_ref = typename std::conditional<IsConst, const T&, T&>::type;

        using difference_type = int;
        using value_type = typename std::conditional<IsConst, const T, T>::type;
        using pointer = conditional_ptr;
        using reference = conditional_ref;
        using iterator_category = std::bidirectional_iterator_tag;
        Node * node;
        common_iterator() = delete;
        common_iterator(Node & node): node(&node) {}
        common_iterator(const Node & node_): node(&node_) {}
        template <bool IsConst1>
        common_iterator(common_iterator<IsConst1>& other): node(other.node) {}
        operator common_iterator<true>() {
            return common_iterator<true>(*node);
        }
        common_iterator & operator++() {
            node = reinterpret_cast<Node*>(node->next);
            return *this;
        }
        common_iterator<false> operator++(int) {
            common_iterator ans = *this;
            ++*this;
            return ans;
        }
        common_iterator & operator--() {
            node = reinterpret_cast<Node*>(node->prev);
            return *this;
        }
        common_iterator operator--(int) {
            common_iterator ans = *this;
            --*this;
            return ans;
        }
        bool operator==(const common_iterator& it) const {
            return (this->node == it.node);
        }
        bool operator!=(const common_iterator& it) const {
            return !(*this == it);
        }
        std::conditional_t<IsConst, const T&, T&> operator*() {
            return node->value;
        }
        std::conditional_t<IsConst, const T*, T*> operator->() {
            return &node->value;
        }
    };
    int size_ = 0;
public:
    using iterator = common_iterator<false>;
    using const_iterator = common_iterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    List() {
        fake = AllocTraits::allocate(alloc, 1);
        fake->prev = fake;
        fake->next = fake;
    }
    explicit List(const Allocator) : List() {}
    List(size_t count, const T& value): List() {
        for (size_t i = 0; i < count; ++i) {
            push_back(value);
        }
    }
    List(size_t count): List() {
        for (size_t i = 0; i < count; ++i) {
            push_back();
        }
    }
    List(const List & list): List() {
        if (alloc != list.alloc) {
            alloc = AllocTraits::select_on_container_copy_construction(list.alloc);
        }
        for (auto it = list.begin(); it != list.end(); ++it)
            push_back(*it);
    }
    List & operator=(const List & list) {
        std::vector<Node*> nodes;
        for (auto node = fake->next; node != fake; node = node->next) {
            nodes.push_back(node);
        }
        for (auto node : nodes) {
            AllocTraits::destroy(alloc, node);
            AllocTraits::deallocate(alloc, node, 1);
        }
        if (AllocTraits::propagate_on_container_copy_assignment::value)
            alloc = list.alloc;
        fake->prev = fake;
        fake->next = fake;
        for (auto it = list.begin(); it != list.end(); ++it)
            push_back(*it);
        size_ = list.size();
        return *this;
    }
    int size() const {
        return size_;
    }
    iterator begin() {
        return iterator(*reinterpret_cast<Node*>(fake->next));
    }
    const_iterator begin() const {
        return const_iterator(*reinterpret_cast<Node*>(fake->next));
    }
    const_iterator cbegin() {
        return const_iterator(*reinterpret_cast<Node*>(fake->next));
    }
    iterator end() {
        return iterator(*reinterpret_cast<Node*>(fake));
    }
    const_iterator end() const {
        return const_iterator(*reinterpret_cast<Node*>(fake));
    }
    const_iterator cend() {
        return const_iterator(fake);
    }
    reverse_iterator rbegin() {
        return reverse_iterator(*reinterpret_cast<Node*>(fake));
    }
    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(*reinterpret_cast<Node*>(fake));
    }
    const_reverse_iterator crbegin() {
        return const_reverse_iterator(fake);
    }
    reverse_iterator rend() {
        return reverse_iterator(*reinterpret_cast<Node*>(fake->next));
    }
    const_reverse_iterator crend() const {
        return const_reverse_iterator(*reinterpret_cast<Node*>(fake->next));
    }
    const_reverse_iterator crend() {
        return const_reverse_iterator(*reinterpret_cast<Node*>(fake->next));
    }
    template <bool IsConst>
    void insert(common_iterator<IsConst> it, const T& value) {
        ++size_;
        Node * node = AllocTraits::allocate(alloc, 1);
        AllocTraits::construct(alloc, node, value);
        Node * last = it.node->prev;
        Node * next = it.node;
        next->prev = node;
        last->next = node;
        node->prev = last;
        node->next = next;
    }
    template <bool IsConst>
    void insert(common_iterator<IsConst> it) {
        ++size_;
        Node * node = AllocTraits::allocate(alloc, 1);
        AllocTraits::construct(alloc, node);
        Node * last = it.node->prev;
        Node * next = it.node;
        next->prev = node;
        last->next = node;
        node->prev = last;
        node->next = next;
    }
    const typename std::allocator_traits<Allocator>::template rebind_alloc<Node> &get_allocator() {
        return alloc;
    }
    void erase(iterator it) {
        --size_;
        Node * node = it.node;
        Node * last = reinterpret_cast<Node*>(it.node->prev);
        Node * next = reinterpret_cast<Node*>(it.node->next);
        last->next = next;
        next->prev = last;
        AllocTraits::destroy(alloc, node);
        AllocTraits::deallocate(alloc, node, 1);
    }
    void erase(const_iterator it) {
        --size_;
        Node * node = it.node;
        Node * last = it.node->prev;
        Node * next = it.node->next;
        last->next = next;
        next->prev = last;
        AllocTraits::destroy(alloc, node);
        AllocTraits::deallocate(alloc, node, 1);
    }
    void push_front(const T& value) {
        insert( ++end(), value);
    }
    void push_back(const T& value) {
        insert( end(), value);
    }
    void push_back() {
        insert(end());
    }
    void pop_front() {
        erase(++end());
    }
    void pop_back() {
        erase(--end());
    }
    ~List() {
        std::vector<Node*> nodes;
        for (auto node = fake->next; node != fake; node = node->next) {
            nodes.push_back(node);
        }
        for (auto node : nodes) {
            AllocTraits::destroy(alloc, node);
            AllocTraits::deallocate(alloc, node, 1);
        }
    }
};
>
