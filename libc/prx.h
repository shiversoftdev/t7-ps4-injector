#pragma once
#include <vector>

#define WORKING_DIR "/data/t7/"
#define GSC_CONF WORKING_DIR "inject.conf"

struct GSCEntry
{
	char Target[256] = { 0 };
	char* Buffer;
	bool IsValid = false;
};

class LibGSC
{
public:
	static void* getGameBase();
	static int load_config_file();
	static uint64_t MODULE_BASE;
	static std::vector<GSCEntry*> ScriptFiles;
};

#define OFFSET(x) (uint64_t)((uint64_t)x + LibGSC::getGameBase())