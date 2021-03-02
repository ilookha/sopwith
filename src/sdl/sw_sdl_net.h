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

#pragma once

/** Connect to the host, fail immediately if no connection could be made. */
void swNetInitConnect(char const* host);
/** Block and listen for incoming connections, until a client connection is established. */
void swNetInitHost();

/** Receive a single byte. Return -1 if no data was received (no client connection on the server), or the number of bytes received (1) otherwise. */
int swNetReceive();
/** Send a single byte. */
void swNetSend(unsigned char payload);

void swNetShutdown();
