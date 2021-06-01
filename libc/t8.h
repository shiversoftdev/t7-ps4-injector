#pragma once
#include "ppc.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <kernel.h>
#include <libsysmodule.h>

// A9F770 = GScr_AllocString
// 1528710 = crypt
// DB6F40 = DB_FindXAssetHeader 0x30 => gsc
// DB4670 = DB_CreateDefaultEntry
// DB4810 = DB_AllocXAssetEntry
// 4A501C0 + (0x30 * 0x20)

#define DUMP_PATH "/data/t8/dump/"

struct T8XAssetPool
{
	void* pool;
	uint32_t itemSize;
	int32_t itemCount;
	bool isSingleton;
	int32_t itemAllocCount;
	void* freeHead;
};

struct T8ScriptParseTree
{
	uint64_t name_hash;     //00
	uint64_t padding;       //08
	void* buffer_pointer;//10
	uint32_t buffer_length;  //18
	uint32_t unk;            //1C
};

typedef struct T8VTABLE {
	void* BaseAddress;
	void(*Cbuf_AddText)(int, const char*);
	char*(*SL_GetStringOfSize)(char*);

} T8VTABLE;

class T8
{
public:
	static T8VTABLE vtable();

private:
	static T8VTABLE __vtable;
	static bool vtable_initialized;
};