#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <type_traits>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept 
        : buffer_(std::exchange(other.buffer_, nullptr))
        , capacity_(std::exchange(other.capacity_, 0)) {
    }

    RawMemory& operator=(RawMemory&& other) noexcept {
        if (this != &other) {
            Deallocate(buffer_);
            buffer_ = std::exchange(other.buffer_, nullptr);
            capacity_ = std::exchange(other.capacity_, 0);
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() noexcept = default;
    
    explicit Vector(size_t size) 
        : data_(size)
        , size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }
    
    Vector(const Vector& other) 
        : data_(other.size_)
        , size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }
    
    Vector(Vector&& other) noexcept 
        : data_(std::move(other.data_))
        , size_(std::exchange(other.size_, 0)) {
    }
    
    Vector& operator=(const Vector& other) {
        if (this == &other) {
            return *this;
        }
        
        if (other.size_ <= data_.Capacity()) {
            size_t i = 0;
            for (; i < other.size_ && i < size_; ++i) {
                data_[i] = other.data_[i];
            }
            for (; i < other.size_; ++i) {
                new (data_ + i) T(other.data_[i]);
            }
            for (; i < size_; ++i) {
                data_[i].~T();
            }
            size_ = other.size_;
        } else {
            Vector tmp(other);
            Swap(tmp);
        }
        return *this;
    }
    
    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }
    
    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    
    const_iterator cbegin() const noexcept {
        return begin();
    }
    
    const_iterator cend() const noexcept {
        return end();
    }
    
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(size_t capacity) {
        if (capacity <= data_.Capacity()) {
            return;
        }
        
        RawMemory<T> new_data(capacity);
        size_t i = 0;
        
        try {
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                for (; i < size_; ++i) {
                    new (new_data + i) T(std::move(data_[i]));
                }
            } else {
                for (; i < size_; ++i) {
                    new (new_data + i) T(data_[i]);
                }
            }
        } catch (...) {
            for (size_t j = 0; j < i; ++j) {
                new_data[j].~T();
            }
            throw;
        }
        
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Resize(size_t new_size) {
        if (new_size <= size_) {
            for (size_t i = new_size; i < size_; ++i) {
                data_[i].~T();
            }
            size_ = new_size;
        } else {
            if (new_size <= data_.Capacity()) {
                for (size_t i = size_; i < new_size; ++i) {
                    new (data_ + i) T();
                }
                size_ = new_size;
            } else {
                size_t new_capacity = std::max(new_size, data_.Capacity() * 2);
                RawMemory<T> new_data(new_capacity);
                size_t i = 0;
                
                try {
                    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                        for (; i < size_; ++i) {
                            new (new_data + i) T(std::move(data_[i]));
                        }
                    } else {
                        for (; i < size_; ++i) {
                            new (new_data + i) T(data_[i]);
                        }
                    }
                    for (; i < new_size; ++i) {
                        new (new_data + i) T();
                    }
                } catch (...) {
                    for (size_t j = 0; j < i; ++j) {
                        new_data[j].~T();
                    }
                    throw;
                }
                
                std::destroy_n(data_.GetAddress(), size_);
                data_.Swap(new_data);
                size_ = new_size;
            }
        }
    }

    // PushBack через EmplaceBack
    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack() noexcept {
        assert(size_ > 0);
        --size_;
        data_[size_].~T();
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ < data_.Capacity()) {
            new (data_ + size_) T(std::forward<Args>(args)...);
            ++size_;
            return data_[size_ - 1];
        } else {
            size_t new_capacity = data_.Capacity() == 0 ? 1 : data_.Capacity() * 2;
            RawMemory<T> new_data(new_capacity);

            new (new_data + size_) T(std::forward<Args>(args)...);
            
            size_t i = 0;
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    for (; i < size_; ++i) {
                        new (new_data + i) T(std::move(data_[i]));
                    }
                } else {
                    for (; i < size_; ++i) {
                        new (new_data + i) T(data_[i]);
                    }
                }
            } catch (...) {
                for (size_t j = 0; j < i; ++j) {
                    new_data[j].~T();
                }
                new_data[size_].~T();
                throw;
            }

            std::destroy_n(data_.GetAddress(), size_);

            data_.Swap(new_data);
            ++size_;
            return data_[size_ - 1];
        }
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
    assert(pos >= begin() && pos <= end());
    
    size_t index = pos - begin();
    
    template <typename... Args>
iterator Emplace(const_iterator pos, Args&&... args) {
    assert(pos >= begin() && pos <= end());
    
    size_t index = pos - begin();
    
    if (size_ < data_.Capacity()) {
        if (index == size_) {
            new (data_ + size_) T(std::forward<Args>(args)...);
            ++size_;
        } else {
            T tmp(std::forward<Args>(args)...);
            new (data_ + size_) T(std::move(data_[size_ - 1]));
            
            for (size_t i = size_ - 1; i > index; --i) {
                data_[i] = std::move(data_[i - 1]);
            }
            T* insert_pos = data_ + index;
            *insert_pos = std::forward<T>(tmp);
            ++size_;
        }
    }  else {
            size_t new_capacity = data_.Capacity() == 0 ? 1 : data_.Capacity() * 2;
            RawMemory<T> new_data(new_capacity);

            new (new_data + index) T(std::forward<Args>(args)...);

            size_t before_count = 0;
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    for (; before_count < index; ++before_count) {
                        new (new_data + before_count) T(std::move(data_[before_count]));
                    }
                } else {
                    for (; before_count < index; ++before_count) {
                        new (new_data + before_count) T(data_[before_count]);
                    }
                }
            } catch (...) {
                for (size_t j = 0; j < before_count; ++j) {
                    new_data[j].~T();
                }
                new_data[index].~T();
                throw;
            }

            size_t after_count = 0;
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    for (; after_count < size_ - index; ++after_count) {
                        new (new_data + index + 1 + after_count) T(std::move(data_[index + after_count]));
                    }
                } else {
                    for (; after_count < size_ - index; ++after_count) {
                        new (new_data + index + 1 + after_count) T(data_[index + after_count]);
                    }
                }
            } catch (...) {
                for (size_t j = 0; j < after_count; ++j) {
                    new_data[index + 1 + j].~T();
                }
                for (size_t j = 0; j < before_count; ++j) {
                    new_data[j].~T();
                }
                new_data[index].~T();
                throw;
            }

            std::destroy_n(data_.GetAddress(), size_);

            data_.Swap(new_data);
            ++size_;
        }
        return begin() + index;
    }

    iterator Erase(const_iterator pos) {
        size_t index = pos - begin();
        assert(index < size_);
        
        for (size_t i = index; i < size_ - 1; ++i) {
            data_[i] = std::move(data_[i + 1]);
        }
        
        data_[size_ - 1].~T();
        --size_;
        
        return begin() + index;
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};