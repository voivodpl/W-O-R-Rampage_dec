/*
Copyright (C) 1997-2001 Id Software, Inc.

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
#include "g_local.h"
#include "m_player.h"
#include "flashlight.h"

edict_t* debug_attack_ent;

char* ClientTeam(edict_t* ent)
{
	char* p;
	static char	value[512];

	value[0] = 0;

	if (!ent->client)
		return value;

	Q_strncpyz(value, sizeof value, Info_ValueForKey(ent->client->pers.userinfo, "skin"));
	p = strchr(value, '/');
	if (!p)
		return value;

	if ((int)(dmflags->value) & DF_MODELTEAMS)
	{
		*p = 0;
		return value;
	}

	// if ((int)(dmflags->value) & DF_SKINTEAMS)
	return ++p;
}

qboolean OnSameTeam(edict_t* ent1, edict_t* ent2)
{
	char	ent1Team[512];
	char	ent2Team[512];

	if (!((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS)))
		return false;

	Q_strncpyz(ent1Team, sizeof ent1Team, ClientTeam(ent1));
	Q_strncpyz(ent2Team, sizeof ent2Team, ClientTeam(ent2));

	if (strcmp(ent1Team, ent2Team) == 0)
		return true;
	return false;
}

void Cmd_PositionPrint(edict_t* ent)
{
	trace_t tr;
	vec3_t forward, end;
	AngleVectors(ent->client->v_angle, forward, NULL, NULL);
	VectorMA(ent->s.origin, 8192, forward, end);
	tr = gi.trace(ent->s.origin, NULL, NULL, end, ent, CONTENTS_SOLID);
	//gi.bprintf(PRINT_HIGH, "DEBUG: POS = %s, DEST = %s END POS = %s, DIST = %f, HIT = %s\n",
	//	vtos(ent->s.origin), vtos(end), vtos(tr.endpos), get_dist_point(ent->s.origin, tr.endpos), tr.ent->classname);
}

void Cmd_jumpa(edict_t* ent)
{
	ent->client->buttonsx |= BUTTON_JUMP;
}

void Cmd_jumpb(edict_t* ent)
{
	ent->client->buttonsx &= ~BUTTON_JUMP;
}

void Cmd_ducka(edict_t* ent)
{
	ent->client->buttonsx |= BUTTON_DUCK;
}

void Cmd_duckb(edict_t* ent)
{
	ent->client->buttonsx &= ~BUTTON_DUCK;
}

void SelectNextItem(edict_t* ent, int itflags)
{
	gclient_t* cl;
	int			i, index;
	gitem_t* it;

	cl = ent->client;

	if (cl->chase_target) {
		ChaseNext(ent);
		return;
	}

	// scan  for the next valid one
	for (i = 1; i <= MAX_ITEMS; i++)
	{
		index = (cl->pers.selected_item + i) % MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		return;
	}

	cl->pers.selected_item = -1;
}

void SelectPrevItem(edict_t* ent, int itflags)
{
	gclient_t* cl;
	int			i, index;
	gitem_t* it;

	cl = ent->client;

	if (cl->chase_target) {
		ChasePrev(ent);
		return;
	}

	// scan  for the next valid one
	for (i = 1; i <= MAX_ITEMS; i++)
	{
		index = (cl->pers.selected_item + MAX_ITEMS - i) % MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		return;
	}

	cl->pers.selected_item = -1;
}

void ValidateSelectedItem(edict_t* ent)
{
	gclient_t* cl;

	cl = ent->client;

	if (cl->pers.inventory[cl->pers.selected_item])
		return;		// valid

	SelectNextItem(ent, -1);
}

void Cmd_MonsterPrint(edict_t* ent)
{
	vec3_t forward;
	trace_t tr;
	AngleVectors(ent->s.angles, forward, NULL, NULL);
	VectorScale(forward, 8192, forward);
	tr = gi.trace(ent->s.origin, NULL, NULL, forward, ent, MASK_SHOT);


	if (tr.ent && tr.ent->svflags & SVF_MONSTER)
	{
		if (tr.ent->goalentity)
			gi.bprintf(PRINT_HIGH, "MONSTER DEBUG: goalentity = %s, origin = %s\n", tr.ent->goalentity->classname, vtos(tr.ent->goalentity->s.origin));

		if (tr.ent->target_ent)
			gi.bprintf(PRINT_HIGH, "MONSTER DEBUG: target_ent = %s, origin = %s\n", tr.ent->target_ent->classname, vtos(tr.ent->target_ent->s.origin));
		if (tr.ent->movetarget)
			gi.bprintf(PRINT_HIGH, "MONSTER DEBUG: movetarget = %s, origin = %s\n", tr.ent->movetarget->classname, vtos(tr.ent->movetarget->s.origin));
		if (tr.ent->combattarget)
			gi.bprintf(PRINT_HIGH, "MONSTER DEBUG: combattarget = %s, origin = %s\n", tr.ent->combattarget, tr.ent->combattarget);
	}
}

//=================================================================================

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f(edict_t* ent)
{
	char* name;
	gitem_t* it;
	int			index;
	int			i;
	qboolean	give_all;
	edict_t* it_ent;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	name = gi.args();

	if (Q_stricmp(name, "all") == 0)
		give_all = true;
	else
		give_all = false;

	if (give_all || Q_stricmp(gi.argv(1), "health") == 0)
	{
		if (gi.argc() == 3)
			ent->health = atoi(gi.argv(2));
		else
			ent->health = ent->max_health;
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "weapons") == 0)
	{
		for (i = 0; i < game.num_items; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IT_WEAPON))
				continue;
			ent->client->pers.inventory[i] += 1;
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "ammo") == 0)
	{
		for (i = 0; i < game.num_items; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IT_AMMO))
				continue;
			Add_Ammo(ent, it, 1000);
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "armor") == 0)
	{
		gitem_armor_t* info;

		it = FindItem("Jacket Armor");
		ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

		it = FindItem("Combat Armor");
		ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

		it = FindItem("Body Armor");
		info = (gitem_armor_t*)it->info;
		ent->client->pers.inventory[ITEM_INDEX(it)] = info->max_count;

		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "Power Shield") == 0)
	{
		it = FindItem("Power Shield");
		it_ent = G_Spawn();
		it_ent->classname = it->classname;
		SpawnItem(it_ent, it);
		Touch_Item(it_ent, ent, NULL, NULL);
		if (it_ent->inuse)
			G_FreeEdict(it_ent);

		if (!give_all)
			return;
	}

	if (give_all)
	{
		for (i = 0; i < game.num_items; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (it->flags & (IT_ARMOR | IT_WEAPON | IT_AMMO))
				continue;
			ent->client->pers.inventory[i]++;

		}
		return;
	}

	it = FindItem(name);
	if (!it)
	{
		name = gi.argv(1);
		it = FindItem(name);
		if (!it)
		{
			gi.cprintf(ent, PRINT_HIGH, "unknown item\n");
			return;
		}
	}

	if (!it->pickup)
	{
		gi.cprintf(ent, PRINT_HIGH, "non-pickup item\n");
		return;
	}

	index = ITEM_INDEX(it);

	if (it->flags & IT_AMMO)
	{
		if (gi.argc() == 3)
			ent->client->pers.inventory[index] = atoi(gi.argv(2));
		else
			ent->client->pers.inventory[index] += it->quantity;
	}
	else
	{
		it_ent = G_Spawn();
		it_ent->classname = it->classname;
		SpawnItem(it_ent, it);
		Touch_Item(it_ent, ent, NULL, NULL);
		if (it_ent->inuse)
			G_FreeEdict(it_ent);
	}
}


/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f(edict_t* ent)
{
	char* msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	ent->flags ^= FL_GODMODE;
	if (!(ent->flags & FL_GODMODE))
		msg = "godmode OFF\n";
	else
		msg = "godmode ON\n";

	gi.cprintf(ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f(edict_t* ent)
{
	char* msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	ent->flags ^= FL_NOTARGET;
	if (!(ent->flags & FL_NOTARGET))
		msg = "notarget OFF\n";
	else
		msg = "notarget ON\n";

	gi.cprintf(ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f(edict_t* ent)
{
	char* msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	if (ent->movetype == MOVETYPE_NOCLIP)
	{
		ent->movetype = MOVETYPE_WALK;
		msg = "noclip OFF\n";
	}
	else
	{
		ent->movetype = MOVETYPE_NOCLIP;
		msg = "noclip ON\n";
	}

	gi.cprintf(ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Use_f

Use an inventory item
==================
*/
void Cmd_Use_f(edict_t* ent)
{
	int			index;
	gitem_t* it;
	char* s;
	//ent->client->ps.gunindex = gi.modelindex(ent->client->pers.weapon->view_model);
	s = gi.args();
	it = FindItem(s);
	if (!it)
	{
		gi.cprintf(ent, PRINT_HIGH, "unknown item: %s\n", s);
		return;
	}
	if (!it->use)
	{
		gi.cprintf(ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if (!ent->client->pers.inventory[index])
	{
		gi.cprintf(ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->use(ent, it);
}

/*
==================
Cmd_pickup

Use an inventory item
==================
*/
void Cmd_pickup_pressed(edict_t* ent)
{
	if (ent->client->kick || ent->client->pers.pickup)
		return;
	//gi.bprintf(PRINT_HIGH, "PRESSED PICKUP KEY\n");
	ent->client->ps.gunframe = 4;
	ent->client->pers.pickup = PICKUP_ATTEMPT;
	//client_cmd(ent, "+speed;");

}
void Cmd_kick2(edict_t* ent)
{
	if (ent->client->kick || ent->client->pers.pickup)
		return;
	ent->client->kick = KICK_HOLSTER_START;

}
void Cmd_pickup_depressed(edict_t* ent)
{
	//gi.bprintf(PRINT_HIGH, "DEPRESSED PICKUP KEY\n");
	if (ent->client->pers.pickup == PICKUP_PICKEDUP)
	{
		ent->client->pers.pickup = PICKUP_THROWING;
		object_throw(ent, ent->style);
		gi.sound(ent, CHAN_VOICE, gi.soundindex("*jump1.wav"), 1, ATTN_NORM, 0);



		//	strcpy(ent->client->pers.view_modelb, ent->client->pers.weapon->view_model);
			//ent->client->ps.gunindex = gi.modelindex(ent->client->pers.weapon->view_model);

	}
	else if (ent->client->pers.pickup < PICKUP_PICKEDUP)
	{
		//client_cmd(ent, "-speed;");
		ent->client->pers.pickup = PICKUP_NONE;
		ent->style = 0;
	}
	ent->client->ps.gunframe = 0;
	ent->client->weaponstate = WEAPON_ACTIVATING;
	ent->gravity = 1.0;
}

/*
==================
Cmd_Drop_f

Drop an inventory item
==================
*/
void Cmd_Drop_f(edict_t* ent)
{
	int			index;
	gitem_t* it;
	char* s;

	s = gi.args();
	it = FindItem(s);
	if (!it)
	{
		gi.cprintf(ent, PRINT_HIGH, "unknown item: %s\n", s);
		return;
	}
	if (!it->drop)
	{
		gi.cprintf(ent, PRINT_HIGH, "Item is not dropable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if (!ent->client->pers.inventory[index])
	{
		gi.cprintf(ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->drop(ent, it);
}


/*
=================
Cmd_Inven_f
=================
*/
void Cmd_Inven_f(edict_t* ent)
{
	int			i;
	gclient_t* cl;

	cl = ent->client;

	cl->showscores = false;
	cl->showhelp = false;

	if (cl->showinventory)
	{
		cl->showinventory = false;
		return;
	}

	cl->showinventory = true;

	gi.WriteByte(svc_inventory);
	for (i = 0; i < MAX_ITEMS; i++)
	{
		gi.WriteShort(cl->pers.inventory[i]);
	}
	gi.unicast(ent, true);
}

/*
=================
Cmd_InvUse_f
=================
*/
void Cmd_InvUse_f(edict_t* ent)
{
	gitem_t* it;
	if (ent->client->pers.items_activated & (FL_HOLSTER_R) ||
		ent->client->pers.items_activated & (FL_HOLSTER_E) ||
		ent->client->pers.items_activated & (FL_ACTIVATING_R) ||
		ent->client->pers.items_activated & (FL_ACTIVATING_E) ||
		ent->client->pers.items_activated & (FL_DEACTIVATING_E) ||
		ent->client->pers.items_activated & (FL_DEACTIVATING_R) ||/* FL_ACTIVATING_R | FL_HOLSTER_E
		| FL_ACTIVATING_E | FL_DEACTIVATING_R | FL_DEACTIVATING_E) ||*/ ent->client->kick || ent->client->pers.pickup)
		return;

	ValidateSelectedItem(ent);

	if (ent->client->pers.selected_item == -1)
	{
		gi.cprintf(ent, PRINT_HIGH, "No item to use.\n");
		return;
	}

	it = &itemlist[ent->client->pers.selected_item];
	if (!it->use)
	{
		gi.cprintf(ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	it->use(ent, it);
}

/*
=================
Cmd_WeapPrev_f
=================
*/
void Cmd_WeapPrev_f(edict_t* ent)
{
	gclient_t* cl;
	int			i, index;
	gitem_t* it;
	int			selected_weapon;

	cl = ent->client;

	if (!cl->pers.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->pers.weapon);

	// scan  for the next valid one
	for (i = 1; i <= MAX_ITEMS; i++)
	{
		index = (selected_weapon + i) % MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & IT_WEAPON))
			continue;
		it->use(ent, it);
		if (cl->pers.weapon == it)
			return;	// successful
	}
}

/*
=================
Cmd_WeapNext_f
=================
*/
void Cmd_WeapNext_f(edict_t* ent)
{
	gclient_t* cl;
	int			i, index;
	gitem_t* it;
	int			selected_weapon;

	cl = ent->client;

	if (!cl->pers.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->pers.weapon);

	// scan  for the next valid one
	for (i = 1; i <= MAX_ITEMS; i++)
	{
		index = (selected_weapon + MAX_ITEMS - i) % MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & IT_WEAPON))
			continue;
		it->use(ent, it);
		if (cl->pers.weapon == it)
			return;	// successful
	}
}

/*
=================
Cmd_WeapLast_f
=================
*/
void Cmd_WeapLast_f(edict_t* ent)
{
	gclient_t* cl;
	int			index;
	gitem_t* it;

	cl = ent->client;

	if (!cl->pers.weapon || !cl->pers.lastweapon)
		return;

	index = ITEM_INDEX(cl->pers.lastweapon);
	if (!cl->pers.inventory[index])
		return;
	it = &itemlist[index];
	if (!it->use)
		return;
	if (!(it->flags & IT_WEAPON))
		return;
	it->use(ent, it);
}

/*
=================
Cmd_InvDrop_f
=================
*/
void Cmd_InvDrop_f(edict_t* ent)
{
	gitem_t* it;

	ValidateSelectedItem(ent);

	if (ent->client->pers.selected_item == -1)
	{
		gi.cprintf(ent, PRINT_HIGH, "No item to drop.\n");
		return;
	}

	it = &itemlist[ent->client->pers.selected_item];
	if (!it->drop)
	{
		gi.cprintf(ent, PRINT_HIGH, "Item is not dropable.\n");
		return;
	}
	it->drop(ent, it);
}

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f(edict_t* ent)
{
	if ((level.time - ent->client->respawn_time) < 5)
		return;
	ent->flags &= ~FL_GODMODE;
	ent->health = 0;
	meansOfDeath = MOD_SUICIDE;
	player_die(ent, ent, ent, 100000, vec3_origin);
}

/*
=================
Cmd_PutAway_f
=================
*/
void Cmd_PutAway_f(edict_t* ent)
{
	ent->client->showscores = false;
	ent->client->showhelp = false;
	ent->client->showinventory = false;
}


int PlayerSort(void const* a, void const* b)
{
	int		anum, bnum;

	anum = *(int*)a;
	bnum = *(int*)b;

	anum = game.clients[anum].ps.stats[STAT_FRAGS];
	bnum = game.clients[bnum].ps.stats[STAT_FRAGS];

	if (anum < bnum)
		return -1;
	if (anum > bnum)
		return 1;
	return 0;
}

/*
=================
Cmd_Players_f
=================
*/
void Cmd_Players_f(edict_t* ent)
{
	int		i;
	int		count;
	char	small[64];
	char	large[1280];
	int		index[256];

	count = 0;
	for (i = 0; i < maxclients->value; i++)
		if (game.clients[i].pers.connected)
		{
			index[count] = i;
			count++;
		}

	// sort by frags
	qsort(index, count, sizeof(index[0]), PlayerSort);

	// print information
	large[0] = 0;

	for (i = 0; i < count; i++)
	{
		Com_sprintf(small, sizeof(small), "%3i %s\n",
			game.clients[index[i]].ps.stats[STAT_FRAGS],
			game.clients[index[i]].pers.netname);
		if (strlen(small) + strlen(large) > sizeof(large) - 100)
		{	// can't print all of them in one packet
			Com_strcat(large, sizeof large, "...\n");
			break;
		}
		Com_strcat(large, sizeof large, small);
	}

	gi.cprintf(ent, PRINT_HIGH, "%s\n%i players\n", large, count);
}

/*
=================
Cmd_Wave_f
=================
*/
void Cmd_Wave_f(edict_t* ent)
{
	int		i;

	i = atoi(gi.argv(1));

	// can't wave when ducked
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
		return;

	if (ent->client->anim_priority > ANIM_WAVE)
		return;

	ent->client->anim_priority = ANIM_WAVE;

	switch (i)
	{
	case 0:
		gi.cprintf(ent, PRINT_HIGH, "flipoff\n");
		ent->s.frame = FRAME_flip01 - 1;
		ent->client->anim_end = FRAME_flip12;
		break;
	case 1:
		gi.cprintf(ent, PRINT_HIGH, "salute\n");
		ent->s.frame = FRAME_salute01 - 1;
		ent->client->anim_end = FRAME_salute11;
		break;
	case 2:
		gi.cprintf(ent, PRINT_HIGH, "taunt\n");
		ent->s.frame = FRAME_taunt01 - 1;
		ent->client->anim_end = FRAME_taunt17;
		break;
	case 3:
		gi.cprintf(ent, PRINT_HIGH, "wave\n");
		ent->s.frame = FRAME_wave01 - 1;
		ent->client->anim_end = FRAME_wave11;
		break;
	case 4:
	default:
		gi.cprintf(ent, PRINT_HIGH, "point\n");
		ent->s.frame = FRAME_point01 - 1;
		ent->client->anim_end = FRAME_point12;
		break;
	}
}

// Manage flood protection.
qboolean CheckFlood(edict_t* ent)
{
	int	i;

	gclient_t* cl;

	if (flood_msgs->value) {
		cl = ent->client;

		if (level.time < cl->flood_locktill) {
			gi.cprintf(ent, PRINT_HIGH, "You can't talk for %d more seconds\n",
				(int)(cl->flood_locktill - level.time));
			return true;
		}

		i = cl->flood_whenhead - flood_msgs->value + 1;

		if (i < 0)
		{
			i = ((int)sizeof(cl->flood_when) / (int)sizeof(cl->flood_when[0])) + i;
		}

		if ((size_t)cl->flood_when[i] && level.time - cl->flood_when[i] < flood_persecond->value)
		{
			cl->flood_locktill = level.time + flood_waitdelay->value;
			gi.cprintf(ent, PRINT_CHAT, "Flood protection:  You can't talk for %d seconds.\n",
				(int)flood_waitdelay->value);
			return true;
		}

		cl->flood_whenhead = (((size_t)cl->flood_whenhead + 1) % (sizeof(cl->flood_when) / sizeof(cl->flood_when[0])));
		cl->flood_when[cl->flood_whenhead] = level.time;
	}
	return false;
}

/*
==================
Cmd_Say_f
==================
*/
void Cmd_Say_f(edict_t* ent, qboolean team, qboolean arg0)
{
	int		j;
	edict_t* other;
	char* p;
	char	text[2048];

	if (gi.argc() < 2 && !arg0)
		return;

	if (!((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS)))
		team = false;

	if (team)
		Com_sprintf(text, sizeof(text), "(%s): ", ent->client->pers.netname);
	else
		Com_sprintf(text, sizeof(text), "%s: ", ent->client->pers.netname);

	if (arg0)
	{
		Com_strcat(text, sizeof text, gi.argv(0));
		Com_strcat(text, sizeof text, " ");
		Com_strcat(text, sizeof text, gi.args());
	}
	else
	{
		p = gi.args();

		if (*p == '"')
		{
			p++;
			p[strlen(p) - 1] = 0;
		}
		Com_strcat(text, sizeof text, p);
	}

	// don't let text be too long for malicious reasons
	if (strlen(text) > 150)
		text[150] = 0;

	Com_strcat(text, sizeof text, "\n");

	if (flood_msgs->value)
	{
		if (CheckFlood(ent))
			return;
	}

	if (dedicated->value)
		gi.cprintf(NULL, PRINT_CHAT, "%s", text);

	for (j = 1; j <= game.maxclients; j++)
	{
		other = &g_edicts[j];
		if (!other->inuse)
			continue;
		if (!other->client)
			continue;
		if (team)
		{
			if (!OnSameTeam(ent, other))
				continue;
		}
		gi.cprintf(other, PRINT_CHAT, "%s", text);
	}
}

void Cmd_PlayerList_f(edict_t* ent)
{
	int i;
	char string[80];
	char text[1400];
	edict_t* e2;

	// connect time, ping, score, name
	*text = 0;
	for (i = 0, e2 = g_edicts + 1; i < maxclients->value; i++, e2++) {
		if (!e2->inuse)
			continue;

		Com_sprintf(string, sizeof(string), "%02d:%02d %4d %3d %s%s\n",
			(level.framenum - e2->client->resp.enterframe) / 600,
			((level.framenum - e2->client->resp.enterframe) % 600) / 10,
			e2->client->ping,
			e2->client->resp.score,
			e2->client->pers.netname,
			e2->client->resp.spectator ? " (spectator)" : "");
		if (strlen(text) + strlen(string) > sizeof(text) - 50) {
			sprintf(text + strlen(text), "And more...\n");
			gi.cprintf(ent, PRINT_HIGH, "%s", text);
			return;
		}
		Com_strcat(text, sizeof text, string);
	}
	gi.cprintf(ent, PRINT_HIGH, "%s", text);
}

void Cmd_debug_attack(edict_t* self)
{
	trace_t tr;
	vec3_t end;
	vec3_t forward, right;
	vec3_t	offset;

	VectorSet(offset, 0, 0, self->viewheight);
	AngleVectors(self->client->v_angle, forward, right, NULL);
	//P_ProjectSource(self->client, self->s.origin, offset , forward, right, start);
	VectorMA(self->s.origin, 8192, forward, end);
	tr = gi.trace(self->s.origin, NULL, NULL, end, self, CONTENTS_MONSTER);
	debug_trail(self->s.origin, end);
	//gi.bprintf(PRINT_HIGH, "DEBUG: start = %s, end = %s, fraction = %f\n", vtos(self->s.origin), vtos(end), tr.fraction);
	//if(tr.ent)
	//	gi.bprintf(PRINT_HIGH, "DEBUG: we hit = %s\n", tr.ent->classname);

	if (tr.ent->svflags & SVF_MONSTER)
	{
		if (debug_attack_ent && debug_attack_ent->s.effects & EF_FLAG1)
		{
			gi.cprintf(self, PRINT_HIGH, "DEBUG ATTACK: %s will attack %s", debug_attack_ent, tr.ent);
			debug_attack_ent->s.effects &= ~EF_FLAG1;
			M_ReactToDamage(debug_attack_ent, tr.ent);
			VectorCopy(debug_attack_ent->s.origin, tr.ent->s.origin);
			tr.ent->s.origin[2] += (fabsf(debug_attack_ent->mins[2]) + fabsf(debug_attack_ent->maxs[2]) * 0.51) + (fabsf(tr.ent->mins[2]) + fabsf(tr.ent->maxs[2]) * 0.51);
			gi.linkentity(tr.ent);
			return;
		}
		if (tr.ent->s.effects & EF_FLAG1) //cancel selection
		{
			debug_attack_ent = NULL;
			gi.cprintf(self, PRINT_HIGH, "DEBUG ATTACK: Cancelling selection");
		}
		else //mark and save the attacker
		{
			tr.ent->s.effects |= EF_FLAG1;
			debug_attack_ent = tr.ent;
			gi.cprintf(self, PRINT_HIGH, "DEBUG ATTACK: %s selected", debug_attack_ent, tr.ent);
		}
	}
}

int CountSpectators(void)
{
	int n;
	int count = 0;
	edict_t* player;

	for (n = 1; n <= maxclients->value; n++)
	{
		player = &g_edicts[n];
		if (player->inuse && player->client->pers.spectator)
			count++;
	}
	return(count);
}

void Cmd_Spectator_f(edict_t* ent)
{
	int i = 0;

	if (gi.argc() > 1)
	{
		i = atoi(gi.argv(1));
	}

	if (ent->client->resp.spectator == 1)
	{
		if (i != 0)
		{
			gi.cprintf(ent, PRINT_HIGH, "You are already a spectator!\n");
			return;
		}
		else
		{
			ent->client->resp.spectator = 0;
			ent->client->pers.spectator = 0;
			PutClientInServer(ent);
			return;
		}
	}

	if ((CountSpectators() >= (maxspectators->value)))
	{
		gi.cprintf(ent, PRINT_HIGH, "Too many spectators already\n");
		return;
	}

	if (ent->client->flashlight)
	{
		G_FreeEdict(ent->client->flashlight);
		ent->client->flashlight = NULL;
	}

	ent->client->resp.score = 0;
	ent->movetype = MOVETYPE_NOCLIP;
	ent->solid = SOLID_NOT;
	ent->svflags |= SVF_NOCLIENT;
	ent->client->ps.pmove.pm_flags &= PMF_NO_PREDICTION;
	ent->client->ps.pmove.pm_type = PM_SPECTATOR;
	ent->client->ps.gunindex = 0;
	ent->client->resp.spectator = 1;
	ent->client->pers.spectator = 1;

	gi.bprintf(PRINT_HIGH, "%s has become a spectator.\n", ent->client->pers.netname);
}

/*
=================
ClientCommand
=================
*/
void ClientCommand(edict_t* ent)
{
	char* cmd;

	if (!ent->client)
		return;		// not fully in game yet

	cmd = gi.argv(0);

	if (Q_stricmp(cmd, "flashlight") == 0)
	{
		Cmd_Flashlight(ent);
		return;
	}
	if (Q_stricmp(cmd, "players") == 0)
	{
		Cmd_Players_f(ent);
		return;
	}
	if (Q_stricmp(cmd, "say") == 0)
	{
		Cmd_Say_f(ent, false, false);
		return;
	}
	if (Q_stricmp(cmd, "say_team") == 0)
	{
		Cmd_Say_f(ent, true, false);
		return;
	}
	if (Q_stricmp(cmd, "score") == 0)
	{
		Cmd_Score_f(ent);
		return;
	}
	if (Q_stricmp(cmd, "help") == 0)
	{
		Cmd_Help_f(ent);
		return;
	}

	if (level.intermissiontime)
		return;

	if (Q_stricmp(cmd, "use") == 0)
		Cmd_Use_f(ent);
	else if (Q_stricmp(cmd, "debuga") == 0)
		Cmd_debug_attack(ent);
	else if (Q_stricmp(cmd, "+a2") == 0)
		Cmd_attack2a(ent);
	else if (Q_stricmp(cmd, "-a2") == 0)
		Cmd_attack2b(ent);
	else if (Q_stricmp(cmd, "+a3") == 0)
		Cmd_attack3a(ent);
	else if (Q_stricmp(cmd, "-a3") == 0)
		Cmd_attack3b(ent);
	else if (Q_stricmp(cmd, "+j") == 0)
		Cmd_jumpa(ent);
	else if (Q_stricmp(cmd, "-j") == 0)
		Cmd_jumpb(ent);
	else if (Q_stricmp(cmd, "+d") == 0)
		Cmd_ducka(ent);
	else if (Q_stricmp(cmd, "-d") == 0)
		Cmd_duckb(ent);
	else if (Q_stricmp(cmd, "+dw") == 0)
		Cmd_DualWielda(ent);
	else if (Q_stricmp(cmd, "-dw") == 0)
		Cmd_DualWieldb(ent);
	else if (Q_stricmp(cmd, "+gh") == 0)
		Cmd_Grapplea(ent);
	else if (Q_stricmp(cmd, "-gh") == 0)
		Cmd_Grappleb(ent);
	else if (Q_stricmp(cmd, "+pic") == 0)
		Cmd_pickup_pressed(ent);
	else if (Q_stricmp(cmd, "-pic") == 0)
		Cmd_pickup_depressed(ent);
	else if (Q_stricmp(cmd, "kick2") == 0)
		Cmd_kick2(ent);
	else if (Q_stricmp(cmd, "drop") == 0)
		Cmd_Drop_f(ent);
	else if (Q_stricmp(cmd, "give") == 0)
		Cmd_Give_f(ent);
	else if (Q_stricmp(cmd, "god") == 0)
		Cmd_God_f(ent);
	else if (Q_stricmp(cmd, "notarget") == 0)
		Cmd_Notarget_f(ent);
	else if (Q_stricmp(cmd, "noclip") == 0)
		Cmd_Noclip_f(ent);
	else if (Q_stricmp(cmd, "inven") == 0)
		Cmd_Inven_f(ent);
	else if (Q_stricmp(cmd, "invnext") == 0)
		SelectNextItem(ent, -1);
	else if (Q_stricmp(cmd, "invprev") == 0)
		SelectPrevItem(ent, -1);
	else if (Q_stricmp(cmd, "invnextw") == 0)
		SelectNextItem(ent, IT_WEAPON);
	else if (Q_stricmp(cmd, "invprevw") == 0)
		SelectPrevItem(ent, IT_WEAPON);
	else if (Q_stricmp(cmd, "invnextp") == 0)
		SelectNextItem(ent, IT_POWERUP);
	else if (Q_stricmp(cmd, "invprevp") == 0)
		SelectPrevItem(ent, IT_POWERUP);
	else if (Q_stricmp(cmd, "invuse") == 0)
		Cmd_InvUse_f(ent);
	else if (Q_stricmp(cmd, "invdrop") == 0)
		Cmd_InvDrop_f(ent);
	else if (Q_stricmp(cmd, "weapprev") == 0)
		Cmd_WeapPrev_f(ent);
	else if (Q_stricmp(cmd, "weapnext") == 0)
		Cmd_WeapNext_f(ent);
	else if (Q_stricmp(cmd, "weaplast") == 0)
		Cmd_WeapLast_f(ent);
	else if (Q_stricmp(cmd, "kill") == 0)
		Cmd_Kill_f(ent);
	else if (Q_stricmp(cmd, "putaway") == 0)
		Cmd_PutAway_f(ent);
	else if (Q_stricmp(cmd, "spectate") == 0
		|| (Q_stricmp(cmd, "observe") == 0)
		|| (Q_stricmp(cmd, "observer") == 0))
	{
		Cmd_Spectator_f(ent);
	}
	else if (Q_stricmp(cmd, "wave") == 0)
		Cmd_Wave_f(ent);
	else if (Q_stricmp(cmd, "playerlist") == 0)
		Cmd_PlayerList_f(ent);
	else if (Q_stricmp(cmd, "mprint") == 0)
		Cmd_MonsterPrint(ent);
	else if (Q_stricmp(cmd, "printpos") == 0)
		Cmd_PositionPrint(ent);
	else	// anything that doesn't match a command will be a chat
		Cmd_Say_f(ent, false, true);
}

