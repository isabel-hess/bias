#ifdef WITH_ARENA
#ifndef BIAS_SYSTEM_ARENA_HPP
#define BIAS_SYSTEM_ARENA_HPP

#include "ArenaCApi.h"

namespace bias {

    // Reference-counted access to a single process-wide Arena system instance.
    //
    // acOpenSystem does not permit multiple concurrent open systems (a second
    // call returns AC_ERR_RESOURCE_IN_USE), so the CameraFinder and every
    // CameraDevice_arena must share one system. This mirrors the singleton
    // semantics of Spinnaker's spinSystemGetInstance. The shared system is
    // opened on the first acquire and closed when the last holder releases it.
    acSystem acquireArenaSystem();
    void releaseArenaSystem();

}

#endif // #ifndef BIAS_SYSTEM_ARENA_HPP
#endif // #ifdef WITH_ARENA
