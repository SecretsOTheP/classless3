/**
 * EQEmulator: Everquest Server Emulator
 * Copyright (C) 2001-2018 EQEmulator Development Team (https://github.com/EQEmu/Server)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY except by those people which sell it, which
 * are required to give you total support for your newly bought product;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "npc_scale_manager.h"
#include "../common/string_util.h"

#include "zone.h"

extern Zone* zone;

/**
 * @param npc 
 */
void NpcScaleManager::ScaleNPC(NPC * npc, float setpower)
{

}

bool NpcScaleManager::LoadScaleData()
{
	auto results = database.QueryDatabase(
		"SELECT "
		"type,"
		"level,"
		"ac,"
		"hp,"
		"accuracy,"
		"slow_mitigation,"
		"attack,"
		"strength,"
		"stamina,"
		"dexterity,"
		"agility,"
		"intelligence,"
		"wisdom,"
		"charisma,"
		"magic_resist,"
		"cold_resist,"
		"fire_resist,"
		"poison_resist,"
		"disease_resist,"
		"corruption_resist,"
		"physical_resist,"
		"min_dmg,"
		"max_dmg,"
		"hp_regen_rate,"
		"attack_delay,"
		"spell_scale,"
		"heal_scale,"
		"special_abilities"
		" FROM `npc_scale_global_base`"
	);

	for (auto row = results.begin(); row != results.end(); ++row) {
		global_npc_scale scale_data;

		scale_data.type              = atoi(row[0]);
		scale_data.level             = atoi(row[1]);
		scale_data.ac                = atoi(row[2]);
		scale_data.hp                = atoi(row[3]);
		scale_data.accuracy          = atoi(row[4]);
		scale_data.slow_mitigation   = atoi(row[5]);
		scale_data.attack            = atoi(row[6]);
		scale_data.strength          = atoi(row[7]);
		scale_data.stamina           = atoi(row[8]);
		scale_data.dexterity         = atoi(row[9]);
		scale_data.agility           = atoi(row[10]);
		scale_data.intelligence      = atoi(row[11]);
		scale_data.wisdom            = atoi(row[12]);
		scale_data.charisma          = atoi(row[13]);
		scale_data.magic_resist      = atoi(row[14]);
		scale_data.cold_resist       = atoi(row[15]);
		scale_data.fire_resist       = atoi(row[16]);
		scale_data.poison_resist     = atoi(row[17]);
		scale_data.disease_resist    = atoi(row[18]);
		scale_data.corruption_resist = atoi(row[19]);
		scale_data.physical_resist   = atoi(row[20]);
		scale_data.min_dmg           = atoi(row[21]);
		scale_data.max_dmg           = atoi(row[22]);
		scale_data.hp_regen_rate     = atoi(row[23]);
		scale_data.attack_delay      = atoi(row[24]);
		scale_data.spell_scale       = atoi(row[25]);
		scale_data.heal_scale        = atoi(row[26]);

		if (row[25]) {
			scale_data.special_abilities = row[27];
		}

		npc_global_base_scaling_data.insert(
			std::make_pair(
				std::make_pair(scale_data.type, scale_data.level),
				scale_data
			)
		);
	}

	LogNPCScaling("Global Base Scaling Data Loaded");

	return true;
}

/**
 * @param npc_type
 * @param npc_level
 * @return NpcScaleManager::global_npc_scale
 */
NpcScaleManager::global_npc_scale NpcScaleManager::GetGlobalScaleDataForTypeLevel(int8 npc_type, int npc_level)
{
	auto iter = npc_global_base_scaling_data.find(std::make_pair(npc_type, npc_level));
	if (iter != npc_global_base_scaling_data.end()) {
		return iter->second;
	}

	return {};
}

/**
 * @param level
 * @param npc_class
 * @return
 */
uint32 NpcScaleManager::GetClassLevelDamageMod(uint32 level, uint32 npc_class)
{
	uint32 multiplier = 0;

	switch (npc_class) {
		case WARRIOR: {
			if (level < 20) {
				multiplier = 220;
			}
			else if (level < 30) {
				multiplier = 230;
			}
			else if (level < 40) {
				multiplier = 250;
			}
			else if (level < 53) {
				multiplier = 270;
			}
			else if (level < 57) {
				multiplier = 280;
			}
			else if (level < 60) {
				multiplier = 290;
			}
			else if (level < 70) {
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
			if (level < 70) {
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
			if (level < 35) {
				multiplier = 210;
			}
			else if (level < 45) {
				multiplier = 220;
			}
			else if (level < 51) {
				multiplier = 230;
			}
			else if (level < 56) {
				multiplier = 240;
			}
			else if (level < 60) {
				multiplier = 250;
			}
			else if (level < 68) {
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
			if (level < 51) {
				multiplier = 180;
			}
			else if (level < 58) {
				multiplier = 190;
			}
			else if (level < 70) {
				multiplier = 200;
			}
			else {
				multiplier = 210;
			}
			break;
		}
		case RANGER: {
			if (level < 58) {
				multiplier = 200;
			}
			else if (level < 70) {
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
			if (level < 70) {
				multiplier = 120;
			}
			else {
				multiplier = 127;
			}
			break;
		}
		default: {
			if (level < 35) {
				multiplier = 210;
			}
			else if (level < 45) {
				multiplier = 220;
			}
			else if (level < 51) {
				multiplier = 230;
			}
			else if (level < 56) {
				multiplier = 240;
			}
			else if (level < 60) {
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

/**
 * @param npc
 * @return int8
 */
int8 NpcScaleManager::GetNPCScalingTier(NPC *&npc)
{
	return npc->GetNPCScaleTier();
}

/**
 * @param npc
 * @return std::string
 */
std::string NpcScaleManager::GetNPCScalingTypeName(NPC *&npc)
{
	int8 scaling_type = GetNPCScalingTier(npc);

	if (scaling_type == 10) {
		return "Named";
	}

	if (scaling_type == 20) {
		return "Raid";
	}

	return "Trash";
}

/**
 * Determines based on minimum criteria if NPC is auto scaled for certain things to be scaled like
 * special abilities. We use this so we don't blindly assume we want things to be applied
 *
 * @param npc
 * @return
 */
bool NpcScaleManager::IsAutoScaled(NPC *npc)
{
	return npc->GetNPCScaleTier() > 0;
}

/**
 * Returns false if scaling data not found
 * @param npc
 * @return
 */
bool NpcScaleManager::ApplyGlobalBaseScalingToNPCStatically(NPC *&npc)
{
	int8 npc_type  = GetNPCScalingTier(npc);
	int  npc_level = npc->GetLevel();

	global_npc_scale scale_data = GetGlobalScaleDataForTypeLevel(npc_type, npc_level);

	if (!scale_data.level) {
		LogNPCScaling(
			"NpcScaleManager::ApplyGlobalBaseScalingToNPCStatically NPC: [{}] - scaling data not found for type: [{}] level: [{}]",
			npc->GetCleanName(),
			npc_type,
			npc_level
		);

		return false;
	}

	std::string query = StringFormat(
		"UPDATE `npc_types` SET "
		"AC = %i, "
		"hp = %i, "
		"Accuracy = %i, "
		"slow_mitigation = %i, "
		"ATK = %i, "
		"STR = %i, "
		"STA = %i, "
		"DEX = %i, "
		"AGI = %i, "
		"_INT = %i, "
		"WIS = %i, "
		"CHA = %i, "
		"MR = %i, "
		"CR = %i, "
		"FR = %i, "
		"PR = %i, "
		"DR = %i, "
		"Corrup = %i, "
		"PhR = %i, "
		"mindmg = %i, "
		"maxdmg = %i, "
		"hp_regen_rate = %i, "
		"attack_delay = %i, "
		"spellscale = %i, "
		"healscale = %i, "
		"special_abilities = '%s' "
		"WHERE `id` = %i",
		scale_data.ac,
		scale_data.hp,
		scale_data.accuracy,
		scale_data.slow_mitigation,
		scale_data.attack,
		scale_data.strength,
		scale_data.stamina,
		scale_data.dexterity,
		scale_data.agility,
		scale_data.intelligence,
		scale_data.wisdom,
		scale_data.charisma,
		scale_data.magic_resist,
		scale_data.cold_resist,
		scale_data.fire_resist,
		scale_data.poison_resist,
		scale_data.disease_resist,
		scale_data.corruption_resist,
		scale_data.physical_resist,
		scale_data.min_dmg,
		scale_data.max_dmg,
		scale_data.hp_regen_rate,
		scale_data.attack_delay,
		scale_data.spell_scale,
		scale_data.heal_scale,
		EscapeString(scale_data.special_abilities).c_str(),
		npc->GetNPCTypeID()
	);

	auto results = database.QueryDatabase(query);

	return results.Success();
}

/**
 * Returns false if scaling data not found
 * @param npc
 * @return
 */
bool NpcScaleManager::ApplyGlobalBaseScalingToNPCDynamically(NPC *&npc)
{
	int8 npc_type  = GetNPCScalingTier(npc);
	int  npc_level = npc->GetLevel();

	global_npc_scale scale_data = GetGlobalScaleDataForTypeLevel(npc_type, npc_level);

	if (!scale_data.level) {
		LogNPCScaling(
			"NpcScaleManager::ApplyGlobalBaseScalingToNPCDynamically NPC: [{}] - scaling data not found for type: [{}] level: [{}]",
			npc->GetCleanName(),
			npc_type,
			npc_level
		);

		return false;
	}

	std::string query = StringFormat(
		"UPDATE `npc_types` SET "
		"AC = 0, "
		"hp = 0, "
		"Accuracy = 0, "
		"slow_mitigation = 0, "
		"ATK = 0, "
		"STR = 0, "
		"STA = 0, "
		"DEX = 0, "
		"AGI = 0, "
		"_INT = 0, "
		"WIS = 0, "
		"CHA = 0, "
		"MR = 0, "
		"CR = 0, "
		"FR = 0, "
		"PR = 0, "
		"DR = 0, "
		"Corrup = 0, "
		"PhR = 0, "
		"mindmg = 0, "
		"maxdmg = 0, "
		"hp_regen_rate = 0, "
		"attack_delay = 0, "
		"spellscale = 0, "
		"healscale = 0, "
		"special_abilities = '' "
		"WHERE `id` = %i",
		npc->GetNPCTypeID()
	);

	auto results = database.QueryDatabase(query);

	return results.Success();
}
