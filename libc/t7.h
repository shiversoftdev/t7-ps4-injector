#pragma once
#include "ppc.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <kernel.h>
#include <libsysmodule.h>

#define T7VBASE (uint64_t)__vtable.BaseAddress
#define T7OFF(x) (T7VBASE + x)

#ifdef USE_STATIC_OFFSETS
#define T7OFF_CBUFF T7OFF(0xEE44C0)
#define T7OFF_SetDvarBySource T7OFF(0xFBC5A0)
#endif

typedef struct T7VTABLE {
	void* BaseAddress;
	void(*Cbuf_AddText)(int, const char*);
	void(*Dvar_SetFromStringByNameFromSource)(const char* dvarName, const char* string, int always_zero, int flags, bool createIfMissing);

} T7VTABLE;

class T7
{
public:
	static T7VTABLE vtable();

private:
	static T7VTABLE __vtable;
	static bool vtable_initialized;
};