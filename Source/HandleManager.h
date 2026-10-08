#pragma once
// Fixed-capacity pool of generational handles, used by the descriptor heaps.

#include <array>
#include <cassert>

/// Hands out 32-bit handles to `Size` slots: 24 bits of slot index plus an 8-bit generation, so a handle to a
/// freed (and possibly reused) slot is detected as stale. Handle 0 is never issued and always invalid. Free slots
/// form a linked list threaded through the slots themselves.
template<size_t Size>
class HandleManager {
    static_assert(Size > 0, "HandleManager size must be positive");
    static_assert(Size < (1 << 24), "HandleManager size too large for 24-bit index");

    struct Data {
        UINT index : 24;   // a live slot's own index; a free slot's link to the next free one
        UINT number : 8;   // generation

        Data() : index(Size), number(0) {}
        explicit Data(UINT handle){ *reinterpret_cast<UINT*>(this) = handle; }
        operator UINT() const { return *reinterpret_cast<const UINT*>(this); }
    };

    std::array<Data, Size> data;
    UINT firstFree = 0;
    UINT genNumber = 0;

public:
    HandleManager(){
        UINT nextIndex = 0;
        for (Data& item : data){
            item.index = ++nextIndex;
            item.number = 0;
        }
    }

    /// Returns 0 (and asserts) when every slot is taken.
    UINT allocHandle(){
        assert(firstFree < Size && "Out of handles");
        if (firstFree >= Size) return 0;

        UINT index = firstFree;
        Data& item = data[index];
        firstFree = item.index;

        genNumber = (genNumber + 1) % (1 << 8);
        // Slot 0 at generation 0 would encode as handle 0, which means "no handle".
        if (index == 0 && genNumber == 0) ++genNumber;

        item.index = index;
        item.number = genNumber;
        return static_cast<UINT>(item);
    }

    void freeHandle(UINT handle){
        assert(validHandle(handle) && "Invalid handle");

        UINT index = Data(handle).index;
        Data& freedItem = data[index];
        freedItem.index = firstFree;
        firstFree = index;
    }

    UINT indexFromHandle(UINT handle) const{
        assert(validHandle(handle) && "Invalid handle");
        return Data(handle).index;
    }

    /// True if `handle` refers to a live slot of the current generation.
    bool validHandle(UINT handle) const{
        if (handle == 0) return false;

        Data item(handle);
        UINT index = item.index;
        UINT number = item.number;
        return index < Size && data[index].index == index && data[index].number == number;
    }

    size_t getSize() const { return Size; }

    /// Walks the free list: O(free slots).
    size_t getFreeCount() const{
        size_t freeCount = 0;
        size_t index = firstFree;
        while (index < Size){
            ++freeCount;
            index = data[index].index;
        }
        return freeCount;
    }
};
