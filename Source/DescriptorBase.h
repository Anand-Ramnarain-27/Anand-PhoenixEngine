#pragma once

class ModuleRTDescriptors;
class ModuleDSDescriptors;

template<typename Derived, typename ModuleType>
class DescriptorBase
{
protected:
    UINT handle = 0;
    UINT* refCount = nullptr;

public:
    DescriptorBase() = default;
    DescriptorBase(UINT handle, UINT* refCount) : handle(handle), refCount(refCount) {
        if (refCount) ++(*refCount);
    }

    DescriptorBase(const DescriptorBase& other) : handle(other.handle), refCount(other.refCount) {
        if (refCount) ++(*refCount);
    }

    DescriptorBase(DescriptorBase&& other) noexcept : handle(other.handle), refCount(other.refCount)
    {
        other.handle = 0;
        other.refCount = nullptr;
    }

    ~DescriptorBase() { release(); }

    DescriptorBase& operator=(const DescriptorBase& other)
    {
        if (this != &other)
        {
            release();
            handle = other.handle;
            refCount = other.refCount;
            if (refCount) ++(*refCount);
        }
        return *this;
    }

    DescriptorBase& operator=(DescriptorBase&& other) noexcept
    {
        if (this != &other)
        {
            release();
            handle = other.handle;
            refCount = other.refCount;
            other.handle = 0;
            other.refCount = nullptr;
        }
        return *this;
    }

    explicit operator bool() const
    {
        return handle != 0 && Derived::getModule()->isValid(handle);
    }

    void reset() { release(); }

    D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle() const
    {
        if (handle == 0) return { 0 };
        return Derived::getModule()->getCPUHandle(handle);
    }

    UINT getHandle() const { return handle; }

private:
    void release()
    {
        if (refCount && --(*refCount) == 0 && handle != 0)
        {
            Derived::getModule()->release(handle);
            handle = 0;
            refCount = nullptr;
        }
    }

};