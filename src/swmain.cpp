// Emacs style mode select -*- C++ -*-
//---------------------------------------------------------------------------
//
// $Id: swmain.c 107 2005-04-29 19:25:29Z fraggle $
//
// Copyright(C) 1984-2000 David L. Clark
// Copyright(C) 2001-2005 Simon Howard
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
//---------------------------------------------------------------------------
//
//        swmain   -      SW mainline
//
//---------------------------------------------------------------------------

#include <stdio.h>

#include "timer.h"

#include "sw.h"
#include "swasynio.h"
#include "swcollsn.h"
#include "swgrpha.h"
#include "swinit.h"
#include "swmain.h"
#include "swmove.h"
#include "swsound.h"
#include "swtitle.h"
#include "swutil.h"
#include "video.h"

// sdh: framerate control

#define FPS 10

// sdh 28/10/2001: game options

bool conf_missiles = 0;             // allow missiles: replaces missok
bool conf_solidground = 0;          // draw ground solid like in sopwith 1
bool conf_hudsplats = 0;            // splatted birds etc
bool conf_wounded = 0;              // enable wounded planes
bool conf_animals = 1;              // birds and oxes
bool conf_harrykeys = 0;            // plane rotation relative to screen
bool conf_medals = 0;

playmode_t playmode;		/* Mode of play                     */
GAMES *currgame;		/* Game parameters and current game */
OBJECTS *consoleplayer;
OBJECTS *targets[MAX_TARG + MAX_OXEN];	/* Status of targets array          */
int numtarg[2];			/* Number of active targets by color */
int savemode;			/* Saved PC display mode            */
int tickmode;			/* Tick action to be performed      */
int counttick, countmove;	/* Performance counters             */

int gamenum = 0;		/* Current game number              */
int initial_gamenum = 0;	/* Initial game number (user spec.) */
int gmaxspeed, gminspeed;	/* Speed range based on game number */
int targrnge;			/* Target range based on game number */

bool disppos;			/* Display position flag            */
bool titleflg;			/* Title flag                       */
int dispdbg;			/* Debug value to display           */
bool soundflg;			/* Sound flag                       */
bool repflag;			/* Report statistics flag           */
bool inplay;			/* Game is in play                  */

int displx;			/* Display left and right           */

OBJECTS *nobjects;		/* Objects list.                    */
OBJECTS oobjects[MAX_PLYR];	/* Original plane object description */
OBJECTS *objbot, *objtop,	/* Top and bottom of object list    */
*objfree,			/* Free list                        */
*deltop, *delbot;		/* Newly deallocated objects        */
OBJECTS topobj, botobj;		/* Top and Bottom of obj. x list    */

OBJECTS *compnear[MAX_PLYR];	/* Planes near computer planes      */
int lcompter[MAX_PLYR] = {	/* Computer plane territory         */
	0, 1155, 0, 2089
};
int rcompter[MAX_PLYR] = {	/* Computer plane territory         */
	0, 2088, 1154, 10000
};

OBJECTS *objsmax = 0;		/* Maximum object allocated         */
int endcount;
int player;			/* Pointer to player's object       */
bool plyrplane;			/* Current object is player flag    */
bool compplane;			/* Current object is a comp plane   */
bool forcdisp;			/* Force display of ground          */
char *histin, *histout;		/* History input and output files   */
unsigned explseed;		/* random seed for explosion        */

int keydelay = -1;		/* Number of displays per keystroke */
int dispcnt;			/* Displays to delay keyboard       */
int endstat;			/* End of game status for curr. move */
int maxcrash;			/* Maximum number of crashes        */

int sintab[ANGLES] = {		/* sine table of pi/8 increments    */
	0, 98, 181, 237,	/*   multiplied by 256              */
	256, 237, 181, 98,
	0, -98, -181, -237,
	-256, -237, -181, -98
};

jmp_buf envrestart;		/* Restart environment for restart  */
				/*  long jump.                      */

/* player commands */

/* buffer of player commands, loops round. 
 * latest_player_commands[plr][latest_player_time[plr] % MAX_NET_LAG] is the
 * very latest command for plr.
 */

int latest_player_commands[MAX_PLYR][MAX_NET_LAG];
int latest_player_time[MAX_PLYR];
int num_players;

/* Time each player command in the buffer was created.
 * We store this to calculate the lag between the player command
 * being created and the command being executed. */

int player_command_time[MAX_NET_LAG];

/* Skip time.  This is used to keep players in sync.
 * Each player waits a slight bit longer than they would normally
 * (ie. in a single player game): the amount equal to skip_time here.
 * skip_time is generated from the lag players experience.
 * This means that lagged players wait a bit to "catch up" with the
 * others, keeping the game in sync.
 */

int skip_time;

/* possibly advance the game */

static int can_move(void)
{
	int i;
	int lowtic = countmove + MAX_NET_LAG;

	/* we can only advance the game if latest_player_time for all
	 * players is > countmove. */

	for (i=0; i<num_players; ++i) {
		if (latest_player_time[i] < lowtic)
			lowtic = latest_player_time[i];
	}

	return lowtic > countmove;
}

/* Calculate lag between the controls and the game */

static void calculate_lag(void)
{
        int lag = Timer_GetMS() - player_command_time[countmove % MAX_NET_LAG];
        int compensation;

        // only make a small adjustment based on the lag, so as not
        // to affect the playability.  however, over a long period
        // this should have the desired effect.
 
        compensation = lag / 100;

        // bound the compensation applied; responds to network traffic
        // spikes

        if (compensation < -5)
                compensation = -5;
        else if (compensation > 5)
                compensation = 5;

        skip_time += compensation;

//        printf("lag: %ims\n", lag);
}

static void new_move(void)
{
	int multkey;
        int tictime;

	/* generate a new move command and save it */

	multkey = Vid_GetGameKeys();

        /* tictime is the game time of the command we are creating */

        tictime = latest_player_time[player];
	latest_player_commands[player][tictime % MAX_NET_LAG] = multkey;
	++latest_player_time[player];

        /* Save the current time for lag calculation */

        player_command_time[tictime % MAX_NET_LAG] = Timer_GetMS();

	/* if this is a multiplayer game, send the command */

	if (playmode == PLAYMODE_ASYNCH) {
		asynput(multkey);
	}
}

#if 0
static void dump_cmds(void)
{
	printf("%i: %i, %i\n", countmove, 
		latest_player_commands[0][countmove % MAX_NET_LAG],
		latest_player_commands[1][countmove % MAX_NET_LAG]
		);
}
#endif

int swmain(int argc, char *argv[])
{
	int nexttic;

	nobjects = (OBJECTS *) malloc(100 * sizeof(OBJECTS));

	swinit(argc, argv);
	setjmp(envrestart);

	// sdh 28/10/2001: playmode is called from here now
	// makes for a more coherent progression through the setup process

	if (!playmode)
		getgamemode();
	swinitlevel();

	nexttic = Timer_GetMS();
        skip_time = 0;

	for (;;) {
                int nowtime;

		/* generate a new move command periodically
		 * and send to other players if neccessary */

                nowtime = Timer_GetMS();

		if (nowtime > nexttic
		 && latest_player_time[player] - countmove < MAX_NET_LAG) {

			new_move();

                        /* Be accurate (exact amount between tics);
                         * However, if a large spike occurs between tics,
                         * catch up immediately.
                         */

                        if (nowtime - nexttic > 1000)
                                nexttic = nowtime + (1000/FPS);
                        else
			        nexttic += (1000 / FPS);

                        // wait a bit longer to compensate for lag

                        nexttic += skip_time;
                        skip_time = 0;
		}

		asynupdate();
		swsndupdate();

		/* if we have all the tic commands we need, we can move */

		if (can_move()) {
                        calculate_lag();
			//dump_cmds();
			swmove();
			swdisp();
			swcollsn();
			swsound();
		}

		// sdh 15/11/2001: dont thrash the 
		// processor while waiting
		Timer_Sleep(1);
	}

	return 0;
}


//---------------------------------------------------------------------------
//
// $Log$
// Revision 1.23  2005/04/29 19:25:28  fraggle
// Update copyright to 2005
//
// Revision 1.22  2005/04/29 19:00:17  fraggle
// Remove debug message
//
// Revision 1.21  2005/04/29 18:50:02  fraggle
// Respond better to spikes
//
// Revision 1.20  2005/04/29 18:42:26  fraggle
// Auto-adjust network sends based on lag
//
// Revision 1.19  2005/04/29 10:10:12  fraggle
// "Medals" feature
// By Christoph Reichenbach <creichen@gmail.com>
//
// Revision 1.18  2005/04/28 10:54:33  fraggle
// -d option to specify start level
//  (Thanks to Christoph Reichenbach <creichen@machine.cs.colorado.edu>)
// Thanks also to Christoph for the plane chasing patch (I forgot to include
// his name in the commit message)
//
// Revision 1.17  2004/10/26 06:54:41  fraggle
// Default options which behave like Sopwith II
//
// Revision 1.16  2004/10/25 19:58:06  fraggle
// Remove 'goingsun' global variable
//
// Revision 1.15  2004/10/20 19:00:01  fraggle
// Remove currobx, endsts variables
//
// Revision 1.14  2004/10/15 22:28:39  fraggle
// Remove some dead variables and code
//
// Revision 1.13  2004/10/15 22:21:51  fraggle
// Remove debug messages
//
// Revision 1.12  2004/10/15 21:30:58  fraggle
// Improve multiplayer
//
// Revision 1.11  2004/10/15 18:57:14  fraggle
// Remove redundant wdisp variable
//
// Revision 1.10  2004/10/15 17:23:32  fraggle
// Restore HUD splats
//
// Revision 1.9  2004/10/14 08:48:46  fraggle
// Wrap the main function in system-specific code.  Remove g_argc/g_argv.
// Fix crash when unable to initialise video subsystem.
//
// Revision 1.8  2003/06/08 18:41:01  fraggle
// Merge changes from 1.7.0 -> 1.7.1 into HEAD
//
// Revision 1.7  2003/06/08 02:48:45  fraggle
// Remove dispdx, always calculated displx from the current player position
// and do proper edge-of-level bounds checking
//
// Revision 1.6  2003/06/08 02:39:25  fraggle
// Initial code to remove XOR based drawing
//
// Revision 1.5.2.1  2003/06/08 18:16:38  fraggle
// Fix networking and some compile bugs
//
// Revision 1.5  2003/06/04 17:13:26  fraggle
// Remove disprx, as it is implied from displx anyway.
//
// Revision 1.4  2003/06/04 16:02:55  fraggle
// Remove broken printscreen function
//
// Revision 1.3  2003/04/05 22:55:11  fraggle
// Remove the FOREVER macro and some unused stuff from std.h
//
// Revision 1.2  2003/04/05 22:31:29  fraggle
// Remove PLAYMODE_MULTIPLE and swnetio.c
//
// Revision 1.1.1.1  2003/02/14 19:03:14  fraggle
// Initial Sourceforge CVS import
//
//
// sdh 14/2/2003: change license header to GPL
// sdh 25/11/2001: remove intson, intsoff calls
// sdh 15/11/2001: dont thrash the processor while waiting between gametics
// sdh 29/10/2001: harrykeys
// sdh 28/10/2001: conf_ game options
// sdh 28/10/2001: moved auxdisp to swgrpha.c
// sdh 21/10/2001: rearranged headers, added cvs tags
// sdh 21/10/2001: reformatted with indent
// sdh 19/10/2001: removed externs, these are now in headers
// sdh 18/10/2001: converted all functions to ANSI-style arguments
//
// 96-12-26        Speed up game a bit
// 87-04-06        Computer plane avoiding oxen.
// 87-03-12        Wounded airplanes.
// 87-03-09        Microsoft compiler.
// 85-10-31        Atari
// 84-06-12        PC-jr Speed-up
// 84-02-02        Development
//
//---------------------------------------------------------------------------

