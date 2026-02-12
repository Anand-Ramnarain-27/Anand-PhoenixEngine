#include "Globals.h"
#include "UUID64.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>

// Static invalid UUID64
const UUID64 UUID64::Invalid = UUID64(0);

UUID64::UUID64()
    : m_value(0)
{
}

UUID64::UUID64(uint64_t value)
    : m_value(value)
{
}

UUID64 UUID64::Generate()
{
    // Thread-safe static random generator (C++11)
    static std::random_device rd;
    static std::mt19937_64 gen(rd());

    uint64_t value = gen();

    // Ensure we never generate 0 (reserved for invalid)
    while (value == 0) {
        value = gen();
    }

    return UUID64(value);
}

std::string UUID64::toString() const
{
    std::stringstream ss;
    ss << "0x" << std::hex << std::uppercase
        << std::setw(16) << std::setfill('0') << m_value;
    return ss.str();
}