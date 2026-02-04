#pragma once

#include <array>
#include <cassert>

template<size_t Size>
class HandleManager
{
    static_assert(Size > 0, "HandleManager size must be positive");
    static_assert(Size < (1 << 24), "HandleManager size too large for 24-bit index");

    struct Data
    {
        UINT index : 24;  
        UINT number : 8;  

        Data() : index(Size), number(0) { ; }
        explicit Data(UINT handle) {
            *reinterpret_cast<UINT*>(this) = handle;
        }
        operator UINT() const {
            return *reinterpret_cast<const UINT*>(this);
        }
    };

    std::array<Data, Size> data;
    UINT firstFree = 0;
    UINT genNumber = 0;

public:
    HandleManager()
    {
        UINT nextIndex = 0;
        for (Data& item : data)
        {
            item.index = ++nextIndex; 
            item.number = 0;
        }
    }

    UINT allocHandle()
    {
        assert(firstFree < Size && "Out of handles");

        if (firstFree < Size)
        {
            UINT index = firstFree;  
            Data& item = data[index];

            firstFree = item.index;

            genNumber = (genNumber + 1) % (1 << 8);

            if (index == 0 && genNumber == 0)
            {
                ++genNumber;
            }

            item.index = index;    
            item.number = genNumber;

            return static_cast<UINT>(item);
        }

        return 0;  
    }

    void freeHandle(UINT handle)
    {
        assert(validHandle(handle) && "Invalid handle");

        Data item(handle);
        UINT index = item.index;

        Data& freedItem = data[index];
        freedItem.index = firstFree;
        firstFree = index;
    }

    UINT indexFromHandle(UINT handle) const
    {
        assert(validHandle(handle) && "Invalid handle");
        return Data(handle).index;
    }

    bool validHandle(UINT handle) const
    {
        if (handle == 0) return false;

        Data item(handle);
        UINT index = item.index;
        UINT number = item.number;

        return index < Size &&
            data[index].index == index &&
            data[index].number == number;
    }

    size_t getSize() const { return Size; }

    size_t getFreeCount() const
    {
        size_t freeCount = 0;
        size_t index = firstFree;

        while (index < Size)
        {
            ++freeCount;
            index = data[index].index;
        }

        return freeCount;
    }

private:
    bool valid(UINT index, UINT genNumber) const
    {
        return index < Size &&
            data[index].index == index &&
            data[index].number == genNumber;
    }
};