#include <Foundation/Foundation.h>

namespace genki {

//======================================================================================================================
// An RAII approach to manage retain/release consistency on CoreFoundation objects
template<typename T>
struct Retained
{
    Retained() = default;

    Retained(std::nullptr_t) {}

    explicit Retained(T t)
    {
        if (t != nil)
            instance = static_cast<T>(CFRetain(t));
    }

    ~Retained()
    {
        if (instance != nil) CFRelease(instance);
    }

    // Copy constructor - retain count increased
    Retained(const Retained& r) noexcept : Retained(r.instance) {}

    // Move constructor - retain count not increased
    Retained(Retained&& r) noexcept
    {
        instance   = r.instance;
        r.instance = nil;
    }

    // Copy assignment - retain count increased
    Retained& operator=(const Retained& r)
    {
        if (instance = r.instance; instance != nil)
            CFRetain(instance);

        return *this;
    }

    // Move assignment - retain count not increased
    Retained& operator=(Retained&& r) noexcept
    {
        std::swap(instance, r.instance);
        return *this;
    }

    // Implicit conversion to the underlying type. Removes cumbersome get() method calls when interacting with obj-c APIs
    operator T() const { return instance; }

    [[nodiscard]] T get() const { return instance; }

    static Retained makeWithoutRetaining(T t)
    {
        Retained r;
        r.instance = t;

        return r;
    }

private:
    T instance = nil;
};

} // namespace genki