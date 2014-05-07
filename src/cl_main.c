/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cl_main.c  -- client main loop

#include "version.h"

#include "quakedef.h"

#ifdef WITH_PERL
#include "perl_defs.h"
#endif

#ifdef WITH_PYTHON
#include "python.h"
#endif

#ifndef _WIN32
#include <netdb.h>		
#endif

cvar_t	rcon_password = {"rcon_password", ""};
cvar_t	rcon_address = {"rcon_address", ""};

cvar_t	cl_timeout = {"cl_timeout", "4"};

cvar_t	cl_shownet = {"cl_shownet", "0"};	// can be 0, 1, or 2

cvar_t	cl_sbar		= {"cl_sbar", "0", CVAR_ARCHIVE};
cvar_t	cl_hudswap	= {"cl_hudswap", "0", CVAR_ARCHIVE};
cvar_t	cl_maxfps	= {"cl_maxfps", "0", CVAR_ARCHIVE};

cvar_t	cl_predictPlayers = {"cl_predictPlayers", "1"};
cvar_t	cl_solidPlayers = {"cl_solidPlayers", "1"};

cvar_t  localid = {"localid", ""};

static qboolean allowremotecmd = true;

cvar_t	cl_deadbodyfilter = {"cl_deadbodyFilter", "0"};
cvar_t	cl_gibfilter = {"cl_gibFilter", "0"};
cvar_t	cl_muzzleflash = {"cl_muzzleflash", "1"};
cvar_t	cl_rocket2grenade = {"cl_r2g", "0"};
cvar_t	cl_demospeed = {"cl_demospeed", "1"};
cvar_t	cl_staticsounds = {"cl_staticSounds", "1"};
cvar_t	cl_trueLightning = {"cl_trueLightning", "0"};
cvar_t	cl_parseWhiteText = {"cl_parseWhiteText", "1"};
cvar_t	cl_filterdrawviewmodel = {"cl_filterdrawviewmodel", "0"};
cvar_t	cl_oldPL = {"cl_oldPL", "0"};
cvar_t	cl_demoPingInterval = {"cl_demoPingInterval", "5"};
cvar_t	cl_chatsound = {"cl_chatsound", "1"};
cvar_t	cl_confirmquit = {"cl_confirmquit", "1", CVAR_INIT};
cvar_t	default_fov = {"default_fov", "0"};
cvar_t	qizmo_dir = {"qizmo_dir", "qizmo"};

cvar_t cl_floodprot			= {"cl_floodprot", "0"};		
cvar_t cl_fp_messages		= {"cl_fp_messages", "4"};		
cvar_t cl_fp_persecond		= {"cl_fp_persecond", "4"};		
cvar_t cl_cmdline			= {"cl_cmdline", "", CVAR_ROM};	
cvar_t cl_useproxy			= {"cl_useproxy", "0"};			

cvar_t cl_model_bobbing		= {"cl_model_bobbing", "1"};	
cvar_t cl_nolerp			= {"cl_nolerp", "1"};

cvar_t r_rocketlight			= {"r_rocketLight", "1"};
cvar_t r_rocketlightcolor		= {"r_rocketLightColor", "0"};
cvar_t r_explosionlightcolor	= {"r_explosionLightColor", "0"};
cvar_t r_explosionlight			= {"r_explosionLight", "1"};
cvar_t r_explosiontype			= {"r_explosionType", "0"};
cvar_t r_flagcolor				= {"r_flagColor", "0"};
cvar_t r_lightflicker			= {"r_lightflicker", "1"};
cvar_t r_rockettrail			= {"r_rocketTrail", "1"};
cvar_t r_grenadetrail			= {"r_grenadeTrail", "1"};
cvar_t r_powerupglow			= {"r_powerupGlow", "1"};

// info mirrors
cvar_t	password = {"password", "", CVAR_USERINFO};
cvar_t	spectator = {"spectator", "0", CVAR_USERINFO};
cvar_t	name = {"name", "player", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	team = {"team", "", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	topcolor = {"topcolor","0", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	bottomcolor = {"bottomcolor","0", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	skin = {"skin", "", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	rate = {"rate", "30000", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	msg = {"msg", "1", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	noaim = {"noaim", "0", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	w_switch = {"w_switch", "", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	b_switch = {"b_switch", "", CVAR_ARCHIVE|CVAR_USERINFO};

cvar_t	print_stufftext = {"print_stufftext", "",CVAR_ARCHIVE|CVAR_USERINFO};

clientPersistent_t	cls;
clientState_t		cl;

centity_t		cl_entities[MAX_EDICTS];
lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t		cl_dlights[MAX_DLIGHTS];


double		connect_time = 0;		// for connection retransmits

qboolean	host_skipframe;			// used in demo playback

byte		*host_basepal;
byte		*host_colormap;

int			fps_count;

// emodel and pmodel are encrypted to prevent llamas from easily hacking them
char emodel_name[] = { 'e'^0xe5, 'm'^0xe5, 'o'^0xe5, 'd'^0xe5, 'e'^0xe5, 'l'^0xe5, 0 };
char pmodel_name[] = { 'p'^0xe5, 'm'^0xe5, 'o'^0xe5, 'd'^0xe5, 'e'^0xe5, 'l'^0xe5, 0 };


vec3_t	vright,vup,vpn;


static void simple_crypt (char *buf, int len) {
	while (len--)
		*buf++ ^= 0xe5;
}

static void CL_FixupModelNames (void) {
	simple_crypt (emodel_name, sizeof(emodel_name) - 1);
	simple_crypt (pmodel_name, sizeof(pmodel_name) - 1);
}

//============================================================================

char *CL_Macro_ConnectionType(void) {
	char *s;
	static char macrobuf[16];

	s = (cls.state < ca_connected) ? "disconnected" : cl.spectator ? "spectator" : "player";
	Q_strncpyz(macrobuf, s, sizeof(macrobuf));
	return macrobuf;
}


char *CL_Macro_Serverstatus(void) {
	char *s;
	static char macrobuf[16];

	s = (cls.state < ca_connected) ? "disconnected" : cl.standby ? "standby" : "normal";
	Q_strncpyz(macrobuf, s, sizeof(macrobuf));
	return macrobuf;
}

int CL_ClientState (void) {
	return cls.state;
}

void CL_MakeActive(void) {
	cls.state = ca_active;


}

//Cvar system calls this when a CVAR_USERINFO cvar changes
void CL_UserinfoChanged (char *key, char *string) {
	char *s;

	s = string;

	if (strcmp(s, Info_ValueForKey (cls.userinfo, key))) {
		Info_SetValueForKey (cls.userinfo, key, s, MAX_INFO_STRING);

		if (cls.state >= ca_connected) {
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			SZ_Print (&cls.netchan.message, va("setinfo \"%s\" \"%s\"\n", key, s));
		}
	}
}

//called by CL_Connect_f and CL_CheckResend
void CL_SendConnectPacket (void) {
	netadr_t adr;
	char data[2048], biguserinfo[MAX_INFO_STRING + 32];
	double t1, t2;

	if (cls.state != ca_disconnected)
		return;

// JACK: Fixed bug where DNS lookups would cause two connects real fast
// Now, adds lookup time to the connect time.
	t1 = Sys_DoubleTime ();
	if (!NET_StringToAdr (cls.servername, &adr)) {
		Com_Printf ("Bad server address\n");
		connect_time = 0;
		return;
	}
	t2 = Sys_DoubleTime ();
	connect_time = cls.realtime + t2 - t1;	// for retransmit requests

	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	cls.qport = Cvar_VariableValue("qport");

	// let the server know what extensions we support
	Q_strncpyz (biguserinfo, cls.userinfo, sizeof(biguserinfo));
	Info_SetValueForStarKey (biguserinfo, "*z_ext", va("%i", CL_SUPPORTED_EXTENSIONS), sizeof(biguserinfo));

	sprintf (data, "\xff\xff\xff\xff" "connect %i %i %i \"%s\"\n", PROTOCOL_VERSION, cls.qport, cls.challenge, biguserinfo);
	NET_SendPacket (NS_CLIENT, strlen(data), data, adr);
}

//Resend a connect message if the last one has timed out
void CL_CheckForResend (void) {
	netadr_t adr;
	char data[2048];
	double t1, t2;

	if (cls.state == ca_disconnected && com_serveractive) {
		// if the local server is running and we are not, then connect
		Q_strncpyz (cls.servername, "local", sizeof(cls.servername));
		CL_SendConnectPacket ();	// we don't need a challenge on the local server
		// FIXME: cls.state = ca_connecting so that we don't send the packet twice?
		return;
	}

	if (cls.state != ca_disconnected || !connect_time)
		return;
	if (cls.realtime - connect_time < 5.0)
		return;

	t1 = Sys_DoubleTime ();
	if (!NET_StringToAdr (cls.servername, &adr)) {
		Com_Printf ("Bad server address\n");
		connect_time = 0;
		return;
	}
	t2 = Sys_DoubleTime ();
	connect_time = cls.realtime + t2 - t1;	// for retransmit requests

	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	Com_Printf ("Connecting to %s...\n", cls.servername);
	sprintf (data, "\xff\xff\xff\xff" "getchallenge\n");
	NET_SendPacket (NS_CLIENT, strlen(data), data, adr);
}

void CL_BeginServerConnect(void) {
	connect_time = -999;	// CL_CheckForResend() will fire immediately
	CL_CheckForResend();
}

void CL_Connect_f (void) {
	qboolean proxy;

	if (Cmd_Argc() != 2) {
		Com_Printf ("Usage: %s <server>\n", Cmd_Argv(0));
		return;
	}

	proxy = cl_useproxy.value && CL_ConnectedToProxy();

	if (proxy) {
		Cbuf_AddText(va("say ,connect %s", Cmd_Argv(1)));
	} else {
		Host_EndGame();
		Q_strncpyz(cls.servername, Cmd_Argv (1), sizeof(cls.servername));
		CL_BeginServerConnect();
	}
}




qboolean CL_ConnectedToProxy(void) {
	cmd_alias_t *alias = NULL;
	char **s, *qizmo_aliases[] = {	"ezcomp", "ezcomp2", "ezcomp3", 
									"f_sens", "f_fps", "f_tj", "f_ta", NULL};

	if (cls.state < ca_active)
		return false;
	for (s = qizmo_aliases; *s; s++) {
		if (!(alias = Cmd_FindAlias(*s)) || !(alias->flags & ALIAS_SERVER))
			return false;
	}
	return true;
}

void CL_Join_f (void) {
	qboolean proxy;

	proxy = cl_useproxy.value && CL_ConnectedToProxy();

	if (Cmd_Argc() > 2) {
		Com_Printf ("Usage: %s [server]\n", Cmd_Argv(0));
		return; 
	}

	Cvar_Set(&spectator, "");

	if (Cmd_Argc() == 2)
		Cbuf_AddText(va("%s %s\n", proxy ? "say ,connect" : "connect", Cmd_Argv(1)));
	else
		Cbuf_AddText(va("%s\n", proxy ? "say ,reconnect" : "reconnect"));
}

void CL_Observe_f (void) {
	qboolean proxy;

	proxy = cl_useproxy.value && CL_ConnectedToProxy();

	if (Cmd_Argc() > 2) {
		Com_Printf ("Usage: %s [server]\n", Cmd_Argv(0));
		return; 
	}

	if (!spectator.string[0] || !strcmp(spectator.string, "0"))
		Cvar_SetValue(&spectator, 1);

	if (Cmd_Argc() == 2)
		Cbuf_AddText(va("%s %s\n", proxy ? "say ,connect" : "connect", Cmd_Argv(1)));
	else
		Cbuf_AddText(va("%s\n", proxy ? "say ,reconnect" : "reconnect"));
}





void CL_DNS_f (void) {
}



void CL_ClearState (void) {
	int i;


	Com_DPrintf ("Clearing memory\n");

	if (!com_serveractive)
		Host_ClearMemory();

	CL_ClearScene ();

	// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));

	SZ_Clear (&cls.netchan.message);

	// clear other arrays
	memset (cl_dlights, 0, sizeof(cl_dlights));
	memset (cl_lightstyle, 0, sizeof(cl_lightstyle));
	memset (cl_entities, 0, sizeof(cl_entities));

	// make sure no centerprint messages are left from previous level

}

//Sends a disconnect message to the server
//This is also called on Host_Error, so it shouldn't cause any errors
void CL_Disconnect (void) {
	byte final[10];

	connect_time = 0;
	cl.teamfortress = false;


	// stop sounds (especially looping!)


	if (cls.state != ca_disconnected) {
		final[0] = clc_stringcmd;
		strcpy (final + 1, "drop");
		Netchan_Transmit (&cls.netchan, 6, final);
		Netchan_Transmit (&cls.netchan, 6, final);
		Netchan_Transmit (&cls.netchan, 6, final);
	}

	memset(&cls.netchan, 0, sizeof(cls.netchan));
	cls.state = ca_disconnected;


	if (cls.download) {
		fclose(cls.download);
		cls.download = NULL;
	}

	CL_StopUpload();
	DeleteServerAliases();	
}

void CL_Disconnect_f (void) {
	cl.intermission = 0;
	Host_EndGame();
}

//The server is changing levels
void CL_Reconnect_f (void) {
	if (cls.download)  // don't change when downloading
		return;


	if (cls.state == ca_connected) {
		Com_Printf ("reconnecting...\n");
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		return;
	}

	if (!*cls.servername) {
		Com_Printf ("No server to reconnect to.\n");
		return;
	}

	Host_EndGame();
	CL_BeginServerConnect();
}

//Responses to broadcasts, etc
void CL_ConnectionlessPacket (void) {
	char *s, cmdtext[2048], data[6];
	int c;

    MSG_BeginReading ();
    MSG_ReadLong ();        // skip the -1

	c = MSG_ReadByte ();
	if (!cls.demoplayback)
		Com_Printf ("%s: ", NET_AdrToString (net_from));

	switch(c) {
	case S2C_CONNECTION:
		Com_Printf ("connection\n");
		if (cls.state >= ca_connected) {
			if (!cls.demoplayback)
				Com_Printf ("Dup connect received.  Ignored.\n");
			break;
		}
		Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, cls.qport);
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		cls.state = ca_connected;
		Com_Printf ("Connected.\n");
		allowremotecmd = false; // localid required now for remote cmds
		break;

	case A2C_CLIENT_COMMAND:	// remote command from gui front end
		Com_Printf ("client command\n");

		if (!NET_IsLocalAddress(net_from)) {
			Com_Printf ("Command packet from remote host.  Ignored.\n");
			return;
		}
#ifdef _WIN32
		ShowWindow (mainwindow, SW_RESTORE);
		SetForegroundWindow (mainwindow);
#endif
		s = MSG_ReadString ();
		Q_strncpyz (cmdtext, s, sizeof(cmdtext));
		s = MSG_ReadString ();

		while (*s && isspace(*s))
			s++;
		while (*s && isspace(s[strlen(s) - 1]))
			s[strlen(s) - 1] = 0;

		if (!allowremotecmd && (!*localid.string || strcmp(localid.string, s))) {
			if (!*localid.string) {
				Com_Printf ("===========================\n");
				Com_Printf ("Command packet received from local host, but no "
					"localid has been set.  You may need to upgrade your server "
					"browser.\n");
				Com_Printf ("===========================\n");
			} else {
				Com_Printf ("===========================\n");
				Com_Printf ("Invalid localid on command packet received from local host. "
					"\n|%s| != |%s|\n"
					"You may need to reload your server browser and FuhQuake.\n",
					s, localid.string);
				Com_Printf ("===========================\n");
				Cvar_Set(&localid, "");
			}
		} else {
			Cbuf_AddText (cmdtext);
			Cbuf_AddText ("\n");
			allowremotecmd = false;
		}
		break;

	case A2C_PRINT:		// print command from somewhere
		Com_Printf ("print\n");

		s = MSG_ReadString ();
		Com_Printf ("%s", s);
		break;

	case A2A_PING:		// ping from somewhere
		Com_Printf ("ping\n");

		data[0] = 0xff;
		data[1] = 0xff;
		data[2] = 0xff;
		data[3] = 0xff;
		data[4] = A2A_ACK;
		data[5] = 0;

		NET_SendPacket (NS_CLIENT, 6, &data, net_from);
		break;

	case S2C_CHALLENGE:
		Com_Printf ("challenge\n");

		s = MSG_ReadString ();
		cls.challenge = atoi(s);
		CL_SendConnectPacket ();
		break;

	case svc_disconnect:
		if (cls.demoplayback) {
			Com_Printf("\n======== End of demo ========\n\n");
			Host_EndGame();
			Host_Abort();
		}
		break;

	default:		
		Com_Printf ("unknown:  %c\n", c);
		break;
	}
}

//Handles playback of demos, on top of NET_ code
qboolean CL_GetMessage (void) {
#ifdef _WIN32
	CL_CheckQizmoCompletion ();
#endif


	if (!NET_GetPacket(NS_CLIENT))
		return false;

	return true;
}

void CL_ReadPackets (void) {
	while (CL_GetMessage()) {
		// remote command packet
		if (*(int *)net_message.data == -1)	{
			CL_ConnectionlessPacket ();
			continue;
		}

		if (net_message.cursize < 8 && !cls.mvdplayback) {	
			Com_DPrintf ("%s: Runt packet\n", NET_AdrToString(net_from));
			continue;
		}

		// packet from server
		if (!cls.demoplayback && !NET_CompareAdr (net_from, cls.netchan.remote_address)) {
			Com_DPrintf ("%s: sequenced packet without connection\n", NET_AdrToString(net_from));
			continue;
		}

		if (!Netchan_Process(&cls.netchan))
			continue;			// wasn't accepted for some reason
		CL_ParseServerMessage ();
	}

	// check timeout
	if (!cls.demoplayback && cls.state >= ca_connected ) {
		if (curtime - cls.netchan.last_received > (cl_timeout.value > 0 ? cl_timeout.value : 60)) {
			Com_Printf("\nServer connection timed out.\n");
			Host_EndGame();
			return;
		}
	}
}

void CL_SendToServer (void) {
	// when recording demos, request new ping times every cl_demoPingInterval.value seconds
	if (cls.demorecording && !cls.demoplayback && cls.state == ca_active && cl_demoPingInterval.value > 0) {
		if (cls.realtime - cl.last_ping_request > cl_demoPingInterval.value) {
			cl.last_ping_request = cls.realtime;
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			SZ_Print (&cls.netchan.message, "pings");
		}
	}

	// send intentions now
	// resend a connection request if necessary
	if (cls.state == ca_disconnected)
		CL_CheckForResend ();
	else
		CL_SendCmd ();
}

//=============================================================================

void CL_SaveArgv(int argc, char **argv) {
	char saved_args[512];
	int i, total_length, length;
	qboolean first = true;

	length = total_length = saved_args[0] = 0;
	for (i = 0; i < argc; i++){
		if (!argv[i][0])
			continue;
		if (!first && total_length + 1 < sizeof(saved_args)) {
			strcat(saved_args, " ");
			total_length++;
		}
		first = false;
		length = strlen(argv[i]);
		if (total_length + length < sizeof(saved_args)) {
			strcat(saved_args, argv[i]);
			total_length += length;
		}
	}
	Cvar_ForceSet(&cl_cmdline, saved_args);
}

void CL_InitCommands (void);

void CL_InitLocal (void) {

	Cvar_SetCurrentGroup(CVAR_GROUP_CHAT);
	Cvar_Register (&cl_parseWhiteText);
	Cvar_Register (&cl_chatsound);

	Cvar_Register (&cl_floodprot);
	Cvar_Register (&cl_fp_messages );
	Cvar_Register (&cl_fp_persecond);


	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&cl_shownet);

	Cvar_SetCurrentGroup(CVAR_GROUP_SBAR);
	Cvar_Register (&cl_sbar);
	Cvar_Register (&cl_hudswap);

	Cvar_SetCurrentGroup(CVAR_GROUP_VIEWMODEL);
	Cvar_Register (&cl_filterdrawviewmodel);

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register (&cl_model_bobbing);
	Cvar_Register (&cl_nolerp);
	Cvar_Register (&cl_maxfps);
	Cvar_Register (&cl_deadbodyfilter);
	Cvar_Register (&cl_gibfilter);
	Cvar_Register (&cl_muzzleflash);
	Cvar_Register (&cl_rocket2grenade);
	Cvar_Register (&r_explosiontype);
	Cvar_Register (&r_lightflicker);
	Cvar_Register (&r_rockettrail);
	Cvar_Register (&r_grenadetrail);
	Cvar_Register (&r_powerupglow);
	Cvar_Register (&r_rocketlight);
	Cvar_Register (&r_explosionlight);
	Cvar_Register (&r_rocketlightcolor);
	Cvar_Register (&r_explosionlightcolor);
	Cvar_Register (&r_flagcolor);
	Cvar_Register (&cl_trueLightning);


	Cvar_SetCurrentGroup(CVAR_GROUP_DEMO);
	Cvar_Register (&cl_demospeed);
	Cvar_Register (&cl_demoPingInterval);
	Cvar_Register (&qizmo_dir);

	Cvar_SetCurrentGroup(CVAR_GROUP_SOUND);
	Cvar_Register (&cl_staticsounds);

	Cvar_SetCurrentGroup(CVAR_GROUP_USERINFO);
	Cvar_Register (&team);
	Cvar_Register (&spectator);
	Cvar_Register (&skin);
	Cvar_Register (&rate);
	Cvar_Register (&noaim);
	Cvar_Register (&name);
	Cvar_Register (&msg);
	Cvar_Register (&topcolor);
	Cvar_Register (&bottomcolor);
	Cvar_Register (&w_switch);
	Cvar_Register (&b_switch);

	Cvar_SetCurrentGroup(CVAR_GROUP_VIEW);
	Cvar_Register (&default_fov);

	Cvar_SetCurrentGroup(CVAR_GROUP_NETWORK);
	Cvar_Register (&cl_predictPlayers);
	Cvar_Register (&cl_solidPlayers);
	Cvar_Register (&cl_oldPL);
	Cvar_Register (&cl_timeout);
	Cvar_Register (&cl_useproxy);

	Cvar_SetCurrentGroup(CVAR_GROUP_NO_GROUP);
	Cvar_Register (&password);
	Cvar_Register (&rcon_password);
	Cvar_Register (&rcon_address);
	Cvar_Register (&localid);
	Cvar_Register (&cl_warncmd);
	Cvar_Register (&cl_cmdline);
	Cvar_Register (&print_stufftext);

	Cvar_ResetCurrentGroup();

	Cvar_Register (&cl_confirmquit);

 	Info_SetValueForStarKey (cls.userinfo, "*ThinClient", FUH_VERSION, MAX_INFO_STRING);

	Cmd_AddLegacyCommand ("demotimescale", "cl_demospeed");

	CL_InitCommands ();

	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	Cmd_AddCommand ("connect", CL_Connect_f);

	Cmd_AddCommand ("join", CL_Join_f);
	Cmd_AddCommand ("observe", CL_Observe_f);


	Cmd_AddCommand ("dns", CL_DNS_f);

	Cmd_AddCommand ("reconnect", CL_Reconnect_f);

	Cmd_AddMacro("connectiontype", CL_Macro_ConnectionType);
	Cmd_AddMacro("matchstatus", CL_Macro_Serverstatus);
}

void CL_Init (void) {
	if (dedicated)
		return;

	cls.state = ca_disconnected;

	strcpy (cls.gamedirfile, com_gamedirfile);
	strcpy (cls.gamedir, com_gamedir);


	//Sys_mkdir(va("%s/qw", com_basedir));
	//Sys_mkdir(va("%s/fuhquake", com_basedir));	


	CL_InitLocal ();
	CL_InitEnts ();

	NET_ClientConfig (true);

#ifdef WITH_PERL
	Perl_Init();
#endif
#ifdef WITH_PYTHON
	Python_Init();
#endif
}

//============================================================================

void CL_BeginLocalConnection (void) {

	// make sure we're not connected to an external server,
	// and demo playback is stopped
	if (!com_serveractive)
		CL_Disconnect ();

	cl.worldmodel = NULL;

	if (cls.state == ca_active)
		cls.state = ca_connected;
}

static double CL_MinFrameTime (void) {
	double fps, fpscap;


		fpscap = 10000;

		fps = cl_maxfps.value ? bound (30.0, cl_maxfps.value, fpscap) : com_serveractive ? fpscap : bound (30.0, rate.value / 80.0, fpscap);

	return 1 / fps;
}

void CL_Frame (double time) {

	static double extratime = 0.001;
	double minframetime;
	static double fps_lft, fps_fpscount;

	extratime += time;
	minframetime = CL_MinFrameTime();

	//printf("%f %f\n",extratime,minframetime);
	if (extratime < minframetime) {
#ifdef _WIN32
		extern cvar_t sys_yieldcpu;
		if (sys_yieldcpu.value)
			Sys_MSleep(0);
#endif
		usleep(10);
		return;
	}

	cls.trueframetime = extratime - 0.001;
	cls.trueframetime = max(cls.trueframetime, minframetime);
	extratime -= cls.trueframetime;

		cls.frametime = min(0.2, cls.trueframetime);


	cls.realtime += cls.frametime;

	if (!cl.paused)
		cl.time += cls.frametime;

#ifdef WITH_PERL
	Perl_Frame();
#endif
#ifdef WITH_PYTHON
	Python_Frame();
#endif


	// process console commands
	Cbuf_Execute();


	// fetch results from server
	CL_ReadPackets();


	// process stuffed commands
	Cbuf_ExecuteEx(&cbuf_svc);

	CL_SendToServer();

	if (cls.state >= ca_onserver) {	// !!! Tonik

		// Set up prediction for other players
		CL_SetUpPlayerPrediction(false);

		// do client side motion prediction
		//CL_PredictMove();

		// Set up prediction for other players
		CL_SetUpPlayerPrediction(true);

		// build a refresh entity list
		CL_EmitEntities();
	}


	fps_fpscount = fps_count;
	if (Sys_DoubleTime() - fps_lft > 1.0){

		//printf("fps: %f\n", fps_count / (Sys_DoubleTime() - fps_lft));
		fps_lft = Sys_DoubleTime();
	}
	cls.framecount++;
	fps_count++;


}

//============================================================================

void CL_Shutdown (void) {
#ifdef WITH_PERL
	Perl_Close();
#endif
#ifdef WITH_PYTHON
	Python_Quit();
#endif
	CL_Disconnect();

	CL_WriteConfiguration();
}
