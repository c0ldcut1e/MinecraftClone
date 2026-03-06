#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

class ThreadStorage
{
public:
    struct Storage
    {
        std::unordered_map<std::string, uintptr_t> values;
    };

    static Storage *createNewThreadStorage();
    static Storage *useDefaultThreadStorage();
    static void releaseThreadStorage();

    static Storage *getStorage();

    static void setValue(const std::string &key, uintptr_t value);
    static uintptr_t getValue(const std::string &key, uintptr_t defaultValue = 0);
};
