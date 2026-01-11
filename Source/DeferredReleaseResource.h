#pragma once
#include "Globals.h"

template<typename T>
class DeferredReleaseResource
{
public:
    DeferredReleaseResource() = default;

    DeferredReleaseResource(T* resource) : m_resource(resource) {}

    ~DeferredReleaseResource()
    {
        reset();
    }

    DeferredReleaseResource(DeferredReleaseResource&& other) noexcept
        : m_resource(other.m_resource)
    {
        other.m_resource = nullptr;
    }

    DeferredReleaseResource& operator=(DeferredReleaseResource&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            m_resource = other.m_resource;
            other.m_resource = nullptr;
        }
        return *this;
    }

    DeferredReleaseResource(const DeferredReleaseResource&) = delete;
    DeferredReleaseResource& operator=(const DeferredReleaseResource&) = delete;

    void reset(T* newResource = nullptr)
    {
        if (m_resource)
        {
            if (auto* app = Application::GetInstance())
            {
                if (auto* d3d12 = app->getD3D12())
                {
                    d3d12->deferRelease(m_resource);
                }
            }
        }
        m_resource = newResource;
    }

    T* get() const { return m_resource; }
    T* operator->() const { return m_resource; }
    T** address() { return &m_resource; }

    bool valid() const { return m_resource != nullptr; }

private:
    T* m_resource = nullptr;
};