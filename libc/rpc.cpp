#include "rpc.h"
#include "t7.h"
#include "prx.h"

int (*RPC::sceSysUtilSendNpDebugNotificationRequest)(const char*, int);
SceNetId RPC::ServerID = 0;
bool RPC::ShouldExit = false;
void* RPC::GameBase = NULL;

void* RPC::rpc_think(void*)
{
	ShouldExit = false;

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

		if (header_packet.Magic != RPC_MAGIC)
		{
			printf("[SMC] unknown magic, discarding...\n");
			continue;
		}

		printf("[SMC] Executing command: (%d)\n", header_packet.Command);
		switch (header_packet.Command)
		{
			case CMD_T7_HOSTDVARS:
			{
				auto g = T7::vtable();
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
				T7::vtable().Cbuf_AddText(0, "lobbyLaunchGame 1 mp_nuketown_x dm\n");
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

	printf("[SMC] Disconnected (%i)\n", client_id);
	sceNetSocketClose(client_id);
	scePthreadExit(NULL);

	return nullptr;
}

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