#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <kernel.h>
#include <libsysmodule.h>
#include <system_service.h>
#include <net.h>
#include <pad.h>

constexpr uint32_t fnv_base_32 = 0x811c9dc5;
constexpr uint32_t fnv_prime_32 = 0x1000193;

inline constexpr uint32_t fnv1a_const(const char* const str, const uint32_t value = fnv_base_32) noexcept
{
    return (str[0] == '\0') ? value : fnv1a_const(&str[1], (value ^ uint32_t(str[0])) * fnv_prime_32);
}

inline constexpr int32_t chash(const char* const str)
{
	return 0x7FFFFFFF & fnv1a_const(str);
}

#define RPC_PORT 9023
#define RPC_INIT_DELAY 10 * 1000000
#define RPC_MAGIC 0xF24E99BC

#define CMD_T7_HOSTDVARS chash("host_dvars_t7")
#define CMD_T7_LAUNCHGAME chash("lobbylaunchgame_t7")
#define CMD_T7_INJECT chash("inject_script_t7")

#define CMD_T8_DUMP chash("dump_scripts_t8")

typedef struct CommandHeader {
	int32_t Magic;
	int32_t Command;
	int64_t AdditionalData;
} CommandHeader;

class RPC
{
public:
	static int(*sceSysUtilSendNpDebugNotificationRequest)(const char*, int);
	static void* rpc_think(void*);
	static void* on_client_connected(void* arg);
	static void notify(const char* msg);
    static void* get_game_base();

#ifdef IS_T7
	static void exec_t7_cmd(CommandHeader* header);
#endif

#ifdef IS_T8
	static void exec_t8_cmd(CommandHeader* header);
#endif

	static SceNetId ServerID;
	static bool ShouldExit;

private:
    static void* GameBase;
	static CommandHeader CryptHeader;
};