#include <cstdio>
#include <stdexcept>

#include "core/Logger.h"
#include "core/Minecraft.h"

int main() {
    Logger::init();
    Logger::logInfo("Logger initialized");

    try {
        Minecraft::getInstance()->start();
    } catch (const std::exception &exception) {
        Logger::logError("Caught an unexpected exception: %s", exception.what());
        return -1;
    }

    Logger::shutdown();

    return 0;
}
