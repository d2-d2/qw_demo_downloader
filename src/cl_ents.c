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

#include "quakedef.h"
#include "pmove.h"
//#include "teamplay.h"



#define ISDEAD(i) ( (i) >= 41 && (i) <= 102 )

extern cvar_t cl_predictPlayers, cl_solidPlayers, cl_rocket2grenade;
extern cvar_t cl_model_bobbing;		
extern cvar_t cl_nolerp;

static struct predicted_player {
	int flags;
	qboolean active;
	vec3_t origin;	// predicted origin

	qboolean predict;
	vec3_t	oldo;
	vec3_t	olda;
	vec3_t	oldv;
	player_state_t *oldstate;

} predicted_players[MAX_CLIENTS];

char *cl_modelnames[cl_num_modelindices];
cl_modelindex_t cl_modelindices[cl_num_modelindices];


void CL_InitEnts(void) {
	int i;
	byte *memalloc;

	memset(cl_modelnames, 0, sizeof(cl_modelnames));

	cl_modelnames[mi_spike] = "progs/spike.mdl";
	cl_modelnames[mi_player] = "progs/player.mdl";
	cl_modelnames[mi_flag] = "progs/flag.mdl";
	cl_modelnames[mi_tf_flag] = "progs/tf_flag.mdl";
	cl_modelnames[mi_tf_stan] = "progs/tf_stan.mdl";
	cl_modelnames[mi_explod1] = "progs/s_explod.spr";
	cl_modelnames[mi_explod2] = "progs/s_expl.spr";
	cl_modelnames[mi_h_player] = "progs/h_player.mdl";
	cl_modelnames[mi_gib1] = "progs/gib1.mdl";
	cl_modelnames[mi_gib2] = "progs/gib2.mdl";
	cl_modelnames[mi_gib3] = "progs/gib3.mdl";
	cl_modelnames[mi_rocket] = "progs/missile.mdl";
	cl_modelnames[mi_grenade] = "progs/grenade.mdl";
	cl_modelnames[mi_bubble] = "progs/s_bubble.spr";
	cl_modelnames[mi_vaxe] = "progs/v_axe.mdl";
	cl_modelnames[mi_vbio] = "progs/v_bio.mdl";
	cl_modelnames[mi_vgrap] = "progs/v_grap.mdl";
	cl_modelnames[mi_vknife] = "progs/v_knife.mdl";
	cl_modelnames[mi_vknife2] = "progs/v_knife2.mdl";
	cl_modelnames[mi_vmedi] = "progs/v_medi.mdl";
	cl_modelnames[mi_vspan] = "progs/v_span.mdl";
	
	for (i = 0; i < cl_num_modelindices; i++) {
		if (!cl_modelnames[i])
			Sys_Error("cl_modelnames[%d] not initialized", i);
	}


	CL_ClearScene();
}

void CL_ClearScene (void) {
}


dlight_t *CL_AllocDlight (int key) {
	int i;
	dlight_t *dl;

	// first look for an exact key match
	if (key) {
		dl = cl_dlights;
		for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
			if (dl->key == key) {
				memset (dl, 0, sizeof(*dl));
				dl->key = key;
				return dl;
			}
		}
	}

	// then look for anything else
	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
		if (dl->die < cl.time) {
			memset (dl, 0, sizeof(*dl));
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof(*dl));
	dl->key = key;
	return dl;
}


void CL_NewDlight (int key, vec3_t origin, float radius, float time, int type, int bubble) {
	dlight_t *dl;

	dl = CL_AllocDlight (key);
	VectorCopy (origin, dl->origin);
	dl->radius = radius;
	dl->die = cl.time + time;
	dl->type = type;
	dl->bubble = bubble;		
}

void CL_DecayLights (void) {
	int i;
	dlight_t *dl;

	if (cls.state < ca_active)
		return;

	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
		if (dl->die < cl.time || !dl->radius)
			continue;

		dl->radius -= cls.frametime * dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}


//Can go from either a baseline or a previous packet_entity
void CL_ParseDelta (entity_state_t *from, entity_state_t *to, int bits) {
	int i;

	// set everything to the state we are delta'ing from
	*to = *from;

	to->number = bits & 511;
	bits &= ~511;

	if (bits & U_MOREBITS) {	// read in the low order bits
		i = MSG_ReadByte ();
		bits |= i;
	}

	to->flags = bits;

	if (bits & U_MODEL)
		to->modelindex = MSG_ReadByte ();

	if (bits & U_FRAME)
		to->frame = MSG_ReadByte ();

	if (bits & U_COLORMAP)
		to->colormap = MSG_ReadByte();

	if (bits & U_SKIN)
		to->skinnum = MSG_ReadByte();

	if (bits & U_EFFECTS)
		to->effects = MSG_ReadByte();

	if (bits & U_ORIGIN1)
		to->origin[0] = MSG_ReadCoord ();

	if (bits & U_ANGLE1)
		to->angles[0] = MSG_ReadAngle();

	if (bits & U_ORIGIN2)
		to->origin[1] = MSG_ReadCoord ();

	if (bits & U_ANGLE2)
		to->angles[1] = MSG_ReadAngle();

	if (bits & U_ORIGIN3)
		to->origin[2] = MSG_ReadCoord ();

	if (bits & U_ANGLE3)
		to->angles[2] = MSG_ReadAngle();

	if (bits & U_SOLID) {
		// FIXME
	}
}

void FlushEntityPacket (void) {
	int word;
	entity_state_t olde, newe;

	//Com_DPrintf ("FlushEntityPacket\n");

	memset (&olde, 0, sizeof(olde));

	cl.delta_sequence = 0;
	cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].invalid = true;

	// read it all, but ignore it
	while (1) {
		word = (unsigned short) MSG_ReadShort ();
		if (msg_badread) {	// something didn't parse right...
			Host_Error ("msg_badread in packetentities");
			return;
		}

		if (!word)
			break;	// done

		CL_ParseDelta (&olde, &newe, word);
	}
}

//An svc_packetentities has just been parsed, deal with the rest of the data stream.
void CL_ParsePacketEntities (qboolean delta) {
	int oldpacket, newpacket, oldindex, newindex, word, newnum, oldnum;
	packet_entities_t *oldp, *newp, dummy;
	qboolean full;
	byte from;

	newpacket = cls.netchan.incoming_sequence & UPDATE_MASK;
	newp = &cl.frames[newpacket].packet_entities;
	cl.frames[newpacket].invalid = false;

	if (delta) {
		from = MSG_ReadByte ();

		oldpacket = cl.frames[newpacket].delta_sequence;

		if (cls.netchan.outgoing_sequence - cls.netchan.incoming_sequence >= UPDATE_BACKUP - 1) {
			// there are no valid frames left, so drop it
			FlushEntityPacket ();
			cl.validsequence = 0;
			return;
		}

		if ((from & UPDATE_MASK) != (oldpacket & UPDATE_MASK)) {
			Com_DPrintf ("WARNING: from mismatch\n");
			FlushEntityPacket ();
			cl.validsequence = 0;
			return;
		}

		if (cls.netchan.outgoing_sequence - oldpacket >= UPDATE_BACKUP - 1) {
			// we can't use this, it is too old
			FlushEntityPacket ();
			// don't clear cl.validsequence, so that frames can still be rendered;
			// it is possible that a fresh packet will be received before
			// (outgoing_sequence - incoming_sequence) exceeds UPDATE_BACKUP - 1
			return;
		}

		oldp = &cl.frames[oldpacket & UPDATE_MASK].packet_entities;
		full = false;
	} else {
		// this is a full update that we can start delta compressing from now
		oldp = &dummy;
		dummy.num_entities = 0;
		full = true;
	}

	cl.oldvalidsequence = cl.validsequence;
	cl.validsequence = cls.netchan.incoming_sequence;
	cl.delta_sequence = cl.validsequence;

	oldindex = 0;
	newindex = 0;
	newp->num_entities = 0;

	while (1) {
		word = (unsigned short) MSG_ReadShort ();
		if (msg_badread) {
			// something didn't parse right...
			Host_Error ("msg_badread in packetentities");
			return;
		}

		if (!word) {
			while (oldindex < oldp->num_entities) {
				// copy all the rest of the entities from the old packet
				if (newindex >= MAX_MVD_PACKET_ENTITIES || (newindex >= MAX_PACKET_ENTITIES && !cls.mvdplayback))
					Host_Error ("CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES");
				newp->entities[newindex] = oldp->entities[oldindex];
				newindex++;
				oldindex++;
			}
			break;
		}
		newnum = word & 511;
		oldnum = oldindex >= oldp->num_entities ? 9999 : oldp->entities[oldindex].number;

		while (newnum > oldnum) 	{
			if (full) {
				Com_Printf ("WARNING: oldcopy on full update");
				FlushEntityPacket ();
				cl.validsequence = 0;	// can't render a frame
				return;
			}

			// copy one of the old entities over to the new packet unchanged
			if (newindex >= MAX_MVD_PACKET_ENTITIES || (newindex >= MAX_PACKET_ENTITIES && !cls.mvdplayback))
				Host_Error ("CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES");
			newp->entities[newindex] = oldp->entities[oldindex];
			newindex++;
			oldindex++;
			oldnum = oldindex >= oldp->num_entities ? 9999 : oldp->entities[oldindex].number;
		}

		if (newnum < oldnum) {
			// new from baseline
			if (word & U_REMOVE) {
				if (full) {
					Com_Printf ("WARNING: U_REMOVE on full update\n");
					FlushEntityPacket ();
					cl.validsequence = 0;	// can't render a frame
					return;
				}
				continue;
			}
			if (newindex >= MAX_MVD_PACKET_ENTITIES || (newindex >= MAX_PACKET_ENTITIES && !cls.mvdplayback))
				Host_Error ("CL_ParsePacketEntities: newindex == MAX_PACKET_ENTITIES");
			CL_ParseDelta (&cl_entities[newnum].baseline, &newp->entities[newindex], word);
			newindex++;
			continue;
		}

		if (newnum == oldnum) {
			// delta from previous
			if (full) {
				cl.validsequence = 0;
				cl.delta_sequence = 0;
				Com_Printf ("WARNING: delta on full update");
			}
			if (word & U_REMOVE) {
				oldindex++;
				continue;
			}
			CL_ParseDelta (&oldp->entities[oldindex], &newp->entities[newindex], word);
			newindex++;
			oldindex++;
		}
	}

	newp->num_entities = newindex;

	if (cls.state == ca_onserver)	// we can now render a frame
		CL_MakeActive();
}

void CL_LinkPacketEntities (void) {
}


typedef struct {
	int		modelindex;
	vec3_t	origin;
	vec3_t	angles;
	int		num;	
} projectile_t;

projectile_t	cl_projectiles[MAX_PROJECTILES];
int				cl_num_projectiles;

projectile_t	cl_oldprojectiles[MAX_PROJECTILES];		
int				cl_num_oldprojectiles;					

void CL_ClearProjectiles (void) {

	cl_num_projectiles = 0;
}

//Nails are passed as efficient temporary entities
void CL_ParseProjectiles (qboolean indexed) {					
	int i, c, j, num;
	byte bits[6];
	projectile_t *pr;
	interpolate_t *int_projectile;	

	c = MSG_ReadByte ();
	for (i = 0; i < c; i++) {
		num = indexed ? MSG_ReadByte() : 0;	

		for (j = 0 ; j < 6 ; j++)
			bits[j] = MSG_ReadByte ();

		if (cl_num_projectiles == MAX_PROJECTILES)
			continue;

		pr = &cl_projectiles[cl_num_projectiles];
		int_projectile = &cl.int_projectiles[cl_num_projectiles];	
		cl_num_projectiles++;

		pr->modelindex = cl_modelindices[mi_spike];
		pr->origin[0] = (( bits[0] + ((bits[1] & 15) << 8)) << 1) - 4096;
		pr->origin[1] = (((bits[1] >> 4) + (bits[2] << 4)) << 1) - 4096;
		pr->origin[2] = ((bits[3] + ((bits[4] & 15) << 8)) << 1) - 4096;
		pr->angles[0] = 360 * (bits[4] >> 4) / 16;
		pr->angles[1] = 360 * bits[5] / 256;

		if (!(pr->num = num))
			continue;

		
		for (j = 0; j < cl_num_oldprojectiles; j++) {
			if (cl_oldprojectiles[j].num == num) {
				int_projectile->interpolate = true;
				int_projectile->oldindex = j;
				VectorCopy(pr->origin, int_projectile->origin);
				break;
			}
		}		
	}	
}

void CL_LinkProjectiles (void) {
}

void SetupPlayerEntity(int num, player_state_t *state) {
}

extern int parsecountmod;
extern double parsecounttime;

player_state_t oldplayerstates[MAX_CLIENTS];	

void CL_ParsePlayerinfo (void) {
	int	msec, flags, pm_code;
	centity_t *cent;
	player_info_t *info;
	player_state_t *state;

	player_state_t *prevstate, dummy;
	int num, i;




	num = MSG_ReadByte ();
	if (num >= MAX_CLIENTS)
		Host_Error ("CL_ParsePlayerinfo: num >= MAX_CLIENTS");

	info = &cl.players[num];
	state = &cl.frames[parsecountmod].playerstate[num];
	cent = &cl_entities[num + 1];



		flags = state->flags = MSG_ReadShort ();

		state->messagenum = cl.parsecount;
		state->origin[0] = MSG_ReadCoord ();
		state->origin[1] = MSG_ReadCoord ();
		state->origin[2] = MSG_ReadCoord ();

		state->frame = MSG_ReadByte ();

		// the other player's last move was likely some time before the packet was sent out,
		// so accurately track the exact time it was valid at
		if (flags & PF_MSEC) {
			msec = MSG_ReadByte ();
			state->state_time = parsecounttime - msec * 0.001;
		} else {
			state->state_time = parsecounttime;
		}

		if (flags & PF_COMMAND)
			MSG_ReadDeltaUsercmd (&nullcmd, &state->command, cl.protoversion);

		for (i = 0; i < 3; i++) {
			if (flags & (PF_VELOCITY1 << i) )
				state->velocity[i] = MSG_ReadShort();
			else
				state->velocity[i] = 0;
		}
		if (flags & PF_MODEL)
			state->modelindex = MSG_ReadByte ();
		else
			state->modelindex = cl_modelindices[mi_player];

		if (flags & PF_SKINNUM)
			state->skinnum = MSG_ReadByte ();
		else
			state->skinnum = 0;

		if (flags & PF_EFFECTS)
			state->effects = MSG_ReadByte ();
		else
			state->effects = 0;

		if (flags & PF_WEAPONFRAME)
			state->weaponframe = MSG_ReadByte ();
		else
			state->weaponframe = 0;

		if (cl.z_ext & Z_EXT_PM_TYPE) {
			pm_code = (flags >> PF_PMC_SHIFT) & PF_PMC_MASK;

			if (pm_code == PMC_NORMAL || pm_code == PMC_NORMAL_JUMP_HELD) {
				if (flags & PF_DEAD) {
					state->pm_type = PM_DEAD;
				} else {
					state->pm_type = PM_NORMAL;
					state->jump_held = (pm_code == PMC_NORMAL_JUMP_HELD);
				}
			} else if (pm_code == PMC_OLD_SPECTATOR) {
				state->pm_type = PM_OLD_SPECTATOR;
			} else if ((cl.z_ext & Z_EXT_PM_TYPE_NEW) && pm_code == PMC_SPECTATOR) {
				state->pm_type = PM_SPECTATOR;
			} else if ((cl.z_ext & Z_EXT_PM_TYPE_NEW) && pm_code == PMC_FLY) {
				state->pm_type = PM_FLY;
			} else {
				// future extension?
				goto guess_pm_type;
			}
		} else {
guess_pm_type:
			if (cl.players[num].spectator)
				state->pm_type = PM_OLD_SPECTATOR;
			else if (flags & PF_DEAD)
				state->pm_type = PM_DEAD;
			else
				state->pm_type = PM_NORMAL;
		}

	VectorCopy (state->command.angles, state->viewangles);
	
	oldplayerstates[num] = *state;
	
	SetupPlayerEntity(num + 1, state);
}



//Create visible entities in the correct position for all current players
void CL_LinkPlayers (void) {
}

//Builds all the pmove physents for the current frame
void CL_SetSolidEntities (void) {
	return;

}

/*
Calculate the new position of players, without other player clipping

We do this to set up real player prediction.
Players are predicted twice, first without clipping other players,
then with clipping against them.
This sets up the first phase.
*/
void CL_SetUpPlayerPrediction(qboolean dopred) {
	int j, msec;
	player_state_t *state, exact;
	double playertime;
	frame_t *frame;
	struct predicted_player *pplayer;
	return;

	playertime = cls.realtime - cls.latency + 0.02;
	if (playertime > cls.realtime)
		playertime = cls.realtime;

	frame = &cl.frames[cl.parsecount & UPDATE_MASK];

	for (j = 0; j < MAX_CLIENTS; j++) {
		pplayer = &predicted_players[j];
		state = &frame->playerstate[j];

		pplayer->active = false;

		if (state->messagenum != cl.parsecount)
			continue;	// not present this frame

		if (!state->modelindex)
			continue;

		pplayer->active = true;
		pplayer->flags = state->flags;

		// note that the local player is special, since he moves locally we use his last predicted postition
		if (j == cl.playernum) {
			VectorCopy(cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].playerstate[cl.playernum].origin, pplayer->origin);
		} else {
			// only predict half the move to minimize overruns
			msec = 500 * (playertime - state->state_time);
			if (msec <= 0 || !cl_predictPlayers.value || !dopred || cls.mvdplayback) { 
				VectorCopy (state->origin, pplayer->origin);
			} else {
				// predict players movement
				if (msec > 255)
					msec = 255;
				state->command.msec = msec;

				//CL_PredictUsercmd (state, &exact, &state->command);
				VectorCopy (exact.origin, pplayer->origin);
			}
		}
	}
}

//Builds all the pmove physents for the current frame.
//Note that CL_SetUpPlayerPrediction() must be called first!
//pmove must be setup with world and solid entity hulls before calling (via CL_PredictMove)
void CL_SetSolidPlayers (int playernum) {
}

//Builds the visedicts array for cl.time
//Made up of: clients, packet_entities, nails, and tents
void CL_EmitEntities (void) {
	if (cls.state != ca_active)
		return;

	if (!cl.validsequence)
		return;

	CL_ClearScene ();
	CL_LinkPlayers ();
	CL_LinkPacketEntities ();
	CL_LinkProjectiles ();
}

int	mvd_fixangle;


void CL_ClearPredict(void) {
	memset(predicted_players, 0, sizeof(predicted_players));
	mvd_fixangle = 0;
}
