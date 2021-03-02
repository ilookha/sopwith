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
static IPaddress g_netIpAddress;
static TCPsocket g_netMainSocket = nullptr;
static TCPsocket g_netClientSocket = nullptr;
static SDLNet_SocketSet g_netPollSocketSet;

void swNetInitConnect(char const* host)
{
	if (SDLNet_Init() != 0)
	{
		printf("SDLNet_Init error: %s\n", SDLNet_GetError());
		return;
	}

	g_isHost = false;

	Uint16 port = SW_NET_DEFAULT_PORT;
	char hostname[SW_NET_MAX_ADDRESS_LENGTH];
	memset(hostname, 0, sizeof(hostname));

	if (const char* p = strchr(host, ':'))
	{
		strncpy_s(hostname, SW_NET_MAX_ADDRESS_LENGTH - 1, host, p - host);
		port = atoi(p + 1);
	}
	else
	{
		strcpy_s(hostname, SW_NET_MAX_ADDRESS_LENGTH - 1, host);
	}

	if (SDLNet_ResolveHost(&g_netIpAddress, hostname, port) != 0)
	{
		printf("SDLNet_ResolveHost error: %s\n", SDLNet_GetError());
		return;
	}

	g_netMainSocket = SDLNet_TCP_Open(&g_netIpAddress);
	if (g_netMainSocket == nullptr)
	{
		printf("SDLNet_TCP_Open error: %s\n", SDLNet_GetError());
		return;
	}

	g_netPollSocketSet = SDLNet_AllocSocketSet(1);
	if (g_netPollSocketSet == nullptr)
	{
		printf("SDLNet_AllocSocketSet error: %s\n", SDLNet_GetError());
		return;
	}
	if (SDLNet_TCP_AddSocket(g_netPollSocketSet, g_netMainSocket) != 1)
	{
		printf("SDLNet_TCP_AddSocket error: %s\n", SDLNet_GetError());
		return;
	}
}

void swNetInitHost()
{
	if (SDLNet_Init() != 0)
	{
		printf("SDLNet_Init error: %s\n", SDLNet_GetError());
		return;
	}

	g_isHost = true;

	Uint16 port = SW_NET_DEFAULT_PORT;
	if (SDLNet_ResolveHost(&g_netIpAddress, nullptr, port) != 0)
	{
		printf("SDLNet_ResolveHost error: %s\n", SDLNet_GetError());
		return;
	}

	g_netMainSocket = SDLNet_TCP_Open(&g_netIpAddress);
	if (g_netMainSocket == nullptr)
	{
		printf("SDLNet_TCP_Open error: %s\n", SDLNet_GetError());
		return;
	}

	// Block until a client is connected
	while (g_netClientSocket == nullptr)
	{
		g_netClientSocket = SDLNet_TCP_Accept(g_netMainSocket);
		if (ctlbreak())
		{
			printf("Connection aborted by user\n");
			return;
		}
	}

	g_netPollSocketSet = SDLNet_AllocSocketSet(1);
	if (g_netPollSocketSet == nullptr)
	{
		printf("SDLNet_AllocSocketSet error: %s\n", SDLNet_GetError());
		return;
	}
	if (SDLNet_TCP_AddSocket(g_netPollSocketSet, g_netClientSocket) != 1)
	{
		printf("SDLNet_TCP_AddSocket error: %s\n", SDLNet_GetError());
		return;
	}
}

int swNetReceive()
{
	unsigned char payload = 0;
	// Assuming the socket set contains only a single socket (1vs1 game)
	// Poll the socket set without waiting and return -1 if there's no data
	if (SDLNet_CheckSockets(g_netPollSocketSet, 0) != 1)
	{
		return -1;
	}

	if (SDLNet_TCP_Recv(g_isHost ? g_netClientSocket : g_netMainSocket, &payload, 1) != 1)
	{
		printf("SDLNet_TCP_Recv error: %s\n", SDLNet_GetError());
	}

	return payload;
}

void swNetSend(unsigned char payload)
{
	if (SDLNet_TCP_Send(g_isHost ? g_netClientSocket : g_netMainSocket, &payload, 1) < 1)
	{
		printf("SDLNet_TCP_Send error: %s\n", SDLNet_GetError());
	}
}

void swNetShutdown()
{
	if (g_netPollSocketSet != nullptr)
	{
		SDLNet_FreeSocketSet(g_netPollSocketSet);
		g_netPollSocketSet = nullptr;
	}

	if (g_netClientSocket != nullptr)
	{
		SDLNet_TCP_Close(g_netClientSocket);
		g_netClientSocket = nullptr;
	}

	if (g_netMainSocket != nullptr)
	{
		SDLNet_TCP_Close(g_netMainSocket);
		g_netMainSocket = nullptr;
	}

	g_isHost = false;

	SDLNet_Quit();
}
