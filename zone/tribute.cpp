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

#include "../common/global_define.h"
#include "../common/eq_packet_structs.h"
#include "../common/features.h"

#include "client.h"

#include <map>

#ifdef _WINDOWS
    #include <winsock2.h>
    #include <windows.h>
    #include <process.h>
    #define snprintf	_snprintf
	#define vsnprintf	_vsnprintf
    #define strncasecmp	_strnicmp
    #define strcasecmp	_stricmp
#else
    #include <stdarg.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include "../common/unix.h"
#endif

/*

The server periodicly sends tribute timer updates to the client on live,
but I dont see a point to that right now, so I dont do it.

*/


class TributeData {
public:
	//this level data stored in regular byte order and must be flipped before sending
	TributeLevel_Struct tiers[MAX_TRIBUTE_TIERS];
	uint8 tier_count;
	uint32 unknown;
	std::string name;
	std::string description;
	bool is_guild;	//is a guild tribute item
};

std::map<uint32, TributeData> tribute_list;

void Client::ToggleTribute(bool enabled) {
	if(enabled) {
		//make sure they have enough points to be activating this...
		int r;
		uint32 cost = 0;
		uint32 level = GetLevel();
		for (r = 0; r < EQ::invtype::TRIBUTE_SIZE; r++) {
			uint32 tid = m_pp.tributes[r].tribute;
			if(tid == TRIBUTE_NONE)
				continue;

			if(tribute_list.count(tid) != 1)
				continue;

			if(m_pp.tributes[r].tier >= MAX_TRIBUTE_TIERS) {
				m_pp.tributes[r].tier = 0;	//sanity check.
				continue;
			}

			TributeData &d = tribute_list[tid];

			TributeLevel_Struct &tier = d.tiers[m_pp.tributes[r].tier];

			if(level < tier.level) {
				Message(0, "You are not high enough level to activate this tribute!");
				ToggleTribute(false);
				continue;
			}

			cost += tier.cost;
		}

		if(cost > m_pp.tribute_points) {
			Message(Chat::Red, "You do not have enough tribute points to activate your tribute!");
			ToggleTribute(false);
			return;
		}
		AddTributePoints(0-cost);

		//reset their timer, since they just paid for a full duration
		m_pp.tribute_time_remaining = Tribute_duration;	//full duration
		tribute_timer.Start(m_pp.tribute_time_remaining);

		m_pp.tribute_active = 1;
	} else {
		m_pp.tribute_active = 0;
		tribute_timer.Disable();
	}
	DoTributeUpdate();
}

void Client::DoTributeUpdate() {
	EQApplicationPacket outapp(OP_TributeUpdate, sizeof(TributeInfo_Struct));
	TributeInfo_Struct *tis = (TributeInfo_Struct *) outapp.pBuffer;

	tis->active = 1;
	tis->tribute_master_id = 1;	//Dont know what this is for
	tis->tributes[0] = 1;
	tis->tiers[0] = 1;
	QueuePacket(&outapp);

	SendTributeTimer();

	//if(true) {
	//	//send and equip tribute items...
	//	for (r = 0; r < EQ::invtype::TRIBUTE_SIZE; r++) {
	//		uint32 tid = m_pp.tributes[r].tribute;
	//		if(tid == TRIBUTE_NONE) {
	//			if (m_inv[EQ::invslot::TRIBUTE_BEGIN + r])
	//				DeleteItemInInventory(EQ::invslot::TRIBUTE_BEGIN + r, 0, false);
	//			continue;
	//		}

	//		if(tribute_list.count(tid) != 1) {
	//			if (m_inv[EQ::invslot::TRIBUTE_BEGIN + r])
	//				DeleteItemInInventory(EQ::invslot::TRIBUTE_BEGIN + r, 0, false);
	//			continue;
	//		}

	//		//sanity check
	//		if(m_pp.tributes[r].tier >= MAX_TRIBUTE_TIERS) {
	//			if (m_inv[EQ::invslot::TRIBUTE_BEGIN + r])
	//				DeleteItemInInventory(EQ::invslot::TRIBUTE_BEGIN + r, 0, false);
	//			m_pp.tributes[r].tier = 0;
	//			continue;
	//		}

	//		TributeData &d = tribute_list[tid];
	//		TributeLevel_Struct &tier = d.tiers[m_pp.tributes[r].tier];
	//		uint32 item_id = tier.tribute_item_id;

	//		//summon the item for them
	//		const EQ::ItemInstance* inst = database.CreateItem(item_id, 1);
	//		if(inst == nullptr)
	//			continue;

		//EQ::ItemData* itemData = (EQ::ItemData*)inst->GetItem();
		//itemData->HeroicSta = std::min((int64)2147483000, (int64)(std::max(m_pp.STA * m_POWER_NORMAL - 10, -9.0f) - (itembonuses.STA+spellbonuses.STA) * (1.0f-m_GEAR_NORMAL) + itembonuses.HeroicSTA * m_GEAR_NORMAL));
		//itemData->HeroicStr = std::min((int64)2147483000, (int64)(std::max(m_pp.STR * m_POWER_NORMAL - 10, -9.0f) - (itembonuses.STR+spellbonuses.STR) * (1.0f-m_GEAR_NORMAL) + itembonuses.HeroicSTR * m_GEAR_NORMAL));
		//itemData->HeroicAgi = std::min((int64)2147483000, (int64)(std::max(m_pp.AGI * m_POWER_NORMAL - 10, -9.0f) - (itembonuses.AGI+spellbonuses.AGI) * (1.0f-m_GEAR_NORMAL) + itembonuses.HeroicAGI * m_GEAR_NORMAL));
		//itemData->HeroicWis = std::min((int64)2147483000, (int64)(std::max(m_pp.WIS * m_POWER_NORMAL - 10, -9.0f) - (itembonuses.WIS+spellbonuses.WIS) * (1.0f-m_GEAR_NORMAL) + itembonuses.HeroicWIS * m_GEAR_NORMAL));
		//itemData->HeroicInt = std::min((int64)2147483000, (int64)(std::max(m_pp.INT * m_POWER_NORMAL - 10, -9.0f) - (itembonuses.INT+spellbonuses.INT) * (1.0f-m_GEAR_NORMAL) + itembonuses.HeroicINT * m_GEAR_NORMAL));
		//itemData->HeroicCha = std::min((int64)2147483000, (int64)(std::max(m_pp.CHA * m_POWER_NORMAL - 10, -9.0f) - (itembonuses.CHA+spellbonuses.CHA) * (1.0f-m_GEAR_NORMAL) + itembonuses.HeroicCHA * m_GEAR_NORMAL));
		//itemData->HeroicDex = std::min((int64)2147483000, (int64)(std::max(m_pp.DEX * m_POWER_NORMAL - 10, -9.0f) - (itembonuses.DEX+spellbonuses.DEX) * (1.0f-m_GEAR_NORMAL) + itembonuses.HeroicDEX * m_GEAR_NORMAL));
		//itemData->HP = std::min((int64)2147483000, CalcBaseHP() - 100);

		////double heroic_mana_stats = (itemData->HeroicInt + GetHeroicINT());
		//itemData->Mana = std::min((int64)2147483000, CalcBaseMana() - 100);
		//
		//int64 heroic_end_stats = (itemData->HeroicStr + GetHeroicSTR() + itemData->HeroicSta + GetHeroicSTA() + itemData->HeroicDex + GetHeroicDEX() + itemData->HeroicAgi + GetHeroicAGI()) / 4;
		//itemData->Endur = std::min((int64)2147483000, (int64)(CalcBaseEndurance() - (heroic_end_stats * 10.0f) - 100));

		//const EQ::ItemInstance* inst2 = database.CreateItem(itemData, 1);
		SendEdgeStatBulkUpdate();
		SendHPUpdate();
		SendManaUpdate();
		SendEnduranceUpdate();
	//		PutItemInInventory(EQ::invslot::TRIBUTE_BEGIN + r, *inst, false);
	//		SendItemPacket(EQ::invslot::TRIBUTE_BEGIN + r, inst, ItemPacketTributeItem);
	//		safe_delete(inst);
	//	}
	//} else {
	//	//unequip tribute items...
	//	for (r = 0; r < EQ::invtype::TRIBUTE_SIZE; r++) {
	//		if (m_inv[EQ::invslot::TRIBUTE_BEGIN + r])
	//			DeleteItemInInventory(EQ::invslot::TRIBUTE_BEGIN + r, 0, false);
	//	}
	//}
}

void Client::SendTributeTimer() {
	//update their timer.
	EQApplicationPacket outapp2(OP_TributeTimer, sizeof(uint32));
	uint32 *timeleft = (uint32 *) outapp2.pBuffer;
	if(m_pp.tribute_active)
		*timeleft = m_pp.tribute_time_remaining;
	else
		*timeleft = Tribute_duration;	//full duration
	QueuePacket(&outapp2);
}

void Client::ChangeTributeSettings(TributeInfo_Struct *t) {
	int r;
	for (r = 0; r < EQ::invtype::TRIBUTE_SIZE; r++) {

		m_pp.tributes[r].tribute = TRIBUTE_NONE;

		uint32 tid = t->tributes[r];
		if(tid == TRIBUTE_NONE)
			continue;

		if(tribute_list.count(tid) != 1)
			continue;	//print a cheater warning?

		TributeData &d = tribute_list[tid];

		//make sure they chose a valid tier
		if(t->tiers[r] >= d.tier_count)
			continue;	//print a cheater warning?

		//might want to check required level, even though its checked before activate

		m_pp.tributes[r].tribute = tid;
		m_pp.tributes[r].tier = t->tiers[r];
	}

	DoTributeUpdate();
}

void Client::SendTributeDetails(uint32 client_id, uint32 tribute_id) {
	if(tribute_list.count(tribute_id) != 1) {
		LogError("Details request for invalid tribute [{}]", (unsigned long)tribute_id);
		return;
	}
	TributeData &td = tribute_list[tribute_id];

	int len = td.description.length();
	EQApplicationPacket outapp(OP_SelectTribute, sizeof(SelectTributeReply_Struct)+len+1);
	SelectTributeReply_Struct *t = (SelectTributeReply_Struct *) outapp.pBuffer;

	t->client_id = client_id;
	t->tribute_id = tribute_id;
	memcpy(t->desc, td.description.c_str(), len);
	t->desc[len] = '\0';

	QueuePacket(&outapp);
}

//returns the number of points received from the tribute
int32 Client::TributeItem(uint32 slot, uint32 quantity) {
	const EQ::ItemInstance*inst = m_inv[slot];

	if(inst == nullptr)
		return(0);

	//figure out what its worth
	int32 pts = inst->GetItem()->Favor;

	pts = mod_tribute_item_value(pts, m_inv[slot]);

	if(pts < 1) {
		Message(Chat::Red, "This item is worthless for favor.");
		return(0);
	}

	/*
		Make sure they have enough of them
		and remove it from inventory
	*/
	if(inst->IsStackable()) {
		if(inst->GetCharges() < (int32)quantity)	//dont have enough....
			return(0);
		DeleteItemInInventory(slot, quantity, false);
	} else {
		quantity = 1;
		DeleteItemInInventory(slot, 0, false);
	}

	pts *= quantity;

	/* Add the tribute value in points */
	AddTributePoints(pts);
	return(pts);
}

//returns the number of points received from the tribute
int32 Client::TributeMoney(uint32 platinum) {
	if(!TakeMoneyFromPP(platinum * 1000)) {
		Message(Chat::Red, "You do not have that much money!");
		return(0);
	}

	/* Add the tribute value in points */
	AddTributePoints(platinum);
	return(platinum);
}

void Client::AddTributePoints(int32 ammount) {
	EQApplicationPacket outapp(OP_TributePointUpdate, sizeof(TributePoint_Struct));
	TributePoint_Struct *t = (TributePoint_Struct *) outapp.pBuffer;

	//change the point values.
	m_pp.tribute_points += ammount;

	//career only tracks points earned, not spent.
	if(ammount > 0)
		m_pp.career_tribute_points += ammount;

	//fill in the packet.
	t->career_tribute_points = m_pp.career_tribute_points;
	t->tribute_points = m_pp.tribute_points;

	QueuePacket(&outapp);
}

void Client::SendTributes() {

	std::map<uint32, TributeData>::iterator cur,end;
	cur = tribute_list.begin();
	end = tribute_list.end();

	for(; cur != end; ++cur) {
		if(cur->second.is_guild)
			continue;	//skip guild tributes here
		int len = cur->second.name.length();
		EQApplicationPacket outapp(OP_TributeInfo, sizeof(TributeAbility_Struct) + len + 1);
		TributeAbility_Struct* tas = (TributeAbility_Struct*)outapp.pBuffer;

		tas->tribute_id = htonl(cur->first);
		tas->tier_count = htonl(cur->second.unknown);

		//gotta copy over the data from tiers, and flip all the
		//byte orders, no idea why its flipped here
		uint32 r, c;
		c = cur->second.tier_count;
		TributeLevel_Struct *dest = tas->tiers;
		TributeLevel_Struct *src = cur->second.tiers;
		for(r = 0; r < c; r++, dest++, src++) {
			dest->cost = htonl(src->cost);
			dest->level = htonl(src->level);
			dest->tribute_item_id = htonl(src->tribute_item_id);
		}

		memcpy(tas->name, cur->second.name.c_str(), len);
		tas->name[len] = '\0';
		QueuePacket(&outapp);
	}
}

void Client::SendGuildTributes() {

	std::map<uint32, TributeData>::iterator cur,end;
	cur = tribute_list.begin();
	end = tribute_list.end();

	for(; cur != end; ++cur) {
		if(!cur->second.is_guild)
			continue;	//skip guild tributes here
		int len = cur->second.name.length();

		//guild tribute has an unknown uint32 at its begining, guild ID?
		EQApplicationPacket outapp(OP_TributeInfo, sizeof(TributeAbility_Struct) + len + 1 + 4);
		uint32 *unknown = (uint32 *) outapp.pBuffer;
		TributeAbility_Struct* tas = (TributeAbility_Struct*) (outapp.pBuffer+4);

		//this is prolly wrong in general, prolly for one specific guild
		*unknown = 0x8A110000;

		tas->tribute_id = htonl(cur->first);
		tas->tier_count = htonl(cur->second.unknown);

		//gotta copy over the data from tiers, and flip all the
		//byte orders, no idea why its flipped here
		uint32 r, c;
		c = cur->second.tier_count;
		TributeLevel_Struct *dest = tas->tiers;
		TributeLevel_Struct *src = cur->second.tiers;
		for(r = 0; r < c; r++, dest++, src++) {
			dest->cost = htonl(src->cost);
			dest->level = htonl(src->level);
			dest->tribute_item_id = htonl(src->tribute_item_id);
		}

		memcpy(tas->name, cur->second.name.c_str(), len);
		tas->name[len] = '\0';

		QueuePacket(&outapp);
	}
}

bool ZoneDatabase::LoadTributes() {

	TributeData tributeData;
	memset(&tributeData.tiers, 0, sizeof(tributeData.tiers));
	tributeData.tier_count = 0;

	tribute_list.clear();

	const std::string query = "SELECT id, name, descr, unknown, isguild FROM tributes";
	auto results = QueryDatabase(query);
	if (!results.Success()) {
		return false;
	}

    for (auto row = results.begin(); row != results.end(); ++row) {
        uint32 id = atoul(row[0]);
		tributeData.name = row[1];
		tributeData.description = row[2];
		tributeData.unknown = strtoul(row[3], nullptr, 10);
		tributeData.is_guild = atol(row[4]) == 0? false: true;

		tribute_list[id] = tributeData;
    }

	const std::string query2 = "SELECT tribute_id, level, cost, item_id FROM tribute_levels ORDER BY tribute_id, level";
	results = QueryDatabase(query2);
	if (!results.Success()) {
		return false;
	}

	for (auto row = results.begin(); row != results.end(); ++row) {
		uint32 id = atoul(row[0]);

		if (tribute_list.count(id) != 1) {
			LogError("Error in LoadTributes: unknown tribute [{}] in tribute_levels", (unsigned long) id);
			continue;
		}

		TributeData &cur = tribute_list[id];

		if (cur.tier_count >= MAX_TRIBUTE_TIERS) {
			LogError("Error in LoadTributes: on tribute [{}]: more tiers defined than permitted", (unsigned long) id);
			continue;
		}

		TributeLevel_Struct &s = cur.tiers[cur.tier_count];

		s.level           = atoul(row[1]);
		s.cost            = atoul(row[2]);
		s.tribute_item_id = atoul(row[3]);
		cur.tier_count++;
	}

	return true;
}
