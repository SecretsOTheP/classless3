/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2003 EQEMu Development Team (http://eqemulator.net)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY except by those people which sell it, which
	are required to give you total support for your newly bought product;
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
/*
New class for handeling corpses and everything associated with them.
Child of the Mob class.
-Quagmire
*/

#ifdef _WINDOWS
	#if (!defined(_MSC_VER) || (defined(_MSC_VER) && _MSC_VER < 1900))
		#define snprintf	_snprintf
		#define vsnprintf	_vsnprintf
	#endif
    #define strncasecmp	_strnicmp
    #define strcasecmp	_stricmp
#endif

#include "../common/global_define.h"
#include "../common/eqemu_logsys.h"
#include "../common/rulesys.h"
#include "../common/string_util.h"
#include "../common/say_link.h"

#include "corpse.h"
#include "entity.h"
#include "groups.h"
#include "mob.h"
#include "raids.h"

#ifdef BOTS
#include "bot.h"
#endif

#include "quest_parser_collection.h"
#include "string_ids.h"
#include "worldserver.h"
#include <iostream>


extern EntityList entity_list;
extern Zone* zone;
extern WorldServer worldserver;
extern npcDecayTimes_Struct npcCorpseDecayTimes[100];

void Corpse::SendEndLootErrorPacket(Client* client) {
	auto outapp = new EQApplicationPacket(OP_LootComplete, 0);
	client->QueuePacket(outapp);
	safe_delete(outapp);
}

void Corpse::SendLootReqErrorPacket(Client* client, LootResponse response) {
	auto outapp = new EQApplicationPacket(OP_MoneyOnCorpse, sizeof(moneyOnCorpseStruct));
	moneyOnCorpseStruct* d = (moneyOnCorpseStruct*) outapp->pBuffer;
	d->response		= static_cast<uint8>(response);
	d->unknown1		= 0x5a;
	d->unknown2		= 0x40;
	client->QueuePacket(outapp);
	safe_delete(outapp);
}

void Corpse::DoProceduralLoot(NPC * in_npc, std::list<ServerLootItem_Struct*>& itemlist, Client* client)
{

}

Corpse* Corpse::LoadCharacterCorpseEntity(uint32 in_dbid, uint32 in_charid, std::string in_charname, const glm::vec4& position, std::string time_of_death, bool rezzed, bool was_at_graveyard, uint32 guild_consent_id) {
	uint32 item_count = database.GetCharacterCorpseItemCount(in_dbid);
	auto buffer =
	    new char[sizeof(PlayerCorpse_Struct) + (item_count * sizeof(player_lootitem::ServerLootItem_Struct))];
	PlayerCorpse_Struct *pcs = (PlayerCorpse_Struct*)buffer;
	database.LoadCharacterCorpseData(in_dbid, pcs);

	/* Create Corpse Entity */
	auto pc = new Corpse(in_dbid,		  // uint32 in_dbid
			     in_charid,		  // uint32 in_charid
			     in_charname.c_str(), // char* in_charname
			     nullptr,		  // ItemList* in_itemlist
			     pcs->copper,	 // uint32 in_copper
			     pcs->silver,	 // uint32 in_silver
			     pcs->gold,		  // uint32 in_gold
			     pcs->plat,		  // uint32 in_plat
			     position,
			     pcs->size,	// float in_size
			     pcs->gender,      // uint8 in_gender
			     pcs->race,	// uint16 in_race
			     pcs->class_,      // uint8 in_class
			     pcs->deity,       // uint8 in_deity
			     pcs->level,       // uint8 in_level
			     pcs->texture,     // uint8 in_texture
			     pcs->helmtexture, // uint8 in_helmtexture
			     pcs->exp,	 // uint32 in_rezexp
			     was_at_graveyard  // bool wasAtGraveyard
			     );

	if (pcs->locked)
		pc->Lock();

	/* Load Item Tints */
	pc->item_tint.Head.Color = pcs->item_tint.Head.Color;
	pc->item_tint.Chest.Color = pcs->item_tint.Chest.Color;
	pc->item_tint.Arms.Color = pcs->item_tint.Arms.Color;
	pc->item_tint.Wrist.Color = pcs->item_tint.Wrist.Color;
	pc->item_tint.Hands.Color = pcs->item_tint.Hands.Color;
	pc->item_tint.Legs.Color = pcs->item_tint.Legs.Color;
	pc->item_tint.Feet.Color = pcs->item_tint.Feet.Color;
	pc->item_tint.Primary.Color = pcs->item_tint.Primary.Color;
	pc->item_tint.Secondary.Color = pcs->item_tint.Secondary.Color;

	/* Load Physical Appearance */
	pc->haircolor = pcs->haircolor;
	pc->beardcolor = pcs->beardcolor;
	pc->eyecolor1 = pcs->eyecolor1;
	pc->eyecolor2 = pcs->eyecolor2;
	pc->hairstyle = pcs->hairstyle;
	pc->luclinface = pcs->face;
	pc->beard = pcs->beard;
	pc->drakkin_heritage = pcs->drakkin_heritage;
	pc->drakkin_tattoo = pcs->drakkin_tattoo;
	pc->drakkin_details = pcs->drakkin_details;
	pc->IsRezzed(rezzed);
	pc->become_npc = false;
	pc->consented_guild_id = guild_consent_id;

	pc->UpdateEquipmentLight(); // itemlist populated above..need to determine actual values
	
	safe_delete_array(pcs);

	return pc;
} 

Corpse::Corpse(NPC* in_npc, /*ItemList* in_itemlist,*/ uint32 in_npctypeid, const NPCType** in_npctypedata, Client* give_exp_client, uint32 in_decaytime, int aapts)
// vesuvias - appearence fix
: Mob("Unnamed_Corpse","",(int64)0, (int64)0,in_npc->GetGender(),in_npc->GetRace(),in_npc->GetClass(),BT_Humanoid,//bodytype added
	in_npc->GetDeity(),in_npc->GetLevel(),in_npc->GetNPCTypeID(),in_npc->GetSize(),0,
	in_npc->GetPosition(), in_npc->GetInnateLightType(), in_npc->GetTexture(),in_npc->GetHelmTexture(),
	0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,EQ::TintProfile(),0xff,0,0,0,0,(int64)0,(int64)0,0,0,0,0,0,0,0,0,
	(*in_npctypedata)->use_model,false),
	corpse_decay_timer(in_decaytime),
	corpse_rez_timer(0),
	corpse_delay_timer(RuleI(NPC, CorpseUnlockTimer)),
	corpse_graveyard_timer(0),
	loot_cooldown_timer(10)
{
	corpse_graveyard_timer.Disable();

	is_corpse_changed = false;
	is_player_corpse = false;
	is_locked = false;
	being_looted_by = 0xFFFFFFFF;

	if (in_npc) {
		auto cur_time = time(nullptr);
		if (give_exp_client)
		{

			if (give_exp_client->IsGrouped())
			{
				Group* kg = give_exp_client->GetGroup();
				for (int i = 0; i < MAX_GROUP_MEMBERS; i++) {
					if (kg->members[i] != nullptr && kg->members[i]->IsClient()) { // If Group Member is Client
						Client *c = kg->members[i]->CastToClient();

						auto tables = zone->GetGlobalLootTables(in_npc);
						std::list<ServerLootItem_Struct*> itemlist;

						bool noLockouts = c && !c->HasLootLockouts(in_npctypeid);

						if (noLockouts)
						{
							bool mine = in_npc->QuestSpawned && (in_npc->QuestLootEntity == c->CharacterID() || in_npc->QuestLootEntity == 0);
							if (mine)
							{
								for (auto q : in_npc->quest_itemlist)
								{
									ServerLootItem_Struct* addItem = new ServerLootItem_Struct(q);
									itemlist.push_back(addItem);
								}
							}

							for (auto gTables : tables)
							{
								database.GenerateLootTableList(gTables, itemlist, c->GetGM());
							}
							for (auto gTables : in_npc->GetLoottableID())
							{
								database.GenerateLootTableList(gTables, itemlist, c->GetGM());
							}

							if (in_npctypedata && (*in_npctypedata) && (*in_npctypedata)->loot_lockout && (*in_npctypedata)->loot_lockout_timer > 0)
							{
								database.SaveCharacterLootLockout(c->loot_lockouts, c->AccountID(), c->GetIP(), (uint32)cur_time + (*in_npctypedata)->loot_lockout_timer * 3600, in_npctypeid);
							}
						}
						else
						{
							c->Message(Chat::System, "You were locked out of %s and receive no standard loot.", in_npc->GetCleanName());
						}
						DoProceduralLoot(in_npc, itemlist, c);
						corpseAccessList[c] = itemlist;
					}
				}
			}
			else if (give_exp_client->IsRaidGrouped())
			{
				Raid* kr = give_exp_client->GetRaid();
				for (uint32 i = 0; i < MAX_RAID_MEMBERS; i++)
				{
					if (kr->members[i].member)
					{
						auto tables = zone->GetGlobalLootTables(in_npc);
						std::list<ServerLootItem_Struct*> itemlist;

						bool noLockouts = !kr->members[i].member->HasLootLockouts(in_npctypeid);

						if (noLockouts)
						{
							bool mine = in_npc->QuestSpawned && (in_npc->QuestLootEntity == !kr->members[i].member->CharacterID() || in_npc->QuestLootEntity == 0);
							if (mine)
							{
								for (auto q : in_npc->quest_itemlist)
								{
									ServerLootItem_Struct* addItem = new ServerLootItem_Struct(q);
									itemlist.push_back(addItem);
								}
							}

							for (auto gTables : tables)
							{
								database.GenerateLootTableList(gTables, itemlist, kr->members[i].member->GetGM());
							}
							for (auto gTables : in_npc->GetLoottableID())
							{
								database.GenerateLootTableList(gTables, itemlist, kr->members[i].member->GetGM());
							}

							if (in_npctypedata && (*in_npctypedata) && (*in_npctypedata)->loot_lockout && (*in_npctypedata)->loot_lockout_timer > 0)
							{
								database.SaveCharacterLootLockout(kr->members[i].member->loot_lockouts, kr->members[i].member->AccountID(), kr->members[i].member->GetIP(), (uint32)cur_time + (*in_npctypedata)->loot_lockout_timer * 3600, in_npctypeid);
							}
						}
						else
						{
							kr->members[i].member->Message(Chat::System, "You were locked out of %s and receive no standard loot.", in_npc->GetCleanName());
						}

						DoProceduralLoot(in_npc, itemlist, kr->members[i].member);
						corpseAccessList[kr->members[i].member] = itemlist;
					}
				}
			}
			else if(give_exp_client)
			{
				auto tables = zone->GetGlobalLootTables(in_npc);
				std::list<ServerLootItem_Struct*> itemlist;

				bool noLockouts = !give_exp_client->HasLootLockouts(in_npctypeid);

				if (noLockouts)
				{

					bool mine = in_npc->QuestSpawned && (in_npc->QuestLootEntity == give_exp_client->CharacterID() || in_npc->QuestLootEntity == 0);
					if (mine)
					{
						for (auto q : in_npc->quest_itemlist)
						{
							ServerLootItem_Struct* addItem = new ServerLootItem_Struct(q);
							itemlist.push_back(addItem);
						}
					}

					for (auto gTables : tables)
					{
						database.GenerateLootTableList(gTables, itemlist, give_exp_client->GetGM());
					}
					for (auto gTables : in_npc->GetLoottableID())
					{
						database.GenerateLootTableList(gTables, itemlist, give_exp_client->GetGM());
					}

					if (in_npctypedata && (*in_npctypedata) && (*in_npctypedata)->loot_lockout && (*in_npctypedata)->loot_lockout_timer > 0)
					{
						database.SaveCharacterLootLockout(give_exp_client->loot_lockouts, give_exp_client->AccountID(), give_exp_client->GetIP(), (uint32)cur_time + (*in_npctypedata)->loot_lockout_timer * 3600, in_npctypeid);
					}
				}
				else
				{
					give_exp_client->Message(Chat::System, "You were locked out of %s and receive no standard loot.", in_npc->GetCleanName());
				}

				DoProceduralLoot(in_npc, itemlist, give_exp_client);
				corpseAccessList[give_exp_client] = itemlist;
			}
		}
	}

	if (give_exp_client)
	{
		SetCash(0, 0, 0, 0);

		uint32 ourCopper = 0;
		uint32 ourSilver = 0;
		uint32 ourGold = 0;
		uint32 ourPlat = 0;

		for (auto lootTable : in_npc->GetLoottableID())
		{
			uint32 copper = 0;
			uint32 silver = 0;
			uint32 gold = 0;
			uint32 plat = 0;
			database.GenerateMoney(lootTable, &copper, &silver, &gold, &plat);
			ourCopper += copper;
			ourSilver += silver;
			ourGold += gold;
			ourPlat += plat;
		}
		Group* cgroup = give_exp_client->GetGroup();
		if(ourCopper || ourSilver || ourGold || ourPlat)
		{
			if (give_exp_client->IsGrouped() && cgroup) {
				cgroup->SplitMoney(ourCopper, ourSilver, ourGold, ourPlat, give_exp_client);
			}
			else {
				char buf[128];
				buf[63] = '\0';
				std::string msg = "You receive";
				bool one = false;

				if(ourPlat > 0) {
					snprintf(buf, 63, " %u platinum", ourPlat);
					msg += buf;
					one = true;
				}
				if(ourGold > 0) {
					if(one)	msg += ",";
					snprintf(buf, 63, " %u gold", ourGold);
					msg += buf;
					one = true;
				}
				if(ourSilver > 0) {
					if(one)	msg += ",";
					snprintf(buf, 63, " %u silver", ourSilver);
					msg += buf;
					one = true;
				}
				if(ourCopper > 0) {
					if(one)	msg += ",";
					snprintf(buf, 63, " %u copper", ourCopper);
					msg += buf;
					one = true;
				}
				msg += " from the corpse.";

				give_exp_client->Message(Chat::MoneySplit, msg.c_str());

				//give_exp_client->Message(Chat::MoneySplit, "You loot %u platinum, %u gold, %u silver and %u copper from the corpse.",
				//	ourPlat, ourGold, ourSilver, ourCopper, in_npc->GetCopper());
				give_exp_client->AddMoneyToPP(ourCopper, ourSilver, ourGold, ourPlat, true);
			}
		}
	}
	else
	{
		SetCash(0, 0, 0, 0);
	}

	npctype_id = in_npctypeid;
	SetPlayerKillItemID(0);
	char_id = 0;
	corpse_db_id = 0;
	player_corpse_depop = false;
	strcpy(corpse_name, in_npc->GetName());
	strcpy(name, in_npc->GetName());

	for(int count = 0; count < 100; count++) {
		if ((level >= npcCorpseDecayTimes[count].minlvl) && (level <= npcCorpseDecayTimes[count].maxlvl)) {
			corpse_decay_timer.SetTimer(npcCorpseDecayTimes[count].seconds*1000);
			break;
		}
	}
	if(corpseAccessList.empty())
	{
		if (IsNPCCorpse() || zone->GetInstanceID() == 0)
		{
			corpse_decay_timer.SetTimer(200);
		}
		else
		{
			corpse_delay_timer.SetTimer(200);
		}
	}
	else
	{
		std::list<Client*> entriesToRemove;
		bool hasEntries = false;
		for (auto clientEntry : corpseAccessList)
		{
			if (!clientEntry.second.empty())
			{
				hasEntries = true;
			}
			else
			{
				entriesToRemove.push_back(clientEntry.first);
			}
		}
		
		for (auto cliRemove : entriesToRemove)
		{
			corpseAccessList.erase(cliRemove);
		}
		
		if (!hasEntries)
		{
			corpse_decay_timer.SetTimer(200);
		}
		else
		{
			corpse_delay_timer.SetTimer(200);
		}
	}
	//auto &entity_list_search = entity_list.GetClientList();

	//for (auto &itr : entity_list_search) {
	//	Client *entity = itr.second;
	//	if(IsEmpty(entity))
	//	{
	//		auto app = new EQApplicationPacket;
	//		CreateDespawnPacket(app, !IsCorpse());
	//		entity->QueuePacket(app);
	//		safe_delete(app);
	//	}
	//}

	//if(in_npc->HasPrivateCorpse()) {
	//	corpse_delay_timer.SetTimer(corpse_decay_timer.GetRemainingTime() + 1000);
	//}

	for (int i = 0; i < MAX_LOOTERS; i++){
		allowed_looters[i] = 0;
	}
	this->rez_experience = 0;

	UpdateEquipmentLight();
	UpdateActiveLight();

	loot_request_type = LootRequestType::Forbidden;

	corpse_npctype_id = in_npctypeid;
}

Corpse::Corpse(Client* client, int32 in_rezexp) : Mob (
	"Unnamed_Corpse",				  // const char*	in_name,
	"",								  // const char*	in_lastname,
	(int64)0,								  // int32		in_cur_hp,
	(int64)0,								  // int32		in_max_hp,
	client->GetGender(),			  // uint8		in_gender,
	client->GetRace(),				  // uint16		in_race,
	client->GetClass(),				  // uint8		in_class,
	BT_Humanoid,					  // bodyType	in_bodytype,
	client->GetDeity(),				  // uint8		in_deity,
	client->GetLevel(),				  // uint8		in_level,
	0,								  // uint32		in_npctype_id,
	client->GetSize(),				  // float		in_size,
	0,								  // float		in_runspeed,
	client->GetPosition(),
	client->GetInnateLightType(),	  // uint8		in_light, - verified for client innate_light value
	client->GetTexture(),			  // uint8		in_texture,
	client->GetHelmTexture(),		  // uint8		in_helmtexture,
	0,								  // uint16		in_ac,
	0,								  // uint16		in_atk,
	0,								  // uint16		in_str,
	0,								  // uint16		in_sta,
	0,								  // uint16		in_dex,
	0,								  // uint16		in_agi,
	0,								  // uint16		in_int,
	0,								  // uint16		in_wis,
	0,								  // uint16		in_cha,
	client->GetPP().haircolor,		  // uint8		in_haircolor,
	client->GetPP().beardcolor,		  // uint8		in_beardcolor,
	client->GetPP().eyecolor1,		  // uint8		in_eyecolor1, // the eyecolors always seem to be the same, maybe left and right eye?
	client->GetPP().eyecolor2,		  // uint8		in_eyecolor2,
	client->GetPP().hairstyle,		  // uint8		in_hairstyle,
	client->GetPP().face,			  // uint8		in_luclinface,
	client->GetPP().beard,			  // uint8		in_beard,
	client->GetPP().drakkin_heritage, // uint32		in_drakkin_heritage,
	client->GetPP().drakkin_tattoo,	  // uint32		in_drakkin_tattoo,
	client->GetPP().drakkin_details,  // uint32		in_drakkin_details,
	EQ::TintProfile(),			  // uint32		in_armor_tint[_MaterialCount],
	0xff,							  // uint8		in_aa_title,
	0,								  // uint8		in_see_invis, // see through invis
	0,								  // uint8		in_see_invis_undead, // see through invis vs. undead
	0,								  // uint8		in_see_hide,
	0,								  // uint8		in_see_improved_hide,
	(int64)0,								  // int32		in_hp_regen,
	(int64)0,								  // int32		in_mana_regen,
	0,								  // uint8		in_qglobal,
	0,								  // uint8		in_maxlevel,
	0,								  // uint32		in_scalerate
	0,								  // uint8		in_armtexture,
	0,								  // uint8		in_bracertexture,
	0,								  // uint8		in_handtexture,
	0,								  // uint8		in_legtexture,
	0,								  // uint8		in_feettexture,
	0,								  // uint8		in_usemodel,
	0								  // bool		in_always_aggro
	),
	corpse_decay_timer(RuleI(Character, CorpseDecayTimeMS)),
	corpse_rez_timer(RuleI(Character, CorpseResTimeMS)),
	corpse_delay_timer(RuleI(NPC, CorpseUnlockTimer)),
	corpse_graveyard_timer(RuleI(Zone, GraveyardTimeMS)),
	loot_cooldown_timer(10)
{
	int i;

	PlayerProfile_Struct *pp = &client->GetPP();
	EQ::ItemInstance *item = nullptr;

	/* Check if Zone has Graveyard First */
	if(!zone->HasGraveyard()) {
		corpse_graveyard_timer.Disable();
	}

	for (i = 0; i < MAX_LOOTERS; i++){
		allowed_looters[i] = 0;
	}

	if (client->AutoConsentGroupEnabled()) {
		Group* grp = client->GetGroup();
		consented_group_id = grp ? grp->GetID() : 0;
	}

	if (client->AutoConsentRaidEnabled()) {
		Raid* raid = client->GetRaid();
		consented_raid_id = raid ? raid->GetID() : 0;
	}

	consented_guild_id = client->AutoConsentGuildEnabled() ? client->GuildID() : 0;

	is_corpse_changed		= true;
	rez_experience			= in_rezexp;
	can_corpse_be_rezzed			= false;
	is_player_corpse	= true;
	is_locked			= false;
	being_looted_by	= 0xFFFFFFFF;
	char_id			= client->CharacterID();
	corpse_db_id	= 0;
	player_corpse_depop			= false;
	copper			= 0;
	silver			= 0;
	gold			= 0;
	platinum		= 0;

	strcpy(corpse_name, pp->name);
	strcpy(name, pp->name);

	/* become_npc was not being initialized which led to some pretty funky things with newly created corpses */
	become_npc = false;

	SetPlayerKillItemID(0);

	/* Check Rule to see if we can leave corpses 
	if(!RuleB(Character, LeaveNakedCorpses) ||
		RuleB(Character, LeaveCorpses) &&
		GetLevel() >= RuleI(Character, DeathItemLossLevel)) {
		// cash
		// Let's not move the cash when 'RespawnFromHover = true' && 'client->GetClientVersion() < EQClientSoF' since the client doesn't.
		// (change to first client that supports 'death hover' mode, if not SoF.)
		if (!RuleB(Character, RespawnFromHover) || client->ClientVersion() < EQ::versions::ClientVersion::SoF) {
			SetCash(pp->copper, pp->silver, pp->gold, pp->platinum);
			pp->copper = 0;
			pp->silver = 0;
			pp->gold = 0;
			pp->platinum = 0;
		}

		// get their tints
		memcpy(&item_tint.Slot, &client->GetPP().item_tint, sizeof(item_tint));

		// TODO soulbound items need not be added to corpse, but they need
		// to go into the regular slots on the player, out of bags
		std::list<uint32> removed_list;
		
		// ideally, we would start at invslot::slotGeneral1 and progress to invslot::slotCursor..
		// ..then regress and process invslot::EQUIPMENT_BEGIN through invslot::EQUIPMENT_END...
		// without additional work to database loading of player corpses, this order is not
		// currently preserved and a re-work of this processing loop is not warranted.
		for (i = EQ::invslot::POSSESSIONS_BEGIN; i <= EQ::invslot::POSSESSIONS_END; ++i) {
			item = client->GetInv().GetItem(i);
			if (item == nullptr) { continue; }

			if(!client->IsBecomeNPC() || (client->IsBecomeNPC() && !item->GetItem()->NoRent))
				MoveItemToCorpse(client, item, i, removed_list);
		}

		database.TransactionBegin();

		// this should not be modified to include the entire range of invtype::TYPE_POSSESSIONS slots by default..
		// ..due to the possibility of 'hidden' items from client version bias..or, possibly, soul-bound items (WoW?)
		if (!removed_list.empty()) {
			std::list<uint32>::const_iterator iter = removed_list.begin();

			if (iter != removed_list.end()) {
				std::stringstream ss("");
				ss << "DELETE FROM `inventory` WHERE `charid` = " << client->CharacterID();
				ss << " AND `slotid` IN (" << (*iter);
				++iter;

				while (iter != removed_list.end()) {
					ss << ", " << (*iter);
					++iter;
				}
				ss << ")";

				database.QueryDatabase(ss.str().c_str());
			}
		}

		auto start = client->GetInv().cursor_cbegin();
		auto finish = client->GetInv().cursor_cend();
		database.SaveCursor(client->CharacterID(), start, finish);

		client->CalcBonuses();
		client->Save();

		IsRezzed(false);
		Save();

		database.TransactionCommit();

		UpdateEquipmentLight();
		UpdateActiveLight();

		return;
	} //end "not leaving naked corpses" */

	UpdateEquipmentLight();
	UpdateActiveLight();

	loot_request_type = LootRequestType::Forbidden;

	IsRezzed(false);
	Save();
	corpse_npctype_id = 0;
}

void Corpse::MoveItemToCorpse(Client *client, EQ::ItemInstance *inst, int16 equipSlot, std::list<uint32> &removedList)
{
}

// To be called from LoadFromDBData
Corpse::Corpse(uint32 in_dbid, uint32 in_charid, const char* in_charname, ItemList* in_itemlist, uint32 in_copper, uint32 in_silver, uint32 in_gold, uint32 in_plat, const glm::vec4& position, float in_size, uint8 in_gender, uint16 in_race, uint8 in_class, uint8 in_deity, uint8 in_level, uint8 in_texture, uint8 in_helmtexture,uint32 in_rezexp, bool wasAtGraveyard)
: Mob("Unnamed_Corpse",
"",
(int64)0,
(int64)0,
in_gender,
in_race,
in_class,
BT_Humanoid,
in_deity,
in_level,
0,
in_size,
0,
position,
0, // verified for client innate_light value
in_texture,
in_helmtexture,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
EQ::TintProfile(),
0xff,
0,
0,
0,
0,
(int64)0,
(int64)0,
0,
0,
0,
0,
0,
0,
0,
0,
0,
false),
	corpse_decay_timer(RuleI(Character, CorpseDecayTimeMS)),
	corpse_rez_timer(RuleI(Character, CorpseResTimeMS)),
	corpse_delay_timer(RuleI(NPC, CorpseUnlockTimer)),
	corpse_graveyard_timer(RuleI(Zone, GraveyardTimeMS)),
	loot_cooldown_timer(10)
{

	LoadPlayerCorpseDecayTime(in_dbid);

	if (!zone->HasGraveyard() || wasAtGraveyard)
		corpse_graveyard_timer.Disable();

	is_corpse_changed = false;
	is_player_corpse = true;
	is_locked = false;
	being_looted_by = 0xFFFFFFFF;
	corpse_db_id = in_dbid;
	player_corpse_depop = false;
	char_id = in_charid;

	strcpy(corpse_name, in_charname);
	strcpy(name, in_charname);

	this->copper = in_copper;
	this->silver = in_silver;
	this->gold = in_gold;
	this->platinum = in_plat;

	rez_experience = in_rezexp;

	for (int i = 0; i < MAX_LOOTERS; i++){
		allowed_looters[i] = 0;
	}
	SetPlayerKillItemID(0);

	UpdateEquipmentLight();
	UpdateActiveLight();

	loot_request_type = LootRequestType::Forbidden;
	corpse_npctype_id = 0;
}

Corpse::~Corpse() {

	if (is_player_corpse && !(player_corpse_depop && corpse_db_id == 0)) {
		Save();
	}
	for (auto corpseItem : corpseAccessList)
	{
		for (auto corpseItemEntry : corpseItem.second)
		{
			safe_delete(corpseItemEntry);
		}
		corpseItem.second.clear();
	}
}

/*
this needs to be called AFTER the entity_id is set
the client does this too, so it's unchangable
*/
void Corpse::CalcCorpseName() {
	EntityList::RemoveNumbers(name);
	char tmp[64];
	if (is_player_corpse){
		snprintf(tmp, sizeof(tmp), "'s corpse%d", GetID());
	}
	else{
		snprintf(tmp, sizeof(tmp), "`s_corpse%d", GetID());
	}
	name[(sizeof(name) - 1) - strlen(tmp)] = 0;
	strcat(name, tmp);
}

bool Corpse::Save() {
	if (!is_player_corpse)
		return true;
	if (!is_corpse_changed)
		return true;

	uint32 tmp = this->CountItems();
	uint32 tmpsize = sizeof(PlayerCorpse_Struct) + (tmp * sizeof(player_lootitem::ServerLootItem_Struct));

	PlayerCorpse_Struct* dbpc = (PlayerCorpse_Struct*) new uchar[tmpsize];
	memset(dbpc, 0, tmpsize);
	dbpc->itemcount = tmp;
	dbpc->size = this->size;
	dbpc->locked = is_locked;
	dbpc->copper = this->copper;
	dbpc->silver = this->silver;
	dbpc->gold = this->gold;
	dbpc->plat = this->platinum;
	dbpc->race = this->race;
	dbpc->class_ = class_;
	dbpc->gender = gender;
	dbpc->deity = deity;
	dbpc->level = level;
	dbpc->texture = this->texture;
	dbpc->helmtexture = this->helmtexture;
	dbpc->exp = rez_experience;

	memcpy(&dbpc->item_tint.Slot, &item_tint.Slot, sizeof(dbpc->item_tint));
	dbpc->haircolor = haircolor;
	dbpc->beardcolor = beardcolor;
	dbpc->eyecolor2 = eyecolor1;
	dbpc->hairstyle = hairstyle;
	dbpc->face = luclinface;
	dbpc->beard = beard;
	dbpc->drakkin_heritage = drakkin_heritage;
	dbpc->drakkin_tattoo = drakkin_tattoo;
	dbpc->drakkin_details = drakkin_details;

	uint32 x = 0;

	/* Create New Corpse*/
	if (corpse_db_id == 0) {
		corpse_db_id = database.SaveCharacterCorpse(char_id, corpse_name, zone->GetZoneID(), zone->GetInstanceID(), dbpc, m_Position, consented_guild_id);
	}
	/* Update Corpse Data */
	else{
		corpse_db_id = database.UpdateCharacterCorpse(corpse_db_id, char_id, corpse_name, zone->GetZoneID(), zone->GetInstanceID(), dbpc, m_Position, consented_guild_id, IsRezzed());
	}

	safe_delete_array(dbpc);

	return true;
}

void Corpse::Delete() {
	if (IsPlayerCorpse() && corpse_db_id != 0)
		database.DeleteCharacterCorpse(corpse_db_id);

	corpse_db_id = 0;
	player_corpse_depop = true;
}

void Corpse::Bury() {
	if (IsPlayerCorpse() && corpse_db_id != 0)
		database.BuryCharacterCorpse(corpse_db_id);
	corpse_db_id = 0;
	player_corpse_depop = true;
}

void Corpse::DepopNPCCorpse() {
	if (IsNPCCorpse())
		player_corpse_depop = true;
}

void Corpse::DepopPlayerCorpse() {
	player_corpse_depop = true;
}

void Corpse::AddConsentName(std::string consent_player_name)
{
	for (const auto& consented_player_name : consented_player_names) {
		if (strcasecmp(consented_player_name.c_str(), consent_player_name.c_str()) == 0) {
			return;
		}
	}
	consented_player_names.emplace_back(consent_player_name);
}

void Corpse::RemoveConsentName(std::string consent_player_name)
{
	consented_player_names.erase(std::remove_if(consented_player_names.begin(), consented_player_names.end(),
		[consent_player_name](const std::string& consented_player_name) {
			return strcasecmp(consented_player_name.c_str(), consent_player_name.c_str()) == 0;
		}
	), consented_player_names.end());
}

uint32 Corpse::CountItems() {
	return 0;
}

void Corpse::AddItem(uint32 itemnum, uint16 charges, int16 slot, uint32 aug1, uint32 aug2, uint32 aug3, uint32 aug4, uint32 aug5, uint32 aug6, uint8 attuned) {
}

ServerLootItem_Struct* Corpse::GetItem(Client* requester, uint16 lootslot) {
	ServerLootItem_Struct* sitem = nullptr;

	if (corpseAccessList.find(requester) == corpseAccessList.end())
	{
		return sitem;
	}

	std::list<ServerLootItem_Struct*> itemlist = corpseAccessList[requester];
	std::list<ServerLootItem_Struct*>::iterator cur,end;


	cur = itemlist.begin();
	end = itemlist.end();
	for(; cur != end; ++cur) {
		if((*cur)->lootslot == lootslot) {
			sitem = *cur;
			break;
		}
	}

	return sitem;
}

void Corpse::RemoveItem(Client* c, uint16 lootslot) {
	if (lootslot == 0xFFFF)
		return;

	if (corpseAccessList.find(c) == corpseAccessList.end())
	{
		return;
	}

	std::list<ServerLootItem_Struct*>::iterator cur, end;

	cur = corpseAccessList[c].begin();
	end = corpseAccessList[c].end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if (sitem->lootslot == lootslot) {
			corpseAccessList[c].erase(cur);
			safe_delete(sitem);
			break;
		}
	}

	if (corpseAccessList[c].empty())
	{
		corpseAccessList.erase(c);
		auto app = new EQApplicationPacket;
		CreateDespawnPacket(app, !IsCorpse());
		c->QueuePacket(app);
		safe_delete(app);
	}
}

void Corpse::SetCash(uint32 in_copper, uint32 in_silver, uint32 in_gold, uint32 in_platinum) {
	this->copper = in_copper;
	this->silver = in_silver;
	this->gold = in_gold;
	this->platinum = in_platinum;
	is_corpse_changed = true;
}

void Corpse::RemoveCash() {
	this->copper = 0;
	this->silver = 0;
	this->gold = 0;
	this->platinum = 0;
	is_corpse_changed = true;
}

bool Corpse::IsEmpty(Client* clientFor) {
	if (corpseAccessList.find(clientFor) == corpseAccessList.end())
	{
		return true;
	}

	return corpseAccessList[clientFor].empty();
}

bool Corpse::IsInvolvedInKill(Client* clientFor) {
	if (corpseAccessList.find(clientFor) == corpseAccessList.end())
	{
		return false;
	}

	return true;
}

bool Corpse::IsFullyEmpty() {
	return corpseAccessList.empty();
}

bool Corpse::Process() {
	if (player_corpse_depop)
		return false;

	if (IsFullyEmpty())
	{
		return false;
	}

	if (corpse_delay_timer.Check()) {
		auto &entity_list_search = entity_list.GetClientList();

		for (auto &itr : entity_list_search) {
			Client *entity = itr.second;
			if(IsEmpty(entity) && (IsNPCCorpse() || zone->GetInstanceID() == 0))
			{
				auto app = new EQApplicationPacket;
				CreateDespawnPacket(app, !IsCorpse());
				entity->QueuePacket(app);
				safe_delete(app);
			}
		}
		
		//for (int i = 0; i < MAX_LOOTERS; i++)
		//	allowed_looters[i] = 0;
		corpse_delay_timer.Disable();
		//return true;
	}

	if (corpse_graveyard_timer.Check()) {
		if (zone->HasGraveyard()) {
			Save();
			player_corpse_depop = true;
			database.SendCharacterCorpseToGraveyard(corpse_db_id, zone->graveyard_zoneid(),
				(zone->GetZoneID() == zone->graveyard_zoneid()) ? zone->GetInstanceID() : 0, zone->GetGraveyardPoint());
			corpse_graveyard_timer.Disable();
			auto pack = new ServerPacket(ServerOP_SpawnPlayerCorpse, sizeof(SpawnPlayerCorpse_Struct));
			SpawnPlayerCorpse_Struct* spc = (SpawnPlayerCorpse_Struct*)pack->pBuffer;
			spc->player_corpse_id = corpse_db_id;
			spc->zone_id = zone->graveyard_zoneid();
			worldserver.SendPacket(pack);
			safe_delete(pack);
			LogDebug("Moved [{}] player corpse to the designated graveyard in zone [{}]", this->GetName(), database.GetZoneName(zone->graveyard_zoneid()));
			corpse_db_id = 0;
		}

		corpse_graveyard_timer.Disable();
		return false;
	}
	/*
	if(corpse_res_timer.Check()) {
		can_rez = false;
		corpse_res_timer.Disable();
	}
	*/

	/* This is when a corpse hits decay timer and does checks*/
	if (corpse_decay_timer.Check()) {
		/* NPC */
		if (IsNPCCorpse()){
			corpse_decay_timer.Disable();
			return false;
		}
		/* Client */
		if (!RuleB(Zone, EnableShadowrest)){
			Delete();
		}
		else {
			if (database.BuryCharacterCorpse(corpse_db_id)) {
				Save();
				player_corpse_depop = true;
				corpse_db_id = 0;
				LogDebug("Tagged [{}] player corpse has buried", this->GetName());
			}
			else {
				LogError("Unable to bury [{}] player corpse", this->GetName());
				return true;
			}
		}
		corpse_decay_timer.Disable();
		return false;
	}

	return true;
}

void Corpse::SetDecayTimer(uint32 decaytime) {
	if (decaytime == 0)
		corpse_decay_timer.Trigger();
	else
		corpse_decay_timer.Start(decaytime);
}

bool Corpse::CanPlayerLoot(int charid) {
	uint8 looters = 0;
	for (int i = 0; i < MAX_LOOTERS; i++) {
		if (allowed_looters[i] != 0){
			looters++;
		}

		if (allowed_looters[i] == charid)
			return true;
	}
	/* If we have no looters, obviously client can loot */
	return looters == 0;
}

void Corpse::AllowPlayerLoot(Mob *them, uint8 slot) {
	if(slot >= MAX_LOOTERS)
		return;
	if(them == nullptr || !them->IsClient())
		return;

	allowed_looters[slot] = them->CastToClient()->CharacterID();
}

void Corpse::MakeLootRequestPackets(Client* client, const EQApplicationPacket* app) {
	if (!client)
		return;

	// Added 12/08. Started compressing loot struct on live.
	if(player_corpse_depop) {
		SendLootReqErrorPacket(client, LootResponse::SomeoneElse);
		return;
	}

	if(IsPlayerCorpse() && !corpse_db_id) { // really should try to resave in this case
		// SendLootReqErrorPacket(client, 0);
		client->Message(Chat::Red, "Warning: Corpse's dbid = 0! Corpse will not survive zone shutdown!");
		std::cout << "Error: PlayerCorpse::MakeLootRequestPackets: dbid = 0!" << std::endl;
		// return;
	}

	if(is_locked && client->Admin() < 100) {
		SendLootReqErrorPacket(client, LootResponse::SomeoneElse);
		client->Message(Chat::Red, "Error: Corpse locked by GM.");
		return;
	}

	if(!being_looted_by || (being_looted_by != 0xFFFFFFFF && !entity_list.GetID(being_looted_by)))
		being_looted_by = 0xFFFFFFFF;

	if (DistanceSquaredNoZ(client->GetPosition(), m_Position) > 625) {
		SendLootReqErrorPacket(client, LootResponse::TooFar);
		return;
	}

	// all loot session disqualifiers should occur before this point as not to interfere with any current looter
	loot_request_type = LootRequestType::Forbidden;

	// loot_request_type is scoped to class Corpse and reset on a per-loot session basis
	if (client->GetGM()) {
		if (client->Admin() >= 100)
			loot_request_type = LootRequestType::GMAllowed;
		else
			loot_request_type = LootRequestType::GMPeek;
	}
	else {
		if (IsPlayerCorpse()) {
			if (char_id == client->CharacterID()) {
				loot_request_type = LootRequestType::Self;
			}
			else if (CanPlayerLoot(client->CharacterID())) {
				if (GetPlayerKillItem() == -1)
					loot_request_type = LootRequestType::AllowedPVPAll;
				else if (GetPlayerKillItem() == 1)
					loot_request_type = LootRequestType::AllowedPVPSingle;
				else if (GetPlayerKillItem() > 1)
					loot_request_type = LootRequestType::AllowedPVPDefined;
			}
		}
		else if ((IsNPCCorpse() || become_npc) && CanPlayerLoot(client->CharacterID())) {
			loot_request_type = LootRequestType::AllowedPVE;
		}

	}

	LogInventory("MakeLootRequestPackets() LootRequestType [{}] for [{}]", (int) loot_request_type, client->GetName());

	if (loot_request_type == LootRequestType::Forbidden) {
		SendLootReqErrorPacket(client, LootResponse::NotAtThisTime);
		return;
	}

	being_looted_by = client->GetID();
	client->CommonBreakInvisible(); // we should be "all good" so lets break invis now instead of earlier before all error checking is done

	// process coin
	bool loot_coin = false;
	std::string tmp;
	if (database.GetVariable("LootCoin", tmp))
		loot_coin = (tmp[0] == 1 && tmp[1] == '\0');

	if (loot_request_type == LootRequestType::GMPeek || loot_request_type == LootRequestType::GMAllowed) {
		client->Message(Chat::Yellow, "This corpse contains %u platinum, %u gold, %u silver and %u copper.",
			GetPlatinum(), GetGold(), GetSilver(), GetCopper());

		auto outapp = new EQApplicationPacket(OP_MoneyOnCorpse, sizeof(moneyOnCorpseStruct));
		moneyOnCorpseStruct* d = (moneyOnCorpseStruct*)outapp->pBuffer;

		d->response = static_cast<uint8>(LootResponse::Normal);
		d->unknown1 = 0x42;
		d->unknown2 = 0xef;

		d->copper = 0;
		d->silver = 0;
		d->gold = 0;
		d->platinum = 0;

		outapp->priority = 6;
		client->QueuePacket(outapp);

		safe_delete(outapp);
	}
	else {
		auto outapp = new EQApplicationPacket(OP_MoneyOnCorpse, sizeof(moneyOnCorpseStruct));
		moneyOnCorpseStruct* d = (moneyOnCorpseStruct*)outapp->pBuffer;

		d->response = static_cast<uint8>(LootResponse::Normal);
		d->unknown1 = 0x42;
		d->unknown2 = 0xef;

		Group* cgroup = client->GetGroup();

		// this can be reworked into a switch and/or massaged to include specialized pve loot rules based on 'LootRequestType'
		if (!IsPlayerCorpse() && client->IsGrouped() && client->AutoSplitEnabled() && cgroup) {
			d->copper = 0;
			d->silver = 0;
			d->gold = 0;
			d->platinum = 0;
			cgroup->SplitMoney(GetCopper(), GetSilver(), GetGold(), GetPlatinum(), client);
		}
		else {
			d->copper = GetCopper();
			d->silver = GetSilver();
			d->gold = GetGold();
			d->platinum = GetPlatinum();
			client->AddMoneyToPP(GetCopper(), GetSilver(), GetGold(), GetPlatinum(), false);
		}

		RemoveCash();
		Save();

		outapp->priority = 6;
		client->QueuePacket(outapp);

		safe_delete(outapp);
	}

	// process items
	auto timestamps = database.GetItemRecastTimestamps(client->CharacterID());

	if (loot_request_type == LootRequestType::AllowedPVPDefined) {
		auto pkitemid = GetPlayerKillItem();
		auto pkitem = database.GetItem(pkitemid);
		auto pkinst = database.CreateItem(&pkitem, pkitem.MaxCharges);

		if (pkinst) {
			if (pkitem.RecastDelay)
				pkinst->SetRecastTimestamp(timestamps.count(pkitem.RecastType) ? timestamps.at(pkitem.RecastType) : 0);

			LogInventory("MakeLootRequestPackets() Slot [{}], Item [{}]", EQ::invslot::CORPSE_BEGIN, pkitem.Name);

			client->SendItemPacket(EQ::invslot::CORPSE_BEGIN, pkinst, ItemPacketLoot);
			safe_delete(pkinst);
		}
		else {
			LogInventory("MakeLootRequestPackets() PlayerKillItem [{}] not found", pkitemid);

			client->Message(Chat::Red, "PlayerKillItem (id: %i) could not be found!", pkitemid);
		}

		client->QueuePacket(app);
		return;
	}

	auto loot_slot = EQ::invslot::CORPSE_BEGIN;
	auto corpse_mask = client->GetInv().GetLookup()->CorpseBitmask;
	
	if (corpseAccessList.find(client) != corpseAccessList.end())
	{
		for (auto item_data : corpseAccessList[client]) {
			// every loot session must either set all items' lootslots to 'invslot::SLOT_INVALID'
			// or to a valid enumerated client-versioned corpse slot (lootslot is not equip_slot)
			item_data->lootslot = 0xFFFF;

			// align server and client corpse slot mappings so translators can function properly
			while (loot_slot <= EQ::invslot::CORPSE_END && (((uint64)1 << loot_slot) & corpse_mask) == 0)
				++loot_slot;

			if (loot_slot > EQ::invslot::CORPSE_END)
				continue;

			if (IsPlayerCorpse()) {
				if (loot_request_type == LootRequestType::AllowedPVPSingle && loot_slot != EQ::invslot::CORPSE_BEGIN)
					continue;

				if (item_data->equip_slot < EQ::invslot::POSSESSIONS_BEGIN || item_data->equip_slot > EQ::invslot::POSSESSIONS_END)
					continue;
			}

			auto item = database.GetItem(item_data->item_id);
			auto inst = database.CreateItem(
				&item,
				item_data->charges,
				item_data->aug_1,
				item_data->aug_2,
				item_data->aug_3,
				item_data->aug_4,
				item_data->aug_5,
				item_data->aug_6,
				item_data->attuned
			);
			if (!inst)
				continue;

			if (item.RecastDelay)
				inst->SetRecastTimestamp(timestamps.count(item.RecastType) ? timestamps.at(item.RecastType) : 0);

			LogInventory("MakeLootRequestPackets() Slot [{}], Item [{}]", loot_slot, item.Name);

			client->SendItemPacket(loot_slot, inst, ItemPacketLoot);
			safe_delete(inst);

			item_data->lootslot = loot_slot++;
		}
	}

	// Disgrace: Client seems to require that we send the packet back...
	client->QueuePacket(app);

	// This is required for the 'Loot All' feature to work for SoD clients. I expect it is to tell the client that the
	// server has now sent all the items on the corpse.
	if (client->ClientVersion() >= EQ::versions::ClientVersion::SoD)
		SendLootReqErrorPacket(client, LootResponse::LootAll);
}

void Corpse::LootItem(Client *client, const EQApplicationPacket *app)
{
	if (!client)
		return;

	auto lootitem = (LootingItem_Struct *)app->pBuffer;

	LogInventory("LootItem() LootRequestType [{}], Slot [{}] for [{}]", (int) loot_request_type, lootitem->slot_id, client->GetName());

	if (!loot_cooldown_timer.Check()) {
		client->QueuePacket(app);
		SendEndLootErrorPacket(client);
		// unlock corpse for others
		if (IsBeingLootedBy(client))
			ResetLooter();
		return;
	}

	/* To prevent item loss for a player using 'Loot All' who doesn't have inventory space for all their items. */
	if (RuleB(Character, CheckCursorEmptyWhenLooting) && !client->GetInv().CursorEmpty()) {
		client->Message(Chat::Red, "You may not loot an item while you have an item on your cursor.");
		client->QueuePacket(app);
		SendEndLootErrorPacket(client);
		/* Unlock corpse for others */
		if (IsBeingLootedBy(client))
			ResetLooter();
		return;
	}

	if (IsPlayerCorpse() && !CanPlayerLoot(client->CharacterID()) && !become_npc &&
		(char_id != client->CharacterID() && client->Admin() < 150)) {
		client->Message(Chat::Red, "Error: This is a player corpse and you dont own it.");
		client->QueuePacket(app);
		SendEndLootErrorPacket(client);
		return;
	}

	if (is_locked && client->Admin() < 100) {
		client->QueuePacket(app);
		SendEndLootErrorPacket(client);
		client->Message(Chat::Red, "Error: Corpse locked by GM.");
		return;
	}

	if (!CanPlayerLoot(client->CharacterID())) {
		client->QueuePacket(app);
		SendEndLootErrorPacket(client);
		return;
	}
	EQ::ItemData item;
	memset(&item, 0, sizeof(EQ::ItemData));
	EQ::ItemInstance *inst = nullptr;
	ServerLootItem_Struct* item_data = GetItem(client, lootitem->slot_id);
	if (item_data)
	{
		item = database.GetItem(item_data->item_id);
		if (item.ID != 0) {
			if (item_data->item_id) {
				inst = database.CreateItem(&item, item_data->charges, item_data->aug_1,
					item_data->aug_2, item_data->aug_3, item_data->aug_4,
					item_data->aug_5, item_data->aug_6, item_data->attuned);
			}
			else {
				inst = database.CreateItem(&item);
			}
		}
	}

	if (client && inst && item.ID != 0) {
		if (client->CheckLoreConflict(&item)) {
			client->MessageString(Chat::White, LOOT_LORE_ERROR);
			client->QueuePacket(app);
			SendEndLootErrorPacket(client);
			ResetLooter();
			delete inst;
			return;
		}

		if (inst->IsAugmented()) {
			for (int i = EQ::invaug::SOCKET_BEGIN; i <= EQ::invaug::SOCKET_END; i++) {
				EQ::ItemInstance *itm = inst->GetAugment(i);
				if (itm) {
					if (client->CheckLoreConflict(itm->GetItem())) {
						client->MessageString(Chat::White, LOOT_LORE_ERROR);
						client->QueuePacket(app);
						SendEndLootErrorPacket(client);
						ResetLooter();
						delete inst;
						return;
					}
				}
			}
		}

		char buf[88];
		char q_corpse_name[64];
		strcpy(q_corpse_name, corpse_name);
		snprintf(buf, 87, "%d %d %s", inst->GetItem()->ID, inst->GetCharges(),
			EntityList::RemoveNumbers(q_corpse_name));
		buf[87] = '\0';
		std::vector<EQ::Any> args;
		args.push_back(inst);
		args.push_back(this);
		if (parse->EventPlayer(EVENT_LOOT, client, buf, 0, &args) != 0) {
			lootitem->auto_loot = -1;
			client->MessageString(Chat::Red, LOOT_NOT_ALLOWED, inst->GetItem()->Name);
			client->QueuePacket(app);
			delete inst;
			return;
		}


		// do we want this to have a fail option too?
		parse->EventItem(EVENT_LOOT, client, inst, this, buf, 0);

		// safe to ACK now
		client->QueuePacket(app);

		if (!IsPlayerCorpse() && RuleB(Character, EnableDiscoveredItems)) {
			if (client && !client->GetGM() && !client->IsDiscovered(inst->GetItem()->ID))
				client->DiscoverItem(inst->GetItem()->ID);
		}

		if (zone->adv_data) {
			ServerZoneAdventureDataReply_Struct *ad = (ServerZoneAdventureDataReply_Struct *)zone->adv_data;
			if (ad->type == Adventure_Collect && !IsPlayerCorpse()) {
				if (ad->data_id == inst->GetItem()->ID) {
					zone->DoAdventureCountIncrease();
				}
			}
		}

		/* First add it to the looter - this will do the bag contents too */
		if (lootitem->auto_loot > 0) {
			if (!client->AutoPutLootInInventory(*inst, true, true))
				client->PutLootInInventory(EQ::invslot::slotCursor, *inst);
		}
		else {
			client->PutLootInInventory(EQ::invslot::slotCursor, *inst);
		}

		/* Update any tasks that have an activity to loot this item */
		if (RuleB(TaskSystem, EnableTaskSystem))
			client->UpdateTasksForItem(ActivityLoot, item.ID);

		/* Remove it from Corpse */
		if (item_data->item_id) {
			/* Delete Item Instance */
			RemoveItem(client, item_data->lootslot);
		}

		/* Send message with item link to groups and such */
		EQ::SayLinkEngine linker;
		linker.SetLinkType(EQ::saylink::SayLinkItemInst);
		linker.SetItemInst(inst);

		linker.GenerateLink();

		client->MessageString(Chat::Loot, LOOTED_MESSAGE, linker.Link().c_str());

		if (!IsPlayerCorpse()) {
			Group *g = client->GetGroup();
			if (g != nullptr) {
				g->GroupMessageString(client, Chat::Loot, OTHER_LOOTED_MESSAGE,
					client->GetName(), linker.Link().c_str());
			}
			else {
				Raid *r = client->GetRaid();
				if (r != nullptr) {
					r->RaidMessageString(client, Chat::Loot, OTHER_LOOTED_MESSAGE,
						client->GetName(), linker.Link().c_str());
				}
			}
		}
	}
	else {
		client->QueuePacket(app);
		SendEndLootErrorPacket(client);
		safe_delete(inst);
		return;
	}

	safe_delete(inst);
}

void Corpse::EndLoot(Client* client, const EQApplicationPacket* app) {
	auto outapp = new EQApplicationPacket;
	outapp->SetOpcode(OP_LootComplete);
	outapp->size = 0;
	client->QueuePacket(outapp);
	safe_delete(outapp);
    if ( client->GetID() == being_looted_by )
    {
		this->being_looted_by = 0xFFFFFFFF;
		if (IsFullyEmpty())
			Delete();
		else
			Save();
	}
}

void Corpse::FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho) {
	Mob::FillSpawnStruct(ns, ForWho);

	ns->spawn.max_hp = 120;
	ns->spawn.NPC = 2;

	UpdateActiveLight();
	ns->spawn.light = m_Light.Type[EQ::lightsource::LightActive];
}

void Corpse::QueryLoot(Client* to) {
}

bool Corpse::Summon(Client* client, bool spell, bool CheckDistance) {
	uint32 dist2 = 10000; // pow(100, 2);
	if (!spell) {
		if (this->GetCharID() == client->CharacterID()) {
			if (IsLocked() && client->Admin() < 100) {
				client->Message(Chat::Red, "That corpse is locked by a GM.");
				return false;
			}
			if (!CheckDistance || (DistanceSquaredNoZ(m_Position, client->GetPosition()) <= dist2)) {
				GMMove(client->GetX(), client->GetY(), client->GetZ());
				is_corpse_changed = true;
			}
			else {
				client->MessageString(Chat::Red, CORPSE_TOO_FAR);
				return false;
			}
		}
		else
		{
			bool consented = false;
			for (const auto& consented_player_name : consented_player_names) {
				if (strcasecmp(client->GetName(), consented_player_name.c_str()) == 0) {
					consented = true;
					break;
				}
			}

			if (!consented && consented_guild_id && consented_guild_id != GUILD_NONE) {
				if (client->GuildID() == consented_guild_id) {
					consented = true;
				}
			}
			if (!consented && consented_group_id) {
				Group* grp = client->GetGroup();
				if (grp && grp->GetID() == consented_group_id) {
					consented = true;
				}
			}
			if (!consented && consented_raid_id) {
				Raid* raid = client->GetRaid();
				if (raid && raid->GetID() == consented_raid_id) {
					consented = true;
				}
			}

			if (consented) {
				if (!CheckDistance || (DistanceSquaredNoZ(m_Position, client->GetPosition()) <= dist2)) {
					GMMove(client->GetX(), client->GetY(), client->GetZ());
					is_corpse_changed = true;
				}
				else {
					client->MessageString(Chat::Red, CORPSE_TOO_FAR);
					return false;
				}
			}
			else {
				client->MessageString(Chat::Red, CONSENT_DENIED);
				return false;
			}
		}
	}
	else {
		GMMove(client->GetX(), client->GetY(), client->GetZ());
		is_corpse_changed = true;
	}
	Save();
	return true;
}

void Corpse::CompleteResurrection(){
	rez_experience = 0;
	is_corpse_changed = true;
	this->Save();
}

void Corpse::Spawn() {
	auto app = new EQApplicationPacket;
	CreateSpawnPacket(app, this);
	entity_list.QueueCorpseClients(this, app);
	safe_delete(app);
}

uint32 Corpse::GetEquippedItemFromTextureSlot(uint8 material_slot) const {

	return 0;
}

uint32 Corpse::GetEquipmentColor(uint8 material_slot) const {
	EQ::ItemData item;

	if (material_slot > EQ::textures::LastTexture) {
		return 0;
	}

	item = database.GetItem(GetEquippedItemFromTextureSlot(material_slot));
	if(item.ID != 0) {
		return (item_tint.Slot[material_slot].UseTint ? item_tint.Slot[material_slot].Color : item.Color);
	}

	return 0;
}

void Corpse::UpdateEquipmentLight()
{
}

void Corpse::AddLooter(Mob* who) {
	for (int i = 0; i < MAX_LOOTERS; i++) {
		if (allowed_looters[i] == 0) {
			allowed_looters[i] = who->CastToClient()->CharacterID();
			break;
		}
	}
}

void Corpse::LoadPlayerCorpseDecayTime(uint32 corpse_db_id){
	if(!corpse_db_id)
		return;

	uint32 active_corpse_decay_timer = database.GetCharacterCorpseDecayTimer(corpse_db_id);
	if (active_corpse_decay_timer > 0 && RuleI(Character, CorpseDecayTimeMS) > (active_corpse_decay_timer * 1000)) {
		corpse_decay_timer.SetTimer(RuleI(Character, CorpseDecayTimeMS) - (active_corpse_decay_timer * 1000));
	}
	else {
		corpse_decay_timer.SetTimer(2000);
	}
	if (active_corpse_decay_timer > 0 && RuleI(Zone, GraveyardTimeMS) > (active_corpse_decay_timer * 1000)) {
		corpse_graveyard_timer.SetTimer(RuleI(Zone, GraveyardTimeMS) - (active_corpse_decay_timer * 1000));
	}
	else {
		corpse_graveyard_timer.SetTimer(3000);
	}
}
