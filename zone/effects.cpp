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
#include "../common/eqemu_logsys.h"
#include "../common/spdat.h"
#include "../common/misc_functions.h"

#include "client.h"
#include "entity.h"
#include "mob.h"

#include "string_ids.h"
#include "worldserver.h"
#include "zonedb.h"
#include "position.h"

float Mob::GetActSpellSynergy(uint16 spell_id, bool usecache) {
	return 1.0f;
}

int64 Mob::GetActSpellCorrection(uint16 spell_id, int64 value, bool isproc, bool istic, EQ::skills::SkillType skill) {
}

float Mob::GetActSpellRange(uint16 spell_id, float range, bool IsBard)
{
	float extrange = 100;

	extrange += GetFocusEffect(focusRange, spell_id);

	return (range * extrange) / 100;
}

int64 Mob::GetActDSDamage(uint16 spell_id, int64 value) {

	if (IsCorpse())
		return 0;

	value = GetActSpellCorrection(spell_id, value, false, false, spells[spell_id].skill);

	//if (IsNPC())
	//	value = EQ::Clamp(-1, (int)spells[spell_id].cast_time / (-2000), -3);

	value *= RollD20(offense(spells[spell_id].skill, true), 1.f, false);
	return value;
}

int64 Mob::GetActSpellDamage(uint16 spell_id, int64 value, Mob* target, bool isproc) {

	if (spells[spell_id].targettype == ST_Self)
		return value;

	if(IsCorpse())
		return 0;

	bool Instant = spells[spell_id].cast_time <= 0 || isproc;
	auto skill = IsBeneficialSpell(spell_id) ? EQ::skills::SkillMend : (isproc ? EQ::skills::SkillEvocation : spells[spell_id].skill);

	value = GetActSpellCorrection(spell_id, value, isproc, false, skill);

	if (IsNPC() && !IsMerc())
		value = EQ::Clamp(value, (int64)-40, (int64)(spells[spell_id].cast_time / (-25.0)));
	
	if (isproc && IsClient())
	{
		if (CastToClient()->GetCharmEffectID(EQ::invslot::slotPowerSource) == CharmSTRProcConvert && CastToClient()->IsItemRecastExpired(EQ::invslot::slotPowerSource))
		{
			skill = EQ::skills::SkillOffense;
		}
	}

	if (IsClient())
	{
		if (IsLifetapSpell(spell_id) && CastToClient()->GetCharmEffectID(EQ::invslot::slotPowerSource) == CharmLich && CastToClient()->IsItemRecastExpired(EQ::invslot::slotPowerSource))
		{
			value *= 2;
		}
	}
	
	value *= RollD20(offense(skill, Instant), target ? target->mitigation() : 1.f) * AntiMitigation();

	m_Range_CritRoll += GetCriticalChanceBonus(skill) / 100.0f;

	float ratio = 3.0f;
	bool override_crit = false;

	if (spells[spell_id].override_crit_chance > 0 && zone->random.Roll(spells[spell_id].override_crit_chance))
		override_crit = true;

	if (override_crit || m_Range_CritRoll >= 1.0f) {
		if(!override_crit)
			m_Range_CritRoll -= 1.0f;

		value *= ratio;

		if (IsClient())
			FilteredMessageString(this, Chat::SpellCrit, FilterSpellCrits, YOU_CRIT_BLAST, itoa(-value));

		if (target != this && target->IsClient())
			FilteredMessageString(this, Chat::SpellCrit, FilterSpellCrits, YOU_CRIT_BLAST, itoa(-value));
	}

	return value;
}

int64 Mob::GetActDoTDamage(uint16 spell_id, int64 value, Mob* target, bool isproc) {

	if (target == nullptr)
		return value;

	if(IsCorpse())
		return 0;

	auto skill = IsBeneficialSpell(spell_id) ? EQ::skills::SkillMend : (isproc ? EQ::skills::SkillEvocation : spells[spell_id].skill);

	value = GetActSpellCorrection(spell_id, value, isproc, true, skill);

	if (isproc && IsClient())
	{
		if (CastToClient()->GetCharmEffectID(EQ::invslot::slotPowerSource) == CharmSTRProcConvert && CastToClient()->IsItemRecastExpired(EQ::invslot::slotPowerSource))
		{
			skill = EQ::skills::SkillOffense;
		}
	}

	if (IsClient())
	{
		if (IsLifetapSpell(spell_id) && CastToClient()->GetCharmEffectID(EQ::invslot::slotPowerSource) == CharmLich && CastToClient()->IsItemRecastExpired(EQ::invslot::slotPowerSource))
		{
			value *= 2.5;
		}
	}

	value *= RollD20(offense(skill, true), target ? target->mitigation() : 1.f) * AntiMitigation();
	float chance = GetCriticalChanceBonus(skill) / 100.0f;

	if (chance > 0.0f && value != 0) {
		value *= 1.0f + (chance * 2.0f);
	}
	return value/3;
}

int64 Mob::GetActSpellHealing(uint16 spell_id, int64 value, Mob* target, bool isproc) {

	if (target == nullptr)
		target = this;

	bool npccast = false;

	bool Instant = spells[spell_id].cast_time <= 0 || isproc;
	auto skill = EQ::skills::SkillMend;

	value = GetActSpellCorrection(spell_id, value, isproc, false, skill);

	if (isproc && IsClient())
	{
		if (CastToClient()->GetCharmEffectID(EQ::invslot::slotPowerSource) == CharmSTRProcConvert && CastToClient()->IsItemRecastExpired(EQ::invslot::slotPowerSource))
		{
			skill = EQ::skills::SkillOffense;
		}
	}

	if (target->IsClient())
	{
		if (target->CastToClient()->GetCharmEffectID(EQ::invslot::slotPowerSource) == CharmLich && target->CastToClient()->IsItemRecastExpired(EQ::invslot::slotPowerSource))
		{
			value = 0;
		}
	}

	value *= RollD20(offense(skill, Instant), target ? target->mitigation() : 1.f);
	m_Range_CritRoll += GetCriticalChanceBonus(skill) / 100.0f;

	float ratio = 3.0f;
	bool override_crit = false;

	if (spells[spell_id].override_crit_chance > 0 && zone->random.Roll(spells[spell_id].override_crit_chance))
		override_crit = true;

	if (override_crit || m_Range_CritRoll >= 1.0f) {
		if(!override_crit)
			m_Range_CritRoll -= 1.0f;

		value *= ratio;

		if (IsClient())
			FilteredMessageString(this, Chat::SpellCrit, FilterSpellCrits, YOU_CRIT_HEAL, itoa(value));

		if (target != this && target->IsClient())
			FilteredMessageString(this, Chat::SpellCrit, FilterSpellCrits, YOU_CRIT_HEAL, itoa(value));
	}

	return value;
}

int64 Mob::GetActHoTHealing(uint16 spell_id, int64 value, Mob* target, bool isproc) {

	if (target == nullptr)
		target = this;

	bool npccast = false;

	auto skill = EQ::skills::SkillMend;

	value = GetActSpellCorrection(spell_id, value, isproc, true, skill);

	if (isproc && IsClient())
	{
		if (CastToClient()->GetCharmEffectID(EQ::invslot::slotPowerSource) == CharmSTRProcConvert && CastToClient()->IsItemRecastExpired(EQ::invslot::slotPowerSource))
		{
			skill = EQ::skills::SkillOffense;
		}
	}

	if (target->IsClient())
	{
		if (target->CastToClient()->GetCharmEffectID(EQ::invslot::slotPowerSource) == CharmLich && target->CastToClient()->IsItemRecastExpired(EQ::invslot::slotPowerSource))
		{
			value = 0;
		}
	}

	value *= RollD20(offense(skill, true), target ? target->mitigation() : 1.f);
	float chance = GetCriticalChanceBonus(skill) / 100.0f;

	if (chance > 0.0f && value != 0) {
		value *= 1.0f + (chance * 2.0f);
	}

	return value/3;
}

int64 Mob::GetExtraSpellAmt(uint16 spell_id, int64 extra_spell_amt, int64 base_spell_dmg)
{
	if (RuleB(Spells, FlatItemExtraSpellAmt))
		return extra_spell_amt;

	int total_cast_time = 0;

	if (spells[spell_id].recast_time >= spells[spell_id].recovery_time)
			total_cast_time = spells[spell_id].recast_time + spells[spell_id].cast_time;
	else
		total_cast_time = spells[spell_id].recovery_time + spells[spell_id].cast_time;

	if (total_cast_time > 0 && total_cast_time <= 2500)
		extra_spell_amt = extra_spell_amt*25/100;
	 else if (total_cast_time > 2500 && total_cast_time < 7000)
		 extra_spell_amt = extra_spell_amt*(167*((total_cast_time - 1000)/1000)) / 1000;
	 else
		 extra_spell_amt = extra_spell_amt * total_cast_time / 7000;

		if(extra_spell_amt*2 < base_spell_dmg)
			return 0;

		return extra_spell_amt;
}

int32 Client::GetActSpellCost(uint16 spell_id, int32 cost)
{
	//FrenziedDevastation doubles mana cost of all DD spells
	int16 FrenziedDevastation = itembonuses.FrenziedDevastation + spellbonuses.FrenziedDevastation + aabonuses.FrenziedDevastation;

	if (FrenziedDevastation && IsPureNukeSpell(spell_id))
		cost *= 2;

	// Formula = Unknown exact, based off a random percent chance up to mana cost(after focuses) of the cast spell
	if(itembonuses.Clairvoyance && spells[spell_id].classes[(GetClass()%17) - 1] >= GetLevel() - 5)
	{
		int16 mana_back = itembonuses.Clairvoyance * zone->random.Int(1, 100) / 100;
		// Doesnt generate mana, so best case is a free spell
		if(mana_back > cost)
			mana_back = cost;

		cost -= mana_back;
	}

	int spec = GetSpecializeSkillValue(spell_id);
	int PercentManaReduction = 0;
	if (spec)
		PercentManaReduction = 1 + spec / 20; // there seems to be some non-obvious rounding here, let's truncate for now.

	int16 focus_redux = GetFocusEffect(focusManaCost, spell_id);
	PercentManaReduction += focus_redux;

	cost -= cost * PercentManaReduction / 100;

	// Gift of Mana - reduces spell cost to 1 mana
	if(focus_redux >= 100) {
		int buff_max = GetMaxTotalSlots();
		for (int buffSlot = 0; buffSlot < buff_max; buffSlot++) {
			if (buffs[buffSlot].spellid == 0 || buffs[buffSlot].spellid >= SPDAT_RECORDS)
				continue;

			if(IsEffectInSpell(buffs[buffSlot].spellid, SE_ReduceManaCost)) {
				if(CalcFocusEffect(focusManaCost, buffs[buffSlot].spellid, spell_id) == 100)
					cost = 1;
			}
		}
	}

	if(cost < 0)
		cost = 0;

	return cost;
}

int32 Mob::GetActSpellDuration(uint16 spell_id, int32 duration)
{
	int increase = 100;
	increase += GetFocusEffect(focusSpellDuration, spell_id);
	int tic_inc = 0;
	tic_inc = GetFocusEffect(focusSpellDurByTic, spell_id);

	float focused = ((duration * increase) / 100.0f) + tic_inc;
	int ifocused = static_cast<int>(focused);

	ifocused *= 3;

	// 7.6 is rounded to 7, 8.6 is rounded to 9
	// 6 is 6, etc
	if (FCMP(focused, ifocused) || ifocused % 2) // equal or odd
		return ifocused;
	else // even and not equal round to odd
		return ifocused + 1;
}

int32 Client::GetActSpellCasttime(uint16 spell_id, int32 casttime)
{
	//int32 cast_reducer = 0;
	//cast_reducer += GetFocusEffect(focusSpellHaste, spell_id);

	//this function loops through the effects of spell_id many times
	//could easily be consolidated.

	//if (GetLevel() >= 51 && casttime >= 3000 && !BeneficialSpell(spell_id)
	//	&& (GetClass() == SHADOWKNIGHT || GetClass() == RANGER
	//		|| GetClass() == PALADIN || GetClass() == BEASTLORD ))
	//	cast_reducer += (GetLevel()-50)*3;

	//LIVE AA SpellCastingDeftness, QuickBuff, QuickSummoning, QuickEvacuation, QuickDamage

	//if (cast_reducer > RuleI(Spells, MaxCastTimeReduction))
	//	cast_reducer = RuleI(Spells, MaxCastTimeReduction);

	//casttime = (casttime*(100 - cast_reducer)/100);


	if (EQ::skills::IsBardInstrumentSkill(spells[spell_id].skill))
		return casttime;

	auto skill = IsBeneficialSpell(spell_id) ? EQ::skills::SkillMend : spells[spell_id].skill;

	int c1 = (casttime + spells[spell_id].recovery_time) / (GetHaste(skill)/100.f) - spells[spell_id].recovery_time;

	return std::max(c1, 333);
}

bool Client::TrainDiscipline(uint32 itemid) {

	//get the item info
	EQ::ItemData item = database.GetItem(itemid);
	if(item.ID == 0) {
		Message(Chat::Red, "Unable to find the tome you turned in!");
		Log(Logs::General, Logs::Error, "Unable to find turned in tome id %lu\n", (unsigned long)itemid);
		return(false);
	}

	if (!item.IsClassCommon() || item.ItemType != EQ::item::ItemTypeSpell) {
		Message(Chat::Red, "Invalid item type, you cannot learn from this item.");
		//summon them the item back...
		SummonItem(itemid);
		return(false);
	}

	//Need a way to determine the difference between a spell and a tome
	//so they cant turn in a spell and get it as a discipline
	//this is kinda a hack:
	if(!(
		item.Name[0] == 'T' &&
		item.Name[1] == 'o' &&
		item.Name[2] == 'm' &&
		item.Name[3] == 'e' &&
		item.Name[4] == ' '
		) && !(
		item.Name[0] == 'S' &&
		item.Name[1] == 'k' &&
		item.Name[2] == 'i' &&
		item.Name[3] == 'l' &&
		item.Name[4] == 'l' &&
		item.Name[5] == ':' &&
		item.Name[6] == ' '
		)) {
		Message(Chat::Red, "This item is not a tome.");
		//summon them the item back...
		SummonItem(itemid);
		return(false);
	}

	int myclass = GetClass();
	if(myclass == WIZARD || myclass == ENCHANTER || myclass == MAGICIAN || myclass == NECROMANCER) {
		Message(Chat::Red, "Your class cannot learn from this tome.");
		//summon them the item back...
		SummonItem(itemid);
		return(false);
	}

	//make sure we can train this...
	//can we use the item?
	uint32 cbit = 1 << (myclass-1);
	if(!(item.Classes & cbit)) {
		Message(Chat::Red, "Your class cannot learn from this tome.");
		//summon them the item back...
		SummonItem(itemid);
		return(false);
	}

	uint32 spell_id = item.Scroll.Effect;
	if(!IsValidSpell(spell_id)) {
		Message(Chat::Red, "This tome contains invalid knowledge.");
		return(false);
	}

	//can we use the spell?
	const SPDat_Spell_Struct &spell = spells[spell_id];
	uint8 level_to_use = spell.classes[myclass - 1];
	if(level_to_use == 255) {
		Message(Chat::Red, "Your class cannot learn from this tome.");
		//summon them the item back...
		SummonItem(itemid);
		return(false);
	}

	if(level_to_use > GetLevel()) {
		Message(Chat::Red, "You must be at least level %d to learn this discipline.", level_to_use);
		//summon them the item back...
		SummonItem(itemid);
		return(false);
	}

	//add it to PP.
	int r;
	for(r = 0; r < MAX_PP_DISCIPLINES; r++) {
		if(m_pp.disciplines.values[r] == spell_id) {
			Message(Chat::Red, "You already know this discipline.");
			//summon them the item back...
			SummonItem(itemid);
			return(false);
		} else if(m_pp.disciplines.values[r] == 0) {
			m_pp.disciplines.values[r] = spell_id;
			database.SaveCharacterDisc(this->CharacterID(), r, spell_id);
			SendDisciplineUpdate();
			Message(0, "You have learned a new discipline!");
			return(true);
		}
	}
	Message(Chat::Red, "You have learned too many disciplines and can learn no more.");
	return(false);
}

void Client::TrainDiscBySpellID(int32 spell_id)
{
	int i;
	for(i = 0; i < MAX_PP_DISCIPLINES; i++) {
		if(m_pp.disciplines.values[i] == 0) {
			m_pp.disciplines.values[i] = spell_id;
			database.SaveCharacterDisc(this->CharacterID(), i, spell_id);
			SendDisciplineUpdate();
			Message(Chat::Yellow, "You have learned a new combat ability!");
			return;
		}
	}
}

int Client::GetDiscSlotBySpellID(int32 spellid)
{
	int i;

	for(i = 0; i < MAX_PP_DISCIPLINES; i++)
	{
		if(m_pp.disciplines.values[i] == spellid)
			return i;
	}
	
	return -1;
}

void Client::SendDisciplineUpdate() {
	EQApplicationPacket app(OP_DisciplineUpdate, sizeof(Disciplines_Struct));
	Disciplines_Struct *d = (Disciplines_Struct*)app.pBuffer;
	memcpy(d, &m_pp.disciplines, sizeof(m_pp.disciplines));
	QueuePacket(&app);
}

bool Client::UseDiscipline(uint32 spell_id, uint32 target) {

	return false;

	// Dont let client waste a reuse timer if they can't use the disc
	if (IsStunned() || IsFeared() || IsMezzed() || IsAmnesiad() || IsPet())
	{
		return(false);
	}

	//make sure we have the spell...
	int r;
	for(r = 0; r < MAX_PP_DISCIPLINES; r++) {
		if(m_pp.disciplines.values[r] == spell_id)
			break;
	}
	if(r == MAX_PP_DISCIPLINES)
		return(false);	//not found.

	//make sure we can use it..
	if(!IsValidSpell(spell_id)) {
		Message(Chat::Red, "This tome contains invalid knowledge.");
		return(false);
	}

	//can we use the spell?
	const SPDat_Spell_Struct &spell = spells[spell_id];
	uint8 level_to_use = spell.classes[GetClass() - 1];
	if(level_to_use == 255) {
		Message(Chat::Red, "Your class cannot learn from this tome.");
		//should summon them a new one...
		return(false);
	}

	if(level_to_use > GetLevel()) {
		MessageString(Chat::Red, DISC_LEVEL_USE_ERROR);
		//should summon them a new one...
		return(false);
	}

	if(GetEndurance() < spell.EndurCost) {
		Message(11, "You are too fatigued to use this skill right now.");
		return(false);
	}

	// sneak attack discs require you to be hidden for 4 seconds before use
	if (spell.sneak && (!hidden || (hidden && (Timer::GetCurrentTime() - tmHidden) < 4000))) {
		MessageString(Chat::SpellFailure, SNEAK_RESTRICT);
		return false;
	}

	// the client does this check before calling CastSpell, should prevent discs being eaten
	if (spell.buffdurationformula != 0 && spell.targettype == ST_Self && HasDiscBuff())
		return false;

	//Check the disc timer
	pTimerType DiscTimer = pTimerDisciplineReuseStart + spell.EndurTimerIndex;
	if(!p_timers.Expired(&database, DiscTimer, false)) { // lets not set the reuse timer in case CastSpell fails (or we would have to turn off the timer, but CastSpell will set it as well)
		/*char val1[20]={0};*/	//unused
		/*char val2[20]={0};*/	//unused
		uint32 remain = p_timers.GetRemainingTime(DiscTimer);
		//MessageString(Chat::White, DISCIPLINE_CANUSEIN, ConvertArray((remain)/60,val1), ConvertArray(remain%60,val2));
		Message(0, "You can use this discipline in %d minutes %d seconds.", ((remain)/60), (remain%60));
		return(false);
	}

	if(spell.recast_time > 0)
	{
		uint32 reduced_recast = spell.recast_time / 1000;
		auto focus = GetFocusEffect(focusReduceRecastTime, spell_id);
		// do stupid stuff because custom servers.
		// we really should be able to just do the -= focus but since custom servers could have shorter reuse timers
		// we have to make sure we don't underflow the uint32 ...
		// and yes, the focus effect can be used to increase the durations (spell 38944)
		if (focus > reduced_recast) {
			reduced_recast = 0;
			if (GetPTimers().Enabled((uint32)DiscTimer))
				GetPTimers().Clear(&database, (uint32)DiscTimer);
		} else {
			reduced_recast -= focus;
		}

		if (reduced_recast > 0)
			CastSpell(spell_id, target, EQ::spells::CastingSlot::Discipline, -1, -1, 0, -1, (uint32)DiscTimer, reduced_recast);
		else{
			CastSpell(spell_id, target, EQ::spells::CastingSlot::Discipline);
			return true;
		}

		SendDisciplineTimer(spells[spell_id].EndurTimerIndex, reduced_recast);
	}
	else
	{
		CastSpell(spell_id, target, EQ::spells::CastingSlot::Discipline);
	}
	return(true);
}

uint32 Client::GetDisciplineTimer(uint32 timer_id) {
	pTimerType disc_timer_id = pTimerDisciplineReuseStart + timer_id;
	uint32 disc_timer = 0;
	if (GetPTimers().Enabled((uint32)disc_timer_id)) {
		disc_timer = GetPTimers().GetRemainingTime(disc_timer_id);
	}
	return disc_timer;
}

void Client::ResetDisciplineTimer(uint32 timer_id) {
	pTimerType disc_timer_id = pTimerDisciplineReuseStart + timer_id;
	if (GetPTimers().Enabled((uint32)disc_timer_id)) {
		GetPTimers().Clear(&database, (uint32)disc_timer_id);
	}
	SendDisciplineTimer(timer_id, 0);
}

void Client::SendDisciplineTimer(uint32 timer_id, uint32 duration)
{
	if (timer_id < MAX_DISCIPLINE_TIMERS)
	{
		auto outapp = new EQApplicationPacket(OP_DisciplineTimer, sizeof(DisciplineTimer_Struct));
		DisciplineTimer_Struct *dts = (DisciplineTimer_Struct *)outapp->pBuffer;
		dts->TimerID = timer_id;
		dts->Duration = duration;
		QueuePacket(outapp);
		safe_delete(outapp);
	}
}

/**
 * @param taunter
 * @param range
 * @param bonus_hate
 */
void EntityList::AETaunt(Client *taunter, float range, int32 bonus_hate)
{

	/**
	 * Live AE taunt range - Hardcoded.
	 */
	if (range == 0) {
		range = 40;
	}

	float range_squared = range * range;

	for (auto &it : entity_list.GetCloseMobList(taunter, range)) {
		Mob *them = it.second;

		if (!them->IsNPC()) {
			continue;
		}

		float z_difference = taunter->GetZ() - them->GetZ();
		if (z_difference < 0) {
			z_difference *= -1;
		}

		if (z_difference < 10
			&& taunter->IsAttackAllowed(them)
			&& DistanceSquaredNoZ(taunter->GetPosition(), them->GetPosition()) <= range_squared) {
			if (taunter->CheckLosFN(them)) {
				taunter->Taunt(them->CastToNPC(), true, 0, true, bonus_hate);
			}
		}
	}
}

// causes caster to hit every mob within dist range of center with
// spell_id.
// NPC spells will only affect other NPCs with compatible faction
void EntityList::AESpell(Mob *caster, Mob *center, uint16 spell_id, bool affect_caster, int16 resist_adjust, int *max_targets, bool isproc)
{
	const auto &cast_target_position =
		spells[spell_id].targettype == ST_Ring ?
		caster->GetTargetRingLocation() :
		static_cast<glm::vec3>(center->GetPosition());

	Mob       *current_mob = nullptr;
	bool      is_detrimental_spell = IsDetrimentalSpell(spell_id);
	bool      is_npc = caster->IsNPC();
	float     distance = caster->GetAOERange(spell_id);
	float     distance_squared = distance * distance;
	float     min_range2 = spells[spell_id].min_range * spells[spell_id].min_range;
	glm::vec2 min = { cast_target_position.x - distance, cast_target_position.y - distance };
	glm::vec2 max = { cast_target_position.x + distance, cast_target_position.y + distance };

	/**
	 * If using Old Rain Targets - there is no max target limitation
	 */
	if (RuleB(Spells, OldRainTargets)) {
		max_targets = nullptr;
	}

	/**
	 * Max AOE targets
	 */
	int max_targets_allowed = 0; // unlimited
	if (max_targets) { // rains pass this in since they need to preserve the count through waves
		max_targets_allowed = *max_targets;
	}
	else if (spells[spell_id].aemaxtargets) {
		max_targets_allowed = spells[spell_id].aemaxtargets;
	}
	else if (IsTargetableAESpell(spell_id) && is_detrimental_spell && !is_npc) {
		max_targets_allowed = 4;
	}

	int   target_hit_counter = 0;
	float distance_to_target = 0;

	LogAoeCast(
		"Close scan distance [{}] cast distance [{}]",
		RuleI(Range, MobCloseScanDistance),
		distance
	);

	for (auto &it : entity_list.GetCloseMobList(caster, distance)) {
		current_mob = it.second;

		if (!current_mob) {
			continue;
		}

		LogAoeCast("Checking AOE against mob [{}]", current_mob->GetCleanName());

		if (current_mob->IsClient() && !current_mob->CastToClient()->ClientFinishedLoading()) {
			continue;
		}

		if (current_mob == caster && !affect_caster) {
			continue;
		}

		if (spells[spell_id].targettype == ST_TargetAENoPlayersPets && current_mob->IsPetOwnerClient()) {
			continue;
		}

		if (spells[spell_id].targettype == ST_AreaClientOnly && !current_mob->IsClient()) {
			continue;
		}

		if (spells[spell_id].targettype == ST_AreaNPCOnly && !current_mob->IsNPC()) {
			continue;
		}

		/**
		 * Check PC / NPC
		 * 1 = PC
		 * 2 = NPC
		 */
		if (spells[spell_id].pcnpc_only_flag == 1 && !current_mob->IsClient() && !current_mob->IsMerc() &&
		    !current_mob->IsBot()) {
			continue;
		}

		if (spells[spell_id].pcnpc_only_flag == 2 &&
		    (current_mob->IsClient() || current_mob->IsMerc() || current_mob->IsBot())) {
			continue;
		}

		if (!IsWithinAxisAlignedBox(static_cast<glm::vec2>(current_mob->GetPosition()), min, max)) {
			continue;
		}

		distance_to_target = DistanceSquared(current_mob->GetPosition(), cast_target_position);

		if (distance_to_target > distance_squared) {
			continue;
		}

		if (distance_to_target < min_range2) {
			continue;
		}

		if (is_npc && current_mob->IsNPC() &&
			spells[spell_id].targettype != ST_AreaNPCOnly) {    //check npc->npc casting
			FACTION_VALUE faction_value = current_mob->GetReverseFactionCon(caster);
			if (is_detrimental_spell) {
				//affect mobs that are on our hate list, or
				//which have bad faction with us
				if (
					!(caster->CheckAggro(current_mob) ||
						faction_value == FACTION_THREATENLY ||
						faction_value == FACTION_SCOWLS)) {
					continue;
				}
			}
			else {
				//only affect mobs we would assist.
				if (!(faction_value <= FACTION_AMIABLE)) {
					continue;
				}
			}
		}

		/**
		 * Finally, make sure they are within range
		 */
		if (is_detrimental_spell) {
			if (!caster->IsAttackAllowed(current_mob, true)) {
				continue;
			}
			if (center && !spells[spell_id].npc_no_los && !center->CheckLosFN(current_mob)) {
				continue;
			}
			if (!center && !spells[spell_id].npc_no_los && !caster->CheckLosFN(
				caster->GetTargetRingX(),
				caster->GetTargetRingY(),
				caster->GetTargetRingZ(),
				current_mob->GetSize())) {
				continue;
			}
		}
		else {

			/**
			 * Check to stop casting beneficial ae buffs (to wit: bard songs) on enemies...
			 * This does not check faction for beneficial AE buffs... only agro and attackable.
			 * I've tested for spells that I can find without problem, but a faction-based
			 * check may still be needed. Any changes here should also reflect in BardAEPulse()
			 */
			if (caster->IsAttackAllowed(current_mob, true)) {
				continue;
			}
			if (caster->CheckAggro(current_mob)) {
				continue;
			}
		}

		//current_mob->CalcSpellPowerDistanceMod(spell_id, distance_to_target);
		//caster->SpellOnTarget(spell_id, current_mob, false, true, resist_adjust, isproc);

		if (max_targets_allowed) { // if we have a limit, increment count
			target_hit_counter++;
			if (target_hit_counter >= max_targets_allowed) // we done
				break;
		}

		current_mob->CalcSpellPowerDistanceMod(spell_id, distance_to_target);
		caster->SpellOnTarget(spell_id, current_mob, false, true, resist_adjust, isproc);
	}
	LogAoeCast("Done iterating [{}]", caster->GetCleanName());

	if (max_targets && max_targets_allowed) {
		*max_targets = *max_targets - target_hit_counter;
	}
}

void EntityList::MassGroupBuff(Mob *caster, Mob *center, uint16 spell_id, bool affect_caster, bool isproc)
{
	Mob   *current_mob         = nullptr;
	float distance             = caster->GetAOERange(spell_id);
	float distance_squared     = distance * distance;
	bool  is_detrimental_spell = IsDetrimentalSpell(spell_id);

	for (auto &it : entity_list.GetCloseMobList(caster, distance)) {
		current_mob = it.second;

		/**
		 * Skip center
		 */
		if (current_mob == center) {
			continue;
		}

		/**
		 * Skip self
		 */
		if (current_mob == caster && !affect_caster) {
			continue;
		}

		if (DistanceSquared(center->GetPosition(), current_mob->GetPosition()) > distance_squared) {    //make sure they are in range
			continue;
		}

		/**
		 * Pets
		 */
		if (current_mob->IsNPC()) {
			Mob *owner = current_mob->GetOwner();
			if (owner) {
				if (!owner->IsClient()) {
					continue;
				}
			}
			else {
				continue;
			}
		}

		if (is_detrimental_spell) {
			continue;
		}
		caster->SpellOnTarget(spell_id, current_mob, false, false, 0, isproc);
	}
}

/**
 * Causes caster to hit every mob within dist range of center with a bard pulse of spell_id
 * NPC spells will only affect other NPCs with compatible faction
 *
 * @param caster
 * @param center
 * @param spell_id
 * @param affect_caster
 */
void EntityList::AEBardPulse(
	Mob *caster,
	Mob *center,
	uint16 spell_id,
	bool affect_caster)
{
	Mob   *current_mob         = nullptr;
	float distance             = caster->GetAOERange(spell_id);
	float distance_squared     = distance * distance;
	bool  is_detrimental_spell = IsDetrimentalSpell(spell_id);
	bool  is_npc               = caster->IsNPC();

	for (auto &it : entity_list.GetCloseMobList(caster, distance)) {
		current_mob = it.second;

		/**
		 * Skip self
		 */
		if (current_mob == center) {
			continue;
		}

		if (current_mob == caster && !affect_caster) {
			continue;
		}

		if (DistanceSquared(center->GetPosition(), current_mob->GetPosition()) > distance_squared) {    //make sure they are in range
			continue;
		}

		/**
		 * check npc->npc casting
		 */
		if (is_npc && current_mob->IsNPC()) {
			FACTION_VALUE faction = current_mob->GetReverseFactionCon(caster);
			if (is_detrimental_spell) {
				//affect mobs that are on our hate list, or
				//which have bad faction with us
				if (!(caster->CheckAggro(current_mob) || faction == FACTION_THREATENLY || faction == FACTION_SCOWLS)) {
					continue;
				}
			}
			else {
				//only affect mobs we would assist.
				if (!(faction <= FACTION_AMIABLE)) {
					continue;
				}
			}
		}

		/**
		 * LOS
		 */
		if (is_detrimental_spell) {
			if (!center->CheckLosFN(current_mob)) {
				continue;
			}
		}
		else { // check to stop casting beneficial ae buffs (to wit: bard songs) on enemies...
			// See notes in AESpell() above for more info.
			if (caster->IsAttackAllowed(current_mob, true)) {
				continue;
			}
			if (caster->CheckAggro(current_mob)) {
				continue;
			}
		}

		current_mob->BardPulse(spell_id, caster);
	}
	if (caster->IsClient()) {
		caster->CastToClient()->CheckSongSkillIncrease(spell_id);
	}
}

/**
 * Rampage - Normal and Duration rampages
 * NPCs handle it differently in Mob::Rampage
 *
 * @param attacker
 * @param distance
 * @param Hand
 * @param count
 * @param is_from_spell
 */
void EntityList::AEAttack(
	Mob *attacker,
	float distance,
	int Hand,
	int count,
	bool is_from_spell)
{
	Mob   *current_mob     = nullptr;
	float distance_squared = distance * distance;
	int   hit_count        = 0;

	for (auto &it : entity_list.GetCloseMobList(attacker, distance)) {
		current_mob = it.second;

		if (current_mob->IsNPC()
			&& current_mob != attacker //this is not needed unless NPCs can use this
			&& (attacker->IsAttackAllowed(current_mob))
			&& current_mob->GetRace() != 216 && current_mob->GetRace() != 472 /* dont attack horses */
			&& (DistanceSquared(current_mob->GetPosition(), attacker->GetPosition()) <= distance_squared)
			) {

			if (!attacker->IsClient() || attacker->GetClass() == MONK || attacker->GetClass() == RANGER) {
				attacker->Attack(current_mob, Hand, false, false, is_from_spell);
			}
			else {
				attacker->CastToClient()->DoAttackRounds(current_mob, Hand, is_from_spell);
			}

			hit_count++;
			if (count != 0 && hit_count >= count) {
				return;
			}
		}
	}
}


