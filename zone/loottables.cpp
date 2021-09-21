 /*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2002 EQEMu Development Team (http://eqemu.org)

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

#include "../common/global_define.h"
#include "../common/loottable.h"
#include "../common/misc_functions.h"
#include "../common/data_verification.h"

#include "client.h"
#include "entity.h"
#include "mob.h"
#include "npc.h"
#include "zonedb.h"
#include "zone_store.h"
#include "global_loot_manager.h"
#include "../common/repositories/criteria/content_filter_criteria.h"
#include "../common/say_link.h"

#include <iostream>
#include <stdlib.h>

#ifdef _WINDOWS
#define snprintf	_snprintf
#endif

void ZoneDatabase::GenerateMoney(uint32 loottable_id, uint32* copper, uint32* silver, uint32* gold, uint32* plat) {

	const LootTable_Struct* lts = database.GetLootTable(loottable_id);
	if (!lts)
		return;

	uint32 min_cash = lts->mincash;
	uint32 max_cash = lts->maxcash;
	if (min_cash > max_cash) {
		uint32 t = min_cash;
		min_cash = max_cash;
		max_cash = t;
	}

	uint32 cash = 0;
	if (max_cash > 0 && lts->avgcoin > 0 && EQ::ValueWithin(lts->avgcoin, min_cash, max_cash)) {
		float upper_chance = (float)(lts->avgcoin - min_cash) / (float)(max_cash - min_cash);
		float avg_cash_roll = (float)zone->random.Real(0.0, 1.0);

		if (avg_cash_roll < upper_chance) {
			cash = zone->random.Int(lts->avgcoin, max_cash);
		}
		else {
			cash = zone->random.Int(min_cash, lts->avgcoin);
		}
	}
	else {
		cash = zone->random.Int(min_cash, max_cash);
	}
	if (cash != 0) {
		*plat = cash / 1000;
		cash -= *plat * 1000;

		*gold = cash / 100;
		cash -= *gold * 100;

		*silver = cash / 10;
		cash -= *silver * 10;

		*copper = cash;
	}
}	

void ZoneDatabase::GenerateLootTableList_Legacy(uint32 loottable_id, std::list<ServerLootItem_Struct*>& itemlist) {
	const LootTable_Struct* lts = database.GetLootTable(loottable_id);
	if (!lts)
		return;

	if (RuleB(Zone, GlobalLootForceAllDropsOverride))
	{
		GenerateFullLootTableList(loottable_id, itemlist);
		return;
	}
	uint32 global_loot_multiplier = RuleI(Zone, GlobalLootMultiplier);
	int32 override_probability = RuleI(Zone, GlobalLootProbabilityOverride);

	for (uint32 i = 0; i < lts->NumEntries; i++) {
		uint8 multiplier_count = 0;
		for (uint32 k = 1; k <= (lts->Entries[i].multiplier * global_loot_multiplier); k++) {
			uint8 droplimit = lts->Entries[i].droplimit;
			uint8 mindrop = lts->Entries[i].mindrop;
			uint8 multiplier_min = lts->Entries[i].multiplier_min;

			//LootTable Entry probability
			float ltchance = 0.0f;

			ltchance = lts->Entries[i].probability;

			if (override_probability > 0)
			{
				ltchance = override_probability;
			}

			float drop_chance = 0.0f;
			if (ltchance > 0.0 && ltchance < 100.0 && multiplier_count >= multiplier_min) {
				drop_chance = (float)zone->random.Real(0.0, 100.0);
			}
			else if (multiplier_count < multiplier_min)
			{
				drop_chance = 0.0f;
			}

			if (ltchance != 0.0 && (ltchance == 100.0 || drop_chance <= ltchance)) {
				GenerateLootDropList_Legacy(itemlist, lts->Entries[i].lootdrop_id, droplimit, mindrop);
			}

			++multiplier_count;
		}
	}
}


void ZoneDatabase::DisplayFullLootTableList(Client* c, uint32 loottable_id) {
	const LootTable_Struct* lts = database.GetLootTable(loottable_id);
	if (!lts)
		return;

	uint32 global_loot_multiplier = RuleI(Zone, GlobalLootMultiplier);

	for (uint32 i = 0; i < lts->NumEntries; i++) {
		uint8 droplimit = lts->Entries[i].droplimit;
		uint8 mindrop = lts->Entries[i].mindrop;
		uint8 multiplier_min = lts->Entries[i].multiplier_min;
		c->Message(
			0,
			"| -- LootDrop: (%i), Droplimit: (%i), Mindrop: (%i), Multiplier: (%i), Multiplier_Min: (%i),  Probability: (%.3f)",
			lts->Entries[i].lootdrop_id,
			lts->Entries[i].droplimit,
			lts->Entries[i].mindrop,
			lts->Entries[i].multiplier,
			lts->Entries[i].multiplier_min,
			lts->Entries[i].probability
		);
		DisplayFullLootDropList(c, lts->Entries[i].lootdrop_id);
	}
}

void ZoneDatabase::GenerateFullLootTableList(uint32 loottable_id, std::list<ServerLootItem_Struct*>& itemlist) {
	const LootTable_Struct* lts = database.GetLootTable(loottable_id);
	if (!lts)
		return;

	uint32 global_loot_multiplier = RuleI(Zone, GlobalLootMultiplier);

	for (uint32 i = 0; i < lts->NumEntries; i++) {
		uint8 droplimit = lts->Entries[i].droplimit;
		uint8 mindrop = lts->Entries[i].mindrop;
		uint8 multiplier_min = lts->Entries[i].multiplier_min;

		GenerateFullLootDropList(itemlist, lts->Entries[i].lootdrop_id, droplimit, mindrop);
	}
}

// Called by AddLootTableToNPC
// maxdrops = size of the array npcd
void ZoneDatabase::GenerateLootDropList_Legacy(std::list<ServerLootItem_Struct*>& item_in, uint32 lootdrop_id, uint8 droplimit, uint8 mindrop) {

	const LootDrop_Struct* lds = GetLootDrop(lootdrop_id);
	if (!lds) {
		return;
	}

	if (lds->NumEntries == 0)
		return;

	if (droplimit == 0 && mindrop == 0) {
		for (uint32 i = 0; i < lds->NumEntries; ++i) {
			int charges = lds->Entries[i].multiplier;
			for (int j = 0; j < charges; ++j) {
				if (zone->random.Real(0.0, 100.0) <= lds->Entries[i].chance) {
					EQ::ItemData dbitem = GetItem(lds->Entries[i].item_id);
					if (dbitem.ID == 0)
						continue;

					ServerLootItem_Struct* itemLoot = new ServerLootItem_Struct;
					memset(itemLoot, 0, sizeof(ServerLootItem_Struct));
					itemLoot->charges = lds->Entries[i].item_charges;
					itemLoot->item_id = dbitem.ID;
					item_in.push_back(itemLoot);
				}
			}
		}
		return;
	}

	if (lds->NumEntries > 100 && droplimit == 0) {
		droplimit = 10;
	}

	if (droplimit < mindrop) {
		droplimit = mindrop;
	}

	float roll_t = 0.0f;
	float roll_t_min = 0.0f;
	bool active_item_list = false;
	for (uint32 i = 0; i < lds->NumEntries; ++i) {
		EQ::ItemData db_item = GetItem(lds->Entries[i].item_id);
		if (db_item.ID) {
			roll_t += lds->Entries[i].chance;
			active_item_list = true;
		}
	}

	roll_t_min = roll_t;
	roll_t = EQ::ClampLower(roll_t, 100.0f);

	if (!active_item_list) {
		return;
	}

	for (int i = 0; i < mindrop; ++i) {
		float roll = (float)zone->random.Real(0.0, roll_t_min);
		for (uint32 j = 0; j < lds->NumEntries; ++j) {
			EQ::ItemData db_item = GetItem(lds->Entries[j].item_id);
			if (db_item.ID) {
				if (roll < lds->Entries[j].chance) {
					bool force_equip = lds->Entries[i].equip_item == 2;

					EQ::ItemData dbitem = GetItem(lds->Entries[i].item_id);
					if (dbitem.ID == 0)
						continue;

					ServerLootItem_Struct* itemLoot = new ServerLootItem_Struct;
					itemLoot->charges = lds->Entries[i].item_charges;
					itemLoot->item_id = dbitem.ID;
					item_in.push_back(itemLoot);

					int charges = (int)lds->Entries[i].multiplier;
					charges = EQ::ClampLower(charges, 1);

					for (int k = 1; k < charges; ++k) {
						bool force_equip = lds->Entries[i].equip_item == 2;
						float c_roll = (float)zone->random.Real(0.0, 100.0);
						if (c_roll <= lds->Entries[i].chance) {
							ServerLootItem_Struct* itemLoot = new ServerLootItem_Struct;
							memset(itemLoot, 0, sizeof(ServerLootItem_Struct));
							itemLoot->charges = lds->Entries[i].item_charges;
							itemLoot->item_id = dbitem.ID;
							item_in.push_back(itemLoot);
						}
					}

					j = lds->NumEntries;
					break;
				}
				else {
					roll -= lds->Entries[j].chance;
				}
			}
		}
	}

	if ((droplimit - mindrop) == 1)
	{
		float roll = (float)zone->random.Real(0.0, roll_t);
		for (uint32 j = 0; j < lds->NumEntries; ++j) {
			EQ::ItemData db_item = GetItem(lds->Entries[j].item_id);
			if (db_item.ID) {
				if (roll < lds->Entries[j].chance) {
					bool force_equip = lds->Entries[j].equip_item == 2;

					EQ::ItemData dbitem = GetItem(lds->Entries[j].item_id);
					if (dbitem.ID == 0)
						continue;

					int charges = (int)lds->Entries[j].multiplier;
					charges = EQ::ClampLower(charges, 1);

					for (int k = 1; k < charges; ++k) {
						float c_roll = (float)zone->random.Real(0.0, 100.0);
						if (c_roll <= lds->Entries[j].chance) {
							ServerLootItem_Struct* itemLoot = new ServerLootItem_Struct;
							memset(itemLoot, 0, sizeof(ServerLootItem_Struct));
							itemLoot->charges = lds->Entries[j].item_charges;
							itemLoot->item_id = dbitem.ID;
							item_in.push_back(itemLoot);
						}
					}

					j = lds->NumEntries;
					break;
				}
				else {
					roll -= lds->Entries[j].chance;
				}
			}
		}
	}

	else if (droplimit > mindrop)
	{
		// If droplimit is 2 or more higher than mindrop, then we run this other loop.
		// This can't be used on droplimit=mindrop+1 without actual drop rates being lower than what the table %s
		// would indicate; however the above solution doesn't work well for droplimits greater than 1 above mindrop.
		// This will not be as precise as the above loop but the deviation is greatly reduced as droplimit increases.
		int dropCount = mindrop;
		int i = zone->random.Int(0, lds->NumEntries);
		int loops = 0;

		while (loops < lds->NumEntries)
		{
			if (dropCount >= droplimit)
				break;

			uint32 itemid = lds->Entries[i].item_id;
			const EQ::ItemData db_item = GetItem(itemid);
			int8 charges = lds->Entries[i].item_charges;
			if (db_item.ID)
			{
				if (zone->random.Real(0.0, 100.0) <= lds->Entries[i].chance)
				{
					bool force_equip = lds->Entries[i].equip_item == 2;

					int multiplier = (int)lds->Entries[i].multiplier;
					multiplier = EQ::ClampLower(multiplier, 1);

					for (int k = 1; k < multiplier; ++k) {
						float c_roll = (float)zone->random.Real(0.0, 100.0);
						if (c_roll <= lds->Entries[i].chance) {
							bool force_equip = lds->Entries[i].equip_item == 2;
							ServerLootItem_Struct* itemLoot = new ServerLootItem_Struct;
							memset(itemLoot, 0, sizeof(ServerLootItem_Struct));
							itemLoot->charges = lds->Entries[i].item_charges;
							itemLoot->item_id = db_item.ID;
							item_in.push_back(itemLoot);
						}
					}
					dropCount = dropCount + 1;
				}
			}
			i++;
			if (i > lds->NumEntries)
				i = 0;
			loops++;
		}
	}

	//npc->UpdateEquipmentLight();
	// no wearchange associated with this function..so, this should not be needed
	//if (npc->UpdateActiveLightValue())
	//	npc->SendAppearancePacket(AT_Light, npc->GetActiveLightValue());
}

void ZoneDatabase::GenerateLootTableList(uint32 loottable_id, std::list<ServerLootItem_Struct*>& itemlist, bool lootdebug) {
	const LootTable_Struct* lts = database.GetLootTable(loottable_id);
	if (!lts)
		return;

	if (RuleB(Zone, GlobalLootForceAllDropsOverride) || lootdebug)
	{
		GenerateFullLootTableList(loottable_id, itemlist);
		return;
	}
	uint32 global_loot_multiplier = RuleI(Zone, GlobalLootMultiplier);
	int32 override_probability = RuleI(Zone, GlobalLootProbabilityOverride);

	for (uint32 i = 0; i < lts->NumEntries; i++) {
		const LootDrop_Struct* lds = GetLootDrop(lts->Entries[i].lootdrop_id);
		if (!lds || lds->NumEntries == 0) {
			continue;
		}
		uint8 multiplier = lts->Entries[i].multiplier > 0 ? lts->Entries[i].multiplier : 1;
		uint8 droplimit = lts->Entries[i].droplimit < lts->Entries[i].mindrop ? lts->Entries[i].mindrop : lts->Entries[i].droplimit;
		uint8 mindrop = lts->Entries[i].mindrop;
		uint8 multiplier_min = lts->Entries[i].multiplier_min;
		uint8 probability = lts->Entries[i].probability;
		float ltchance =  override_probability > 0 ? override_probability : probability;

		int RollTableRange = 0;	
		float RollTableProb = 1.0f;	
		bool RollTableChanceBypass = false;
		for (uint32 i = 0; i < lds->NumEntries; ++i) {
			EQ::ItemData dbitem = GetItem(lds->Entries[i].item_id);
			RollTableRange += dbitem.ID == lds->Entries[i].item_id ? (lds->Entries[i].chance < 10 ? 10 : lds->Entries[i].chance) * (lds->Entries[i].multiplier > 0 ? lds->Entries[i].multiplier : 1) : 0;
			if(lds->Entries[i].chance >= 100){
				RollTableChanceBypass = true;
			}
			else
			{
				RollTableProb *= lds->Entries[i].item_id ? pow((100-(lds->Entries[i].chance < 10 ? 10 : lds->Entries[i].chance))/100.0f, (float)(lds->Entries[i].multiplier > 0 ? lds->Entries[i].multiplier : 1)) : 1.0f;
			}
		}

		for (uint32 k = 1; k <= (multiplier * global_loot_multiplier); k++) {
			if( k <= multiplier_min || probability >= 100 || zone->random.Real(0.0, 100.0) < ltchance ) {
				uint32 Drops = 0;
				
				if(droplimit <= 0) {
					for (uint32 i = 0; i < lds->NumEntries; ++i) {
						int charges = lds->Entries[i].multiplier;
						for (int j = 0; j < charges; ++j) {
							if (zone->random.Real(0.0, 100.0) <= lds->Entries[i].chance) {
								EQ::ItemData dbitem = GetItem(lds->Entries[i].item_id);
								if (dbitem.ID != lds->Entries[i].item_id)
									continue;

								ServerLootItem_Struct* itemLoot = new ServerLootItem_Struct;
								memset(itemLoot, 0, sizeof(ServerLootItem_Struct));
								itemLoot->charges = lds->Entries[i].item_charges;
								itemLoot->item_id = dbitem.ID;
								itemlist.push_back(itemLoot);
								Drops++;
							}
						}
					}
				}

				for(uint32 i = Drops; i < droplimit; i++)		
				{
					if (Drops > mindrop && !RollTableChanceBypass && zone->random.Real(0.0, 1.0) < RollTableProb)
						continue;
						
					int Roll = zone->random.Real(0, RollTableRange);

					for (uint32 i = 0; i < lds->NumEntries; ++i) {
						EQ::ItemData dbitem = GetItem(lds->Entries[i].item_id);
						int Chance = lds->Entries[i].item_id ? (lds->Entries[i].chance < 10 ? 10 : lds->Entries[i].chance) * (lds->Entries[i].multiplier > 0 ? lds->Entries[i].multiplier : 1) : 0;
						if(Roll > Chance){ Roll -= Chance; }
						else if (Roll <= Chance ) {
							ServerLootItem_Struct* itemLoot = new ServerLootItem_Struct;
							memset(itemLoot, 0, sizeof(ServerLootItem_Struct));
							itemLoot->charges = lds->Entries[i].item_charges;
							itemLoot->item_id = dbitem.ID;
							itemlist.push_back(itemLoot);
							break;
						}
					}
					Drops++;
				}
			}
		}
	}
}

// Called by AddLootTableToNPC
// maxdrops = size of the array npcd
void ZoneDatabase::GenerateFullLootDropList(std::list<ServerLootItem_Struct*>& item_in, uint32 lootdrop_id, uint8 droplimit, uint8 mindrop) {

	const LootDrop_Struct* lds = GetLootDrop(lootdrop_id);
	if (!lds) {
		return;
	}

	if (lds->NumEntries == 0)
		return;

	//if (droplimit == 0 && mindrop == 0) {
		for (uint32 i = 0; i < lds->NumEntries; ++i) {
			//int charges = lds->Entries[i].multiplier;
			//for (int j = 0; j < charges; ++j) {
			//if (zone->random.Real(0.0, 100.0) <= lds->Entries[i].chance) {
				EQ::ItemData dbitem = GetItem(lds->Entries[i].item_id);
				if (dbitem.ID == 0)
					continue;

				ServerLootItem_Struct* itemLoot = new ServerLootItem_Struct;
				memset(itemLoot, 0, sizeof(ServerLootItem_Struct));
				itemLoot->charges = lds->Entries[i].item_charges;
				itemLoot->item_id = dbitem.ID;
				item_in.push_back(itemLoot);
				//}
				//}
		}
		return;
}

void ZoneDatabase::DisplayFullLootDropList(Client* c, uint32 lootdrop_id) {

	const LootDrop_Struct* lds = GetLootDrop(lootdrop_id);
	if (!lds) {
		return;
	}

	if (lds->NumEntries == 0)
		return;

	//if (droplimit == 0 && mindrop == 0) {
	for (uint32 i = 0; i < lds->NumEntries; ++i) {
		//int charges = lds->Entries[i].multiplier;
		//for (int j = 0; j < charges; ++j) {
		//if (zone->random.Real(0.0, 100.0) <= lds->Entries[i].chance) {
		EQ::ItemData dbitem = GetItem(lds->Entries[i].item_id);
		if (dbitem.ID == 0)
			continue;

		EQ::SayLinkEngine linker;
		linker.SetLinkType(EQ::saylink::SayLinkItemData);
		linker.SetItemData(dbitem);

		c->Message(0, "| -- Item: %i - (%s), Charges: (%i), Chance: (%i), Will Equip?: (%s), Multiplier: (%i),  Min/Max Level: (%i / %i)",
			lds->Entries[i].item_id,
			linker.GenerateLink().c_str(),
			lds->Entries[i].item_charges,
			lds->Entries[i].chance,
			lds->Entries[i].equip_item ? "true" : "false",
			lds->Entries[i].multiplier,
			lds->Entries[i].minlevel,
			lds->Entries[i].maxlevel);
		//}
		//}
	}
	return;
}

// Queries the loottable: adds item & coin to the npc
void ZoneDatabase::AddLootTableToNPC(NPC* npc,uint32 loottable_id, ItemList* itemlist, uint32* copper, uint32* silver, uint32* gold, uint32* plat) {
	const LootTable_Struct* lts = nullptr;
	// global loot passes nullptr for these
	bool bGlobal = copper == nullptr && silver == nullptr && gold == nullptr && plat == nullptr;
	if (!bGlobal) {
		*copper = 0;
		*silver = 0;
		*gold = 0;
		*plat = 0;
	}

	lts = database.GetLootTable(loottable_id);
	if (!lts)
		return;

	uint32 min_cash = lts->mincash;
	uint32 max_cash = lts->maxcash;
	if(min_cash > max_cash) {
		uint32 t = min_cash;
		min_cash = max_cash;
		max_cash = t;
	}

	uint32 cash = 0;
	if (!bGlobal) {
		if(max_cash > 0 && lts->avgcoin > 0 && EQ::ValueWithin(lts->avgcoin, min_cash, max_cash)) {
			float upper_chance = (float)(lts->avgcoin - min_cash) / (float)(max_cash - min_cash);
			float avg_cash_roll = (float)zone->random.Real(0.0, 1.0);

			if(avg_cash_roll < upper_chance) {
				cash = zone->random.Int(lts->avgcoin, max_cash);
			} else {
				cash = zone->random.Int(min_cash, lts->avgcoin);
			}
		} else {
			cash = zone->random.Int(min_cash, max_cash);
		}
	}

	if(cash != 0) {
		*plat = cash / 1000;
		cash -= *plat * 1000;

		*gold = cash / 100;
		cash -= *gold * 100;

		*silver = cash / 10;
		cash -= *silver * 10;

		*copper = cash;
	}

	uint32 global_loot_multiplier = RuleI(Zone, GlobalLootMultiplier);



	//float difficultymultiplier = zone->newzone_data.NPCZoneDifficulty + npc->GetNPCScaleTier() + (npc->GetNPCScaleDifficulty() / 100);

	//difficultymultiplier *= 0.2f;

	//do{
	// Do items
		for (uint32 i=0; i<lts->NumEntries; i++) {
		uint8 multiplier_count = 0;
			for (uint32 k = 1; k <= (lts->Entries[i].multiplier * global_loot_multiplier); k++) {
				uint8 droplimit = lts->Entries[i].droplimit;
				uint8 mindrop = lts->Entries[i].mindrop;
			uint8 multiplier_min = lts->Entries[i].multiplier_min;

				//LootTable Entry probability
				float ltchance = 0.0f;

				ltchance = lts->Entries[i].probability;

				float drop_chance = 0.0f;
			if (ltchance > 0.0 && ltchance < 100.0 && multiplier_count >= multiplier_min) {
					drop_chance = (float)zone->random.Real(0.0, 100.0);
				}
			else if (multiplier_count < multiplier_min)
			{
				drop_chance = 0.0f;
			}

				if (ltchance != 0.0 && (ltchance == 100.0 || drop_chance <= ltchance)) {
					AddLootDropToNPC(npc, lts->Entries[i].lootdrop_id, itemlist, droplimit, mindrop);
				}

			++multiplier_count;
			}
		}
		//difficultymultiplier -= 1;
		//if(difficultymultiplier > 0 && difficultymultiplier < 1)
		//{
		//	if(zone->random.Roll(difficultymultiplier))
		//	{
		//		difficultymultiplier = 1;
		//	}
		//	else
		//	{
		//		difficultymultiplier = 0;
		//	}
		//}
	//} while (difficultymultiplier > 0);
}

void ZoneDatabase::AddLootDropToNPC(NPC* npc,uint32 lootdrop_id, ItemList* itemlist, uint8 droplimit, uint8 mindrop) {
	const LootDrop_Struct* lds = GetLootDrop(lootdrop_id);
	if (!lds) {
		return;
	}

	if(lds->NumEntries == 0)
		return;

	if(droplimit == 0 && mindrop == 0) {
		for(uint32 i = 0; i < lds->NumEntries; ++i) {
			int charges = lds->Entries[i].multiplier;
			for(int j = 0; j < charges; ++j) {
				if(zone->random.Real(0.0, 100.0) <= lds->Entries[i].chance) {
					const EQ::ItemData *dbitem = GetItem(lds->Entries[i].item_id);
					if(!database_item)
						continue;
					bool force_equip = lds->Entries[i].equip_item == 2;
					npc->AddLootDrop(dbitem, itemlist, lds->Entries[i].item_charges, lds->Entries[i].minlevel,
									lds->Entries[i].maxlevel, lds->Entries[i].equip_item > 0 ? true : false, false, false, false, force_equip);
				}
			}
		}
		return;
	}

	if(lds->NumEntries > 100 && droplimit == 0) {
		droplimit = 10;
	}

	if(droplimit < mindrop) {
		droplimit = mindrop;
	}

	float roll_t = 0.0f;
	float roll_t_min = 0.0f;
	bool active_item_list = false;
	for(uint32 i = 0; i < lds->NumEntries; ++i) {
		const EQ::ItemData *db_item = GetItem(lds->Entries[i].item_id);
		if(db_item.ID) {
			roll_t += lds->Entries[i].chance;
			active_item_list = true;
		}
	}

	roll_t_min = roll_t;
	roll_t = EQ::ClampLower(roll_t, 100.0f);

	if(!active_item_list) {
		return;
	}

	for(int i = 0; i < mindrop; ++i) {
		float roll = (float)zone->random.Real(0.0, roll_t_min);
		for (uint32 j    = 0; j < loot_drop->NumEntries; ++j) {
			const EQ::ItemData *db_item = GetItem(loot_drop->Entries[j].item_id);
			if(db_item) {
				if(roll < lds->Entries[j].chance) {
					bool force_equip = lds->Entries[i].equip_item == 2;
					npc->AddLootDrop(db_item, itemlist, lds->Entries[j].item_charges, lds->Entries[j].minlevel,
									 lds->Entries[j].maxlevel, lds->Entries[j].equip_item > 0 ? true : false, false, false, false, force_equip);

					int charges = (int)lds->Entries[i].multiplier;
					charges = EQ::ClampLower(charges, 1);

					for(int k = 1; k < charges; ++k) {
						bool force_equip = lds->Entries[i].equip_item == 2;
						float c_roll = (float)zone->random.Real(0.0, 100.0);
						if(c_roll <= lds->Entries[i].chance) {
							npc->AddLootDrop(db_item, itemlist, lds->Entries[j].item_charges, lds->Entries[j].minlevel,
											 lds->Entries[j].maxlevel, lds->Entries[j].equip_item > 0 ? true : false, false, false, false, force_equip);
						}
					}

					j = lds->NumEntries;
					break;
				}
				else {
					roll -= lds->Entries[j].chance;
				}
			}
		}
	}

	for(int i = mindrop; i < droplimit; ++i) {
		float roll = (float)zone->random.Real(0.0, roll_t);
		for(uint32 j = 0; j < lds->NumEntries; ++j) {
			const EQ::ItemData* db_item = GetItem(lds->Entries[j].item_id);
			if(db_item) {
				if(roll < lds->Entries[j].chance) {
					bool force_equip = lds->Entries[i].equip_item == 2;
					npc->AddLootDrop(db_item, itemlist, lds->Entries[j].item_charges, lds->Entries[j].minlevel,
										lds->Entries[j].maxlevel, lds->Entries[j].equip_item > 0 ? true : false, false, false, false, force_equip);

					int charges = (int)lds->Entries[i].multiplier;
					charges = EQ::ClampLower(charges, 1);

					for(int k = 1; k < charges; ++k) {
						float c_roll = (float)zone->random.Real(0.0, 100.0);
						if(c_roll <= lds->Entries[i].chance) {
							bool force_equip = lds->Entries[i].equip_item == 2;
							npc->AddLootDrop(db_item, itemlist, lds->Entries[j].item_charges, lds->Entries[j].minlevel,
											lds->Entries[j].maxlevel, lds->Entries[j].equip_item > 0 ? true : false, false, false, force_equip);
						}
					}

					j = lds->NumEntries;
					break;
				}
				else {
					roll -= lds->Entries[j].chance;
				}
			}
		}
	} // We either ran out of items or reached our limit.

	npc->UpdateEquipmentLight();
	// no wearchange associated with this function..so, this should not be needed
	//if (npc->UpdateActiveLightValue())
	//	npc->SendAppearancePacket(AT_Light, npc->GetActiveLightValue());
}

bool NPC::MeetsLootDropLevelRequirements(LootDropEntries_Struct loot_drop)
{
	if (loot_drop.npc_min_level > 0 && GetLevel() < loot_drop.npc_min_level) {
		LogLootDetail(
			"NPC [{}] does not meet loot_drop level requirements (min_level) level [{}] current [{}] for item [{}]",
			GetCleanName(),
			loot_drop.npc_min_level,
			GetLevel(),
			database.CreateItemLink(loot_drop.item_id)
		);
		return false;
	}

	if (loot_drop.npc_max_level > 0 && GetLevel() > loot_drop.npc_max_level) {
		LogLootDetail(
			"NPC [{}] does not meet loot_drop level requirements (max_level) level [{}] current [{}] for item [{}]",
			GetCleanName(),
			loot_drop.npc_max_level,
			GetLevel(),
			database.CreateItemLink(loot_drop.item_id)
		);
		return false;
	}

	return true;
}

LootDropEntries_Struct NPC::NewLootDropEntry()
{
	LootDropEntries_Struct loot_drop{};
	loot_drop.item_id           = 0;
	loot_drop.item_charges      = 1;
	loot_drop.equip_item        = 1;
	loot_drop.chance            = 0;
	loot_drop.trivial_min_level = 0;
	loot_drop.trivial_max_level = 0;
	loot_drop.npc_min_level     = 0;
	loot_drop.npc_max_level     = 0;
	loot_drop.multiplier        = 0;

	return loot_drop;
}

//if itemlist is null, just send wear changes
void NPC::AddLootDrop(const EQ::ItemData *item2, ItemList* itemlist, int16 charges, uint8 minlevel, uint8 maxlevel, bool equipit, bool wearchange, bool quest, bool pet, bool force_equip, uint32 aug1, uint32 aug2, uint32 aug3, uint32 aug4, uint32 aug5, uint32 aug6) {
	if(item2 == nullptr || item2->ID > 10000000)
		return;

	//make sure we are doing something...
	if(!itemlist && !wearchange)
		return;

	if (CountQuestItems() >= 8)
		return;
	auto item = new ServerLootItem_Struct;
	memset(item, 0, sizeof(ServerLootItem_Struct));

	if (quest || pet)
		LogDebug("Adding drop to npc: [{}], Item: [{}]", GetName(), item2.ID);

	EQApplicationPacket* outapp = nullptr;
	WearChange_Struct* wc = nullptr;
	outapp = new EQApplicationPacket(OP_WearChange, sizeof(WearChange_Struct));
	wc = (WearChange_Struct*)outapp->pBuffer;
	wc->spawn_id = GetID();
	wc->material=0;

	item->item_id = item2.ID;
	item->charges = charges;
	item->quest = quest;
	item->pet = pet;
	item->forced = false;
	if (pet && quest)
	{
		item->pet = 0;
	}

	item->aug_1 = aug1;
	item->aug_2 = aug2;
	item->aug_3 = aug3;
	item->aug_4 = aug4;
	item->aug_5 = aug5;
	item->aug_6 = aug6;

	item->attuned = 0;
	item->min_level = minlevel;
	item->max_level = maxlevel;
	item->equip_slot = EQ::invslot::SLOT_INVALID;

	bool send_wearchange = wearchange;

	if (equipit) {
		uint8 eslot = 0xFF;
		char newid[64];
		const EQ::ItemData* compitem = nullptr;
		ServerLootItem_Struct* sitem = nullptr;
		bool found = false; // track if we found an empty slot we fit into
		int32 foundslot = -1; // for multi-slot items
		bool upgrade = false;
		bool special_slot = false; // Ear, Ring, and Wrist only allows 1 item for NPCs.

		// Equip rules are as follows:
		// If the item has the NoPet flag set it will not be equipped.
		// An empty slot takes priority. The first empty one that an item can
		// fit into will be the one picked for the item.
		// AC is the primary choice for which item gets picked for a slot.
		// If AC is identical HP is considered next.
		// If an item can fit into multiple slots we'll pick the last one where
		// it is an improvement.

		if (!item2->NoPet) {
			for (int i = EQ::invslot::EQUIPMENT_BEGIN; !found && i <= EQ::invslot::EQUIPMENT_END; i++) {
				uint32 slots = (1 << i);
				if (item2->Slots & slots) {
					if(equipment[i])
					{
						compitem = database.GetItem(equipment[i]);
						if (!compitem)
							continue;
						if (item2->AC > compitem->AC ||
							(item2->AC == compitem->AC && item2->HP > compitem->HP))
						{
							// item would be an upgrade
							// check if we're multi-slot, if yes then we have to keep
							// looking in case any of the other slots we can fit into are empty.
							if (item2->Slots != slots) {
								foundslot = i;
							}
							else {
								equipment[i] = item2->ID;
								foundslot = i;
								found = true;
							}
						} // end if ac
					}
					else
					{
						equipment[i] = item2->ID;
						foundslot = i;
						found = true;
					}
				} // end if (slots)
			} // end for
		} // end if NoPet

		// Possible slot was found but not selected. Pick it now.
		if (!found && foundslot >= 0) {
			equipment[foundslot] = item2->ID;
			found = true;
		}

		bool range_forced = false;
		sitem = GetItem(EQ::invslot::slotRange);
		if (sitem && sitem->forced)
		{
			range_forced = true;
			sitem = nullptr;
		}

		// @merth: IDFile size has been increased, this needs to change
		uint16 emat;
		if(item2->Material <= 0
			|| (item2->Slots & ((1 << EQ::invslot::slotPrimary) | (1 << EQ::invslot::slotSecondary)))) {
			memset(newid, 0, sizeof(newid));
			for(int i=0;i<7;i++){
				if (!isalpha(item2->IDFile[i])){
					strn0cpy(newid, &item2->IDFile[i],6);
					i=8;
				}
			}

			emat = std::stoi(newid);
		}
		else {
			emat = item2.Material;
		}

		if (foundslot == EQ::invslot::slotPrimary && !range_forced) {
			bool is2h = item2->ItemType == EQ::item::ItemType::ItemType2HBlunt || item2->ItemType == EQ::item::ItemType::ItemType2HPiercing || item2->ItemType == EQ::item::ItemType::ItemType2HSlash;
			if (item2->Proc.Effect != 0)
				CastToMob()->AddProcToWeapon(item2->Proc.Effect, true);

			eslot = EQ::textures::weaponPrimary;
			// This prevents us from equipping a 2H item when a shield or misc item is already in the off-hand
						// and prevents innate dual wielding NPCs from equipping 2handers
			if ((GetEquipmentMaterial(EQ::textures::weaponSecondary) == 0 || !is2h) && (!CanThisClassDualWield() || !is2h))
			{
				if (d_melee_texture1 > 0)
				{
					d_melee_texture1 = 0;
					WearChange(EQ::textures::weaponPrimary, 0, 0);
				}

				if (equipment[EQ::invslot::slotRange])
				{
					sitem = GetItem(EQ::invslot::slotRange);
					if (sitem)
					{
						MoveItemToGeneralInventory(sitem);
						sitem = nullptr;
					}
				}

				eslot = EQ::textures::weaponPrimary;
				item->equip_slot = EQ::invslot::slotPrimary;
			}
		}
		else if (foundslot == EQ::invslot::slotSecondary && !range_forced)
		{
			if (item2.ItemType != EQ::item::ItemTypeShield)
			{
				if (GetSkill(EQ::skills::SkillDualWield) || (item2.ItemType != EQ::item::ItemType1HSlash && item2.ItemType != EQ::item::ItemType1HBlunt && item2.ItemType != EQ::item::ItemType1HPiercing))
				{
					EQ::ItemData mainHand;
					memset(&mainHand, 0, sizeof(EQ::ItemData));
					if (equipment[EQ::invslot::slotPrimary] > 0)
						mainHand = database.GetItem(equipment[EQ::invslot::slotPrimary]);

					if (!mainHand.ID || (mainHand.ItemType != EQ::item::ItemType2HSlash && mainHand.ItemType != EQ::item::ItemType2HBlunt && mainHand.ItemType != EQ::item::ItemType2HPiercing))
					{
						if (item2.ItemType == EQ::item::ItemTypeShield)
						{
							SetShieldEquiped(true);
						}

						if (d_melee_texture2 > 0)
						{
							d_melee_texture2 = 0;
							WearChange(EQ::textures::weaponSecondary, 0, 0);
						}

						if (equipment[EQ::invslot::slotRange])
						{
							sitem = GetItem(EQ::invslot::slotRange);
							if (sitem)
							{
								MoveItemToGeneralInventory(sitem);
								sitem = nullptr;
							}
						}

						eslot = EQ::textures::weaponSecondary;
						item->equip_slot = EQ::invslot::slotSecondary;
					}
				}
			}
		}
		else if (foundslot == EQ::invslot::slotRange)
		{
			if (force_equip)
			{
				if (equipment[EQ::invslot::slotPrimary])
				{
					sitem = GetItem(EQ::invslot::slotPrimary);
					if (sitem)
					{
						MoveItemToGeneralInventory(sitem);
						sitem = nullptr;
					}
				}

				if (equipment[EQ::invslot::slotSecondary])
				{
					sitem = GetItem(EQ::invslot::slotSecondary);
					if (sitem)
					{
						MoveItemToGeneralInventory(sitem);
						sitem = nullptr;
					}
				}
			}

			eslot = EQ::textures::weaponPrimary;
			item->equip_slot = EQ::invslot::slotRange;
		}
		else if (foundslot == EQ::invslot::slotHead)
		{
			eslot = EQ::textures::armorHead;
			item->equip_slot = EQ::invslot::slotHead;

		}
		else if (foundslot == EQ::invslot::slotChest)
		{
			eslot = EQ::textures::armorChest;
			item->equip_slot = EQ::invslot::slotChest;

		}
		else if (foundslot == EQ::invslot::slotArms)
		{
			eslot = EQ::textures::armorArms;
			item->equip_slot = EQ::invslot::slotArms;
		}
		else if (foundslot == EQ::invslot::slotWrist1 || foundslot == EQ::invslot::slotWrist2)
		{
			foundslot = EQ::invslot::slotWrist1;
			special_slot = true;
			eslot = EQ::textures::armorWrist;
			item->equip_slot = EQ::invslot::slotWrist1;
		}
		else if (foundslot == EQ::invslot::slotHands)
		{
			eslot = EQ::textures::armorHands;
			item->equip_slot = EQ::invslot::slotHands;
		}
		else if (foundslot == EQ::invslot::slotLegs)
		{
			eslot = EQ::textures::armorLegs;
			item->equip_slot = EQ::invslot::slotLegs;
		}
		else if (foundslot == EQ::invslot::slotFeet)
		{
			eslot = EQ::textures::armorFeet;
			item->equip_slot = EQ::invslot::slotFeet;
		}
		else if (foundslot == EQ::invslot::slotEar1 || foundslot == EQ::invslot::slotEar2)
		{
			foundslot = EQ::invslot::slotEar1;
			special_slot = true;
			item->equip_slot = EQ::invslot::slotEar1;
		}
		else if (foundslot == EQ::invslot::slotFinger1 || foundslot == EQ::invslot::slotFinger2)
		{
			foundslot = EQ::invslot::slotFinger1;
			special_slot = true;
			item->equip_slot = EQ::invslot::slotFinger1;
		}
		else if (foundslot == EQ::invslot::slotFace || foundslot == EQ::invslot::slotNeck || foundslot == EQ::invslot::slotShoulders ||
			foundslot == EQ::invslot::slotBack || foundslot == EQ::invslot::slotWaist || foundslot == EQ::invslot::slotAmmo)
		{
			item->equip_slot = foundslot;
		}

		// We found an item, handle unequipping old items and forced column here.
		if (foundslot != INVALID_INDEX && item->equip_slot <= EQ::invslot::EQUIPMENT_END)
		{
			if (upgrade || special_slot)
			{
				sitem = GetItem(foundslot);
				if (sitem)
				{
					MoveItemToGeneralInventory(sitem);
					sitem = nullptr;
				}
			}

			if (force_equip)
			{
				item->forced = true;
			}
		}

		/*
		what was this about???

		if (((npc->GetRace()==127) && (npc->CastToMob()->GetOwnerID()!=0)) && (item2->Slots==24576) || (item2->Slots==8192) || (item2->Slots==16384)){
			npc->d_melee_texture2=atoi(newid);
			wc->wear_slot_id=8;
			if (item2->Material >0)
				wc->material=item2->Material;
			else
				wc->material=atoi(newid);
			npc->AC+=item2->AC;
			npc->STR+=item2->STR;
			npc->INT+=item2->INT;
		}
		*/
		// We found an item, handle wearchange variables here.
		if (eslot != EQ::textures::textureInvalid)
		{

			uint16 emat = 0;
			if (item2->Material <= 0 ||
				eslot == EQ::textures::weaponPrimary ||
				eslot == EQ::textures::weaponSecondary)
			{
				if (strlen(item2->IDFile) > 2)
				{
					emat = atoi(&item2->IDFile[2]);
				}
			}

			// Hack to force custom crowns to show correctly.
			if (eslot == EQ::textures::armorHead)
			{
				if (strlen(item2.IDFile) > 2)
				{
					uint16 hmat = atoi(&item2.IDFile[2]);
					if (hmat == 240 && item2.Material == 1)
					{
						emat = hmat;
					}
				}
			}

			if (emat == 0)
			{
				emat = item2.Material;
			}

			wc->wear_slot_id = eslot;
			wc->material = emat;
			send_wearchange = true;
		}
		}

	int32 equip_slot = item->equip_slot;
	if (equip_slot <= EQ::invslot::EQUIPMENT_END)
	{
		equipment[equip_slot] = item2.ID;
		CalcBonuses();
	}

	if(itemlist != nullptr)
		itemlist->push_back(item);
	else
		safe_delete(item);


	if(send_wearchange && outapp) {
		entity_list.QueueClients(this, outapp);
	}
	safe_delete(outapp);

	UpdateEquipmentLight();
	if (UpdateActiveLight())
		SendAppearancePacket(AT_Light, GetActiveLightType());
}

void NPC::AddItem(const EQ::ItemData *item, uint16 charges, bool equipitem, bool quest) {
	//slot isnt needed, its determined from the item.
	AddLootDrop(item, &itemlist, charges, 1, 255, equipitem, equipitem, quest);
}

void NPC::AddItem(uint32 itemid, uint16 charges, bool equipitem, bool quest, uint32 aug1, uint32 aug2, uint32 aug3, uint32 aug4, uint32 aug5, uint32 aug6) {
	//slot isnt needed, its determined from the item.
	const EQ::ItemData i = database.GetItem(itemid);
	if (i == nullptr)
		return;
	AddLootDrop(i, &itemlist, charges, 1, 255, equipitem, equipitem, quest, false, false, aug1, aug2, aug3, aug4, aug5, aug6);
}

void NPC::AddLootTable() {
	//if (npctype_id != 0) { // check if it's a GM spawn
	//	for (auto lt_id : loottable_id)
	//	{
	//		database.AddLootTableToNPC(this, lt_id, &itemlist, &copper, &silver, &gold, &platinum);
	//	}
	//}
}

void NPC::AddLootTable(uint32 ldid) {
	loottable_id.push_back(ldid);
	//if (npctype_id != 0) { // check if it's a GM spawn
	//  database.AddLootTableToNPC(this,ldid, &itemlist, &copper, &silver, &gold, &platinum);
	//}
}

void NPC::CheckGlobalLootTables()
{
	auto tables = zone->GetGlobalLootTables(this);

	for (auto &id : tables)
	{
		loottable_id.push_back(id);
		//database.AddLootTableToNPC(this, id, &itemlist, nullptr, nullptr, nullptr, nullptr);
	}
}

void ZoneDatabase::LoadGlobalLoot()
{
	auto query = StringFormat("SELECT id, loottable_id, description, min_level, max_level, rare, raid, race, "
				  "class, bodytype, zone, hot_zone FROM global_loot WHERE enabled = 1");

	auto results = QueryDatabase(query);
	if (!results.Success() || results.RowCount() == 0)
		return;

	// we might need this, lets not keep doing it in a loop
	auto zoneid = std::to_string(zone->GetZoneID());
	for (auto row = results.begin(); row != results.end(); ++row) {
		// checking zone limits
		if (row[10]) {
			auto zones = SplitString(row[10], '|');

			auto it = std::find(zones.begin(), zones.end(), zoneid);
			if (it == zones.end())  // not in here, skip
				continue;
		}

		GlobalLootEntry e(atoi(row[0]), atoi(row[1]), row[2] ? row[2] : "");

		auto min_level = atoi(row[3]);
		if (min_level)
			e.AddRule(GlobalLoot::RuleTypes::LevelMin, min_level);

		auto max_level = atoi(row[4]);
		if (max_level)
			e.AddRule(GlobalLoot::RuleTypes::LevelMax, max_level);

		// null is not used
		if (row[5])
			e.AddRule(GlobalLoot::RuleTypes::Rare, atoi(row[5]));

		// null is not used
		if (row[6])
			e.AddRule(GlobalLoot::RuleTypes::Raid, atoi(row[6]));

		if (row[7]) {
			auto races = SplitString(row[7], '|');

			for (auto &r : races)
				e.AddRule(GlobalLoot::RuleTypes::Race, std::stoi(r));
		}

		if (row[8]) {
			auto classes = SplitString(row[8], '|');

			for (auto &c : classes)
				e.AddRule(GlobalLoot::RuleTypes::Class, std::stoi(c));
		}

		if (row[9]) {
			auto bodytypes = SplitString(row[9], '|');

			for (auto &b : bodytypes)
				e.AddRule(GlobalLoot::RuleTypes::BodyType, std::stoi(b));
		}

		// null is not used
		if (row[11])
			e.AddRule(GlobalLoot::RuleTypes::HotZone, atoi(row[11]));

		zone->AddGlobalLootEntry(e);
	}
}

// itemid only needs to be passed here if the item is in general inventory (slot 22) since that can contain multiple items.
ServerLootItem_Struct* NPC::GetItem(int slot_id, uint32 itemid)
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* item = *cur;
		if (item->equip_slot == slot_id && (itemid == 0 || itemid == item->item_id))
		{
			return item;
		}
	}
	return(nullptr);
}

ServerLootItem_Struct* NPC::GetItemByID(uint32 itemid)
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* item = *cur;
		if (item->item_id == itemid) {
			return item;
		}
	}
	return(nullptr);
}

bool NPC::HasQuestLootItem(uint32 itemid)
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if (sitem && sitem->quest == 1 && sitem->item_id == itemid)
		{
			return true;
		}
	}

	return false;
}

bool NPC::HasPetLootItem(uint32 itemid)
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if (sitem && sitem->pet == 1 && sitem->item_id == itemid)
		{
			return true;
		}
	}

	return false;
}

bool NPC::HasRequiredQuestLoot(uint32 itemid1, uint32 itemid2, uint32 itemid3, uint32 itemid4)
{
	if (itemid2 == 0 && itemid3 == 0 && itemid4 == 0)
		return true;

	uint8 item2count = 0, item3count = 0, item4count = 0, item1npc = 0, item2npc = 0, item3npc = 0, item4npc = 0;
	uint8 item1count = 1;
	if (itemid2 > 0)
		item2count = 1;
	if (itemid3 > 0)
		item3count = 1;
	if (itemid4 > 0)
		item4count = 1;

	if (itemid1 == itemid2 && itemid2 > 0)
	{
		item2count = item1count;
		++item1count;
		++item2count;
	}
	if (itemid1 == itemid3 && itemid3 > 0)
	{
		item3count = item1count;
		++item1count;
		++item3count;
	}
	if (itemid1 == itemid4 && itemid4 > 0)
	{
		item4count = item1count;
		++item1count;
		++item4count;
	}

	if (itemid2 == itemid3 && itemid2 > 0 && itemid3 > 0)
	{
		item3count = item2count;
		++item2count;
		++item3count;
	}
	if (itemid2 == itemid4 && itemid2 > 0 && itemid4 > 0)
	{
		item4count = item2count;
		++item2count;
		++item4count;
	}

	if (itemid3 == itemid4 && itemid3 > 0 && itemid4 > 0)
	{
		item4count = item3count;
		++item3count;
		++item4count;
	}

	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if (sitem && sitem->quest == 1)
		{
			if (sitem->item_id == itemid1)
				++item1npc;

			if (sitem->item_id == itemid2 && itemid2 > 0)
				++item2npc;

			if (sitem->item_id == itemid3 && itemid3 > 0)
				++item3npc;

			if (sitem->item_id == itemid4 && itemid4 > 0)
				++item4npc;
		}
	}

	if (item1npc < item1count)
	{
		return false;
	}

	if (itemid2 > 0 && item2npc < item2count)
		return false;

	if (itemid3 > 0 && item3npc < item3count)
		return false;

	if (itemid4 > 0 && item4npc < item4count)
		return false;

	return true;
}

bool NPC::HasQuestLoot()
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if (sitem && sitem->quest == 1)
		{
			return true;
		}
	}

	return false;
}

void NPC::CleanQuestLootItems()
{
	//Removes nodrop or multiple quest loot items from a NPC before sending the corpse items to the client.

	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	uint8 count = 0;
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if (sitem && (sitem->quest == 1 || sitem->pet == 1))
		{
			uint8 count = CountQuestItem(sitem->item_id);
			if (count > 1 && sitem->pet != 1)
			{
				RemoveItem(sitem);
				return;
			}
			else
			{
				const EQ::ItemData item = database.GetItem(sitem->item_id);
				if (item.ID && item.NoDrop == 0)
				{
					RemoveItem(sitem);
					return;
				}
			}
		}
	}
}

uint8 NPC::CountQuestItem(uint32 itemid)
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	uint8 count = 0;
	for (; cur != end; ++cur)
	{
		ServerLootItem_Struct* sitem = *cur;
		if (sitem && sitem->item_id == itemid)
		{
			++count;
		}
	}

	return count;
}

uint8 NPC::CountQuestItems()
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	uint8 count = 0;
	for (; cur != end; ++cur)
	{
		ServerLootItem_Struct* sitem = *cur;
		if (sitem && sitem->quest == 1)
		{
			++count;
		}
	}

	return count;
}

bool NPC::RemoveQuestLootItems(uint32 itemid)
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if (sitem && sitem->quest == 1) {
			if (itemid == 0 || itemid == sitem->item_id)
			{
				RemoveItem(sitem);
				return true;
			}
		}
	}

	return false;
}

bool NPC::RemovePetLootItems(uint32 itemid)
{
	ItemList::iterator cur, end;
	cur = itemlist.begin();
	end = itemlist.end();
	for (; cur != end; ++cur) {
		ServerLootItem_Struct* sitem = *cur;
		if (sitem && sitem->pet == 1) {
			if (itemid == 0 || itemid == sitem->item_id)
			{
				RemoveItem(sitem);
				return true;
			}
		}
	}

	return false;
}

bool NPC::MoveItemToGeneralInventory(ServerLootItem_Struct* weapon)
{

	if (!weapon)
		return false;

	uint32 weaponid = weapon->item_id;
	int16 slot = weapon->equip_slot;
	int8 charges = weapon->charges;
	bool pet = weapon->pet;
	bool quest = weapon->quest;
	uint8 min_level = weapon->min_level;
	uint8 max_level = weapon->max_level;

	uint32 aug1 = weapon->aug_1;
	uint32 aug2 = weapon->aug_2;
	uint32 aug3 = weapon->aug_3;
	uint32 aug4 = weapon->aug_4;
	uint32 aug5 = weapon->aug_5;
	uint32 aug6 = weapon->aug_6;

	const EQ::ItemData  item = database.GetItem(weaponid);
	if (item.ID)
	{
		RemoveItem(weapon);

		Log(Logs::Detail, Logs::Trading, "%s is moving %d in slot %d to general inventory. quantity: %d", GetCleanName(), weaponid, slot, charges);
		AddLootDrop(item, &itemlist, charges, min_level, max_level, false, false, quest, pet, false, aug1, aug2, aug3, aug4, aug5, aug6);

		return true;
	}

	return false;
}

void NPC::RemoveItem(ServerLootItem_Struct* item_data, uint8 quantity)
{

	if (!item_data)
	{
		return;
	}

	for (auto iter = itemlist.begin(); iter != itemlist.end(); ++iter)
	{
		ServerLootItem_Struct* sitem = *iter;
		if (sitem != item_data) { continue; }

		if (!sitem)
			return;

		int16 eslot = sitem->equip_slot;
		if (sitem->charges <= 1 || quantity == 0)
		{
			DeleteEquipment(eslot);

			Log(Logs::General, Logs::Trading, "%s is deleting item %d from slot %d", GetName(), sitem->item_id, eslot);
			itemlist.erase(iter);

			UpdateEquipmentLight();
			if (UpdateActiveLight())
				SendAppearancePacket(AT_Light, GetActiveLightType());
		}
		else
		{
			sitem->charges -= quantity;
			Log(Logs::General, Logs::Trading, "%s is deleting %d charges from item %d in slot %d", GetName(), quantity, sitem->item_id, eslot);
		}

		return;
	}
}

void NPC::CheckMinMaxLevel(Mob *them)
{
	if (them == nullptr || !them->IsClient())
		return;

	uint16 themlevel = them->GetLevel();

	for (auto iter = itemlist.begin(); iter != itemlist.end();)
	{
		if (!(*iter))
			return;

		if (themlevel < (*iter)->min_level || themlevel >(*iter)->max_level)
		{
			int16 eslot = (*iter)->equip_slot;
			DeleteEquipment(eslot);
			iter = itemlist.erase(iter);
			continue;
		}

		++iter;
	}

	UpdateEquipmentLight();
	if (UpdateActiveLight())
		SendAppearancePacket(AT_Light, GetActiveLightType());
}

void NPC::ClearItemList()
{
	for (auto iter = itemlist.begin(); iter != itemlist.end(); ++iter)
	{
		if (!(*iter))
			return;

		int16 eslot = (*iter)->equip_slot;
		DeleteEquipment(eslot);
	}

	itemlist.clear();

	UpdateEquipmentLight();
	if (UpdateActiveLight())
		SendAppearancePacket(AT_Light, GetActiveLightType());
}

void NPC::DeleteEquipment(int16 slotid)
{
	if (slotid <= EQ::invslot::EQUIPMENT_END)
	{
		equipment[slotid] = 0;
		uint8 material = EQ::InventoryProfile::CalcMaterialFromSlot(slotid);

		if (slotid == EQ::invslot::slotRange && material == EQ::textures::textureInvalid)
		{
			material = EQ::textures::weaponPrimary;
		}

		if (material != EQ::textures::textureInvalid)
		{
			WearChange(material, 0, 0);
			if (!equipment[EQ::invslot::slotPrimary] && !equipment[EQ::invslot::slotSecondary] && equipment[EQ::invslot::slotRange])
			{
				SendWearChange(EQ::textures::weaponPrimary);
			}
		}

		CalcBonuses();
	}
}

void NPC::QueryLoot(Client* to)
{

	for (auto entry : GetLoottableID())
	{
		to->Message(0, "| # Possible Loot (%s) LootTableID: %i", GetName(), entry);
		database.DisplayFullLootTableList(to, entry);
	}

	to->Message(0, "| %i Platinum %i Gold %i Silver %i Copper", platinum, gold, silver, copper);
}

void NPC::AddCash(uint16 in_copper, uint16 in_silver, uint16 in_gold, uint16 in_platinum) {
	if (in_copper >= 0)
		copper = in_copper;
	else
		copper = 0;

	if (in_silver >= 0)
		silver = in_silver;
	else
		silver = 0;

	if (in_gold >= 0)
		gold = in_gold;
	else
		gold = 0;

	if (in_platinum >= 0)
		platinum = in_platinum;
	else
		platinum = 0;
}

void NPC::AddCash() {
	copper = zone->random.Int(1, 100);
	silver = zone->random.Int(1, 50);
	gold = zone->random.Int(1, 10);
	platinum = zone->random.Int(1, 5);
}

void NPC::RemoveCash() {
	copper = 0;
	silver = 0;
	gold = 0;
	platinum = 0;
}

bool NPC::AddQuestLoot(uint32 itemid, int8 charges)
{
	const EQ::ItemData item = database.GetItem(itemid);
	if (item.ID)
	{

		ServerLootItem_Struct qitem;
		qitem.item_id = itemid;
		qitem.charges = charges;
		qitem.quest = true;
		qitem.pet = false;
		qitem.forced = false;
		qitem.aug_1 = 0;
		qitem.aug_2 = 0;
		qitem.aug_3 = 0;
		qitem.aug_4 = 0;
		qitem.aug_5 = 0;
		qitem.aug_6 = 0;

		qitem.attuned = 0;
		qitem.min_level = 0;
		qitem.max_level = 255;
		qitem.equip_slot = EQ::invslot::SLOT_INVALID;

		quest_itemlist.push_back(qitem);

		AddLootDrop(item, &itemlist, charges, 0, 255, false, false, true);
		Log(Logs::General, Logs::Trading, "Adding item %d to the NPC's loot marked as quest.", itemid);
		if (itemid > 0 && HasPetLootItem(itemid))
		{
			Log(Logs::General, Logs::Trading, "Deleting quest item %d from NPC's pet loot.", itemid);
			RemovePetLootItems(itemid);
		}
	}
	else
		return false;

	return true;
}

bool NPC::AddPetLoot(uint32 itemid, int8 charges, bool fromquest)
{
	const EQ::ItemData item = database.GetItem(itemid);

	if (!item.ID)
		return false;

	bool valid = (item.NoDrop != 0 && (!charmed || (charmed && CountQuestItem(item.ID) == 0)));
	if (!fromquest || valid)
	{
		if (item.ID)
		{
			AddLootDrop(item, &itemlist, charges, 0, 255, true, true, false, true);
			Log(Logs::General, Logs::Trading, "Adding item %d to the NPC's loot marked as pet.", itemid);
			return true;
		}
	}
	else
	{
		Log(Logs::General, Logs::Trading, "Item %d is a duplicate or no drop. Deleting...", itemid);
		return false;
	}

	return false;
}

void NPC::DeleteQuestLoot(uint32 itemid1, uint32 itemid2, uint32 itemid3, uint32 itemid4)
{
	int16 items = itemlist.size();
	for (int i = 0; i < items; ++i)
	{
		if (itemid1 == 0)
		{
			if (!RemoveQuestLootItems(itemid1))
				break;
		}
		else
		{
			if (itemid1 != 0)
			{
				RemoveQuestLootItems(itemid1);
			}
			if (itemid2 != 0)
			{
				RemoveQuestLootItems(itemid2);
			}
			if (itemid3 != 0)
			{
				RemoveQuestLootItems(itemid3);
			}
			if (itemid4 != 0)
			{
				RemoveQuestLootItems(itemid4);
			}
		}
	}
}

void NPC::DeleteInvalidQuestLoot()
{
	int16 items = itemlist.size();
	for (int i = 0; i < items; ++i)
	{
		CleanQuestLootItems();
	}
}

int32 NPC::GetEquipmentMaterial(uint8 material_slot) const
{
	if (material_slot >= EQ::textures::TextureSlot::materialCount)
		return 0;

	int inv_slot = EQ::InventoryProfile::CalcSlotFromMaterial(material_slot);

	if (material_slot == EQ::textures::weaponPrimary && !equipment[EQ::invslot::slotPrimary] && !equipment[EQ::invslot::slotSecondary] && equipment[EQ::invslot::slotRange])
	{
		inv_slot = EQ::invslot::slotRange;
	}

	if (inv_slot == -1)
		return 0;
	if (equipment[inv_slot] == 0) {
		switch (material_slot) {
		case EQ::textures::armorHead:
			return helmtexture;
		case EQ::textures::armorChest:
			return texture;
		case EQ::textures::armorArms:
			return armtexture;
		case EQ::textures::armorWrist:
			return bracertexture;
		case EQ::textures::armorHands:
			return handtexture;
		case EQ::textures::armorLegs:
			return legtexture;
		case EQ::textures::armorFeet:
			return feettexture;
		case EQ::textures::weaponPrimary:
			return d_melee_texture1;
		case EQ::textures::weaponSecondary:
			return d_melee_texture2;
		default:
			//they have nothing in the slot, and its not a special slot... they get nothing.
			return(0);
		}
	}

	//they have some loot item in this slot, pass it up to the default handler
	return(Mob::GetEquipmentMaterial(material_slot));
}
