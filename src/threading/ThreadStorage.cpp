#include "ThreadStorage.h"

#include <memory>

static thread_local std::unique_ptr<ThreadStorage::Storage> s_threadStorage;

ThreadStorage::Storage *ThreadStorage::createNewThreadStorage()
{
    s_threadStorage = std::make_unique<Storage>();
    return s_threadStorage.get();
}

ThreadStorage::Storage *ThreadStorage::useDefaultThreadStorage()
{
    if (!s_threadStorage)
    {
        s_threadStorage = std::make_unique<Storage>();
    }
    return s_threadStorage.get();
}

void ThreadStorage::releaseThreadStorage() { s_threadStorage.reset(); }

ThreadStorage::Storage *ThreadStorage::getStorage() { return s_threadStorage.get(); }

void ThreadStorage::setValue(const std::string &key, uintptr_t value)
{
    Storage *storage     = useDefaultThreadStorage();
    storage->values[key] = value;
}

uintptr_t ThreadStorage::getValue(const std::string &key, uintptr_t defaultValue)
{
    Storage *storage = useDefaultThreadStorage();
    auto it          = storage->values.find(key);
    if (it == storage->values.end())
    {
        return defaultValue;
    }

    return it->second;
}
