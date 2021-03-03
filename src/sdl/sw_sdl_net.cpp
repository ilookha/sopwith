// Emacs style mode select -*- C++ -*-
//---------------------------------------------------------------------------
//
// Copyright(C) 2021 Illya Loshchinin
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2 of the License, or (at your
// option) any later version. This program is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
// the GNU General Public License for more details. You should have
// received a copy of the GNU General Public License along with this
// program; if not, write to the Free Software Foundation, Inc., 59 Temple
// Place - Suite 330, Boston, MA 02111-1307, USA.
//
//--------------------------------------------------------------------------

#include "sw_sdl_net.h"
#include "swtitle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL_net.h>

static const Uint16 SW_NET_DEFAULT_PORT = 3847;
static const int SW_NET_MAX_ADDRESS_LENGTH = 17;

static bool g_isHost = false;
static bool g_hasErrors = false;
static IPaddress g_ipAddress;
static TCPsocket g_serverListenSocket = nullptr; // Server-side socket that listens for incoming connections
static TCPsocket g_serverClientSocket = nullptr; // Server-side socket representing the client connection
static TCPsocket g_clientServerSocket = nullptr; // Client-side socket representing the server connection
static SDLNet_SocketSet g_netPollSocketSet;

bool swNetInitConnect(char const* host)
{
	if (SDLNet_Init() != 0)
	{
		printf("SDLNet_Init error: %s\n", SDLNet_GetError());
		return false;
	}

	g_isHost = false;
	g_hasErrors = false;

	Uint16 port = SW_NET_DEFAULT_PORT;
	char hostname[SW_NET_MAX_ADDRESS_LENGTH];
	memset(hostname, 0, sizeof(hostname));

	if (const char* p = strchr(host, ':'))
	{
		if (p - host >= SW_NET_MAX_ADDRESS_LENGTH)
		{
			return false;
		}
		strncpy(hostname, host, p - host);
		port = atoi(p + 1);
	}
	else
	{
		if (strlen(host) >= SW_NET_MAX_ADDRESS_LENGTH)
		{
			return false;
		}
		strcpy(hostname, host);
	}

	if (SDLNet_ResolveHost(&g_ipAddress, hostname, port) != 0)
	{
		printf("SDLNet_ResolveHost error: %s\n", SDLNet_GetError());
		return false;
	}

	g_clientServerSocket = SDLNet_TCP_Open(&g_ipAddress);
	if (g_clientServerSocket == nullptr)
	{
		printf("SDLNet_TCP_Open error: %s\n", SDLNet_GetError());
		return false;
	}

	g_netPollSocketSet = SDLNet_AllocSocketSet(1);
	if (g_netPollSocketSet == nullptr)
	{
		g_hasErrors = true;
		printf("SDLNet_AllocSocketSet error: %s\n", SDLNet_GetError());
		return false;
	}
	if (SDLNet_TCP_AddSocket(g_netPollSocketSet, g_clientServerSocket) != 1)
	{
		g_hasErrors = true;
		printf("SDLNet_TCP_AddSocket error: %s\n", SDLNet_GetError());
		return false;
	}

	return true;
}

bool swNetInitHost()
{
	if (SDLNet_Init() != 0)
	{
		printf("SDLNet_Init error: %s\n", SDLNet_GetError());
		return false;
	}

	g_isHost = true;

	Uint16 port = SW_NET_DEFAULT_PORT;
	if (SDLNet_ResolveHost(&g_ipAddress, nullptr, port) != 0)
	{
		printf("SDLNet_ResolveHost error: %s\n", SDLNet_GetError());
		return false;
	}

	g_serverListenSocket = SDLNet_TCP_Open(&g_ipAddress);
	if (g_serverListenSocket == nullptr)
	{
		printf("SDLNet_TCP_Open error: %s\n", SDLNet_GetError());
		return false;
	}

	// Block until a client is connected
	while (g_serverClientSocket == nullptr)
	{
		if (ctlbreak())
		{
			printf("Connection aborted by user\n");
			return false;
		}
		g_serverClientSocket = SDLNet_TCP_Accept(g_serverListenSocket);
	}

	g_netPollSocketSet = SDLNet_AllocSocketSet(1);
	if (g_netPollSocketSet == nullptr)
	{
		g_hasErrors = true;
		printf("SDLNet_AllocSocketSet error: %s\n", SDLNet_GetError());
		return false;
	}
	if (SDLNet_TCP_AddSocket(g_netPollSocketSet, g_serverClientSocket) != 1)
	{
		g_hasErrors = true;
		printf("SDLNet_TCP_AddSocket error: %s\n", SDLNet_GetError());
		return false;
	}

	return true;
}

int swNetReceive()
{
	unsigned char payload = 0;
	// Assuming the socket set contains only a single socket (1vs1 game)
	// Poll the socket set without waiting and return -1 if there's no data
	if (g_netPollSocketSet == nullptr || SDLNet_CheckSockets(g_netPollSocketSet, 0) != 1)
	{
		return -1;
	}

	if (SDLNet_TCP_Recv(g_isHost ? g_serverClientSocket : g_clientServerSocket, &payload, 1) != 1)
	{
		g_hasErrors = true;
		printf("SDLNet_TCP_Recv error: %s\n", SDLNet_GetError());
	}

	return payload;
}

void swNetSend(unsigned char payload)
{
	if ((g_isHost && g_serverClientSocket == nullptr) || (!g_isHost && g_clientServerSocket == nullptr))
	{
		return;
	}
	if (SDLNet_TCP_Send(g_isHost ? g_serverClientSocket : g_clientServerSocket, &payload, 1) < 1)
	{
		g_hasErrors = true;
		printf("SDLNet_TCP_Send error: %s\n", SDLNet_GetError());
	}
}

bool swNetIsConnected()
{
	return (g_serverClientSocket != nullptr || g_clientServerSocket != nullptr) && !g_hasErrors;
}

void swNetShutdown()
{
	if (g_netPollSocketSet != nullptr)
	{
		SDLNet_FreeSocketSet(g_netPollSocketSet);
		g_netPollSocketSet = nullptr;
	}

	if (g_serverClientSocket != nullptr)
	{
		SDLNet_TCP_Close(g_serverClientSocket);
		g_serverClientSocket = nullptr;
	}

	if (g_serverListenSocket != nullptr)
	{
		SDLNet_TCP_Close(g_serverListenSocket);
		g_serverListenSocket = nullptr;
	}

	if (g_clientServerSocket != nullptr)
	{
		SDLNet_TCP_Close(g_clientServerSocket);
		g_clientServerSocket = nullptr;
	}

	g_isHost = false;
	g_hasErrors = false;

	SDLNet_Quit();
}
