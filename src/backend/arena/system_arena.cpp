#ifdef WITH_ARENA

#include "system_arena.hpp"
#include "exception.hpp"
#include "basic_types.hpp"
#include <sstream>

namespace bias {

    // Process-wide shared Arena system with a simple reference count. Camera
    // setup in BIAS happens on the main thread, so no locking is used here.
    static acSystem g_arenaSystem = nullptr;
    static int g_arenaSystemRefCount = 0;


    acSystem acquireArenaSystem()
    {
        if (g_arenaSystemRefCount == 0)
        {
            AC_ERROR err = acOpenSystem(&g_arenaSystem);
            if (err != AC_ERR_SUCCESS)
            {
                g_arenaSystem = nullptr;
                std::stringstream ssError;
                ssError << __PRETTY_FUNCTION__;
                ssError << ": unable to open Arena system, error=" << err;
                throw RuntimeError(ERROR_ARENA_OPEN_SYSTEM, ssError.str());
            }
        }
        g_arenaSystemRefCount++;
        return g_arenaSystem;
    }


    void releaseArenaSystem()
    {
        if (g_arenaSystemRefCount > 0)
        {
            g_arenaSystemRefCount--;
            if (g_arenaSystemRefCount == 0 && g_arenaSystem != nullptr)
            {
                AC_ERROR err = acCloseSystem(g_arenaSystem);
                g_arenaSystem = nullptr;
                if (err != AC_ERR_SUCCESS)
                {
                    std::stringstream ssError;
                    ssError << __PRETTY_FUNCTION__;
                    ssError << ": unable to close Arena system, error=" << err;
                    throw RuntimeError(ERROR_ARENA_CLOSE_SYSTEM, ssError.str());
                }
            }
        }
    }

}

#endif // #ifdef WITH_ARENA
