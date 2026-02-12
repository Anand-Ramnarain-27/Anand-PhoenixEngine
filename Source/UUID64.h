#pragma once

#include <cstdint>
#include <string>

// Renamed from UUID to UUID64 to avoid Windows _GUID/UUID conflict
class UUID64
{
public:
    UUID64();
    explicit UUID64(uint64_t value);

    // Generate a new random UUID64
    static UUID64 Generate();

    // Get the underlying value
    uint64_t getValue() const { return m_value; }

    // Check if this is a valid UUID64 (non-zero)
    bool isValid() const { return m_value != 0; }

    // String representation for debugging
    std::string toString() const;

    // Comparison operators
    bool operator==(const UUID64& other) const { return m_value == other.m_value; }
    bool operator!=(const UUID64& other) const { return m_value != other.m_value; }
    bool operator<(const UUID64& other) const { return m_value < other.m_value; }

    // Invalid/null UUID64
    static const UUID64 Invalid;

private:
    uint64_t m_value;
};

// Hash function for using UUID64 in unordered containers
namespace std
{
    template<>
    struct hash<UUID64>
    {
        size_t operator()(const UUID64& uuid) const
        {
            return hash<uint64_t>()(uuid.getValue());
        }
    };
}