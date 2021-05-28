#include <kernel.h>
#include "prx.h"
#include <string.h>
#include <thread>
#include "Controller.h"
#include "rpc.h"

uint64_t T7LibGSC::MODULE_BASE = 0;
uint64_t T7LibGSC::MODULE_SIZE = 0;
uint64_t T7LibGSC::SPT_GLOB = 0;
std::vector<T7GSCEntry*> T7LibGSC::ScriptFiles;
ScePthread main_thread;
ScePthreadAttr attr;

// We need to provide an export to force the expected stub library to be generated
__declspec (dllexport) void dummy()
{
}

int module_start(size_t args, const void* argp)
{
    sceKernelLoadStartModule("/app0/sce_module/_libSceNpToolkit.prx", 0, 0, 0, 0, 0);
    printf("[SMC] Initializing...\n");

    int libsysutil_id = sceKernelLoadStartModule("libSceSysUtil.sprx", 0, 0, 0, 0, 0);
    printf("[SMC] Loaded System Utility...\n");

    sceKernelDlsym(libsysutil_id, "sceSysUtilSendNpDebugNotificationRequest", (void**)&RPC::sceSysUtilSendNpDebugNotificationRequest);
    printf("[SMC] libsysutil_id: 0x%08x sceSysUtilSendNpDebugNotificationRequest: %p\n", libsysutil_id, RPC::sceSysUtilSendNpDebugNotificationRequest);

    scePthreadCreate(&main_thread, &attr, RPC::rpc_think, NULL, "smc");
    return 0;
}

void dump_mem_to_console()
{
    SceKernelVirtualQueryInfo info;
    void* addr = 0;
    while (sceKernelVirtualQuery(addr, SCE_KERNEL_VQ_FIND_NEXT, &info, sizeof(SceKernelVirtualQueryInfo)) >= 0)
    {
        addr = (void*)((uint64_t)info.end + 1);
        auto size = (size_t)info.end - (size_t)info.start;
        printf("[ENVIRONMENT DUMP] %s: 0x%p - 0x%p (size 0x%p)\n", info.name, (void*)info.start, (void*)info.end, (void*)size);
    }
}

void* T7LibGSC::getGameBase()
{
    if (T7LibGSC::MODULE_BASE) return (void*)T7LibGSC::MODULE_BASE;
    //ump_mem_to_console();
    SceKernelVirtualQueryInfo info;
    void* addr = 0;
    while (sceKernelVirtualQuery(addr, SCE_KERNEL_VQ_FIND_NEXT, &info, sizeof(SceKernelVirtualQueryInfo)) >= 0)
    {
        addr = (void*)((uint64_t)info.end + 1);
        auto size = (size_t)info.end - (size_t)info.start;
        if (/*info.protection == 5 && */strcmp(info.name, "executable") == 0 && size >= 100072000) //131072000
        {
            T7LibGSC::MODULE_SIZE = size;
            return (void*)(T7LibGSC::MODULE_BASE = (uint64_t)info.start);
        }
    }
    return 0;
}

void T7LibGSC::clear_old_config()
{
    printf("gsc: clearing old config...\n");
    for (auto it = ScriptFiles.begin(); it != ScriptFiles.end(); it++)
    {
        auto script = *it;
        if (!script->IsValid) continue;
        if (!script->OriginalBuffer || !script->Buffer || !script->OriginalBufferPtr) continue;
        printf("gsc: freeing an old script\n");
        *script->OriginalBufferPtr = script->OriginalBuffer; // reset old buffer so the crc32 patch works correctly
        free(script->Buffer);
        free(script);
    }
    ScriptFiles.clear();
}

int T7LibGSC::load_config_file()
{
    int fd;
    SceKernelStat sb;
    clear_old_config();
    if ((fd = sceKernelOpen(GSC_CONF, SCE_KERNEL_O_RDONLY, SCE_KERNEL_S_IRWU)) < 0) return -1;
    if (sceKernelFstat(fd, &sb)) return -2;

    char* buffer = new char[sb.st_size + 1];
    memset(buffer, 0, sb.st_size + 1);

    if (sceKernelRead(fd, buffer, sb.st_size) != sb.st_size) return -3;
    sceKernelClose(fd);

    printf("gsc: read the config file...\n");
    printf("%s\n", buffer);
    // parse the config
    // \r\n deliminated, with ';' as terminator for a line. Whitespace is parsed as part of the token, so eliminate it.
    // file; // signifies new file
    // file.target=scripts/shared/clientids_shared.gsc; // file target (inject point)
    // file.data=/data/t7/clientids.gsc; // data to load

    char* key;
    char* value;
    char* line;
    char* remaining_lines = buffer;
    T7GSCEntry* entry = NULL;
    while ((line = strtok_r(remaining_lines, "\r\n", &remaining_lines)))
    {
        value = strtok_r(line, ";", &line); // assignment
        key = strtok_r(value, "=", &value);
        printf("gsc: line (%s), value (%s), key (%s)\n", line, value, key);
        if (!strcmp(key, "file"))
        {
            printf("gsc: config is starting a new file entry...\n");
            entry = new T7GSCEntry();
            ScriptFiles.push_back(entry);
            continue;
        }

        if (!entry) continue;

        if (!strcmp(key, "file.target"))
        {
            printf("gsc: config is applying a target...\n");
            memset(entry->Target, 0, sizeof(entry->Target));
            memcpy(entry->Target, value, std::min(sizeof(entry->Target) - 1, strlen(value)));
            continue;
        }

        if (!strcmp(key, "file.data"))
        {
            printf("gsc: config is trying to read the script buffer...\n");
            if ((fd = sceKernelOpen(value, SCE_KERNEL_O_RDONLY, SCE_KERNEL_S_IRWU)) < 0) continue;
            if (sceKernelFstat(fd, &sb)) continue;
            entry->Buffer = new char[sb.st_size];
            entry->IsValid = sceKernelRead(fd, entry->Buffer, sb.st_size) == sb.st_size;
            printf("gsc: buffer was loaded...\n");
        }
    }

    free(buffer);
    sceKernelClose(fd);
    return 0;
}

int T7LibGSC::read_serial()
{
    int fd;
    SceKernelStat sb;
    if ((fd = sceKernelOpen(GSC_SERIAL, SCE_KERNEL_O_RDONLY, SCE_KERNEL_S_IRWU)) < 0) return 0;
    if (sceKernelFstat(fd, &sb)) return 0;

    char* buffer = new char[sb.st_size + 1];
    memset(buffer, 0, sb.st_size + 1);
    if (sceKernelRead(fd, buffer, sb.st_size) != sb.st_size) return 0;
    auto v = atoi(buffer);
    free(buffer);
    sceKernelClose(fd);
    return v;
}

void* T7LibGSC::injector_think(void*)
{
    printf("gsc: injector think thread activated\n");
    //auto ctrl_ctx = Controller::Input::ControllerContext();
    //SceUserServiceUserId sce_user;
    //sce_user = -1;
    //printf("gsc: Controller context created\n");
    //while (ctrl_ctx.initialize(sce_user)) sceKernelUsleep(__S(1)); // sleep 1s, controller is not available
    //printf("gsc: Controller initialized\n");
    printf("gsc: Reading serial...\n");
    auto serial = T7LibGSC::read_serial();
    auto old_serial = serial;
    if (!serial)
    {
        printf("gsc: serial null! serial must be a number.\n");
    }
    // we now have a controller and can read from it
    while (true)
    {
        sceKernelUsleep(THINK_WAIT_ACTUAL); // sleep 16ms (60fps)

        // check if we are able to inject. if not, why do we care what the controller is doing?
        if (!can_inject()) continue;

        serial = T7LibGSC::read_serial();

        if (serial == old_serial) continue;

        old_serial = serial;
        // update controller data
        //ctrl_ctx.update((double)THINK_WAIT_MS / (double)1000);

        //if (!(ctrl_ctx.isButtonPressed(0, Controller::Input::BUTTON_L1) && ctrl_ctx.isButtonPressed(0, Controller::Input::BUTTON_R1) && ctrl_ctx.isButtonPressed(0, Controller::Input::BUTTON_TRIANGLE))) 
        //    continue; // if our buttons are not pressed, wait another frame
        
        // attempt an injection
        do_inject();

        // await release of all buttons before resuming worker thread
        //do
        //{
        //    sceKernelUsleep(THINK_WAIT_ACTUAL); // sleep 16ms (60fps)
        //    ctrl_ctx.update((double)THINK_WAIT_MS / (double)1000);
        //}
        //while (ctrl_ctx.isButtonPressed(0, Controller::Input::BUTTON_L1) || ctrl_ctx.isButtonPressed(0, Controller::Input::BUTTON_R1) || ctrl_ctx.isButtonPressed(0, Controller::Input::BUTTON_TRIANGLE));
    }
    return NULL;
}

uint64_t locate_spt_glob()
{
    if (T7LibGSC::SPT_GLOB) return T7LibGSC::SPT_GLOB;
    uint64_t start = (uint64_t)T7LibGSC::getGameBase();
    uint64_t stop = start + T7LibGSC::MODULE_SIZE;
    uint64_t spt_start = 0;
    uint64_t spt_glob = 0;
    for (uint64_t addy = start; addy < stop; addy += 8)
    {
        if (*(uint64_t*)addy == 0x20003201E0)
        {
            spt_start = addy - 0x28;
            break;
        }
    }
    printf("gsc: found spt %016lX\n", spt_start);
    if (!spt_start) return 0;

    /*
    auto n_count = 0x40;
    for (int i = 0; i < n_count; i++)
    {
        printf("%02X ", *(unsigned char*)(spt_start - n_count + i));
        if (!((i + 1) % 0x10)) printf("\n");
    }
    */

    for (uint64_t addy = start; addy < stop; addy += 8)
    {
        if (*(uint64_t*)addy == spt_start)
        {
            spt_glob = addy;
            break;
        }
    }
    /*
    n_count = 0x40;
    for (int i = 0; i < n_count; i++)
    {
        printf("%02X ", *(unsigned char*)(spt_glob + i));
        if (!((i + 1) % 0x10)) printf("\n");
    }
    */
    printf("gsc: found glob %016llX\n", (long long)spt_glob);
    return T7LibGSC::SPT_GLOB = spt_glob;
}

void T7LibGSC::do_inject()
{
    printf("gsc: attempting injection\n");
    if (T7LibGSC::load_config_file()) return; // couldnt load config
    printf("gsc: config loaded\n");
    T7ScriptParsetreeGlob* SPTGlob = (T7ScriptParsetreeGlob*)locate_spt_glob();
    if (SPTGlob == NULL) return;
    printf("gsc: scriptparsetree[ptr(%p),count(%d)]\n", (void*)SPTGlob->llpSPT, SPTGlob->NumEntries);
    T7ScriptParseTreeEntry* entry = (T7ScriptParseTreeEntry*)SPTGlob->llpSPT;

    for (auto it = ScriptFiles.begin(); it != ScriptFiles.end(); it++)
    {
        auto script = *it;
        if (!script->IsValid)
        {
            printf("ignored script %s because it is invalid.\n", script->Target);
            continue;
        }

        entry = (T7ScriptParseTreeEntry*)SPTGlob->llpSPT;
        for (int i = 0; i < SPTGlob->NumEntries; i++, entry++)
        {
            if (strcmp((char*)entry->NameOffset, script->Target)) continue;
            printf("gsc: found an injection target (%s)\n", script->Target);
            write_script(entry, script);
            break;
        }
    }
}

bool T7LibGSC::can_inject()
{
    return true;
    return (*(char*)PTR_s_runningUILevel) & 1;
}

void T7LibGSC::write_script(T7ScriptParseTreeEntry* target, T7GSCEntry* script)
{
    if (!script->IsValid || !script->Buffer) return;
    script->OriginalBuffer = target->Buffer;
    script->OriginalBufferPtr = &target->Buffer;
    target->Buffer = script->Buffer;
    if(script->OriginalBuffer) *(uint32_t*)(script->Buffer + 0x8) = *(uint32_t*)(script->OriginalBuffer + 0x8); // crc32 patch
    printf("gsc: script injected (%s)\n", script->Target);
}