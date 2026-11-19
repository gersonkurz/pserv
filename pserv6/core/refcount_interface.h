/// @file refcount_interface.h
/// @brief Reference counting infrastructure for managed object lifetimes.
///
/// Provides IRefCounted interface and RefCountImpl base class for automatic
/// memory management of DataObject instances in containers.
#pragma once

/// @def USE_REFCOUNT_DEBUGGING
/// @brief Enable to add file/line tracking to Retain/Release calls for debugging.
#undef USE_REFCOUNT_DEBUGGING
#ifdef USE_REFCOUNT_DEBUGGING
#define REFCOUNT_DEBUG_ARGS __FILE__, __LINE__
#define REFCOUNT_DEBUG_SPEC const char *file, int line
#else
#define REFCOUNT_DEBUG_ARGS
#define REFCOUNT_DEBUG_SPEC
#endif

namespace pserv
{
    /// @brief Interface for reference-counted objects.
    ///
    /// Objects implementing IRefCounted can be shared across multiple owners
    /// with automatic cleanup when the last reference is released.
    class IRefCounted
    {
    public:
        IRefCounted() = default;
        virtual ~IRefCounted() = default;

        // Non-copyable but movable
        IRefCounted(const IRefCounted &) = delete;
        IRefCounted &operator=(const IRefCounted &) = delete;
        IRefCounted(IRefCounted &&) = default;
        IRefCounted &operator=(IRefCounted &&) = default;

        /// @brief Increment the reference count.
        /// This method should be called when a new reference to the object is created.
        /// It is expected to be thread-safe.
        virtual void Retain(REFCOUNT_DEBUG_SPEC) const = 0;

        /// @brief Decrement the reference count.
        /// This method should be called when a reference to the object is no longer needed.
        /// If the reference count reaches zero, the object should be deleted.
        /// It is expected to be thread-safe.
        /// @note After calling this method, the object should not be used anymore.
        /// @note If the object is deleted, it should not call any methods on itself after this point.
        virtual void Release(REFCOUNT_DEBUG_SPEC) const = 0;

        /// @brief Clear the internal state of the object.
        /// This method is intended to reset the internal state of the object without deleting it.
        /// It can be used to free resources or reset data. It does NOT touch the reference count.
        virtual void Clear() = 0;

        /// @brief Get a stable identifier for the object.
        /// This is used so that we can update objects previously created instead of creating them anew.
        virtual std::string GetStableID() const = 0;
    };

    /// @brief Mixin template that adds reference counting to any base class.
    /// @tparam T Base class that will gain reference counting capability.
    ///
    /// Use this when you need to add reference counting to an existing class
    /// hierarchy without modifying the base class.
    template <typename T> class RefCounted : public T
    {
    public:
        explicit RefCounted<T>()
            : m_refCount{1}
        {
        }

        void Retain(REFCOUNT_DEBUG_SPEC) const override final
        {
            ++m_refCount;
        }
        void Release(REFCOUNT_DEBUG_SPEC) const override final
        {
            if (--m_refCount == 0)
                delete this;
        }
        void Clear() override
        {
        }

    private:
        mutable std::atomic<int> m_refCount; ///< Thread-safe reference counter.
    };

    /// @brief Concrete implementation of IRefCounted for direct inheritance.
    ///
    /// Use this as a base class for objects that need reference counting.
    /// The object starts with a reference count of 1 and is deleted when
    /// the count reaches 0.
    class RefCountImpl : public IRefCounted
    {
    public:
        RefCountImpl()
            : m_refCount{1}
        {
        }

        void Retain(REFCOUNT_DEBUG_SPEC) const override final
        {
            ++m_refCount;
        }

        void Release(REFCOUNT_DEBUG_SPEC) const override final
        {
            if (--m_refCount == 0)
                delete this;
        }

        void Clear() override
        {
        }

    private:
        mutable std::atomic<int> m_refCount; ///< Thread-safe reference counter.
    };
} // namespace pserv
