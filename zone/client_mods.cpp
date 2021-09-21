/*	EQEMu: Everquest Server Emulator
	Copyright (C) 2001-2016 EQEMu Development Team (http://eqemulator.net)

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
#include "../common/eqemu_logsys.h"

#include "../common/rulesys.h"
#include "../common/spdat.h"

#include "../common/data_verification.h"

#include "client.h"
#include "mob.h"

#ifdef BOTS
	#include "bot.h"
#endif

#include <algorithm>


int64 Client::GetMaxStat() const
{
	int64 level = GetLevel();
	int64 base = 0;
	if (level < 61) {
		base = 255;
	}
	else if (ClientVersion() >= EQ::versions::ClientVersion::SoF) {
		base = 255 + 5 * (level - 60);
	}
	else if (level < 71) {
		base = 255 + 5 * (level - 60);
	}
	else {
		base = 330;
	}
	return (base);
}

int32 Client::GetMaxResist() const
{
	int level = GetLevel();
	int32 base = 500;
	if (level > 65) {
		base += ((level - 65) * 5);
	}
	return base;
}

int64 Client::GetMaxSTR() const
{
	return GetMaxStat()
	       + itembonuses.STRCapMod
	       + spellbonuses.STRCapMod
	       + aabonuses.STRCapMod;
}
int64 Client::GetMaxSTA() const
{
	return GetMaxStat()
	       + itembonuses.STACapMod
	       + spellbonuses.STACapMod
	       + aabonuses.STACapMod;
}
int64 Client::GetMaxDEX() const
{
	return GetMaxStat()
	       + itembonuses.DEXCapMod
	       + spellbonuses.DEXCapMod
	       + aabonuses.DEXCapMod;
}
int64 Client::GetMaxAGI() const
{
	return GetMaxStat()
	       + itembonuses.AGICapMod
	       + spellbonuses.AGICapMod
	       + aabonuses.AGICapMod;
}
int64 Client::GetMaxINT() const
{
	return GetMaxStat()
	       + itembonuses.INTCapMod
	       + spellbonuses.INTCapMod
	       + aabonuses.INTCapMod;
}
int64 Client::GetMaxWIS() const
{
	return GetMaxStat()
	       + itembonuses.WISCapMod
	       + spellbonuses.WISCapMod
	       + aabonuses.WISCapMod;
}
int64 Client::GetMaxCHA() const
{
	return GetMaxStat()
	       + itembonuses.CHACapMod
	       + spellbonuses.CHACapMod
	       + aabonuses.CHACapMod;
}
int32 Client::GetMaxMR() const
{
	return GetMaxResist()
	       + itembonuses.MRCapMod
	       + spellbonuses.MRCapMod
	       + aabonuses.MRCapMod;
}
int32 Client::GetMaxPR() const
{
	return GetMaxResist()
	       + itembonuses.PRCapMod
	       + spellbonuses.PRCapMod
	       + aabonuses.PRCapMod;
}
int32 Client::GetMaxDR() const
{
	return GetMaxResist()
	       + itembonuses.DRCapMod
	       + spellbonuses.DRCapMod
	       + aabonuses.DRCapMod;
}
int32 Client::GetMaxCR() const
{
	return GetMaxResist()
	       + itembonuses.CRCapMod
	       + spellbonuses.CRCapMod
	       + aabonuses.CRCapMod;
}
int32 Client::GetMaxFR() const
{
	return GetMaxResist()
	       + itembonuses.FRCapMod
	       + spellbonuses.FRCapMod
	       + aabonuses.FRCapMod;
}
int32 Client::GetMaxCorrup() const
{
	return GetMaxResist()
	       + itembonuses.CorrupCapMod
	       + spellbonuses.CorrupCapMod
	       + aabonuses.CorrupCapMod;
}
int32 Client::LevelRegen()
{
	bool sitting = IsSitting();
	bool feigned = GetFeigned();
	int level = GetLevel();
	bool bonus = GetPlayerRaceBit(GetBaseRace()) & RuleI(Character, BaseHPRegenBonusRaces);
	uint8 multiplier1 = bonus ? 2 : 1;
	int64 hp = 0;
	//these calculations should match up with the info from Monkly Business, which was last updated ~05/2008: http://www.monkly-business.net/index.php?pageid=abilities
	if (level < 51) {
		if (sitting) {
			if (level < 20) {
				hp += 2 * multiplier1;
			}
			else if (level < 50) {
				hp += 3 * multiplier1;
			}
			else {	//level == 50
				hp += 4 * multiplier1;
			}
		}
		else {	//feigned or standing
			hp += 1 * multiplier1;
		}
	}
	//there may be an easier way to calculate this next part, but I don't know what it is
	else {	//level >= 51
		int32 tmp = 0;
		float multiplier2 = 1;
		if (level < 56) {
			tmp = 2;
			if (bonus) {
				multiplier2 = 3;
			}
		}
		else if (level < 60) {
			tmp = 3;
			if (bonus) {
				multiplier2 = 3.34;
			}
		}
		else if (level < 61) {
			tmp = 4;
			if (bonus) {
				multiplier2 = 3;
			}
		}
		else if (level < 63) {
			tmp = 5;
			if (bonus) {
				multiplier2 = 2.8;
			}
		}
		else if (level < 65) {
			tmp = 6;
			if (bonus) {
				multiplier2 = 2.67;
			}
		}
		else {	//level >= 65
			tmp = 7;
			if (bonus) {
				multiplier2 = 2.58;
			}
		}
		hp += int32(float(tmp) * multiplier2);
		if (sitting) {
			hp += 3 * multiplier1;
		}
		else if (feigned) {
			hp += 1 * multiplier1;
		}
	}
	return hp;
}

int64 Client::CalcHPRegen(bool bCombat)
{
	return itembonuses.HPRegen + aabonuses.HPRegen + spellbonuses.HPRegen;
}

int32 Client::CalcHPRegenCap()
{
	int cap = RuleI(Character, ItemHealthRegenCap);
	if (GetLevel() > 60)
		cap = std::max(cap, GetLevel() - 30); // if the rule is set greater than normal I guess
	if (GetLevel() > 65)
		cap += GetLevel() - 65;
	cap += aabonuses.ItemHPRegenCap + spellbonuses.ItemHPRegenCap + itembonuses.ItemHPRegenCap;
	return (cap * RuleI(Character, HPRegenMultiplier) / 100);
}

int64 Client::CalcMaxHP()
{
	float nd = 10000;
	max_hp = (CalcBaseHP() + itembonuses.HP);
	//The AA desc clearly says it only applies to base hp..
	//but the actual effect sent on live causes the client
	//to apply it to (basehp + itemhp).. I will oblige to the client's whims over
	//the aa description
	nd += aabonuses.MaxHP;	//Natural Durability, Physical Enhancement, Planar Durability
	max_hp = (float)max_hp * (float)nd / (float)10000; //this is to fix the HP-above-495k issue
	max_hp += spellbonuses.HP + aabonuses.HP;
	max_hp += GroupLeadershipAAHealthEnhancement();
	max_hp += max_hp * ((spellbonuses.MaxHPChange + itembonuses.MaxHPChange) / 10000.0f);
	if (current_hp > max_hp) {
		current_hp = max_hp;
	}
	int hp_perc_cap = spellbonuses.HPPercCap[0];
	if (hp_perc_cap) {
		int curHP_cap = (max_hp * hp_perc_cap) / 100;
		if (current_hp > curHP_cap || (spellbonuses.HPPercCap[1] && current_hp > spellbonuses.HPPercCap[1])) {

			current_hp = curHP_cap;
		}
	}

	// hack fix for client health not reflecting server value
	last_max_hp = 0;

	return max_hp;
}

uint32 Mob::GetClassLevelFactor()
{
	uint32 multiplier = 0;
	uint8  mlevel     = GetLevel();
	switch (GetClass()) {
		case WARRIOR: {
			if (mlevel < 20) {
				multiplier = 220;
			}
			else if (mlevel < 30) {
				multiplier = 230;
			}
			else if (mlevel < 40) {
				multiplier = 250;
			}
			else if (mlevel < 53) {
				multiplier = 270;
			}
			else if (mlevel < 57) {
				multiplier = 280;
			}
			else if (mlevel < 60) {
				multiplier = 290;
			}
			else if (mlevel < 70) {
				multiplier = 300;
			}
			else {
				multiplier = 311;
			}
			break;
		}
		case DRUID:
		case CLERIC:
		case SHAMAN: {
			if (mlevel < 70) {
				multiplier = 150;
			}
			else {
				multiplier = 157;
			}
			break;
		}
		case BERSERKER:
		case PALADIN:
		case SHADOWKNIGHT: {
			if (mlevel < 35) {
				multiplier = 210;
			}
			else if (mlevel < 45) {
				multiplier = 220;
			}
			else if (mlevel < 51) {
				multiplier = 230;
			}
			else if (mlevel < 56) {
				multiplier = 240;
			}
			else if (mlevel < 60) {
				multiplier = 250;
			}
			else if (mlevel < 68) {
				multiplier = 260;
			}
			else {
				multiplier = 270;
			}
			break;
		}
		case MONK:
		case BARD:
		case ROGUE:
		case BEASTLORD: {
			if (mlevel < 51) {
				multiplier = 180;
			}
			else if (mlevel < 58) {
				multiplier = 190;
			}
			else if (mlevel < 70) {
				multiplier = 200;
			}
			else {
				multiplier = 210;
			}
			break;
		}
		case RANGER: {
			if (mlevel < 58) {
				multiplier = 200;
			}
			else if (mlevel < 70) {
				multiplier = 210;
			}
			else {
				multiplier = 220;
			}
			break;
		}
		case MAGICIAN:
		case WIZARD:
		case NECROMANCER:
		case ENCHANTER: {
			if (mlevel < 70) {
				multiplier = 120;
			}
			else {
				multiplier = 127;
			}
			break;
		}
		default: {
			if (mlevel < 35) {
				multiplier = 210;
			}
			else if (mlevel < 45) {
				multiplier = 220;
			}
			else if (mlevel < 51) {
				multiplier = 230;
			}
			else if (mlevel < 56) {
				multiplier = 240;
			}
			else if (mlevel < 60) {
				multiplier = 250;
			}
			else {
				multiplier = 260;
			}
			break;
		}
	}

	return multiplier;
}

int64 Client::CalcBaseHP()
{
#ifndef TLP_STYLE
	return Mob::CalcBaseHP();
#else
	if (ClientVersion() >= EQ::versions::ClientVersion::SoF && RuleB(Character, SoDClientUseSoDHPManaEnd)) {
		int stats = GetSTA();
		if (stats > 255) {
			stats = (stats - 255) / 2;
			stats += 255;
		}
		base_hp = 5;
		auto base_data = database.GetBaseData(GetLevel(), GetClass());
		if (base_data) {
			base_hp += base_data->base_hp + (base_data->hp_factor * stats);
			base_hp += (GetHeroicSTA() * 10);
		}
	}
	else {
		uint32 Post255;
		uint32 lm = GetClassLevelFactor();
		if ((GetSTA() - 255) / 2 > 0) {
			Post255 = (GetSTA() - 255) / 2;
		}
		else {
			Post255 = 0;
		}
		base_hp = (5) + (GetLevel() * lm / 10) + (((GetSTA() - Post255) * GetLevel() * lm / 3000)) + ((Post255 * GetLevel()) * lm / 6000);
	}
	return base_hp;
#endif
}
// This is for calculating Base HPs + STA bonus for SoD or later clients.
int64 Client::GetClassHPFactor()
{
	int factor;
	// Note: Base HP factor under level 41 is equal to factor / 12, and from level 41 to 80 is factor / 6.
	// Base HP over level 80 is factor / 10
	// HP per STA point per level is factor / 30 for level 80+
	// HP per STA under level 40 is the level 80 HP Per STA / 120, and for over 40 it is / 60.
	switch (GetClass()) {
		case DRUID:
		case ENCHANTER:
		case NECROMANCER:
		case MAGICIAN:
		case WIZARD:
			factor = 240;
			break;
		case BEASTLORD:
		case BERSERKER:
		case MONK:
		case ROGUE:
		case SHAMAN:
			factor = 255;
			break;
		case BARD:
		case CLERIC:
			factor = 264;
			break;
		case SHADOWKNIGHT:
		case PALADIN:
			factor = 288;
			break;
		case RANGER:
			factor = 276;
			break;
		case WARRIOR:
			factor = 300;
			break;
		default:
			factor = 240;
			break;
	}
	return factor;
}

// This should return the combined AC of all the items the player is wearing.
int32 Client::GetRawItemAC()
{
	int32 Total = 0;
	// this skips MainAmmo..add an '=' conditional if that slot is required (original behavior)
	for (int16 slot_id = EQ::invslot::BONUS_BEGIN; slot_id <= EQ::invslot::BONUS_STAT_END; slot_id++) {
		const EQ::ItemInstance* inst = m_inv[slot_id];
		if (inst && inst->IsClassCommon()) {
			Total += inst->GetItem()->AC;
		}
	}
	return Total;
}

int64 Client::CalcBaseManaRegen()
{
#ifndef TLP_STYLE
	uint8 clevel = GetLevel();
	int64 regen = 0;
	if (IsSitting() || (GetHorseId() != 0)) {
		if (HasSkill(EQ::skills::SkillMeditate)) {
			regen = (((GetSkill(EQ::skills::SkillMeditate) / 10) + (clevel - (clevel / 4))) / 4) + 4;
		}
		else {
			regen = 2;
		}
	}
	else {
		regen = 2;
	}
	return regen;
#else
	switch (GetCasterClass()) {
		case 'I':
		case 'W': {
				max_mana = (CalcBaseMana() + itembonuses.Mana + spellbonuses.Mana + aabonuses.Mana + GroupLeadershipAAManaEnhancement());
				break;
			}
		case 'N': {
				max_mana = 0;
				break;
			}
		default: {
				LogSpells("Invalid Class [{}] in CalcMaxMana", GetCasterClass());
				max_mana = 0;
				break;
			}
	}
	if (max_mana < 0) {
		max_mana = 0;
	}
	if (current_mana > max_mana) {
		current_mana = max_mana;
	}
	int mana_perc_cap = spellbonuses.ManaPercCap[0];
	if (mana_perc_cap) {
		int curMana_cap = (max_mana * mana_perc_cap) / 100;
		if (current_mana > curMana_cap || (spellbonuses.ManaPercCap[1] && current_mana > spellbonuses.ManaPercCap[1])) {
			current_mana = curMana_cap;
		}
	}
	LogSpells("Client::CalcMaxMana() called for [{}] - returning [{}]", GetName(), max_mana);
	return max_mana;
#endif
}

int64 Client::CalcBaseMana()
{
#ifndef TLP_STYLE
	return Mob::CalcBaseMana();
#else
	
	int ConvertedWisInt = 0;
	int MindLesserFactor, MindFactor;
	int WisInt = 0;
	int base_mana = 0;
	int wisint_mana = 0;
	int32 max_m = 0;
	switch (GetCasterClass()) {
		case 'I':
			WisInt = GetINT();
			if (ClientVersion() >= EQ::versions::ClientVersion::SoF && RuleB(Character, SoDClientUseSoDHPManaEnd)) {
				ConvertedWisInt = WisInt;
				int over200 = WisInt;
				if (WisInt > 100) {
					if (WisInt > 200) {
						over200 = (WisInt - 200) / -2 + WisInt;
					}
					ConvertedWisInt = (3 * over200 - 300) / 2 + over200;
				}
				auto base_data = database.GetBaseData(GetLevel(), GetClass());
				if (base_data) {
					max_m = base_data->base_mana + (ConvertedWisInt * base_data->mana_factor) + (GetHeroicINT() * 10);
				}
			}
			else {
				if ((( WisInt - 199 ) / 2) > 0) {
					MindLesserFactor = ( WisInt - 199 ) / 2;
				}
				else {
					MindLesserFactor = 0;
				}
				MindFactor = WisInt - MindLesserFactor;
				if (WisInt > 100) {
					max_m = (((5 * (MindFactor + 20)) / 2) * 3 * GetLevel() / 40);
				}
				else {
					max_m = (((5 * (MindFactor + 200)) / 2) * 3 * GetLevel() / 100);
				}
			}
			break;
		case 'W':
			WisInt = GetWIS();
			if (ClientVersion() >= EQ::versions::ClientVersion::SoF && RuleB(Character, SoDClientUseSoDHPManaEnd)) {
				ConvertedWisInt = WisInt;
				int over200 = WisInt;
				if (WisInt > 100) {
					if (WisInt > 200) {
						over200 = (WisInt - 200) / -2 + WisInt;
					}
					ConvertedWisInt = (3 * over200 - 300) / 2 + over200;
				}
				auto base_data = database.GetBaseData(GetLevel(), GetClass());
				if (base_data) {
					max_m = base_data->base_mana + (ConvertedWisInt * base_data->mana_factor) + (GetHeroicWIS() * 10);
				}
			}
			else {
				if ((( WisInt - 199 ) / 2) > 0) {
					MindLesserFactor = ( WisInt - 199 ) / 2;
				}
				else {
					MindLesserFactor = 0;
				}
				MindFactor = WisInt - MindLesserFactor;
				if (WisInt > 100) {
					max_m = (((5 * (MindFactor + 20)) / 2) * 3 * GetLevel() / 40);
				}
				else {
					max_m = (((5 * (MindFactor + 200)) / 2) * 3 * GetLevel() / 100);
				}
			}
			break;
		case 'N': {
				max_m = 0;
				break;
			}
		default: {
				LogDebug("Invalid Class [{}] in CalcMaxMana", GetCasterClass());
				max_m = 0;
				break;
			}
	}
	#if EQDEBUG >= 11
	LogDebug("Client::CalcBaseMana() called for [{}] - returning [{}]", GetName(), max_m);
	#endif
	return max_m;
#endif
}

int64 Client::CalcManaRegen(bool bCombat)
{
	return aabonuses.ManaRegen + itembonuses.ManaRegen + spellbonuses.ManaRegen;
}

int64 Client::CalcManaRegenCap()
{
	int64 cap = RuleI(Character, ItemManaRegenCap) + aabonuses.ItemManaRegenCap;
	return (cap * RuleI(Character, ManaRegenMultiplier) / 100);
}

uint32 Client::CalcCurrentWeight()
{
	const EQ::ItemData* TempItem = nullptr;
	EQ::ItemInstance* ins = nullptr;
	uint32 Total = 0;
	int x;
	for (x = EQ::invslot::POSSESSIONS_BEGIN; x <= EQ::invslot::POSSESSIONS_END; x++) {
		TempItem = 0;
		ins = GetInv().GetItem(x);
		if (ins) {
			TempItem = ins->GetItem();
		}
		if (TempItem) {
			Total += TempItem->Weight;
		}
	}
	for (x = EQ::invbag::GENERAL_BAGS_BEGIN; x <= EQ::invbag::CURSOR_BAG_END; x++) {
		int TmpWeight = 0;
		TempItem = 0;
		ins = GetInv().GetItem(x);
		if (ins) {
			TempItem = ins->GetItem();
		}
		if (TempItem) {
			TmpWeight = TempItem->Weight;
		}
		if (TmpWeight > 0) {
			// this code indicates that weight redux bags can only be in the first general inventory slot to be effective...
			// is this correct? or can we scan for the highest weight redux and use that? (need client verifications)
			int bagslot = EQ::invslot::slotGeneral1;
			int reduction = 0;
			for (int m = EQ::invbag::GENERAL_BAGS_BEGIN + EQ::invbag::SLOT_COUNT; m <= EQ::invbag::CURSOR_BAG_END; m += EQ::invbag::SLOT_COUNT) {
				if (x >= m) {
					bagslot += 1;
				}
			}
			EQ::ItemInstance* baginst = GetInv().GetItem(bagslot);
			if (baginst && baginst->GetItem() && baginst->IsClassBag()) {
				reduction = baginst->GetItem()->BagWR;
			}
			if (reduction > 0) {
				TmpWeight -= TmpWeight * reduction / 100;
			}
			Total += TmpWeight;
		}
	}
	//TODO: coin weight reduction (from purses, etc), since client already calculates it
	/*  From the Wiki http://www.eqemulator.net/wiki/wikka.php?wakka=EQEmuDBSchemaitems under bagwr (thanks Trevius):
	    Interestingly, you can also have bags that reduce coin weight. However, in order to set bags to reduce coin weight, you MUST set the Item ID somewhere between 17201 and 17230. This is hard coded into the client.
	    The client is set to have certain coin weight reduction on a per Item ID basis within this range. The best way to create an new item to reduce coin weight is to examine existing bags in this range.
	    Search for the words "coin purse" with the #finditem command in game and the Bag WR setting on those bags is the amount they will reduce coin weight. It is easiest to overwrite one of those bags if you wish to create one with the
	    same weight reduction amount for coins. You can use other Item IDs in this range for setting coin weight reduction, but by using an existing item, at least you will know the amount the client will reduce it by before you create it.
	    This is the ONLY instance I have seen where the client is hard coded to particular Item IDs to set a certain property for an item. It is very odd.
	*/
	// SoD+ client has no weight for coin
	if (EQ::behavior::StaticLookup(EQ::versions::ConvertClientVersionToMobVersion(ClientVersion()))->CoinHasWeight) {
		Total += (m_pp.platinum + m_pp.gold + m_pp.silver + m_pp.copper) / 4;
	}
	float Packrat = (float)spellbonuses.Packrat + (float)aabonuses.Packrat + (float)itembonuses.Packrat;
	if (Packrat > 0) {
		Total = (uint32)((float)Total * (1.0f - ((Packrat * 1.0f) / 100.0f)));    //AndMetal: 1% per level, up to 5% (calculated from Titanium client). verified thru client that it reduces coin weight by the same %
	}
	//without casting to float & back to uint32, this didn't work right
	return Total;
}

int32 Client::CalcAlcoholPhysicalEffect()
{
	if (m_pp.intoxication <= 55) {
		return 0;
	}
	return (m_pp.intoxication - 40) / 16;
}

int64 Client::CalcSTR()
{
	int64 val = m_pp.STR  + itembonuses.STR  + spellbonuses.STR + CalcAlcoholPhysicalEffect();
	int64 mod = aabonuses.STR;
	STR = val + mod;
	if (STR < 1) {
		STR = 1;
	}
	return (STR);
}

int64 Client::CalcSTA()
{
	int64 val = m_pp.STA + itembonuses.STA + spellbonuses.STA + CalcAlcoholPhysicalEffect();;
	int64 mod = aabonuses.STA;
	STA = val + mod;
	if (STA < 1) {
		STA = 1;
	}

	return (STA);
}

int64 Client::CalcAGI()
{
	int64 val = m_pp.AGI + itembonuses.AGI + spellbonuses.AGI - CalcAlcoholPhysicalEffect();;
	int64 mod = aabonuses.AGI;
	//int64 str = GetSTR();
	//Encumbered penalty
	//if (weight > (str * 10)) {
	//	//AGI is halved when we double our weight, zeroed (defaults to 1) when we triple it. this includes AGI from AAs
	//	float total_agi = float(val + mod);
	//	float str_float = float(str);
	//	AGI = (int64)(((-total_agi) / (str_float * 2)) * (((float)weight / 10) - str_float) + total_agi);	//casting to an int assumes this will be floor'd. without using floats & casting to int16, the calculation doesn't work right
	//}
	//else {
		AGI = val + mod;
	//}
	if (AGI < 1) {
		AGI = 1;
	}

	return (AGI);
}

int64 Client::CalcDEX()
{
	int64 val = m_pp.DEX + itembonuses.DEX + spellbonuses.DEX - CalcAlcoholPhysicalEffect();;
	int64 mod = aabonuses.DEX;
	DEX = val + mod;
	if (DEX < 1) {
		DEX = 1;
	}

	return (DEX);
}

int64 Client::CalcINT()
{
	int64 val = m_pp.INT + itembonuses.INT + spellbonuses.INT;
	int64 mod = aabonuses.INT;
	INT = val + mod;
	if (m_pp.intoxication) {
		int32 AlcINT = INT - (int32)((float)m_pp.intoxication / 200.0f * (float)INT) - 1;
		if ((AlcINT < (int)(0.2 * INT))) {
			INT = (int)(0.2f * (float)INT);
		}
		else {
			INT = AlcINT;
		}
	}
	if (INT < 1) {
		INT = 1;
	}
	return (INT);
}

int64 Client::CalcWIS()
{
	int64 val = m_pp.WIS + itembonuses.WIS + spellbonuses.WIS;
	int64 mod = aabonuses.WIS;
	WIS = val + mod;
	if (m_pp.intoxication) {
		int64 AlcWIS = WIS - (int64)((float)m_pp.intoxication / 200.0f * (float)WIS) - 1;
		if ((AlcWIS < (int)(0.2 * WIS))) {
			WIS = (int)(0.2f * (float)WIS);
		}
		else {
			WIS = AlcWIS;
		}
	}
	if (WIS < 1) {
		WIS = 1;
	}
	return (WIS);
}

int64 Client::CalcCHA()
{
	int64 val = m_pp.CHA + itembonuses.CHA + spellbonuses.CHA;
	int64 mod = aabonuses.CHA;
	CHA = val + mod;
	if (CHA < 1) {
		CHA = 1;
	}
	return (CHA);
}

float Client::CalcPOWER()
{

}

float Client::CalcNORMAL()
{

}

float Client::CalcNORMAL_Legacy()
{

}

int Client::CalcHaste()
{
	/*  Tests: (based on results in newer char window)
	    68 v1 + 46 item + 25 over + 35 inhib = 204%
	    46 item + 5 v2 + 25 over + 35 inhib = 65%
	    68 v1 + 46 item + 5 v2 + 25 over + 35 inhib = 209%
	    75% slow + 35 inhib = 25%
	    35 inhib = 65%
	    75% slow = 25%
	    Conclusions:
	    the bigger effect in slow v. inhib wins
	    slow negates all other hastes
	    inhib will only negate all other hastes if you don't have v1 (ex. VQ)
	*/
	// slow beats all! Besides a better inhibit
	if (spellbonuses.haste < 0) {
		if (-spellbonuses.haste <= spellbonuses.inhibitmelee) {
			Haste = 100 - spellbonuses.inhibitmelee;
		}
		else {
			Haste = 100 + spellbonuses.haste;
		}
		return Haste;
	}
	// No haste and inhibit, kills all other hastes
	if (spellbonuses.haste == 0 && spellbonuses.inhibitmelee) {
		Haste = 100 - spellbonuses.inhibitmelee;
		return Haste;
	}
	int h = 0;
	int cap = 0;
	int level = GetLevel();
	// we know we have a haste spell and not slowed, no extra inhibit melee checks needed
	if (spellbonuses.haste) {
		h += spellbonuses.haste - spellbonuses.inhibitmelee;
	}
	if (spellbonuses.hastetype2 && level > 49) { // type 2 is capped at 10% and only available to 50+
		h += spellbonuses.hastetype2 > 10 ? 10 : spellbonuses.hastetype2;
	}
	// 26+ no cap, 1-25 10
	if (level > 25) { // 26+
		h += itembonuses.haste;
	}
	else {   // 1-25
		h += itembonuses.haste > 10 ? 10 : itembonuses.haste;
	}
	// 60+ 100, 51-59 85, 1-50 level+25
	if (level > 59) { // 60+
		cap = RuleI(Character, HasteCap);
	}
	else if (level > 50) {  // 51-59
		cap = 85;
	}
	else {   // 1-50
		cap = level + 25;
	}
	cap = mod_client_haste_cap(cap);
	if (h > cap) {
		h = cap;
	}
	// 51+ 25 (despite there being higher spells...), 1-50 10
	if (level > 50) { // 51+
		h += spellbonuses.hastetype3 > 25 ? 25 : spellbonuses.hastetype3;
	}
	else {   // 1-50
		h += spellbonuses.hastetype3 > 10 ? 10 : spellbonuses.hastetype3;
	}
	h += ExtraHaste;	//GM granted haste.
	h = mod_client_haste(h);
	Haste = 100 + h;
	return Haste;
}

//The AA multipliers are set to be 5, but were 2 on WR
//The resistant discipline which I think should be here is implemented
//in Mob::ResistSpell
int32	Client::CalcMR()
{
	MR = itembonuses.MR + spellbonuses.MR + aabonuses.MR;
	//if (MR < 1) {
	//	MR = 1;
	//}
	if (MR > GetMaxMR()) {
		MR = GetMaxMR();
	}
	return (MR);
}

int32	Client::CalcFR()
{
	FR = itembonuses.FR + spellbonuses.FR + aabonuses.FR;
	//if (FR < 1) {
	//	FR = 1;
	//}
	if (FR > GetMaxFR()) {
		FR = GetMaxFR();
	}
	return (FR);
}

int32	Client::CalcDR()
{
	DR = itembonuses.DR + spellbonuses.DR + aabonuses.DR;
	//if (DR < 1) {
	//	DR = 1;
	//}
	if (DR > GetMaxDR()) {
		DR = GetMaxDR();
	}
	return (DR);
}

int32	Client::CalcPR()
{
	PR = itembonuses.PR + spellbonuses.PR + aabonuses.PR;
	//if (PR < 1) {
	//	PR = 1;
	//}
	if (PR > GetMaxPR()) {
		PR = GetMaxPR();
	}
	return (PR);
}

int32	Client::CalcCR()
{
	CR = itembonuses.CR + spellbonuses.CR + aabonuses.CR;
	//if (CR < 1) {
	//	CR = 1;
	//}
	if (CR > GetMaxCR()) {
		CR = GetMaxCR();
	}
	return (CR);
}

int32	Client::CalcCorrup()
{
	Corrup = GetBaseCorrup() + itembonuses.Corrup + spellbonuses.Corrup + aabonuses.Corrup;
	if (Corrup > GetMaxCorrup()) {
		Corrup = GetMaxCorrup();
	}
	return (Corrup);
}

int32 Client::CalcATK()
{
	ATK = itembonuses.ATK + spellbonuses.ATK + aabonuses.ATK + GroupLeadershipAAOffenseEnhancement();
	return (ATK);
}

uint32 Mob::GetInstrumentMod(uint16 spell_id) const
{
	if (!IsBardSong(spell_id) || spells[spell_id].IsDisciplineBuff) // Puretone is Singing but doesn't get any mod
		return 10;

	uint32 effectmod = 10;
	int effectmodcap = 0;
	bool nocap = false;
	if (RuleB(Character, UseSpellFileSongCap)) {
		effectmodcap = spells[spell_id].songcap / 10;
		// this looks a bit weird, but easiest way I could think to keep both systems working
		if (effectmodcap == 0)
			nocap = true;
		else
			effectmodcap += 10;
	} else {
		effectmodcap = RuleI(Character, BaseInstrumentSoftCap);
	}
	// this should never use spell modifiers...
	// if a spell grants better modifers, they are copied into the item mods
	// because the spells are supposed to act just like having the intrument.
	// item mods are in 10ths of percent increases
	// clickies (Symphony of Battle) that have a song skill don't get AA bonus for some reason
	// but clickies that are songs (selo's on Composers Greaves) do get AA mod as well
	switch (spells[spell_id].skill) {
	case EQ::skills::SkillPercussionInstruments:
		if (itembonuses.percussionMod == 0 && spellbonuses.percussionMod == 0)
			effectmod = 10;
#ifdef TLP_STYLE
		else if (GetSkill(EQ::skills::SkillPercussionInstruments) == 0)
			effectmod = 10;
#endif
		else if (itembonuses.percussionMod > spellbonuses.percussionMod)
			effectmod = itembonuses.percussionMod;
		else
			effectmod = spellbonuses.percussionMod;
		if (IsBardSong(spell_id))
			effectmod += aabonuses.percussionMod;
		break;
	case EQ::skills::SkillStringedInstruments:
		if (itembonuses.stringedMod == 0 && spellbonuses.stringedMod == 0)
			effectmod = 10;
#ifdef TLP_STYLE
		else if (GetSkill(EQ::skills::SkillStringedInstruments) == 0)
			effectmod = 10;
#endif
		else if (itembonuses.stringedMod > spellbonuses.stringedMod)
			effectmod = itembonuses.stringedMod;
		else
			effectmod = spellbonuses.stringedMod;
		if (IsBardSong(spell_id))
			effectmod += aabonuses.stringedMod;
		break;
	case EQ::skills::SkillWindInstruments:
		if (itembonuses.windMod == 0 && spellbonuses.windMod == 0)
			effectmod = 10;
#ifdef TLP_STYLE
		else if (GetSkill(EQ::skills::SkillWindInstruments) == 0)
			effectmod = 10;
#endif
		else if (itembonuses.windMod > spellbonuses.windMod)
			effectmod = itembonuses.windMod;
		else
			effectmod = spellbonuses.windMod;
		if (IsBardSong(spell_id))
			effectmod += aabonuses.windMod;
		break;
	case EQ::skills::SkillBrassInstruments:
		if (itembonuses.brassMod == 0 && spellbonuses.brassMod == 0)
			effectmod = 10;
#ifdef TLP_STYLE
		else if (GetSkill(EQ::skills::SkillBrassInstruments) == 0)
			effectmod = 10;
#endif
		else if (itembonuses.brassMod > spellbonuses.brassMod)
			effectmod = itembonuses.brassMod;
		else
			effectmod = spellbonuses.brassMod;
		if (IsBardSong(spell_id))
			effectmod += aabonuses.brassMod;
		break;
	case EQ::skills::SkillSinging:
		if (itembonuses.singingMod == 0 && spellbonuses.singingMod == 0)
			effectmod = 10;
		else if (itembonuses.singingMod > spellbonuses.singingMod)
			effectmod = itembonuses.singingMod;
		else
			effectmod = spellbonuses.singingMod;
		if (IsBardSong(spell_id))
			effectmod += aabonuses.singingMod + spellbonuses.Amplification;
		break;
	default:
		effectmod = 10;
		return effectmod;
	}
	if (!RuleB(Character, UseSpellFileSongCap))
		effectmodcap += aabonuses.songModCap + spellbonuses.songModCap + itembonuses.songModCap;
	if (effectmod < 10)
		effectmod = 10;
	if (!nocap && effectmod > effectmodcap) // if the cap is calculated to be 0 using new rules, no cap.
		effectmod = effectmodcap;

	LogSpells("[{}]::GetInstrumentMod() spell=[{}] mod=[{}] modcap=[{}]\n", GetName(), spell_id, effectmod, effectmodcap);

	return effectmod;
}

int64 Client::CalcMaxEndurance()
{
	max_end = CalcBaseEndurance() + spellbonuses.Endurance + itembonuses.Endurance + aabonuses.Endurance;
	if (max_end < 0) {
		max_end = 0;
	}
	if (current_endurance > max_end) {
		current_endurance = max_end;
	}
	int end_perc_cap = spellbonuses.EndPercCap[0];
	if (end_perc_cap) {
		int curEnd_cap = (max_end * end_perc_cap) / 100;
		if (current_endurance > curEnd_cap || (spellbonuses.EndPercCap[1] && current_endurance > spellbonuses.EndPercCap[1])) {
			current_endurance = curEnd_cap;
		}
	}
	return max_end;
}

int64 Client::CalcBaseEndurance()
{
	bool CharmingPersonality = false;
	if (IsClient() && CastToClient()->GetCharmEffectID(EQ::invslot::slotPowerSource) == CharmCharmingPersonality && CastToClient()->IsItemRecastExpired(EQ::invslot::slotPowerSource))
	{
		CharmingPersonality = true;
	}

	int64 baseEndCalc = 500.0f * sqrt(( + (double)GetSTA() + (CharmingPersonality ? ((double)GetCHA()) / 2.0 : GetCHA()))/10.0);

	return std::max((int64)100, baseEndCalc);
}

int64 Client::CalcEnduranceRegen(bool bCombat)
{
	int64 base = 0;
	auto base_data = database.GetBaseData(GetLevel(), GetClass());
	if (base_data) {
		base = static_cast<int>(base_data->end_regen);
		if (!auto_attack && base > 0)
			base += base / 2;
	}

	base += itembonuses.EnduranceRegen + spellbonuses.EnduranceRegen + aabonuses.EnduranceRegen;

	base = base * AreaEndRegen + 0.5f;

	return base;
}

int32 Client::CalcEnduranceRegenCap()
{
	int32 cap = RuleI(Character, ItemEnduranceRegenCap);
	return (cap * RuleI(Character, EnduranceRegenMultiplier) / 100);
}

int32 Client::CalcItemATKCap()
{
	int32 cap = RuleI(Character, ItemATKCap) + itembonuses.ItemATKCap + spellbonuses.ItemATKCap + aabonuses.ItemATKCap;
	return cap;
}

int Client::GetRawACNoShield(int &shield_ac) const
{
	int ac = itembonuses.AC + spellbonuses.AC + aabonuses.AC;
	shield_ac = 0;
	const EQ::ItemInstance *inst = m_inv.GetItem(EQ::invslot::slotSecondary);
	if (inst) {
		if (inst->GetItem()->ItemType == EQ::item::ItemTypeShield) {
			ac -= inst->GetItem()->AC;
			shield_ac = inst->GetItem()->AC;
			for (uint8 i = EQ::invaug::SOCKET_BEGIN; i <= EQ::invaug::SOCKET_END; i++) {
				if (inst->GetAugment(i)) {
					ac -= inst->GetAugment(i)->GetItem()->AC;
					shield_ac += inst->GetAugment(i)->GetItem()->AC;
				}
			}
		}
	}
	return ac;
}
