#include "rpc.h"
#include "t7.h"
#include "prx.h"
#include "t8.h"

int (*RPC::sceSysUtilSendNpDebugNotificationRequest)(const char*, int);
SceNetId RPC::ServerID = 0;
bool RPC::ShouldExit = false;
void* RPC::GameBase = NULL;
CommandHeader RPC::CryptHeader = { 0 };

void* RPC::rpc_think(void*)
{
	ShouldExit = false;

	if (!CryptHeader.Magic)
	{
		CryptHeader.Magic =						0x3975EA99;
		CryptHeader.Command =					0x41763EEE;
		CryptHeader.AdditionalData =	0x7F413ECC51E9BC77;
	}

	// sleep for initialization
	sceKernelUsleep(RPC_INIT_DELAY);

	printf("[SMC] Initializing network...\n");

	if (sceNetShowNetstat() < 0) 
	{
		printf("[SMC] Creating network pool ...\n");
		sceNetInit();
		sceNetPoolCreate("NetPool", (1 * 1024 * 1024), 0);
	}

	printf("[SMC] Initializing server ...\n");
	ServerID = sceNetSocket("SrvDB", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);

	SceNetSockaddrIn serverAddress;
	serverAddress.sin_family = SCE_NET_AF_INET;
	serverAddress.sin_addr.s_addr = sceNetHtonl(SCE_NET_INADDR_ANY);
	serverAddress.sin_port = sceNetHtons(RPC_PORT);
	memset(serverAddress.sin_zero, 0, sizeof(serverAddress.sin_zero));

	int val = 1;
	sceNetSetsockopt(ServerID, SCE_NET_SOL_SOCKET, SCE_NET_SO_REUSEADDR, &val, 4);
	printf("sceNetBind: 0x%08x\n", sceNetBind(ServerID, (SceNetSockaddr*)&serverAddress, sizeof(serverAddress)));
	printf("sceNetListen: 0x%08x\n", sceNetListen(ServerID, 10));
	val = 0;
	sceNetSetsockopt(ServerID, SCE_NET_SOL_SOCKET, SCE_NET_SO_REUSEADDR, &val, 4);

	RPC::notify("[SMC] Ready to mod!");

	while (!ShouldExit)
	{
		SceNetId client_id = sceNetAccept(ServerID, NULL, NULL);
		ScePthread thread;
		scePthreadCreate(&thread, NULL, on_client_connected, reinterpret_cast<void*>(client_id), "smc");
		RPC::notify("[SMC] Client Connected!");
	}

	sceNetSocketClose(ServerID);
	scePthreadExit(NULL);

	return nullptr;
}

void RPC::notify(const char* notify)
{
	if (sceSysUtilSendNpDebugNotificationRequest) 
	{
		printf("[SMC] Client Notify: %s\n", notify);
		sceSysUtilSendNpDebugNotificationRequest(notify, 0);
	}
	else
	{
		printf("[SMC] notifications are not present...\n");
	}
}

void* RPC::on_client_connected(void* arg)
{
	SceNetId client_id = static_cast<SceNetId>(reinterpret_cast<uint64_t>(arg));
	printf("[SMC] Connected (%i): Waiting for command...\n", client_id);

	while (!ShouldExit)
	{
		CommandHeader header_packet;
		int wait_client_recv = sceNetRecv(client_id, &header_packet, sizeof(CommandHeader), 0);
		if (wait_client_recv < 0)
		{
			printf("[SMC] sceNetRecv(header) failed: 0x%08x\n", wait_client_recv);
			break;
		}

		header_packet.Magic ^= CryptHeader.Magic;
		header_packet.Command ^= CryptHeader.Command;
		header_packet.AdditionalData ^= CryptHeader.AdditionalData;

		if (header_packet.Magic != RPC_MAGIC)
		{
			printf("[SMC] corrupt data, aborting connection...\n");
			break;
		}

		printf("[SMC] Executing command\n");

#ifdef IS_T7
		exec_t7_cmd(&header_packet);
#endif

#ifdef IS_T8
		exec_t8_cmd(&header_packet);
#endif
	}

	printf("[SMC] Disconnected (%i)\n", client_id);
	sceNetSocketClose(client_id);
	scePthreadExit(NULL);

	return nullptr;
}

#ifdef IS_T7
#define GT7OFF(x) ((int64_t)g.BaseAddress + (x))
void RPC::exec_t7_cmd(CommandHeader* header_packet)
{
	auto g = T7::vtable();
	switch (header_packet->Command)
	{
		case CMD_T7_HOSTDVARS:
		{
	#ifndef USE_STATIC_OFFSETS
			g.Dvar_SetFromStringByNameFromSource = (void(*)(const char*, const char*, int, int, bool)) GT7OFF(header_packet->AdditionalData);
	#endif
			g.Dvar_SetFromStringByNameFromSource("party_minPlayers", "1", 0, 0, true);
			g.Dvar_SetFromStringByNameFromSource("lobbyDedicatedSearchSkip", "1", 0, 0, true);
			g.Dvar_SetFromStringByNameFromSource("lobbySearchTeamSize", "1", 0, 0, true);
			g.Dvar_SetFromStringByNameFromSource("lobbySearchSkip", "1", 0, 0, true);
			g.Dvar_SetFromStringByNameFromSource("lobbyMergeDedicatedEnabled", "0", 0, 0, true);
			g.Dvar_SetFromStringByNameFromSource("lobbyMergeEnabled", "0", 0, 0, true);
		}
		break;
		case CMD_T7_LAUNCHGAME:
		{
	#ifndef USE_STATIC_OFFSETS
			g.Cbuf_AddText = (void(*)(int, const char*)) GT7OFF(header_packet->AdditionalData);
	#endif
			g.Cbuf_AddText(0, "lobbyLaunchGame 1 mp_nuketown_x dm\n");
		}
		break;
		case CMD_T7_INJECT:
		{
			T7LibGSC::do_inject();
		}
		break;
		default:
			printf("unknown command!\n");
			break;
	}
}
#endif

#ifdef IS_T8
#define GT8OFF(x) ((int64_t)g.BaseAddress + (x))
void RPC::exec_t8_cmd(CommandHeader* header_packet)
{
	auto g = T8::vtable();
	switch (header_packet->Command)
	{
		case CMD_T8_DUMP:
		{
			int32_t base_slget = (int32_t)(0xFFFFFFFF & header_packet->AdditionalData);
			int32_t base_xasset = (int32_t)((0xFFFFFFFF00000000 & header_packet->AdditionalData) >> 32);
			g.SL_GetStringOfSize = (char*(*)(char*)) GT8OFF(base_slget);
			T8XAssetPool* xAssetPool_SPT = (T8XAssetPool*)GT8OFF(base_xasset);
			
			printf("[SMC] SL_GetStringOfSize(%016lX)\n", (int64_t)g.SL_GetStringOfSize);
			printf("[SMC] xAssetPool_SPT(%016lX)\n", (int64_t)xAssetPool_SPT);

			auto sptPtr = (T8ScriptParseTree*)xAssetPool_SPT->pool;
			auto count = xAssetPool_SPT->itemCount;

			printf("[SMC] ScriptParseTree(%016lX)\n", (int64_t)sptPtr);
			printf("[SMC] Num Entries(%08X)\n", count);
			// foreach script
			for (int32_t i = 0; i < count; i++, sptPtr++)
			{
				auto name = sptPtr->name_hash;
				char* buff = (char*)sptPtr->buffer_pointer;
				int32_t numStrings = *(int16_t*)(buff + 0x1C);
				char* stringsPtr = buff + *(int32_t*)(buff + 0x24);

				printf("[SMC] Script Name(%016lX)\n", name);
				printf("[SMC] Script Buff(%016lX)\n", (int64_t)buff);
				printf("[SMC] Num Strings(%d)\n", numStrings);
				printf("[SMC] Strings Pointer(%016lX)\n", (int64_t)stringsPtr);

				// decrypt the strings
				for (int j = 0; j < numStrings; j++)
				{
					char* str = buff + *(int32_t*)stringsPtr;
					printf("[SMC] StringPtr[%d](%016lX)\n", j, (int64_t)str);

					auto result = g.SL_GetStringOfSize(str);
					if (result)
					{
						printf("SL_GetStringOfSize: %016lX, (%s)\n", (int64_t)result, str);
					}
					else
					{
						printf("SL_GetStringOfSize: none\n");
					}

					char refCount = *(char*)(stringsPtr + 4);
					stringsPtr += 8 + (refCount * 4);
				}

				// dump the script to disk
				char path[256];
				memset(path, 0, 256);
				sprintf(path, "%s%016lX.gscc", DUMP_PATH, name);
				int fd;
				if ((fd = sceKernelOpen(path, SCE_KERNEL_O_RDWR | SCE_KERNEL_O_CREAT, SCE_KERNEL_S_IRWU)) < 0)
				{
					printf("Failed to emit file '%s' because the kernel said no.\n", path);
					continue;
				}
				*(char*)((char*)sptPtr->buffer_pointer + 0x6) = 0xFF;
				auto bytes = sceKernelWrite(fd, sptPtr->buffer_pointer, sptPtr->buffer_length);
				*(char*)((char*)sptPtr->buffer_pointer + 0x6) = 0x0;
				printf("wrote '%s' [%ld bytes]\n", path, bytes);
				sceKernelClose(fd);
			}
		}
		break;
		default:
			printf("unknown command!\n");
			break;
	}
}
#endif

void* RPC::get_game_base()
{
	if (GameBase) return GameBase;
	SceKernelVirtualQueryInfo info;
	void* start_addr = 0;
	void* addr = 0;
	while (sceKernelVirtualQuery(addr, SCE_KERNEL_VQ_FIND_NEXT, &info, sizeof(SceKernelVirtualQueryInfo)) >= 0)
	{
		addr = (void*)((uint64_t)info.end + 1);
		if (info.protection == 5) {
			start_addr = info.start;
			break;
		}
	}
	return GameBase = start_addr;
}