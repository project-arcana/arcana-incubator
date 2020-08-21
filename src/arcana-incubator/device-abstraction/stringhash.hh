#pragma once

#include <cstddef>
#include <cstdint>

namespace inc::da
{
// ref https://xueyouchao.github.io/2016/11/16/CompileTimeString/
template <size_t N>
constexpr uint64_t stringhash(const char (&str)[N], size_t prime = 31, size_t length = N - 1)
{
    return (length <= 1) ? str[0] : (prime * stringhash(str, prime, length - 1) + str[length - 1]);
}

inline uint64_t stringhash_runtime(char const* str, size_t prime = 31)
{
    if (str == nullptr)
        return 0;
    uint64_t hash = *str;
    for (; *(str + 1) != 0; str++)
    {
        hash = prime * hash + *(str + 1);
    }
    return hash;
}
}
