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
#include "../common/features.h"
#include "../common/rulesys.h"
#include "../common/string_util.h"
#include "../common/servertalk.h"
#include "client.h"
#include "groups.h"
#include "mob.h"
#include "raids.h"

#include "queryserv.h"
#include "worldserver.h"
#include "quest_parser_collection.h"
#include "lua_parser.h"
#include "string_ids.h"

#ifdef BOTS
#include "bot.h"
#endif

extern QueryServ* QServ;
extern WorldServer worldserver;

static uint32 ScaleAAXPBasedOnCurrentAATotal(int earnedAA, uint32 add_aaxp)
{
	float baseModifier = RuleR(AA, ModernAAScalingStartPercent);
	int aaMinimum = RuleI(AA, ModernAAScalingAAMinimum);
	int aaLimit = RuleI(AA, ModernAAScalingAALimit);

	// Are we within the scaling window?
	if (earnedAA >= aaLimit || earnedAA < aaMinimum)
	{
		LogDebug("Not within AA scaling window");

		// At or past the limit.  We're done.
		return add_aaxp;
	}

	// We're not at the limit yet.  How close are we?
	int remainingAA = aaLimit - earnedAA;

	// We might not always be X - 0
	int scaleRange = aaLimit - aaMinimum;

	// Normalize and get the effectiveness based on the range and the character's
	// current spent AA.
	float normalizedScale = (float)remainingAA / scaleRange;

	// Scale.
	uint32 totalWithExpMod = add_aaxp * (baseModifier / 100) * normalizedScale;

	// Are we so close to the scale limit that we're earning more XP without scaling?  This
	// will happen when we get very close to the limit.  In this case, just grant the unscaled
	// amount.
	if (totalWithExpMod < add_aaxp)
	{
		return add_aaxp;
	}

	Log(Logs::Detail,
		Logs::None,
		"Total before the modifier %d :: NewTotal %d :: ScaleRange: %d, SpentAA: %d, RemainingAA: %d, normalizedScale: %0.3f",
		add_aaxp, totalWithExpMod, scaleRange, earnedAA, remainingAA, normalizedScale);

	return totalWithExpMod;
}

static uint32 MaxBankedGroupLeadershipPoints(int Level)
{
	if(Level < 35)
		return 4;

	if(Level < 51)
		return 6;

	return 8;
}

static uint32 MaxBankedRaidLeadershipPoints(int Level)
{
	if(Level < 45)
		return 6;

	if(Level < 55)
		return 8;

	return 10;
}

uint32 Client::CalcEXP(uint8 conlevel) {

	uint32 in_add_exp = EXP_FORMULA;


	if((XPRate != 0))
		in_add_exp = static_cast<uint32>(in_add_exp * (static_cast<float>(XPRate) / 100.0f));

	float totalmod = 1.0;
	float zemmod = 1.0;
	//get modifiers
	if(RuleR(Character, ExpMultiplier) >= 0){
		totalmod *= RuleR(Character, ExpMultiplier);
	}

	if(zone->newzone_data.zone_exp_multiplier >= 0){
		zemmod *= zone->newzone_data.zone_exp_multiplier;
	}

	if(RuleB(Character,UseRaceClassExpBonuses))
	{
		if(GetBaseRace() == HALFLING){
			totalmod *= 1.05;
		}

		if(GetClass() == ROGUE || GetClass() == WARRIOR){
			totalmod *= 1.05;
		}
	}

	if(zone->IsHotzone())
	{
		totalmod += RuleR(Zone, HotZoneBonus);
	}

	in_add_exp = uint32(float(in_add_exp) * totalmod * zemmod);

	if(RuleB(Character,UseXPConScaling))
	{
		if (conlevel != 0xFF) {
			switch (conlevel)
			{
			case CON_GRAY:
				in_add_exp = 0;
				return 0;
			case CON_GREEN:
				in_add_exp = in_add_exp * RuleI(Character, GreenModifier) / 100;
				break;
			case CON_LIGHTBLUE:
				in_add_exp = in_add_exp * RuleI(Character, LightBlueModifier)/100;
				break;
			case CON_BLUE:
				in_add_exp = in_add_exp * RuleI(Character, BlueModifier)/100;
				break;
			case CON_WHITE:
				in_add_exp = in_add_exp * RuleI(Character, WhiteModifier)/100;
				break;
			case CON_YELLOW:
				in_add_exp = in_add_exp * RuleI(Character, YellowModifier)/100;
				break;
			case CON_RED:
				in_add_exp = in_add_exp * RuleI(Character, RedModifier)/100;
				break;
			}
		}
	}

	float aatotalmod = 1.0;
	if(zone->newzone_data.zone_exp_multiplier >= 0){
		aatotalmod *= zone->newzone_data.zone_exp_multiplier;
	}



	if(RuleB(Character,UseRaceClassExpBonuses))
	{
		if(GetBaseRace() == HALFLING){
			aatotalmod *= 1.05;
		}

		if(GetClass() == ROGUE || GetClass() == WARRIOR){
			aatotalmod *= 1.05;
		}
	}

	if(RuleB(Zone, LevelBasedEXPMods)){
		if(zone->level_exp_mod[GetLevel()].ExpMod){
			in_add_exp *= zone->level_exp_mod[GetLevel()].ExpMod;
		}
	}

	return in_add_exp;
}

uint32 Client::GetExperienceForKill(Mob *against)
{
#ifdef LUA_EQEMU
	uint32 lua_ret = 0;
	bool ignoreDefault = false;
	lua_ret = LuaParser::Instance()->GetExperienceForKill(this, against, ignoreDefault);

	if (ignoreDefault) {
		return lua_ret;
	}
#endif

	//if (against && against->IsNPC()) {
	//	int level = std::max((int)against->GetLevel(),1);
	//	int level2 = std::max((int)GetLevel(),1);

	//	return 1000*std::pow(1.2,level-level2);
	//	//return EXP_FORMULA;
	//}

	if (against && against->IsNPC()) {
		return std::max((int)against->GetLevel(),1);
	}

	return 0;
}

float static GetConLevelModifierPercent(uint8 conlevel)
{
	switch (conlevel)
	{
	case CON_GREEN:
		return (float)RuleI(Character, GreenModifier) / 100;
		break;
	case CON_LIGHTBLUE:
		return (float)RuleI(Character, LightBlueModifier) / 100;
		break;
	case CON_BLUE:
		return (float)RuleI(Character, BlueModifier) / 100;
		break;
	case CON_WHITE:
		return (float)RuleI(Character, WhiteModifier) / 100;
		break;
	case CON_YELLOW:
		return (float)RuleI(Character, YellowModifier) / 100;
		break;
	case CON_RED:
		return (float)RuleI(Character, RedModifier) / 100;
		break;
	default:
		return 0;
	}
}

void Client::CalculateNormalizedAAExp(uint32 &add_aaxp, uint8 conlevel, bool resexp)
{
	// Functionally this is the same as having the case in the switch, but this is
	// cleaner to read.
	if (CON_GRAY == conlevel || resexp)
	{
		add_aaxp = 0;
		return;
	}

	// For this, we ignore the provided value of add_aaxp because it doesn't
	// apply.  XP per AA is normalized such that there are X white con kills
	// per AA.

	uint32 whiteConKillsPerAA = RuleI(AA, NormalizedAANumberOfWhiteConPerAA);
	uint32 xpPerAA = RuleI(AA, ExpPerPoint);

	float colorModifier = GetConLevelModifierPercent(conlevel);
	float percentToAAXp = (float)m_epp.perAA / 100;

	// Normalize the amount of AA XP we earned for this kill.
	add_aaxp = percentToAAXp * (xpPerAA / (whiteConKillsPerAA / colorModifier));
}

void Client::CalculateStandardAAExp(uint32 &add_aaxp, uint8 conlevel, bool resexp)
{
	if (!resexp)
	{
		//if XP scaling is based on the con of a monster, do that now.
		if (RuleB(Character, UseXPConScaling))
		{
			if (conlevel != 0xFF && !resexp)
			{
				add_aaxp *= GetConLevelModifierPercent(conlevel);
			}
		}
	}	//end !resexp

	float aatotalmod = 1.0;
	if (zone->newzone_data.zone_exp_multiplier >= 0) {
		aatotalmod *= zone->newzone_data.zone_exp_multiplier;
	}

	// Shouldn't race not affect AA XP?
	if (RuleB(Character, UseRaceClassExpBonuses))
	{
		if (GetBaseRace() == HALFLING) {
			aatotalmod *= 1.05;
		}

		if (GetClass() == ROGUE || GetClass() == WARRIOR) {
			aatotalmod *= 1.05;
		}
	}

	// why wasn't this here? Where should it be?
	if (zone->IsHotzone())
	{
		aatotalmod += RuleR(Zone, HotZoneBonus);
	}

	if (RuleB(Zone, LevelBasedEXPMods)) {
		if (zone->level_exp_mod[GetLevel()].ExpMod) {
			add_aaxp *= zone->level_exp_mod[GetLevel()].AAExpMod;
		}
	}

	add_aaxp = (uint32)(RuleR(Character, AAExpMultiplier) * add_aaxp * aatotalmod);
}

void Client::CalculateLeadershipExp(uint32 &add_exp, uint8 conlevel)
{
	if (IsLeadershipEXPOn() && (conlevel == CON_BLUE || conlevel == CON_WHITE || conlevel == CON_YELLOW || conlevel == CON_RED))
	{
		add_exp = static_cast<uint32>(static_cast<float>(add_exp) * 0.8f);

		if (GetGroup())
		{
			if (m_pp.group_leadership_points < MaxBankedGroupLeadershipPoints(GetLevel())
				&& RuleI(Character, KillsPerGroupLeadershipAA) > 0)
			{
				uint32 exp = GROUP_EXP_PER_POINT / RuleI(Character, KillsPerGroupLeadershipAA);
				Client *mentoree = GetGroup()->GetMentoree();
				if (GetGroup()->GetMentorPercent() && mentoree &&
					mentoree->GetGroupPoints() < MaxBankedGroupLeadershipPoints(mentoree->GetLevel()))
				{
					uint32 mentor_exp = exp * (GetGroup()->GetMentorPercent() / 100.0f);
					exp -= mentor_exp;
					mentoree->AddLeadershipEXP(mentor_exp, 0); // ends up rounded down
					mentoree->MessageString(Chat::LeaderShip, GAIN_GROUP_LEADERSHIP_EXP);
				}
				if (exp > 0)
				{
					// possible if you mentor 100% to the other client
					AddLeadershipEXP(exp, 0); // ends up rounded up if mentored, no idea how live actually does it
					MessageString(Chat::LeaderShip, GAIN_GROUP_LEADERSHIP_EXP);
				}
			}
			else
			{
				MessageString(Chat::LeaderShip, MAX_GROUP_LEADERSHIP_POINTS);
			}
		}
		else
		{
			Raid *raid = GetRaid();
			// Raid leaders CAN NOT gain group AA XP, other group leaders can though!
			if (raid->IsLeader(this))
			{
				if (m_pp.raid_leadership_points < MaxBankedRaidLeadershipPoints(GetLevel())
					&& RuleI(Character, KillsPerRaidLeadershipAA) > 0)
				{
					AddLeadershipEXP(0, RAID_EXP_PER_POINT / RuleI(Character, KillsPerRaidLeadershipAA));
					MessageString(Chat::LeaderShip, GAIN_RAID_LEADERSHIP_EXP);
				}
				else
				{
					MessageString(Chat::LeaderShip, MAX_RAID_LEADERSHIP_POINTS);
				}
			}
			else
			{
				if (m_pp.group_leadership_points < MaxBankedGroupLeadershipPoints(GetLevel())
					&& RuleI(Character, KillsPerGroupLeadershipAA) > 0)
				{
					uint32 group_id = raid->GetGroup(this);
					uint32 exp = GROUP_EXP_PER_POINT / RuleI(Character, KillsPerGroupLeadershipAA);
					Client *mentoree = raid->GetMentoree(group_id);
					if (raid->GetMentorPercent(group_id) && mentoree &&
						mentoree->GetGroupPoints() < MaxBankedGroupLeadershipPoints(mentoree->GetLevel()))
					{
						uint32 mentor_exp = exp * (raid->GetMentorPercent(group_id) / 100.0f);
						exp -= mentor_exp;
						mentoree->AddLeadershipEXP(mentor_exp, 0);
						mentoree->MessageString(Chat::LeaderShip, GAIN_GROUP_LEADERSHIP_EXP);
					}
					if (exp > 0)
					{
						AddLeadershipEXP(exp, 0);
						MessageString(Chat::LeaderShip, GAIN_GROUP_LEADERSHIP_EXP);
					}
				}
				else
				{
					MessageString(Chat::LeaderShip, MAX_GROUP_LEADERSHIP_POINTS);
				}
			}
		}
	}
}

double Client::MobLevelToExp(int level)
{
	return 1000 * std::pow(1.2, (double)(level - std::max((int)GetLevel(), 1)));
}

void Client::CalculateExp(uint32 in_add_exp, uint32 &add_exp, uint32 &add_aaxp, uint8 conlevel, bool resexp) // used for non-group/raid exp
{
	add_exp = 0;

	//if (!resexp && (XPRate != 0))
	//{
	//	add_exp = static_cast<uint32>(in_add_exp * (static_cast<float>(XPRate) / 100.0f));
	//}

	// Make sure it was initialized.
	add_aaxp = in_add_exp;

	if (!resexp)
	{
		//figure out how much of this goes to AAs
		//add_aaxp = add_exp * m_epp.perAA / 100;

		//take that amount away from regular exp
		//add_exp -= add_aaxp;

		float totalmod = 1.0;
		float zemmod = 1.0;

		//get modifiers
		//if (RuleR(Character, ExpMultiplier) >= 0) {
		//	totalmod *= RuleR(Character, ExpMultiplier);
		//}

		//add the zone exp modifier.
		//if (zone->newzone_data.zone_exp_multiplier >= 0) {
		//	zemmod *= zone->newzone_data.zone_exp_multiplier;
		//}

		//if (RuleB(Character, UseRaceClassExpBonuses))
		//{
		//	if (GetBaseRace() == HALFLING) {
		//		totalmod *= 1.05;
		//	}
//
		//	if (GetClass() == ROGUE || GetClass() == WARRIOR) {
		//		totalmod *= 1.05;
		//	}
		//}

		//add hotzone modifier if one has been set.
		//if (zone->IsHotzone())
		//{
		//	totalmod += RuleR(Zone, HotZoneBonus);
		//}

		//add_exp = uint32(float(add_exp) * totalmod * zemmod);

		//if XP scaling is based on the con of a monster, do that now.
		//if (RuleB(Character, UseXPConScaling))
		//{
		//	if (conlevel != 0xFF && !resexp)
		//	{
		//		add_exp = add_exp * GetConLevelModifierPercent(conlevel);
		//	}
		//}

		//add_aaxp = MobLevelToExp(add_aaxp);

		// Calculate any changes to leadership experience.
		//CalculateLeadershipExp(add_aaxp, conlevel);
	}	//end !resexp

	//if (RuleB(Zone, LevelBasedEXPMods)) {
	//	if (zone->level_exp_mod[GetLevel()].ExpMod) {
	//		add_exp *= zone->level_exp_mod[GetLevel()].ExpMod;
	//	}
	//}

	add_exp = GetEXP() + add_exp;
}

void Client::AddEXP(uint32 in_add_exp, uint8 conlevel, bool resexp) {

	this->EVENT_ITEM_ScriptStopReturn();

	//if (GetPVP(false))
	//	return;

	if (GetFeigned())
		return;

	uint32 exp = 0;
	uint32 aaexp = 0;

	//if (m_epp.perAA<0 || m_epp.perAA>100)
	//	m_epp.perAA=0;	// stop exploit with sanity check

	// Calculate regular XP
	//if (GetLevel() < 60)
	//{
	//	float expToAdd = (float)in_add_exp * 2.5f;
	//	in_add_exp = expToAdd;
	//}

	CalculateExp(in_add_exp, exp, aaexp, conlevel, resexp);

	// Calculate regular AA XP
	//if (!RuleB(AA, NormalizedAAEnabled))
	//{
	//	CalculateStandardAAExp(aaexp, conlevel, resexp);
	//}
	//else
	//{
	//	CalculateNormalizedAAExp(aaexp, conlevel, resexp);
	//}

	// Are we also doing linear AA acceleration?
	//if (RuleB(AA, ModernAAScalingEnabled) && aaexp > 0)
	//{
	//	aaexp = ScaleAAXPBasedOnCurrentAATotal(GetAAPoints(), aaexp);
	//}

	// Get current AA XP total
	uint32 had_aaexp = GetAAXP();

	// Add it to the XP we just earned.
	aaexp += had_aaexp;

	// Make sure our new total (existing + just earned) isn't lower than the
	// existing total.  If it is, we overflowed the bounds of uint32 and wrapped.
	// Reset to the existing total.
	if (aaexp < had_aaexp)
	{
		aaexp = had_aaexp;	//watch for wrap
	}

	// AA Sanity Checking for players who set aa exp and deleveled below allowed aa level.
	//if (GetLevel() <= 50 && m_epp.perAA > 0) {
	//	Message(15, "You are below the level allowed to gain AA Experience. AA Experience set to 0%");
	//	aaexp = 0;
	//	m_epp.perAA = 0;
	//}

	// Now update our character's normal and AA xp
	SetEXP(exp, aaexp, resexp);
}

void Client::SetEXP(uint32 set_exp, uint32 set_aaxp, bool isrezzexp) {
	LogDebug("Attempting to Set Exp for [{}] (XP: [{}], AAXP: [{}], Rez: [{}])", this->GetCleanName(), set_exp, set_aaxp, isrezzexp ? "true" : "false");

	auto max_AAXP = GetRequiredAAExperience();
	uint32 i = 0;
	uint32 membercount = 0;
	if(GetGroup())
	{
		for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
			if (GetGroup()->members[i] != nullptr) {
				membercount++;
			}
		}
	}

	if ((set_aaxp) > (m_pp.expAA)) {
		uint32 aaxp_gained = set_aaxp - m_pp.expAA;
		float aaxp_percent = (float)((float)aaxp_gained / (float)(max_AAXP))*(float)100; //AAEXP needed for level
		std::string exp_amount_message = "";

		exp_amount_message = StringFormat("%u", aaxp_gained);

		if (isrezzexp) {
			//if (RuleI(Character, ShowExpValues) > 0) 
				Message(Chat::Experience, "You regain %s experience from resurrection.", exp_amount_message.c_str());
			//else MessageString(MT_Experience, REZ_REGAIN);
		} else {
			if (membercount > 1) {
				//if (RuleI(Character, ShowExpValues) > 0) 
					Message(Chat::Experience, "You have gained %s party experience!", exp_amount_message.c_str());
				//else MessageString(MT_Experience, GAIN_GROUPXP);
			}
			else if (IsRaidGrouped()) {
				//if (RuleI(Character, ShowExpValues) > 0) 
					Message(Chat::Experience, "You have gained %s raid experience!", exp_amount_message.c_str());
				//else MessageString(MT_Experience, GAIN_RAIDEXP);
			} 
			else {
				//if (RuleI(Character, ShowExpValues) > 0) 
					Message(Chat::Experience, "You have gained %s experience!", exp_amount_message.c_str());
				//else MessageString(MT_Experience, GAIN_XP);				
			}
		}
	}

	//see if we gained any AAs
	if (set_aaxp >= max_AAXP) {
		//record how many points we have
		uint32 last_unspentAA = m_pp.aapoints;

		//figure out how many AA points we get from the exp were setting
		m_pp.aapoints = set_aaxp / max_AAXP;
		LogDebug("Calculating additional AA Points from AAXP for [{}]: [{}] / [{}] = [{}] points", this->GetCleanName(), set_aaxp, max_AAXP, (float)set_aaxp / (float)max_AAXP);

		//get remainder exp points, set in PP below
		set_aaxp = set_aaxp - (max_AAXP * m_pp.aapoints);

		//add in how many points we had
		m_pp.aapoints += last_unspentAA;

		char val1[20]={0};
		MessageString(Chat::Experience, GAIN_ABILITY_POINT, ConvertArray(m_pp.aapoints, val1),m_pp.aapoints == 1 ? "" : "(s)");	//You have gained an ability point! You now have %1 ability point%2.
		if (RuleB(AA, SoundForAAEarned)) {
			SendSound();
		}
		
		/* QS: PlayerLogAARate */
		if (RuleB(QueryServ, PlayerLogAARate)){
			int add_points = (m_pp.aapoints - last_unspentAA);
			std::string query = StringFormat("INSERT INTO `qs_player_aa_rate_hourly` (char_id, aa_count, hour_time) VALUES (%i, %i, UNIX_TIMESTAMP() - MOD(UNIX_TIMESTAMP(), 3600)) ON DUPLICATE KEY UPDATE `aa_count` = `aa_count` + %i", this->CharacterID(), add_points, add_points);
			QServ->SendQuery(query.c_str());
		}
	}
	m_pp.expAA = set_aaxp;

	if(GetLevel() != targetlevel)
	{
		char val1[20]={0};		
		if (targetlevel - GetLevel() == 1)
		{
			if (targetlevel % 10 == 0)
			{
				if(GetRawSkill(EQ::skills::SkillAlchemy) > 0 ||
					GetRawSkill(EQ::skills::SkillFishing) > 1 ||
					GetRawSkill(EQ::skills::SkillTailoring) > 0 ||
					GetRawSkill(EQ::skills::SkillBlacksmithing) > 0 ||
					GetRawSkill(EQ::skills::SkillFletching) > 0 ||
					GetRawSkill(EQ::skills::SkillBrewing) > 0 ||
					GetRawSkill(EQ::skills::SkillBaking) > 0 ||
					GetRawSkill(EQ::skills::SkillPottery) > 0 ||
					GetRawSkill(EQ::skills::SkillJewelryMaking) > 0 ||
					GetRawSkill(EQ::skills::SkillTinkering) > 25 ||
					GetRawSkill(EQ::skills::SkillMakePoison) > 1
					)
				{

					std::string msg = StringFormat("%s is now level %i! (With Tradeskills)", GetCleanName(), targetlevel);
					worldserver.SendChannelMessage(0, 0, ChatChannel_Broadcast, 0, 0, 100, msg.c_str());
				}
				else
				{
					std::string msg = StringFormat("%s is now level %i! (Without Tradeskills!)", GetCleanName(), targetlevel);
					worldserver.SendChannelMessage(0, 0, ChatChannel_Broadcast, 0, 0, 100, msg.c_str());
				}
			}
			MessageString(Chat::Experience, GAIN_LEVEL, ConvertArray(targetlevel, val1));
		}
		else
			Message(Chat::Experience, "Welcome to level %i!", targetlevel);

		SetLevel(targetlevel);
	}

	SendAlternateAdvancementStats();
	auto outapp = new EQApplicationPacket(OP_ExpUpdate, sizeof(ExpUpdate_Struct));
	ExpUpdate_Struct* eu = (ExpUpdate_Struct*)outapp->pBuffer;
	eu->exp = (uint32)(330.0f * (targetleveltest - GetLevel()));
	FastQueuePacket(&outapp);
}

void Client::SetLevel(uint8 set_level, bool command)
{

	auto outapp = new EQApplicationPacket(OP_LevelUpdate, sizeof(LevelUpdate_Struct));
	LevelUpdate_Struct* lu = (LevelUpdate_Struct*)outapp->pBuffer;
	lu->level = set_level;
	if(m_pp.level2 != 0)
		lu->level_old = m_pp.level2;
	else
		lu->level_old = level;

	level = set_level;

	if (set_level != m_pp.level)
	{
		if(GetLevel() >= 10)  
		{
			AddAA(1071, EQ::Clamp(GetLevel() / 10, 0, 4));  // EDGE Mnemonic Retention AA
			AddAA(141, EQ::Clamp(GetLevel() / 10, 0, 6));  // Craft Mastery AA
		}
		
		SendClearAA();
		SendAlternateAdvancementTable();
		SendAlternateAdvancementPoints();
		SendAlternateAdvancementStats();
	}

	if(IsRaidGrouped()) {
		Raid *r = this->GetRaid();
		if(r){
			r->UpdateLevel(GetName(), set_level);
		}
	}
	if(set_level > m_pp.level2) {
		if(m_pp.level2 == 0)
			m_pp.points += 5;
		else
			m_pp.points += (5 * (set_level - m_pp.level2));

		m_pp.level2 = set_level;
	}
	if(set_level > m_pp.level) {
		parse->EventPlayer(EVENT_LEVEL_UP, this, "", 0);
		/* QS: PlayerLogLevels */
		if (RuleB(QueryServ, PlayerLogLevels)){
			std::string event_desc = StringFormat("Leveled UP :: to Level:%i from Level:%i in zoneid:%i instid:%i", set_level, m_pp.level, this->GetZoneID(), this->GetInstanceID());
			QServ->PlayerLogEvent(Player_Log_Levels, this->CharacterID(), event_desc); 
		}
	}
	else if (set_level < m_pp.level){
		/* QS: PlayerLogLevels */
		if (RuleB(QueryServ, PlayerLogLevels)){
			std::string event_desc = StringFormat("Leveled DOWN :: to Level:%i from Level:%i in zoneid:%i instid:%i", set_level, m_pp.level, this->GetZoneID(), this->GetInstanceID());
			QServ->PlayerLogEvent(Player_Log_Levels, this->CharacterID(), event_desc);
		}
	}

	m_pp.level = set_level;
	if (command){
		m_pp.exp = GetEXPForLevel(set_level);
		Message(Chat::Yellow, "Welcome to level %i!", set_level);
		lu->exp = 0;
	}
	else {
		float tmpxp = (float) ( (float) m_pp.exp - GetEXPForLevel( GetLevel() )) / ( (float) GetEXPForLevel(GetLevel()+1) - GetEXPForLevel(GetLevel()));
		lu->exp = (uint32)(330.0f * tmpxp);
	}
	QueuePacket(outapp);
	safe_delete(outapp);
	this->SendAppearancePacket(AT_WhoLevel, set_level); // who level change

	LogInfo("Setting Level for [{}] to [{}]", GetName(), set_level);

	CalcBonuses();

	if(!RuleB(Character, HealOnLevel)) {
		int64 mhp = CalcMaxHP();
		if(GetHP() > mhp)
			SetHP(mhp);
	}
	else {
		SetHP(CalcMaxHP()); // Why not, lets give them a free heal
	}

	//if (RuleI(World, PVPMinLevel) > 0 && level >= RuleI(World, PVPMinLevel) && m_pp.pvp == 0) SetPVP(true);	

	DoTributeUpdate();
	SendHPUpdate();
	SetMana(CalcMaxMana());
	UpdateWho();

	UpdateMercLevel();

	SmartScribeSpells();

	//for (auto npc : entity_list.GetNPCList())
	//{
	//	if (npc.second)
	//	{
	//		npc.second->SendAppearancePacket(AT_WhoLevel, GetLevel(), false, false, this);
	//	}
	//}

	//LevelChangeInstances();

	Save();
}

void Client::SmartScribeSpells()
{
	if(GetClass() < 17){
		int book_slot = GetNextAvailableSpellBookSlot();
		int spell_id = 0;
		int count = 0;

		for ( ; spell_id < SPDAT_RECORDS && book_slot < EQ::spells::SPELLBOOK_SIZE; ++spell_id) {
			if (book_slot == -1) {
				Message(
					13,
					"Unable to scribe spell %s (%i) to spellbook: no more spell book slots available.",
					((spell_id >= 0 && spell_id < SPDAT_RECORDS) ? spells[spell_id].name : "Out-of-range"),
					spell_id
				);
				break;
			}
			if (spell_id < 0 || spell_id >= SPDAT_RECORDS) {
				Message(Chat::Red, "FATAL ERROR: Spell id out-of-range (id: %i, min: 0, max: %i)", spell_id, SPDAT_RECORDS);
				return;
			}
			if (book_slot < 0 || book_slot >= EQ::spells::SPELLBOOK_SIZE) {
				Message(Chat::Red, "FATAL ERROR: Book slot out-of-range (slot: %i, min: 0, max: %i)", book_slot, EQ::spells::SPELLBOOK_SIZE);
				return;
			}

			if (spells[spell_id].classes[WARRIOR] == 0) // check if spell exists
				continue;
			if (spells[spell_id].classes[GetPP().class_ - 1] > GetLevel()) // maximum level
				continue;

			uint16 spell_id_ = (uint16)spell_id;
			if ((spell_id_ != spell_id) || (spell_id != spell_id_)) {
				Message(Chat::Red, "FATAL ERROR: Type conversion data loss with spell_id (%i != %u)", spell_id, spell_id_);
				return;
			}
			if (!HasSpellScribed(spell_id)) { // isn't a discipline & we don't already have it scribed
				ScribeSpell(spell_id_, book_slot);
				book_slot = GetNextAvailableSpellBookSlot(book_slot);
			}
		}
	}
}

// Note: The client calculates exp separately, we cant change this function
// Add: You can set the values you want now, client will be always sync :) - Merkur
double Client::GetEXPForLevel(uint16 check_level)
{
	return (std::pow(1.2, check_level)-1) * GetRequiredAAExperience();
}

void Client::AddLevelBasedExp(uint8 exp_percentage, uint8 max_level, bool ignore_mods) 
{ 
	uint32	award;
	uint32	xp_for_level;

	if (exp_percentage > 100) 
	{ 
		exp_percentage = 100; 
	} 

	if (!max_level || GetLevel() < max_level)
	{ 
		max_level = GetLevel(); 
	} 

	xp_for_level = GetEXPForLevel(max_level + 1) - GetEXPForLevel(max_level);
	award = xp_for_level * exp_percentage / 100; 

	if(RuleB(Zone, LevelBasedEXPMods) && !ignore_mods)
	{
		if(zone->level_exp_mod[GetLevel()].ExpMod)
		{
			award *= zone->level_exp_mod[GetLevel()].ExpMod;
		}
	}

	uint32 newexp = GetEXP() + award;
	SetEXP(newexp, GetAAXP());
}

void Group::SplitExp(uint32 exp, Mob* other) {
	//if( other->CastToNPC()->MerchantType != 0 ) // Ensure NPC isn't a merchant
	//	return;

	if (!other)
		return;

	if(other->GetOwner() && other->GetOwner()->IsClient()) // Ensure owner isn't pc
		return;

	if (leader == nullptr)
		return;

	int32 i;
	uint32 groupexp = exp;
	uint8 membercount = 0;
	int32 membersInRange = 0;
	uint8 maxlevel = 1;


	//for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
	//	if (members[i] != nullptr) {
	//		if (members[i]->IsClient() && members[i]->CastToClient()->GetPVP())
	//			return;
	//	}
	//}

	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (members[i] != nullptr) {
			if(members[i]->GetLevel() > maxlevel)
				maxlevel = members[i]->GetLevel();

			membercount++;
		}
	}

	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (members[i] != nullptr) {

			if (DistanceNoZ(members[i]->GetPosition(), other->GetPosition()) <= 300.0f)
			{
				membersInRange++;
			}
		}
	}

	uint32 expVal = exp;

	expVal *= EQ::Clamp((1 + 0.1 * (membersInRange - 1)), 1., 1.5);

	//float groupmod;
	//if (membercount > 1 && membercount < 6)
	//	groupmod = 1 + .2*(membercount - 1); //2members=1.2exp, 3=1.4, 4=1.6, 5=1.8
	//else if (membercount == 6)
	//	groupmod = 2.16;
	//else
	//	groupmod = 1.0;
	//if(membercount > 1 &&  membercount <= 6)
	//	groupexp += (uint32)((float)exp * groupmod * (RuleR(Character, GroupExpMultiplier)));

	//int conlevel = Mob::GetLevelCon(maxlevel, other->GetLevel());
	//if(conlevel == CON_GRAY)
	//	return;	//no exp for greenies...

	if (membercount == 0)
		return;

	//if(zone->expansion > 2)
	//{
		expVal *= 0.75;
	//}
	//else
	//{
	//	expVal /= membercount;
	//}

	for (i = 0; i < MAX_GROUP_MEMBERS; i++) {
		if (members[i] != nullptr && members[i]->IsClient()) // If Group Member is Client
		{
			Client *cmember = members[i]->CastToClient();
			cmember->AddEXP( expVal, 0 );
			//Client *cmember = members[i]->CastToClient();
			//// add exp + exp cap
			//int16 diff = cmember->GetLevel() - maxlevel;
			//int16 maxdiff = -(cmember->GetLevel()*15/10 - cmember->GetLevel());
			//	if(maxdiff > -5)
			//		maxdiff = -5;
			//if (diff >= (maxdiff)) { /*Instead of person who killed the mob, the person who has the highest level in the group*/
			//	uint32 tmp = (cmember->GetLevel()+3) * (cmember->GetLevel()+3) * 75 * 35 / 10;
			//	uint32 tmp2 = groupexp / membercount;
			//	cmember->AddEXP( tmp < tmp2 ? tmp : tmp2, conlevel );
			//}
		}
	}
}

void Raid::SplitExp(uint32 exp, Mob* other) {

	return;

	//if( other->CastToNPC()->MerchantType != 0 ) // Ensure NPC isn't a merchant
	//	return;

	if(other->GetOwner() && other->GetOwner()->IsClient()) // Ensure owner isn't pc
		return;

	uint32 groupexp = exp;
	uint8 membercount = 0;
	uint8 maxlevel = 1;

	for (int i = 0; i < MAX_RAID_MEMBERS; i++) {
		if (members[i].member != nullptr) {
			if(members[i].member->GetLevel() > maxlevel)
				maxlevel = members[i].member->GetLevel();

			membercount++;
		}
	}

	//groupexp = (uint32)((float)groupexp * (1.0f-(RuleR(Character, RaidExpMultiplier))));

	//int conlevel = Mob::GetLevelCon(maxlevel, other->GetLevel());
	//if(conlevel == CON_GRAY)
	//	return;	//no exp for greenies...

	if (membercount == 0)
		return;

	for (unsigned int x = 0; x < MAX_RAID_MEMBERS; x++) {
		if (members[x].member != nullptr) // If Group Member is Client
		{
			Client *cmember = members[x].member;
			cmember->AddEXP( exp / membercount, 0 );
			//Client *cmember = members[x].member;
			//// add exp + exp cap
			//int16 diff = cmember->GetLevel() - maxlevel;
			//int16 maxdiff = -(cmember->GetLevel()*15/10 - cmember->GetLevel());
			//if(maxdiff > -5)
			//	maxdiff = -5;
			//if (diff >= (maxdiff)) { /*Instead of person who killed the mob, the person who has the highest level in the group*/
			//	uint32 tmp = (cmember->GetLevel()+3) * (cmember->GetLevel()+3) * 75 * 35 / 10;
			//	uint32 tmp2 = (groupexp / membercount) + 1;
			//	cmember->AddEXP( tmp < tmp2 ? tmp : tmp2, conlevel );
			//}
		}
	}
}

void Client::SetLeadershipEXP(uint32 group_exp, uint32 raid_exp) {
	while(group_exp >= GROUP_EXP_PER_POINT) {
		group_exp -= GROUP_EXP_PER_POINT;
		m_pp.group_leadership_points++;
		MessageString(Chat::LeaderShip, GAIN_GROUP_LEADERSHIP_POINT);
	}
	while(raid_exp >= RAID_EXP_PER_POINT) {
		raid_exp -= RAID_EXP_PER_POINT;
		m_pp.raid_leadership_points++;
		MessageString(Chat::LeaderShip, GAIN_RAID_LEADERSHIP_POINT);
	}

	m_pp.group_leadership_exp = group_exp;
	m_pp.raid_leadership_exp = raid_exp;

	SendLeadershipEXPUpdate();
}

void Client::AddLeadershipEXP(uint32 group_exp, uint32 raid_exp) {
	SetLeadershipEXP(GetGroupEXP() + group_exp, GetRaidEXP() + raid_exp);
}

void Client::SendLeadershipEXPUpdate() {
	auto outapp = new EQApplicationPacket(OP_LeadershipExpUpdate, sizeof(LeadershipExpUpdate_Struct));
	LeadershipExpUpdate_Struct* eu = (LeadershipExpUpdate_Struct *) outapp->pBuffer;

	eu->group_leadership_exp = m_pp.group_leadership_exp;
	eu->group_leadership_points = m_pp.group_leadership_points;
	eu->raid_leadership_exp = m_pp.raid_leadership_exp;
	eu->raid_leadership_points = m_pp.raid_leadership_points;

	FastQueuePacket(&outapp);
}

uint32 Client::GetCharMaxLevelFromQGlobal() {
	QGlobalCache *char_c = nullptr;
	char_c = this->GetQGlobals();

	std::list<QGlobal> globalMap;
	uint32 ntype = 0;

	if(char_c) {
		QGlobalCache::Combine(globalMap, char_c->GetBucket(), ntype, this->CharacterID(), zone->GetZoneID());
	}

	auto iter = globalMap.begin();
	uint32 gcount = 0;
	while(iter != globalMap.end()) {
		if((*iter).name.compare("CharMaxLevel") == 0){
			return atoi((*iter).value.c_str());
		} 
		++iter;
		++gcount;
	}

	return 0;
}

uint32 Client::GetCharMaxLevelFromBucket()
{
	uint32      char_id = this->CharacterID();
	std::string query   = StringFormat("SELECT value FROM data_buckets WHERE `key` = '%i-CharMaxLevel'", char_id);
	auto        results = database.QueryDatabase(query);
	if (!results.Success()) {
		LogError("Data bucket for CharMaxLevel for char ID [{}] failed", char_id);
		return 0;
	}

	if (results.RowCount() > 0) {
		auto row = results.begin();
		return atoi(row[0]);
	}
	return 0;
}

uint32 Client::GetRequiredAAExperience() {
#ifdef LUA_EQEMU
	uint32 lua_ret = 0;
	bool ignoreDefault = false;
	lua_ret = LuaParser::Instance()->GetRequiredAAExperience(this, ignoreDefault);

	if (ignoreDefault) {
		return lua_ret;
	}
#endif

	return 10000;
}
