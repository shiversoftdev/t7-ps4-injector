#include "t7.h"
#include "rpc.h"

T7VTABLE T7::__vtable;
bool T7::vtable_initialized = false;


T7VTABLE T7::vtable()
{
#ifdef IS_T7
    if (!vtable_initialized)
    {
        __vtable.BaseAddress = RPC::get_game_base();
#if USE_STATIC_OFFSETS
        __vtable.Cbuf_AddText = (void(*)(int, const char*)) T7OFF_CBUFF;
        __vtable.Dvar_SetFromStringByNameFromSource = (void(*)(const char*, const char*, int, int, bool)) T7OFF_SetDvarBySource;
#endif
        vtable_initialized = true;

    }
#endif
    return __vtable;
}
