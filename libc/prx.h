#pragma once
#include <vector>

#define WORKING_DIR "/data/t7/"
#define GSC_CONF WORKING_DIR "inject.conf"

#define GSC_SERIAL WORKING_DIR "serial"

struct T7GSCEntry
{
	char Target[256] = { 0 };
	char* Buffer = NULL;
	bool IsValid = false;
	char* OriginalBuffer = NULL;
	char** OriginalBufferPtr = NULL;
};

struct T7ScriptParseTreeEntry
{
	uint64_t NameOffset;
	uint64_t BufferSize;
	char* Buffer;
};

struct T7ScriptParsetreeGlob
{
	uint64_t llpSPT;
	uint64_t garbage;
	uint32_t garbage2;
	uint32_t NumEntries;
};

class T7LibGSC
{
public:
	static int read_serial();
	static void* getGameBase();
	static void clear_old_config();
	static int load_config_file();
	static void* injector_think(void*);
	static void do_inject();
	static void write_script(T7ScriptParseTreeEntry* target, T7GSCEntry* script);
	static bool can_inject();
	static uint64_t MODULE_BASE;
	static uint64_t MODULE_SIZE;
	static uint64_t SPT_GLOB;
	static std::vector<T7GSCEntry*> ScriptFiles;
};

#define OFFSET(x) ((uint64_t)x + (uint64_t)T7LibGSC::getGameBase())
#define __MS(x) (x * 1000)
#define __S(x) (__MS(x) * 1000)

#define THINK_WAIT_MS 10000
#define THINK_WAIT_ACTUAL __MS(THINK_WAIT_MS)

#define PTR_s_runningUILevel OFFSET(0xC0A160D)
#define PTR_SPTGlob OFFSET(0x21A4AB0)

#define T7_Script_Pointer 0x20003201E0
#define T7_Heap_Start 0x2000070000
#define T7_Head_Size 0x17b0000