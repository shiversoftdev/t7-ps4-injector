#include "t8.h"
#include "rpc.h"

T8VTABLE T8::__vtable;
bool T8::vtable_initialized = false;

T8VTABLE T8::vtable()
{
#ifdef IS_T8
    if (!vtable_initialized)
    {
        __vtable.BaseAddress = RPC::get_game_base();
        vtable_initialized = true;
    }
#endif
    return __vtable;
}
