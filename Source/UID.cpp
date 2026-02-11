#include "Globals.h"
#include "UID.h"
#include <random>

UID GenerateUID()
{
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<UID> dist;

    return dist(gen);
}
