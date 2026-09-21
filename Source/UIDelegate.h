#pragma once
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

// A list of callbacks taking `Args`. Invocation runs a snapshot, so a listener may add or remove listeners, or
// even destroy the widget that owns this list, while it is being called.
template<typename... Args>
class UIEvent {
public:
    using Callback = std::function<void(Args...)>;

    uint32_t add(Callback cb){
        m_entries.push_back({ m_nextId, std::move(cb) });
        return m_nextId++;
    }

    bool remove(uint32_t id){
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it){
            if (it->id != id) continue;
            m_entries.erase(it);
            return true;
        }
        return false;
    }

    void clear(){ m_entries.clear(); }
    bool empty() const { return m_entries.empty(); }

    void invoke(Args... args) const {
        const std::vector<Entry> snapshot = m_entries;
        for (const Entry& e : snapshot)
            if (e.fn) e.fn(args...);
    }

private:
    struct Entry {
        uint32_t id;
        Callback fn;
    };

    std::vector<Entry> m_entries;
    uint32_t m_nextId = 1;
};

using UIDelegate = UIEvent<>;
