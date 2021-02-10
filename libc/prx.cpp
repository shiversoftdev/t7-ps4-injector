#include <kernel.h>
#include "prx.h"
#include <string.h>

uint64_t LibGSC::MODULE_BASE = 0;
std::vector<GSCEntry*> LibGSC::ScriptFiles;

int module_start(size_t args, const void* argp)
{
    sceKernelLoadStartModule("/app0/sce_module/default_libc.prx", 0, 0, 0, 0, 0);
    if (LibGSC::load_config_file()) return 0; // couldnt load config

    //todo: start a thread to inject when player holds buttons

    return 0;
}

void* LibGSC::getGameBase()
{
    if (LibGSC::MODULE_BASE) return (void*)LibGSC::MODULE_BASE;
    SceKernelVirtualQueryInfo info;
    void* addr = 0;
    while (sceKernelVirtualQuery(addr, SCE_KERNEL_VQ_FIND_NEXT, &info, sizeof(SceKernelVirtualQueryInfo)) >= 0)
    {
        addr = (void*)((uint64_t)info.end + 1);
        if (info.protection == 5 && strcmp(info.name, "executable") == 0)
        {
            return (void*)(LibGSC::MODULE_BASE = (uint64_t)info.start);
        }
    }
    return 0;
}

int LibGSC::load_config_file()
{
    int fd;
    SceKernelStat sb;
    if ((fd = sceKernelOpen(GSC_CONF, SCE_KERNEL_O_RDONLY, SCE_KERNEL_S_IRWU)) < 0) return -1;
    if (sceKernelFstat(fd, &sb)) return -2;

    char* buffer = new char[sb.st_size];
    if (sceKernelRead(fd, buffer, sb.st_size) != sb.st_size) return -3;

    // parse the config
    // \r\n deliminated, with ';' as terminator for a line. Whitespace is parsed as part of the token, so eliminate it.
    // file; // signifies new file
    // file.target=scripts/shared/clientids_shared.gsc; // file target (inject point)
    // file.data=/data/t7/clientids.gsc; // data to load

    char* key;
    char* value;
    char* line;
    char* remaining_lines = buffer;
    GSCEntry* entry = NULL;
    while ((line = strtok_r(remaining_lines, "\r\n", &remaining_lines)))
    {
        value = strtok_r(line, ";", &line); // assignment
        key = strtok_r(value, "=", &value);

        if (!strcmp(key, "file"))
        {
            entry = new GSCEntry();
            LibGSC::ScriptFiles.push_back(entry);
            continue;
        }

        if (!entry) continue;

        if (!strcmp(key, "file.target"))
        {
            memcpy(entry->Target, value, std::min(sizeof(entry->Target) - 1, strlen(value)));
            continue;
        }

        if (!strcmp(key, "file.data"))
        {
            if ((fd = sceKernelOpen(value, SCE_KERNEL_O_RDONLY, SCE_KERNEL_S_IRWU)) < 0) continue;
            if (sceKernelFstat(fd, &sb)) continue;
            entry->Buffer = new char[sb.st_size];
            entry->IsValid = sceKernelRead(fd, entry->Buffer, sb.st_size) == sb.st_size;
        }
    }

    free(buffer);
    sceKernelClose(fd);
    return 0;
}