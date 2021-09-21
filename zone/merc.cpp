
#include "merc.h"
#include "client.h"
#include "corpse.h"
#include "entity.h"
#include "groups.h"
#include "mob.h"

#include "../common/eqemu_logsys.h"
#include "../common/eq_packet_structs.h"
#include "../common/eq_constants.h"
#include "../common/skills.h"
#include "../common/spdat.h"
#include "../common/extprofile.h"

#include "zone.h"
#include "string_ids.h"

#include "../common/string_util.h"
#include "../common/rulesys.h"

extern volatile bool is_zone_loaded;

#if EQDEBUG >= 12
	#define MercAI_DEBUG_Spells	25
#elif EQDEBUG >= 9
	#define MercAI_DEBUG_Spells	10
#else
	#define MercAI_DEBUG_Spells	-1
#endif

Merc::Merc(const NPCType* d, float x, float y, float z, float heading)
: NPC(d, nullptr, glm::vec4(x, y, z, heading), GravityBehavior::Water, false), check_target_timer(2000)
{
	_OwnerClientVersion = static_cast<unsigned int>(EQ::versions::ClientVersion::Titanium);

	_medding = false;
	_suspended = false;
	p_depop = false;
	_check_confidence = false;
	_lost_confidence = false;
	_hatedCount = 0;


	// Private "base stats" Members
	_characterID = 0;


	_MercID = 0;
	_MercTemplateID = 0;
	_MercType = 0;
	_MercSubType = 0;
	_ProficiencyID = 0;
	_TierID = 0;
	_CostFormula = 0;
	_NameType = 0;

	memset(&m_pp, 0, sizeof(PlayerProfile_Struct));
	memset(&m_epp, 0, sizeof(ExtendedProfile_Struct));

	memset(equipment, 0, sizeof(equipment));

	SetMercID(0);
	SetStance(EQ::constants::stanceBalanced);

	if (GetClass() == ROGUE)
		evade_timer.Start();

	int r;
	for (r = 0; r <= EQ::skills::HIGHEST_SKILL; r++) {
		skills[r] = database.GetSkillCap(GetClass(), (EQ::skills::SkillType)r, GetLevel());
	}

	size = d->size;
	CalcBonuses();
	SetHP(GetMaxHP());
	SetMana(GetMaxMana());
	SetEndurance(GetMaxEndurance());
}

Merc::~Merc() {
}

int64 Merc::CalcAlcoholPhysicalEffect()
{
	return 0;
}

int64 Merc::CalcSTR()
{
	int64 val = m_pp.STR + itembonuses.STR + spellbonuses.STR + CalcAlcoholPhysicalEffect();
	int64 mod = aabonuses.STR;
	STR = val + mod;
	if (STR < 1) {
		STR = 1;
	}
	return (STR);
}

int64 Merc::CalcSTA()
{
	int64 val = m_pp.STA + itembonuses.STA + spellbonuses.STA + CalcAlcoholPhysicalEffect();;
	int64 mod = aabonuses.STA;
	STA = val + mod;
	if (STA < 1) {
		STA = 1;
	}

	return (STA);
}

int64 Merc::CalcAGI()
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

int64 Merc::CalcDEX()
{
	int64 val = m_pp.DEX + itembonuses.DEX + spellbonuses.DEX - CalcAlcoholPhysicalEffect();;
	int64 mod = aabonuses.DEX;
	DEX = val + mod;
	if (DEX < 1) {
		DEX = 1;
	}

	return (DEX);
}

int64 Merc::CalcINT()
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

int64 Merc::CalcWIS()
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

int64 Merc::CalcCHA()
{
	int64 val = m_pp.CHA + itembonuses.CHA + spellbonuses.CHA;
	int64 mod = aabonuses.CHA;
	CHA = val + mod;
	if (CHA < 1) {
		CHA = 1;
	}
	return (CHA);
}

float Merc::CalcPOWER()
{

}

float Merc::CalcNORMAL()
{

}

float Merc::CalcNORMAL_Legacy()
{

}

//The AA multipliers are set to be 5, but were 2 on WR
//The resistant discipline which I think should be here is implemented
//in Mob::ResistSpell
int32	Merc::CalcMR()
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

int32	Merc::CalcFR()
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

int32	Merc::CalcDR()
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

int32	Merc::CalcPR()
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

int32	Merc::CalcCR()
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

void Merc::CalcBonuses()
{
	memset(&itembonuses, 0, sizeof(StatBonuses));
	memset(&spellbonuses, 0, sizeof(StatBonuses));
	memset(&aabonuses, 0, sizeof(StatBonuses));
	CalcItemBonuses(&itembonuses);
	CalcSpellBonuses(&spellbonuses);
	CalcAABonuses(&aabonuses);

	CalcPOWER();
	CalcNORMAL();

	//ProcessItemCaps(); // caps that depend on spell/aa bonuses

	CalcAC();

	CalcSTR();
	CalcSTA();
	CalcDEX();
	CalcAGI();
	CalcINT();
	CalcWIS();
	CalcCHA();

	CalcMR();
	CalcFR();
	CalcDR();
	CalcPR();
	CalcCR();

	CalcMaxHP();
	CalcMaxMana();
	CalcMaxEndurance();
	CalcPetBonuses();

	rooted = FindType(SE_Root);

	CalcSynergy();

}
void Merc::CalcSynergy()
{
	for (unsigned int i = 0; i < EQ::spells::SPELL_GEM_COUNT; ++i) {
		m_SYNERGY[i] = IsValidSpell(m_pp.mem_spells[i]) ? GetActSpellSynergy(m_pp.mem_spells[i], false) : 1.0f;
	}
}


float Merc::GetActSpellSynergy(uint16 spell_id, bool usecache) {

}

float Merc::GetDefaultSize() {

	float MercSize = GetSize();

	switch(this->GetRace())
	{
		case 1: // Humans
			MercSize = 6.0;
			break;
		case 2: // Barbarian
			MercSize = 7.0;
			break;
		case 3: // Erudite
			MercSize = 6.0;
			break;
		case 4: // Wood Elf
			MercSize = 5.0;
			break;
		case 5: // High Elf
			MercSize = 6.0;
			break;
		case 6: // Dark Elf
			MercSize = 5.0;
			break;
		case 7: // Half Elf
			MercSize = 5.5;
			break;
		case 8: // Dwarf
			MercSize = 4.0;
			break;
		case 9: // Troll
			MercSize = 8.0;
			break;
		case 10: // Ogre
			MercSize = 9.0;
			break;
		case 11: // Halfling
			MercSize = 3.5;
			break;
		case 12: // Gnome
			MercSize = 3.0;
			break;
		case 128: // Iksar
			MercSize = 6.0;
			break;
		case 130: // Vah Shir
			MercSize = 7.0;
			break;
		case 330: // Froglok
			MercSize = 5.0;
			break;
		case 522: // Drakkin
			MercSize = 5.0;
			break;
		default:
			MercSize = 6.0;
			break;
	}

	return MercSize;
}

int Merc::CalcRecommendedLevelBonus(uint8 level, uint8 reclevel, int basestat)
{
	if( (reclevel > 0) && (level < reclevel) )
	{
		int32 statmod = (level * 10000 / reclevel) * basestat;

		if( statmod < 0 )
		{
			statmod -= 5000;
			return (statmod/10000);
		}
		else
		{
			statmod += 5000;
			return (statmod/10000);
		}
	}

	return 0;
}

void Merc::CalcItemBonuses(StatBonuses* newbon) {
	//memset assumed to be done by caller.

	SetShieldEquiped(false);
	SetTwoHandBluntEquiped(false);
	SetTwoHanderEquipped(false);

	unsigned int i;
	// Update: MainAmmo should only calc skill mods (TODO: Check for other cases)
	for (i = EQ::invslot::BONUS_BEGIN; i <= EQ::invslot::BONUS_SKILL_END; i++) {
		const EQ::ItemInstance* inst = m_inv[i];
		if(inst == 0)
			continue;
		AddItemBonuses(inst, newbon, false, false, 0, (i == EQ::invslot::slotAmmo));

		//These are given special flags due to how often they are checked for various spell effects.
		const EQ::ItemData *item = inst->GetItem();
		if (i == EQ::invslot::slotSecondary && (item && item->ItemType == EQ::item::ItemTypeShield))
			SetShieldEquiped(true);
		else if (i == EQ::invslot::slotPrimary && (item && item->ItemType == EQ::item::ItemType2HBlunt)) {
			SetTwoHandBluntEquiped(true);
			SetTwoHanderEquipped(true);
		}
		else if (i == EQ::invslot::slotPrimary && (item && (item->ItemType == EQ::item::ItemType2HSlash || item->ItemType == EQ::item::ItemType2HPiercing)))
			SetTwoHanderEquipped(true);
	}

	//tribute items
	for (i = EQ::invslot::TRIBUTE_BEGIN; i <= EQ::invslot::TRIBUTE_END; i++) {
		const EQ::ItemInstance* inst = m_inv[i];
		if(inst == 0)
			continue;
		AddItemBonuses(inst, newbon, false, true);
	}

	//Optional ability to have worn effects calculate as an addititive bonus instead of highest value
	if (RuleI(Spells, AdditiveBonusWornType) && RuleI(Spells, AdditiveBonusWornType) != EQ::item::ItemEffectWorn){
		for (i = EQ::invslot::BONUS_BEGIN; i <= EQ::invslot::BONUS_STAT_END; i++) {
			const EQ::ItemInstance* inst = m_inv[i];
			if(inst == 0)
				continue;
			AdditiveWornBonuses(inst, newbon);
		}
	}

	SetAttackTimer();
}

void Merc::SetAttackTimer()
{

	float haste_mod = GetHaste() * 0.01f * 1.25f;

	//default value for attack timer in case they have
	//an invalid weapon equipped:
	attack_timer.SetAtTrigger(4000, true);

	Timer* TimerToUse = nullptr;

	for (int i = EQ::invslot::slotRange; i <= EQ::invslot::slotSecondary; i++) {
		//pick a timer
		if (i == EQ::invslot::slotPrimary)
			TimerToUse = &attack_timer;
		else if (i == EQ::invslot::slotRange)
			TimerToUse = &ranged_timer;
		else if (i == EQ::invslot::slotSecondary)
			TimerToUse = &attack_dw_timer;
		else	//invalid slot (hands will always hit this)
			continue;

		const EQ::ItemData* ItemToUse = nullptr;

		//find our item
		EQ::ItemInstance* ci = GetInv().GetItem(i);
		if (ci)
			ItemToUse = ci->GetItem();

		//special offhand stuff
		if (i == EQ::invslot::slotSecondary) {
			//if we cant dual wield, skip it
			if (!CanThisClassDualWield() || HasTwoHanderEquipped()) {
				attack_dw_timer.Disable();
				continue;
			}
		}

		//see if we have a valid weapon
		if (ItemToUse != nullptr) {
			//check type and damage/delay
			if (!ItemToUse->IsClassCommon()
				|| ItemToUse->Damage == 0
				|| ItemToUse->Delay == 0) {
				//no weapon
				ItemToUse = nullptr;
			}
			// Check to see if skill is valid
			else if ((ItemToUse->ItemType > EQ::item::ItemTypeLargeThrowing) &&
				(ItemToUse->ItemType != EQ::item::ItemTypeMartial) &&
				(ItemToUse->ItemType != EQ::item::ItemType2HPiercing)) {
				//no weapon
				ItemToUse = nullptr;
			}
		}

		int hhe = itembonuses.HundredHands + spellbonuses.HundredHands;
		int speed = 0;
		int delay = 3500;

		//if we have no weapon..
		if (ItemToUse == nullptr)
			delay = 100 * GetHandToHandDelay();
		else
			//we have a weapon, use its delay
			delay = 100 * ItemToUse->Delay;

		speed = delay / haste_mod;

		if (ItemToUse && ItemToUse->ItemType == EQ::item::ItemTypeBow) {
			// Live actually had a bug here where they would return the non-modified attack speed
			// rather than the cap ...
			speed = std::max(speed - GetQuiverHaste(speed), RuleI(Combat, QuiverHasteCap));
		}
		else {
			if (RuleB(Spells, Jun182014HundredHandsRevamp))
				speed = static_cast<int>(speed + ((hhe / 1000.0f) * speed));
			else
				speed = static_cast<int>(speed + ((hhe / 100.0f) * delay));
		}
		TimerToUse->SetAtTrigger(std::max(RuleI(Combat, MinHastedDelay), speed), true, true);
	}
}

int Merc::GetQuiverHaste(int delay)
{
	const EQ::ItemInstance* pi = nullptr;
	for (int r = EQ::invslot::GENERAL_BEGIN; r <= EQ::invslot::GENERAL_END; r++) {
		pi = GetInv().GetItem(r);
		if (pi && pi->IsClassBag() && pi->GetItem()->BagType == EQ::item::BagTypeQuiver &&
			pi->GetItem()->BagWR > 0)
			break;
		if (r == EQ::invslot::GENERAL_END)
			// we will get here if we don't find a valid quiver
			return 0;
	}
	return (pi->GetItem()->BagWR * 0.0025f * delay) + 1;
}

void Merc::AdditiveWornBonuses(const EQ::ItemInstance* inst, StatBonuses* newbon, bool isAug) {

	/*
	Powerful Non-live like option allows developers to add worn effects on items that
	can stack with other worn effects of the same spell effect type, instead of only taking the highest value.
	Ie Cleave I = 40 pct cleave - So if you equip 3 cleave I items you will have a 120 pct cleave bonus.
	To enable use RuleI(Spells, AdditiveBonusWornType)
	Setting value =  2  Will force all live items to automatically be calculated additivily
	Setting value to anything else will indicate the item 'worntype' that if set to the same, will cause the bonuses to use this calculation
	which will also stack with regular (worntype 2) effects. [Ie set rule = 3 and item worntype = 3]
	*/

	if (!inst || !inst->IsClassCommon())
		return;

	if (inst->GetAugmentType() == 0 && isAug == true)
		return;

	const EQ::ItemData* item = inst->GetItem();

	if (!inst->IsEquipable(GetBaseRace(), GetClass(), true))
		return;

	if (GetLevel() < item->ReqLevel)
		return;

	if (item->Worn.Effect > 0 && item->Worn.Type == RuleI(Spells, AdditiveBonusWornType))
		ApplySpellsBonuses(item->Worn.Effect, item->Worn.Level, newbon, 0, item->Worn.Type);// Non-live like - Addititive latent effects


	if (!isAug)
	{
		int i;
		for (i = EQ::invaug::SOCKET_BEGIN; i <= EQ::invaug::SOCKET_END; i++) {
			AdditiveWornBonuses(inst->GetAugment(i), newbon, true);
		}
	}
}

void Merc::AddFakeItemBonuses(const EQ::ItemInstance* inst, StatBonuses* newbon, bool isAug, const EQ::ItemInstance* baseinst)
{
	if (!inst || !baseinst) {
		return;
	}

	if (inst->GetAugmentType() == 0 && isAug == true) {
		return;
	}
	if (inst->GetID() < 10000000)
	{
		return;
	}

	const EQ::ItemData* item = inst->GetItem();
	const EQ::ItemData* base = baseinst->GetItem();

	uint32 itemId = inst->GetID();

	itemId -= 10000000;
	int value = itemId / 10;

	switch (itemId % 10)
	{
	case 0:
		newbon->HP += value + ((base->HP / 100.0f) * value);
		break;
	case 1:
		newbon->AC += value + ((base->AC / 100.0f) * value);
		break;
	case 2:
		newbon->ATK += value + ((base->Attack / 100.0f) * value);
		break;
	case 3:
		newbon->STR += value + ((base->AStr / 100.0f) * value);
		break;
	case 4:
		newbon->STA += value + ((base->ASta / 100.0f) * value);
		break;
	case 5:
		newbon->AGI += value + ((base->AAgi / 100.0f) * value);
		break;
	case 6:
		newbon->DEX += value + ((base->ADex / 100.0f) * value);
		break;
	case 7:
		newbon->INT += value + ((base->AInt / 100.0f) * value);
		break;
	case 8:
		newbon->WIS += value + ((base->AWis / 100.0f) * value);
		break;
	case 9:
		newbon->CHA += value + ((base->ACha / 100.0f) * value);
		break;
	default:
		break;
	}

	return;

}

void Merc::AddItemBonuses(const EQ::ItemInstance* inst, StatBonuses* newbon, bool isAug, bool isTribute, int rec_override, bool ammo_slot_item)
{

	if (!inst || !inst->IsClassCommon()) {
		return;
	}

	if (inst->GetAugmentType() == 0 && isAug == true) {
		return;
	}

	const EQ::ItemData* item = inst->GetItem();

	if (!isTribute && !inst->IsEquipable(GetBaseRace(), GetClass(), true)) {
		if (item->ItemType != EQ::item::ItemTypeFood && item->ItemType != EQ::item::ItemTypeDrink)
			return;
	}

	if (GetLevel() < inst->GetItemRequiredLevel(true)) {
		return;
	}

	// So there isn't a very nice way to get the real rec level from the aug's inst, so we just pass it in, only
	// used for augs
	auto rec_level = isAug ? rec_override : inst->GetItemRecommendedLevel(true);

	if (!ammo_slot_item) {
		if (GetLevel() >= rec_level) {
			newbon->AC += item->AC;
			newbon->HP += item->HP;
			newbon->Mana += item->Mana;
			newbon->Endurance += item->Endur;
			newbon->ATK += item->Attack;
			newbon->STR += (item->AStr);
			newbon->STA += (item->ASta);
			newbon->DEX += (item->ADex);
			newbon->AGI += (item->AAgi);
			newbon->INT += (item->AInt);
			newbon->WIS += (item->AWis);
			newbon->CHA += (item->ACha);

			newbon->MR += (item->MR);
			newbon->FR += (item->FR);
			newbon->CR += (item->CR);
			newbon->PR += (item->PR);
			newbon->DR += (item->DR);
			newbon->Corrup += (item->SVCorruption);
		}
		else {
			int lvl = GetLevel();

			newbon->AC += CalcRecommendedLevelBonus(lvl, rec_level, item->AC);
			newbon->HP += CalcRecommendedLevelBonus(lvl, rec_level, item->HP);
			newbon->Mana += CalcRecommendedLevelBonus(lvl, rec_level, item->Mana);
			newbon->Endurance += CalcRecommendedLevelBonus(lvl, rec_level, item->Endur);
			newbon->ATK += CalcRecommendedLevelBonus(lvl, rec_level, item->Attack);
			newbon->STR += CalcRecommendedLevelBonus(lvl, rec_level, (item->AStr));
			newbon->STA += CalcRecommendedLevelBonus(lvl, rec_level, (item->ASta));
			newbon->DEX += CalcRecommendedLevelBonus(lvl, rec_level, (item->ADex));
			newbon->AGI += CalcRecommendedLevelBonus(lvl, rec_level, (item->AAgi));
			newbon->INT += CalcRecommendedLevelBonus(lvl, rec_level, (item->AInt));
			newbon->WIS += CalcRecommendedLevelBonus(lvl, rec_level, (item->AWis));
			newbon->CHA += CalcRecommendedLevelBonus(lvl, rec_level, (item->ACha));

			newbon->MR += CalcRecommendedLevelBonus(lvl, rec_level, (item->MR));
			newbon->FR += CalcRecommendedLevelBonus(lvl, rec_level, (item->FR));
			newbon->CR += CalcRecommendedLevelBonus(lvl, rec_level, (item->CR));
			newbon->PR += CalcRecommendedLevelBonus(lvl, rec_level, (item->PR));
			newbon->DR += CalcRecommendedLevelBonus(lvl, rec_level, (item->DR));
			newbon->Corrup +=
				CalcRecommendedLevelBonus(lvl, rec_level, (item->SVCorruption));
		}

		// FatherNitwit: New style haste, shields, and regens
		if (newbon->haste < (int32)item->Haste) {
			newbon->haste = item->Haste;
		}
		if (item->Regen > 0)
			newbon->HPRegen += item->Regen;

		if (item->ManaRegen > 0)
			newbon->ManaRegen += item->ManaRegen;

		if (item->EnduranceRegen > 0)
			newbon->EnduranceRegen += item->EnduranceRegen;

		if (item->DamageShield > 0) {
			if ((newbon->DamageShield + item->DamageShield) > RuleI(Character, ItemDamageShieldCap))
				newbon->DamageShield = RuleI(Character, ItemDamageShieldCap);
			else
				newbon->DamageShield += item->DamageShield;
		}
		if (item->SpellShield > 0) {
			if ((newbon->SpellShield + item->SpellShield) > RuleI(Character, ItemSpellShieldingCap))
				newbon->SpellShield = RuleI(Character, ItemSpellShieldingCap);
			else
				newbon->SpellShield += item->SpellShield;
		}
		if (item->Shielding > 0) {
			if ((newbon->MeleeMitigation + item->Shielding) > RuleI(Character, ItemShieldingCap))
				newbon->MeleeMitigation = RuleI(Character, ItemShieldingCap);
			else
				newbon->MeleeMitigation += item->Shielding;
		}
		if (item->StunResist > 0) {
			if ((newbon->StunResist + item->StunResist) > RuleI(Character, ItemStunResistCap))
				newbon->StunResist = RuleI(Character, ItemStunResistCap);
			else
				newbon->StunResist += item->StunResist;
		}
		if (item->StrikeThrough > 0) {
			if ((newbon->StrikeThrough + item->StrikeThrough) > RuleI(Character, ItemStrikethroughCap))
				newbon->StrikeThrough = RuleI(Character, ItemStrikethroughCap);
			else
				newbon->StrikeThrough += item->StrikeThrough;
		}
		if (item->Avoidance > 0) {
			if ((newbon->AvoidMeleeChance + item->Avoidance) > RuleI(Character, ItemAvoidanceCap))
				newbon->AvoidMeleeChance = RuleI(Character, ItemAvoidanceCap);
			else
				newbon->AvoidMeleeChance += item->Avoidance;
		}
		if (item->Accuracy > 0) {
			if ((newbon->HitChance + item->Accuracy) > RuleI(Character, ItemAccuracyCap))
				newbon->HitChance = RuleI(Character, ItemAccuracyCap);
			else
				newbon->HitChance += item->Accuracy;
		}
		if (item->CombatEffects > 0) {
			if ((newbon->ProcChance + item->CombatEffects) > RuleI(Character, ItemCombatEffectsCap))
				newbon->ProcChance = RuleI(Character, ItemCombatEffectsCap);
			else
				newbon->ProcChance += item->CombatEffects;
		}
		if (item->DotShielding > 0) {
			if ((newbon->DoTShielding + item->DotShielding) > RuleI(Character, ItemDoTShieldingCap))
				newbon->DoTShielding = RuleI(Character, ItemDoTShieldingCap);
			else
				newbon->DoTShielding += item->DotShielding;
		}

		if (item->HealAmt > 0) {
			if ((newbon->HealAmt + item->HealAmt) > RuleI(Character, ItemHealAmtCap))
				newbon->HealAmt = RuleI(Character, ItemHealAmtCap);
			else
				newbon->HealAmt += item->HealAmt;
		}
		if (item->SpellDmg > 0) {
			if ((newbon->SpellDmg + item->SpellDmg) > RuleI(Character, ItemSpellDmgCap))
				newbon->SpellDmg = RuleI(Character, ItemSpellDmgCap);
			else
				newbon->SpellDmg += item->SpellDmg;
		}
		if (item->Clairvoyance > 0) {
			if ((newbon->Clairvoyance + item->Clairvoyance) > RuleI(Character, ItemClairvoyanceCap))
				newbon->Clairvoyance = RuleI(Character, ItemClairvoyanceCap);
			else
				newbon->Clairvoyance += item->Clairvoyance;
		}

		if (item->DSMitigation > 0) {
			if ((newbon->DSMitigation + item->DSMitigation) > RuleI(Character, ItemDSMitigationCap))
				newbon->DSMitigation = RuleI(Character, ItemDSMitigationCap);
			else
				newbon->DSMitigation += item->DSMitigation;
		}
		if (item->Worn.Effect > 0 && item->Worn.Type == EQ::item::ItemEffectWorn) { // latent effects
			ApplySpellsBonuses(item->Worn.Effect, item->Worn.Level, newbon, 0, item->Worn.Type);
		}

		if (item->Focus.Effect > 0 && (item->Focus.Type == EQ::item::ItemEffectFocus)) { // focus effects
			ApplySpellsBonuses(item->Focus.Effect, item->Focus.Level, newbon, 0);
		}

		switch (item->BardType) {
		case 51: /* All (e.g. Singing Short Sword) */
			if (item->BardValue > newbon->singingMod)
				newbon->singingMod = item->BardValue;
			if (item->BardValue > newbon->brassMod)
				newbon->brassMod = item->BardValue;
			if (item->BardValue > newbon->stringedMod)
				newbon->stringedMod = item->BardValue;
			if (item->BardValue > newbon->percussionMod)
				newbon->percussionMod = item->BardValue;
			if (item->BardValue > newbon->windMod)
				newbon->windMod = item->BardValue;
			break;
		case 50: /* Singing */
			if (item->BardValue > newbon->singingMod)
				newbon->singingMod = item->BardValue;
			break;
		case 23: /* Wind */
			if (item->BardValue > newbon->windMod)
				newbon->windMod = item->BardValue;
			break;
		case 24: /* stringed */
			if (item->BardValue > newbon->stringedMod)
				newbon->stringedMod = item->BardValue;
			break;
		case 25: /* brass */
			if (item->BardValue > newbon->brassMod)
				newbon->brassMod = item->BardValue;
			break;
		case 26: /* Percussion */
			if (item->BardValue > newbon->percussionMod)
				newbon->percussionMod = item->BardValue;
			break;
		}

		// Add Item Faction Mods
		if (item->FactionMod1) {
			if (item->FactionAmt1 > 0 && item->FactionAmt1 > GetItemFactionBonus(item->FactionMod1)) {
				AddItemFactionBonus(item->FactionMod1, item->FactionAmt1);
			}
			else if (item->FactionAmt1 < 0 && item->FactionAmt1 < GetItemFactionBonus(item->FactionMod1)) {
				AddItemFactionBonus(item->FactionMod1, item->FactionAmt1);
			}
		}
		if (item->FactionMod2) {
			if (item->FactionAmt2 > 0 && item->FactionAmt2 > GetItemFactionBonus(item->FactionMod2)) {
				AddItemFactionBonus(item->FactionMod2, item->FactionAmt2);
			}
			else if (item->FactionAmt2 < 0 && item->FactionAmt2 < GetItemFactionBonus(item->FactionMod2)) {
				AddItemFactionBonus(item->FactionMod2, item->FactionAmt2);
			}
		}
		if (item->FactionMod3) {
			if (item->FactionAmt3 > 0 && item->FactionAmt3 > GetItemFactionBonus(item->FactionMod3)) {
				AddItemFactionBonus(item->FactionMod3, item->FactionAmt3);
			}
			else if (item->FactionAmt3 < 0 && item->FactionAmt3 < GetItemFactionBonus(item->FactionMod3)) {
				AddItemFactionBonus(item->FactionMod3, item->FactionAmt3);
			}
		}
		if (item->FactionMod4) {
			if (item->FactionAmt4 > 0 && item->FactionAmt4 > GetItemFactionBonus(item->FactionMod4)) {
				AddItemFactionBonus(item->FactionMod4, item->FactionAmt4);
			}
			else if (item->FactionAmt4 < 0 && item->FactionAmt4 < GetItemFactionBonus(item->FactionMod4)) {
				AddItemFactionBonus(item->FactionMod4, item->FactionAmt4);
			}
		}

		if (item->ExtraDmgSkill != 0 && item->ExtraDmgSkill <= EQ::skills::HIGHEST_SKILL) {
			if ((newbon->SkillDamageAmount[item->ExtraDmgSkill] + item->ExtraDmgAmt) >
				RuleI(Character, ItemExtraDmgCap))
				newbon->SkillDamageAmount[item->ExtraDmgSkill] = RuleI(Character, ItemExtraDmgCap);
			else
				newbon->SkillDamageAmount[item->ExtraDmgSkill] += item->ExtraDmgAmt;
		}
	}

	// Process when ammo_slot_item = true or false
	if (item->SkillModValue != 0 && item->SkillModType <= EQ::skills::HIGHEST_SKILL) {
		if ((item->SkillModValue > 0 && newbon->skillmod[item->SkillModType] < item->SkillModValue) ||
			(item->SkillModValue < 0 && newbon->skillmod[item->SkillModType] > item->SkillModValue)) {

			newbon->skillmod[item->SkillModType] = item->SkillModValue;
			newbon->skillmodmax[item->SkillModType] = item->SkillModMax;
		}
	}

	if (!isAug) {

#ifndef TLP_STYLE
		for (int i = EQ::invaug::SOCKET_BEGIN; i <= EQ::invaug::SOCKET_END; i++)
			AddFakeItemBonuses(inst->GetAugment(i), newbon, true, inst);
#else
		for (int i = EQ::invaug::SOCKET_BEGIN; i <= EQ::invaug::SOCKET_END; i++)
			AddItemBonuses(inst->GetAugment(i), newbon, true, false, rec_level, ammo_slot_item);
#endif
	}
}

int Merc::GroupLeadershipAAHealthEnhancement()
{
	return 0;
}

int Merc::GroupLeadershipAAManaEnhancement()
{
	return 0;
}

int Merc::GroupLeadershipAAHealthRegeneration()
{
	return 0;
}

int Merc::GroupLeadershipAAOffenseEnhancement()
{
	return 0;
}

int64 Merc::CalcMaxHP() {
	return Mob::CalcMaxHP();
}

int64 Merc::CalcBaseHP()
{
	return Mob::CalcBaseHP();
}

int64 Merc::CalcMaxMana()
{
	return Mob::CalcMaxMana();
}

int64 Merc::CalcEnduranceRegen(bool bCombat)
{
	int64 base = 0;
	auto base_data = database.GetBaseData(GetLevel(), GetClass());
	if (base_data) {
		base = static_cast<int>(base_data->end_regen);
		if (IsEngaged() && base > 0)
			base += base / 2;
	}

	base += itembonuses.EnduranceRegen + spellbonuses.EnduranceRegen + aabonuses.EnduranceRegen;

	base = base * 1.0f + 0.5f;

	return base;
}

int64 Merc::CalcBaseMana()
{
	return Mob::CalcBaseMana();
}

int64 Merc::CalcBaseEndurance()
{
	return Mob::CalcBaseEndurance();
}
void Merc::SetEndurance(int64 newEnd)
{
	Mob::SetEndurance(newEnd);
}

bool Merc::HasSkill(EQ::skills::SkillType skill_id) const {
	return((GetSkill(skill_id) > 0) && CanHaveSkill(skill_id));
}

bool Merc::CanHaveSkill(EQ::skills::SkillType skill_id) const {
	return(database.GetSkillCap(GetClass(), skill_id, level) > 0);
	//if you don't have it by max level, then odds are you never will?
}

uint16 Merc::MaxSkill(EQ::skills::SkillType skillid, uint16 class_, uint16 level) const {
	return(database.GetSkillCap(class_, skillid, level));
}

void Merc::FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho) {
	if(ns) {
		Mob::FillSpawnStruct(ns, ForWho);

		ns->spawn.afk = 0;
		ns->spawn.lfg = 0;
		ns->spawn.anon = 0;
		ns->spawn.gm = 0;
		ns->spawn.guildID = 0xFFFFFFFF;         // 0xFFFFFFFF = NO GUILD, 0 = Unknown Guild
		ns->spawn.is_npc = 1;                           // 0=no, 1=yes
		ns->spawn.is_pet = 0;
		ns->spawn.guildrank = 0;
		ns->spawn.showhelm = 1;
		ns->spawn.flymode = 0;
		ns->spawn.NPC = 1;                                      // 0=player,1=npc,2=pc corpse,3=npc corpse
		ns->spawn.IsMercenary = 1;
		ns->spawn.show_name = true;
		ns->spawn.petOwnerId = GetOwner() ? GetOwner()->GetID() : 0;

		UpdateActiveLight();
		ns->spawn.light = m_Light.Type[EQ::lightsource::LightActive];

	}
}

uint32 Merc::GetEquippedItemFromTextureSlot(uint8 material_slot) const
{
	int16 inventory_slot;

	const EQ::ItemInstance* item;

	if (material_slot > EQ::textures::LastTexture) {
		return 0;
	}

	inventory_slot = EQ::InventoryProfile::CalcSlotFromMaterial(material_slot);
	if (inventory_slot == INVALID_INDEX) {
		return 0;
	}

	item = m_inv.GetItem(inventory_slot);

	if (item && item->GetItem()) {
		return item->GetItem()->ID;
	}

	return 0;
}

int32 Merc::GetEquipmentMaterial(uint8 material_slot) const
{
	uint32 equipment_material = 0;
	int32  ornamentation_augment_type = RuleI(Character, OrnamentationAugmentType);

	//int32 texture_profile_material = GetTextureProfileMaterial(material_slot);

	//Log(Logs::Detail, Logs::MobAppearance,
	//	"Mob::GetEquipmentMaterial [%s] material_slot: %u texture_profile_material: %i",
	//	this->clean_name,
	//	material_slot,
	//	texture_profile_material
	//);

	//if (texture_profile_material > 0) {
	//	return texture_profile_material;
	//}

	auto item = database.GetItem(GetEquippedItemFromTextureSlot(material_slot));

	if (item.ID) {

		/**
		 * Handle primary / secondary texture
		 */
		bool is_primary_or_secondary_weapon =
			material_slot == EQ::textures::weaponPrimary ||
			material_slot == EQ::textures::weaponSecondary;

		if (is_primary_or_secondary_weapon) {

				int16 inventory_slot = EQ::InventoryProfile::CalcSlotFromMaterial(material_slot);
				if (inventory_slot == INVALID_INDEX) {
					return 0;
				}

				const EQ::ItemInstance* item_instance = m_inv[inventory_slot];
				if (item_instance) {
					if (item_instance->GetOrnamentationAug(ornamentation_augment_type)) {
						item = database.GetItem(item_instance->GetOrnamentationAug(ornamentation_augment_type)->GetID());
						if (item.ID && strlen(item.IDFile) > 2) {
							equipment_material = atoi(&item.IDFile[2]);
						}
					}
					else if (item_instance->GetOrnamentationIDFile()) {
						equipment_material = item_instance->GetOrnamentationIDFile();
					}
				}

			if (equipment_material == 0 && strlen(item.IDFile) > 2) {
				equipment_material = atoi(&item.IDFile[2]);
			}
		}
		else {
			equipment_material = item.Material;
		}
	}

	return equipment_material;
}

uint32 Merc::GetEquipmentColor(uint8 material_slot) const
{
	if (material_slot > EQ::textures::LastTexture)
		return 0;

	const EQ::ItemData* item = database.GetItem(GetEquippedItemFromTextureSlot(material_slot));
	if (item != nullptr)
		return ((m_pp.item_tint.Slot[material_slot].UseTint) ? m_pp.item_tint.Slot[material_slot].Color : item.Color);

	return 0;
}

int32 Merc::GetCharmEffectID(int16 slot)
{
	if (slot == -1)
		return -1;

	EQ::ItemInstance* itm = GetInv().GetItem(slot);

	if (!itm)
		return -1;

	if (itm && itm->GetItem()->ID > 150000)
	{
		return itm->GetItem()->ID;
	}
	return -1;
}


void Merc::DoHPRegen() {

	int64 hpRegenDiv = 50;

	if (GetCharmEffectID(EQ::invslot::slotCharm) == CharmRegeneration)
	{
		hpRegenDiv -= 25;
	}

	if (!HasSpellEffect(SE_NegateRegen))
	{
		SetHP(GetHP() + spellbonuses.HPRegen + itembonuses.HPRegen + GetMaxHP() / hpRegenDiv);
	}
}

void Merc::DoManaRegen() {
	if (hate_list.IsHateListEmpty())
	{
		SetMana(GetMana() + 1 + aabonuses.ManaRegen + itembonuses.ManaRegen + spellbonuses.ManaRegen + GetMaxMana() / 10);
	}
}


void Merc::DoEnduranceRegen() {
	int64 baseregen = GetMaxEndurance() / 10;

	int64 diff = GetMaxEndurance() / 2 - GetEndurance();

	if ((diff > 0 ? diff : diff * -1) < baseregen)
	{
		SetEndurance(GetMaxEndurance() / 2 + CalcEnduranceRegen(IsEngaged()));
	}
	else
	{
		SetEndurance(GetEndurance() + baseregen * (diff > (int64)0 ? (int64)1 : (int64)-1) + CalcEnduranceRegen(IsEngaged()));
	}
}

bool Merc::Process()
{
	if (p_depop)
	{
		SetOwner(nullptr);
		RemoveAllPets();
		return false;
	}

	if (IsStunned() && stunned_timer.Check()) {
		Mob::UnStun();
		this->spun_timer.Disable();
	}

	SpellProcess();

	if (mob_scan_close.Check()) {

		entity_list.ScanCloseMobs(close_mobs, this);

		if (moving) {
			mob_scan_close.Disable();
			mob_scan_close.Start(RandomTimer(3000, 6000));
		}
		else {
			mob_scan_close.Disable();
			mob_scan_close.Start(RandomTimer(6000, 60000));
		}
	}

	if (mob_check_moving_timer.Check() && moving) {
		mob_scan_close.Trigger();
	}

	if (tic_timer.Check()) {
		DoHPRegen();
		DoManaRegen();
		DoEnduranceRegen();
		SendHPUpdate();
		BuffProcess();
	}

	if (currently_fleeing) {
		ProcessFlee();
	}

	/**
	 * Send HP updates when engaged
	 */
	if (send_hp_update_timer.Check(false) && this->IsEngaged()) {
		SendHPUpdate();
	}

	if (HasVirus()) {
		if (viral_timer.Check()) {
			viral_timer_counter++;
			for (int i = 0; i < MAX_SPELL_TRIGGER * 2; i += 2) {
				if (viral_spells[i] && spells[viral_spells[i]].viral_timer > 0) {
					if (viral_timer_counter % spells[viral_spells[i]].viral_timer == 0) {
						SpreadVirus(viral_spells[i], viral_spells[i + 1]);
					}
				}
			}
		}
		if (viral_timer_counter > 999)
			viral_timer_counter = 0;
	}

	if (spellbonuses.GravityEffect == 1) {
		if (gravity_timer.Check())
			DoGravityEffect();
	}

	if (reface_timer->Check() && !IsEngaged() && IsPositionEqualWithinCertainZ(m_Position, m_GuardPoint, 5.0f)) {
		RotateTo(m_GuardPoint.w);
		reface_timer->Disable();
	}

	// needs to be done before mez and stun
	if (ForcedMovement)
		ProcessForcedMovement();

	if (IsMezzed())
		return true;

	if (IsStunned()) {
		if (spun_timer.Check())
			Spin();
		return true;
	}

	AI_Process();

	return true;
}

void Merc::AI_Process() 
{
	if(!IsAIControlled())
		return;

	if(IsCasting())
		return;

	// A merc wont start its AI if not grouped
	if(!HasGroup()) {
		return;
	}

	Mob* MercOwner = GetMercOwner();

	if(GetAppearance() == eaDead)
	{
		if(!MercOwner)
		{
			Depop();
		}
		return;
	}

	// The merc needs an owner
	if(!MercOwner) {
		// TODO: Need to wait and try casting rez if merc is a healer with a dead owner
		return;
	}

	if(check_target_timer.Check()) {
		CheckHateList();
	}
	if (IsEngaged() && GetStance() != EQ::constants::StanceType::stancePassive)
	{
		if (IsRooted())
			SetTarget(hate_list.GetClosestEntOnHateList(this));
		else
			FindTarget();

		if (!GetTarget())
			return;

		if (!IsSitting())
			FaceTarget(GetTarget());

		if (DivineAura())
			return;

		int hateCount = entity_list.GetHatedCount(this, nullptr, false);
		if (GetHatedCount() < hateCount) {
			SetHatedCount(hateCount);
		}

		// Let's check if we have a los with our target.
		// If we don't, our hate_list is wiped.
		// Else, it was causing the merc to aggro behind wall etc... causing massive trains.
		if (GetTarget()->IsMezzed() || !IsAttackAllowed(GetTarget())) {
			WipeHateList();

			if (IsMoving()) {
				SetHeading(0);
				SetRunAnimSpeed(0);

				if (moved) {
					moved = false;
					StopNavigation();
				}
			}

			return;
		}
		else if (!CheckLosFN(GetTarget())) {
			auto Goal = GetTarget()->GetPosition();
			RunTo(Goal.x, Goal.y, Goal.z);

			return;
		}

		if (!(m_PlayerState & static_cast<uint32>(PlayerState::Aggressive)))
			SendAddPlayerState(PlayerState::Aggressive);

		bool atCombatRange = false;

		float meleeDistance = GetMaxMeleeRangeToTarget(GetTarget());

		if (GetClass() == SHADOWKNIGHT || GetClass() == PALADIN || GetClass() == WARRIOR) {
			meleeDistance = meleeDistance * .30;
		}
		else {
			meleeDistance *= (float)zone->random.Real(.50, .85);
		}
		if (DistanceSquared(m_Position, GetTarget()->GetPosition()) <= meleeDistance) {
			atCombatRange = true;
		}

		if (atCombatRange)
		{

			if (AI_EngagedCastCheck())
			{
				if (IsCasting())
				{
					StopNavigation();
					FaceTarget();
				}
			}

			if (IsMoving())
			{
				SetHeading(CalculateHeadingToTarget(GetTarget()->GetX(), GetTarget()->GetY()));
				SetRunAnimSpeed(0);

				if (moved) {
					StopNavigation();
				}
			}

			if (AI_movement_timer->Check()) {
				if (!IsMoving()) {
					if (GetClass() == ROGUE) {
						if (HasTargetReflection() && !GetTarget()->IsFeared() && !GetTarget()->IsStunned()) {
							// Hate redux actions
							if (evade_timer.Check(false)) {
								// Attempt to evade
								int timer_duration = (HideReuseTime - GetSkillReuseTime(EQ::skills::SkillHide)) * 1000;
								if (timer_duration < 0)
									timer_duration = 0;
								evade_timer.Start(timer_duration);

								if (zone->random.Int(0, 260) < (int)GetSkill(EQ::skills::SkillHide))
									RogueEvade(GetTarget());

								return;
							}
							else if (GetTarget()->IsRooted()) {
								// Move rogue back from rooted mob - out of combat range, if necessary
								float melee_distance = GetMaxMeleeRangeToTarget(GetTarget());
								float current_distance = DistanceSquared(static_cast<glm::vec3>(m_Position), static_cast<glm::vec3>(GetTarget()->GetPosition()));

								if (current_distance <= melee_distance) {
									float newX = 0;
									float newY = 0;
									float newZ = 0;
									FaceTarget(GetTarget());
									if (PlotPositionAroundTarget(this, newX, newY, newZ)) {
										RunTo(newX, newY, newZ);
										return;
									}
								}
							}
						}
						else if (!BehindMob(GetTarget(), GetX(), GetY())) {
							// Move the rogue to behind the mob
							float newX = 0;
							float newY = 0;
							float newZ = 0;
							if (PlotPositionAroundTarget(GetTarget(), newX, newY, newZ)) {
								RunTo(newX, newY, newZ);
								return;
							}
						}
					}
					else if (GetClass() != ROGUE && (DistanceSquaredNoZ(m_Position, GetTarget()->GetPosition()) < GetTarget()->GetSize())) {
						// If we are not a rogue trying to backstab, let's try to adjust our melee range so we don't appear to be bunched up
						float newX = 0;
						float newY = 0;
						float newZ = 0;
						if (PlotPositionAroundTarget(GetTarget(), newX, newY, newZ, false)) {
							RunTo(newX, newY, newZ);
							return;
						}
					}
				}

				//if (IsMoving())
				//	SendPositionUpdate();
				//else
				//	SendPosition();
			}

			if (!IsMercCaster() && GetTarget() && !IsStunned() && !IsMezzed() && (GetAppearance() != eaDead))
			{
				// we can't fight if we don't have a target, are stun/mezzed or dead..
				// Stop attacking if the target is enraged
				if (IsEngaged() && !BehindMob(GetTarget(), GetX(), GetY()) && GetTarget()->IsEnraged())
					return;

				//try main hand first
				if (attack_timer.Check())
				{
					Attack(GetTarget(), EQ::invslot::slotPrimary);

					bool tripleSuccess = false;

					if (GetOwner() && GetTarget() && CanThisClassDoubleAttack())
					{
						if (GetOwner()) {
							Attack(GetTarget(), EQ::invslot::slotPrimary, true);
						}

						if (GetOwner() && GetTarget() && GetSpecialAbility(SPECATK_TRIPLE)) {
							tripleSuccess = true;
							Attack(GetTarget(), EQ::invslot::slotPrimary, true);
						}

						//quad attack, does this belong here??
						if (GetOwner() && GetTarget() && GetSpecialAbility(SPECATK_QUAD)) {
							Attack(GetTarget(), EQ::invslot::slotPrimary, true);
						}
					}

					//Live AA - Flurry, Rapid Strikes ect (Flurry does not require Triple Attack).
					int16 flurrychance = aabonuses.FlurryChance + spellbonuses.FlurryChance + itembonuses.FlurryChance;

					if (GetTarget() && flurrychance)
					{
						if (zone->random.Roll(flurrychance))
						{
							MessageString(Chat::NPCFlurry, YOU_FLURRY);
							Attack(GetTarget(), EQ::invslot::slotPrimary, false);
							Attack(GetTarget(), EQ::invslot::slotPrimary, false);
						}
					}

					int16 ExtraAttackChanceBonus = spellbonuses.ExtraAttackChance + itembonuses.ExtraAttackChance + aabonuses.ExtraAttackChance;

					if (GetTarget() && ExtraAttackChanceBonus) {
						if (zone->random.Roll(ExtraAttackChanceBonus))
						{
							Attack(GetTarget(), EQ::invslot::slotPrimary, false);
						}
					}
				}

				// TODO: Do mercs berserk? Find this out on live...
				//if (GetClass() == WARRIOR || GetClass() == BERSERKER) {
				//      if(GetHP() > 0 && !berserk && this->GetHPRatio() < 30) {
				//              entity_list.MessageCloseString(this, false, 200, 0, BERSERK_START, GetName());
				//              this->berserk = true;
				//      }
				//      if (berserk && this->GetHPRatio() > 30) {
				//              entity_list.MessageCloseString(this, false, 200, 0, BERSERK_END, GetName());
				//              this->berserk = false;
				//      }
				//}

				//now off hand
				if (GetTarget() && attack_dw_timer.Check() && CanThisClassDualWield())
				{
					int weapontype = 0; // No weapon type
					bool bIsFist = true;

					// why are we checking 'weapontype' when we know it's set to '0' above?
					if (bIsFist || ((weapontype != EQ::item::ItemType2HSlash) && (weapontype != EQ::item::ItemType2HPiercing) && (weapontype != EQ::item::ItemType2HBlunt)))
					{
						float DualWieldProbability = 0.0f;

						int16 Ambidexterity = aabonuses.Ambidexterity + spellbonuses.Ambidexterity + itembonuses.Ambidexterity;
						DualWieldProbability = (GetSkill(EQ::skills::SkillDualWield) + GetLevel() + Ambidexterity) / 400.0f; // 78.0 max
						int16 DWBonus = spellbonuses.DualWieldChance + itembonuses.DualWieldChance;
						DualWieldProbability += DualWieldProbability * float(DWBonus) / 100.0f;

						// Max 78% of DW
						if (zone->random.Roll(DualWieldProbability))
						{
							Attack(GetTarget(), EQ::invslot::slotSecondary);     // Single attack with offhand

							if (CanThisClassDoubleAttack()) {
								if (GetTarget() && GetTarget()->GetHP() > -10)
									Attack(GetTarget(), EQ::invslot::slotSecondary);     // Single attack with offhand
							}
						}
					}
				}
			}
		}
		else
		{
			if (AI_EngagedCastCheck())
			{
				if (IsCasting())
				{
					StopNavigation();
					FaceTarget();
				}
			}

			if (AI_movement_timer->Check())
			{
				if (!IsRooted()) {
					LogAI("Pursuing [{}] while engaged", GetTarget()->GetCleanName());
					RunTo(GetTarget()->GetX(), GetTarget()->GetY(), GetTarget()->GetZ());
					return;
				}
				else
				{
					FaceTarget();
				}
			}
		} // end not in combat range
	}
	else
	{
		// Not engaged in combat
		SetTarget(0);
		SetHatedCount(0);

		if (m_PlayerState & static_cast<uint32>(PlayerState::Aggressive))
			SendRemovePlayerState(PlayerState::Aggressive);

		if(!check_target_timer.Enabled())
			check_target_timer.Start(2000, false);

		if(!IsMoving() && !spellend_timer.Enabled())
		{
			//TODO: Implement passive stances.
			if(AI_IdleCastCheck()) {
				if(IsCasting())
					StopNavigation();
			}
		}

		if(AI_movement_timer->Check()) {
			if(GetFollowID()) {
				Mob* follow = entity_list.GetMob(GetFollowID());

				if (follow) {
					float dist = DistanceSquared(m_Position, follow->GetPosition());
					bool running = true;

					if (dist < GetFollowDistance() + 1000)
						running = false;

					SetRunAnimSpeed(0);

					if (dist > GetFollowDistance()) {
						if (running) {
							RunTo(follow->GetX(), follow->GetY(), follow->GetZ());
						}
						else {
							WalkTo(follow->GetX(), follow->GetY(), follow->GetZ());
						}
					}
					else {
						if (moved) {
							moved = false;
							StopNavigation();
						}
					}
				}
			}
		}
	}
}

void Merc::AI_Start(int32 iMoveDelay) {
	if (!pAIControlled)
		return;

	if (merc_spells.empty()) {
		AIautocastspell_timer->SetTimer(1000);
		AIautocastspell_timer->Disable();
	} else {
		AIautocastspell_timer->SetTimer(750);
		AIautocastspell_timer->Start(RandomTimer(0, 2000), false);
	}

	SendTo(GetX(), GetY(), GetZ());
	SaveGuardSpot(GetPosition());
}

bool Merc::AI_EngagedCastCheck() {
	bool result = false;
	bool failedToCast = false;

	if (GetTarget() && AIautocastspell_timer->Check(false))
	{
		AIautocastspell_timer->Disable();       //prevent the timer from going off AGAIN while we are casting.

		LogAI("Merc Engaged autocast check triggered");


		if (!AICastSpell(100, SpellType_Escape)) {
			if (!AICastSpell(100, SpellType_Root)) {
				if (!AICastSpell(100, SpellType_Lifetap)) {
					if (!AICastSpell(100, SpellType_DOT)) {
						if (!AICastSpell(100, SpellType_Nuke)) {
							if (!AICastSpell(100, SpellType_Debuff)) {
								if (!AICastSpell(100, SpellType_InCombatBuff)) {
									if (!entity_list.Merc_AICheckCloseBeneficialSpells(this, 100, MercAISpellRange, SpellType_Heal)) {
										failedToCast = true;
									}
								}
							}
						}
					}
				}
			}
		}

		if(!AIautocastspell_timer->Enabled()) {
			AIautocastspell_timer->Start(RandomTimer(100, 250), false);
		}

		if(!failedToCast)
			result = true;
	}

	return result;
}

bool Merc::AI_IdleCastCheck() {
	bool result = false;
	bool failedToCast = false;

	if (AIautocastspell_timer->Check(false)) {
#if MercAI_DEBUG_Spells >= 25
		LogAI("Merc Non-Engaged autocast check triggered: [{}]", this->GetCleanName());
#endif
		AIautocastspell_timer->Disable();       //prevent the timer from going off AGAIN while we are casting.


		if(!entity_list.Merc_AICheckCloseBeneficialSpells(this, 100, MercAISpellRange, SpellType_Cure)) {
			if(!entity_list.Merc_AICheckCloseBeneficialSpells(this, 100, MercAISpellRange, SpellType_Heal)) {
				if(!entity_list.Merc_AICheckCloseBeneficialSpells(this, 100, MercAISpellRange, SpellType_Resurrect)) {
					if (!entity_list.Merc_AICheckCloseBeneficialSpells(this, 100, MercAISpellRange, SpellType_Buff)) {
						if (!entity_list.Merc_AICheckCloseBeneficialSpells(this, 100, MercAISpellRange, SpellType_Pet)) {
							failedToCast = true;
						}
					}
				}
			}
		}

		if(!AIautocastspell_timer->Enabled())
			AIautocastspell_timer->Start(RandomTimer(500, 1000), false);

		if(!failedToCast)
			result = true;
	}

	return result;
}

bool EntityList::Merc_AICheckCloseBeneficialSpells(Merc* caster, uint8 iChance, float iRange, uint32 iSpellTypes) {

	if((iSpellTypes & SPELL_TYPES_DETRIMENTAL) != 0) {
		//according to live, you can buff and heal through walls...
		//now with PCs, this only applies if you can TARGET the target, but
		// according to Rogean, Live NPCs will just cast through walls/floors, no problem..
		//
		// This check was put in to address an idle-mob CPU issue
		LogError("Error: detrimental spells requested from AICheckCloseBeneficialSpells!!");
		return(false);
	}

	if(!caster)
		return false;

	if(!caster->AI_HasSpells())
		return false;

	int8 mercCasterClass = caster->GetClass();

	if (caster->HasGroup()) {
		if (iSpellTypes == SpellType_Heal) {
			if (caster->AICastSpell(100, SpellType_Heal))
				return true;
		}

		if (iSpellTypes == SpellType_Cure) {
			if (caster->AICastSpell(100, SpellType_Cure))
				return true;
		}

		if (iSpellTypes == SpellType_Resurrect) {
			if (caster->AICastSpell(100, SpellType_Resurrect))
				return true;
		}

		//Pets > buffs like npcs
		if (iSpellTypes == SpellType_Pet) {
			if (caster->AICastSpell(100, SpellType_Pet))
				return true;
		}
		//Ok for the buffs..
		if (iSpellTypes == SpellType_Buff) {
			if (caster->AICastSpell(100, SpellType_Buff))
				return true;
		}
	}

	return false;
}

bool Merc::AIDoSpellCast(int32 spellid, Mob* tar, int32 mana_cost, uint32* oDontDoAgainBefore) {
	bool result = false;
	MercSpell mercSpell = GetMercSpellBySpellID(spellid);



	// manacost has special values, -1 is no mana cost, -2 is instant cast (no mana)
	int64 manaCost = spells[mercSpell.spellid].mana;
	int64 endurCost = spells[mercSpell.spellid].EndurCost;

	float dist2 = 0;

	Log(Logs::General, Logs::Mercenaries, "%s Casting spelltype %i. ", GetName(), mercSpell.type);

	if (mercSpell.type == SpellType_Escape) {
		dist2 = 0;
	}
	else
		dist2 = DistanceSquared(m_Position, tar->GetPosition());

	if (
		//range check
			(
				//check teleports, group heals, etc
						(
								spells[spellid].targettype == ST_GroupTeleport
							||	mercSpell.type == SpellType_Heal
							||	spells[spellid].targettype == ST_AECaster
							||	spells[spellid].targettype == ST_Group
							||	spells[spellid].targettype == ST_AEBard && dist2 <= spells[spellid].aoerange * spells[spellid].aoerange
						)
				//other individual checks, like range
				|| dist2 <= spells[spellid].range * spells[spellid].range
				|| mercSpell.type == SpellType_Pet
				|| spells[spellid].targettype == ST_Self
				|| spells[spellid].targettype == ST_TargetOptional
				//we're always in range of ourselves
				|| tar == this
			)
		
			&&

		//mana or endurance check. whichever != 0
			(
					(endurCost > 0 && endurCost <= GetEndurance())
				||	(manaCost > 0 && manaCost <= GetMana())
			)
		)
	{
		StopNavigation();

		Log(Logs::General, Logs::Mercenaries, "%s Casting %i. ", GetName(), spellid);

		result = CastSpell(spellid, tar->GetID(), EQ::spells::CastingSlot::Gem2, -1, manaCost > 0 ? manaCost : endurCost, oDontDoAgainBefore, -1, -1, 0, 0);

		if (IsCasting() && IsSitting())
			Stand();
	}

	return result;
}

bool Merc::AICastSpell(int8 iChance, uint32 iSpellTypes) {

	if(!AI_HasSpells())
		return false;

	std::vector<MercSpell> buffSpellList = GetMercSpellsBySpellType(SpellType_Root);

	int selectedSpell = 0;

	Log(Logs::General, Logs::Mercenaries, "%s has spells, trying...", GetName());

	int8 mercClass = GetClass();
	uint8 mercLevel = GetLevel();

	bool checked_los = false;       //we do not check LOS until we are absolutely sure we need to, and we only do it once.
	bool castedSpell = false;
	bool isDiscipline = false;

	if (HasGroup()) {
		Group* g = GetGroup();

		if (g) {
			MercSpell selectedMercSpell;
			selectedMercSpell.spellid = 0;
			selectedMercSpell.stance = 0;
			selectedMercSpell.type = SpellType_PreCombatBuffSong;
			selectedMercSpell.slot = 0;
			selectedMercSpell.proc_chance = 0;
			selectedMercSpell.time_cancast = 0;

			switch (iSpellTypes) {
			case SpellType_Heal: {
				Mob* tar = nullptr;
				int8 numToHeal = g->GetNumberNeedingHealedInGroup(IsEngaged() ? 75 : 95, true);
				int8 checkHPR = IsEngaged() ? 95 : 99;
				int8 checkPetHPR = IsEngaged() ? 95 : 99;

				//todo: check stance to determine healing spell selection

				for (int i = 0; i < MAX_GROUP_MEMBERS; i++) {
					if (g->members[i] && !g->members[i]->qglobal) {
						int8 hpr = (int8)g->members[i]->GetHPRatio();

						if (hpr > checkHPR) {
							continue;
						}

						if (IsEngaged() && (g->members[i]->GetClass() == NECROMANCER && hpr >= 50)
							|| (g->members[i]->GetClass() == SHAMAN && hpr >= 80)) {
							//allow necros to lifetap & shaman to canni without wasting mana
							continue;
						}

						if (hpr < checkHPR && g->members[i] == GetMercOwner()) {
							if (!tar || (hpr < tar->GetHPRatio() || (tar->IsPet() && hpr < checkPetHPR)))
								tar = g->members[i];            //check owner first
						}
						else if (hpr < checkHPR && g->HasRole(g->members[i], RoleTank)) {
							if (!tar || (hpr < tar->GetHPRatio() || (tar->IsPet() && hpr < checkPetHPR)))
								tar = g->members[i];
						}
						else if (hpr < checkHPR && (!tar || (hpr < tar->GetHPRatio() || (tar->IsPet() && hpr < checkPetHPR)))) {
							tar = g->members[i];
						}
					}
				}

				if (numToHeal > 2) {
					selectedMercSpell = GetBestMercSpellForGroupHeal();
				}

				if (tar && selectedMercSpell.spellid == 0) {
					if (tar->GetHPRatio() < 15) {
						//check for very fast heals first (casting time < 1 s)
						selectedMercSpell = GetBestMercSpellForVeryFastHeal();

						//check for fast heals next (casting time < 2 s)
						if (selectedMercSpell.spellid == 0) {
							selectedMercSpell = GetBestMercSpellForFastHeal();
						}

						//get regular heal
						if (selectedMercSpell.spellid == 0) {
							selectedMercSpell = GetBestMercSpellForRegularSingleTargetHeal();
						}
					}
					else if (tar->GetHPRatio() < 35) {
						//check for fast heals next (casting time < 2 s)
						selectedMercSpell = GetBestMercSpellForFastHeal();

						//get regular heal
						if (selectedMercSpell.spellid == 0) {
							selectedMercSpell = GetBestMercSpellForRegularSingleTargetHeal();
						}
					}
					else if (tar->GetHPRatio() < 80) {
						selectedMercSpell = GetBestMercSpellForPercentageHeal();

						//get regular heal
						if (selectedMercSpell.spellid == 0) {
							selectedMercSpell = GetBestMercSpellForRegularSingleTargetHeal();
						}
					}
					else {
						//check for heal over time. if not present, try it first
						if (!tar->FindType(SE_HealOverTime)) {
							selectedMercSpell = GetBestMercSpellForHealOverTime();

							//get regular heal
							if (selectedMercSpell.spellid == 0) {
								selectedMercSpell = GetBestMercSpellForRegularSingleTargetHeal();
							}
						}
					}
				}

				if (selectedMercSpell.spellid > 0) {
					castedSpell = AIDoSpellCast(selectedMercSpell.spellid, tar, -1);
				}

				if (castedSpell) {
					if (tar && tar != this) { // [tar] was implicitly valid at this point..this change is to catch any bad logic
						//we don't need spam of bots healing themselves
						std::string gmsg = std::string("Casting ") + std::string(spells[selectedMercSpell.spellid].name) + std::string(" on ") + std::string(tar->GetCleanName()) + std::string("."); 
						if (gmsg.length() > 0)
						{
							MercGroupSay(this, gmsg);
						}
					}
				}

				break;
			}
			case SpellType_Root: {

				if (!GetTarget())
					break;

				if (!GetTarget()->IsFleeing())
					break;

				for (auto spell : buffSpellList)
				{

					if (!IsValidSpell(spell.spellid))
					{
						break;
					}

					castedSpell = AIDoSpellCast(selectedMercSpell.spellid, GetTarget(), -1);
					if (castedSpell)
						break;
				}
				break;
			}
			case SpellType_Pet: {


				buffSpellList = GetMercSpellsBySpellType(SpellType_Pet);

				int selectedSpell = 0;

				for (auto itr = buffSpellList.begin();
					itr != buffSpellList.end(); ++itr) {
					if (!IsValidSpell(itr->spellid))
					{
						break;
					}
					Log(Logs::General, Logs::Mercenaries, "%s checking spell  %i. ", GetName(), itr->spellid);
					bool returnearly = false;
					for (Mob* pet : petlist)
					{
						if (pet->CastToNPC()->GetPetSpellID() == itr->spellid && itr->spellid != 43300)
							returnearly = true;

						Log(Logs::General, Logs::Mercenaries, "%s pet found  %i. ", GetName(), itr->spellid);
					}

					if (returnearly)
						continue;

					castedSpell = AIDoSpellCast(itr->spellid, this, -1);

					if (castedSpell)
						break;

				}
				break;
			}
			case SpellType_Buff: {

				buffSpellList = GetMercSpellsBySpellType(SpellType_Buff);

				for (auto itr = buffSpellList.begin();
					itr != buffSpellList.end(); ++itr) {
					MercSpell selectedMercSpell = *itr;

					if (!((spells[selectedMercSpell.spellid].targettype == ST_Target || spells[selectedMercSpell.spellid].targettype == ST_Pet ||
						spells[selectedMercSpell.spellid].targettype == ST_Group || spells[selectedMercSpell.spellid].targettype == ST_GroupTeleport ||
						spells[selectedMercSpell.spellid].targettype == ST_Self))) {
						continue;
					}

					if (spells[selectedMercSpell.spellid].targettype == ST_Self) {
						if (!this->IsImmuneToSpell(selectedMercSpell.spellid, this)
							&& (this->CanBuffStack(selectedMercSpell.spellid, mercLevel, true) >= 0)) {

							uint32 TempDontBuffMeBeforeTime = this->DontBuffMeBefore();

							if (selectedMercSpell.spellid > 0) {
								if (isDiscipline) {
									castedSpell = AIDoSpellCast(selectedMercSpell.spellid, this, -1);
								}
								else {
									castedSpell = AIDoSpellCast(selectedMercSpell.spellid, this, -1, &TempDontBuffMeBeforeTime);

									if (TempDontBuffMeBeforeTime != this->DontBuffMeBefore())
										this->SetDontBuffMeBefore(TempDontBuffMeBeforeTime);
								}
							}
						}
					}
					else {
						for (int i = 0; i < MAX_GROUP_MEMBERS; i++) {
							if (g->members[i]) {
								Mob* tar = g->members[i];

								if (!tar->IsImmuneToSpell(selectedMercSpell.spellid, this)
									&& (tar->CanBuffStack(selectedMercSpell.spellid, mercLevel, true) >= 0)) {

									uint32 TempDontBuffMeBeforeTime = tar->DontBuffMeBefore();

									if (selectedMercSpell.spellid > 0) {
										if (isDiscipline) {
											castedSpell = AIDoSpellCast(selectedMercSpell.spellid, this, -1);
										}
										else {
											castedSpell = AIDoSpellCast(selectedMercSpell.spellid, tar, -1, &TempDontBuffMeBeforeTime);

											if (TempDontBuffMeBeforeTime != tar->DontBuffMeBefore())
												tar->SetDontBuffMeBefore(TempDontBuffMeBeforeTime);
										}
									}
								}
							}
						}
					}
				}
				break;
			}


			case SpellType_Debuff: {

				for (auto itr = buffSpellList.begin();
					itr != buffSpellList.end(); ++itr) {

					Mob* tar = GetTarget();
					if (!tar || tar == this)
						continue;

					auto buffSpellList = GetMercSpellsBySpellType(SpellType_Debuff);
					if (!CheckSpellRecastTimers(itr->spellid))
						continue;
					if (spells[itr->spellid].zonetype != -1 && zone->GetZoneType() != -1 && spells[itr->spellid].zonetype != zone->GetZoneType()) // is this bit or index?
						continue;
					switch (spells[itr->spellid].targettype) {
						continue;
					}
					if (CanBuffStack(itr->spellid, GetLevel(), true) < 0)
						continue;

					castedSpell = AIDoSpellCast(itr->spellid, tar, -1);
					if (castedSpell)
						break;
				}
				break;
			}

			case SpellType_DOT: {
				buffSpellList = GetMercSpellsBySpellType(SpellType_DOT);

				for (auto itr = buffSpellList.begin();
					itr != buffSpellList.end(); ++itr) {

					Mob* tar = GetTarget();
					if (!tar || tar == this)
						continue;

					if (!CheckSpellRecastTimers(itr->spellid))
						continue;
					if (spells[itr->spellid].zonetype != -1 && zone->GetZoneType() != -1 && spells[itr->spellid].zonetype != zone->GetZoneType()) // is this bit or index?
						continue;
					switch (spells[itr->spellid].targettype) {
						continue;
					}
					if (CanBuffStack(itr->spellid, GetLevel(), true) < 0)
						continue;

					castedSpell = AIDoSpellCast(itr->spellid, tar, -1);
					if (castedSpell)
						break;
				}
				break;
			}

			case SpellType_Snare: {
				auto buffSpellList = GetMercSpellsBySpellType(SpellType_Snare);
				for (auto itr = buffSpellList.begin();
					itr != buffSpellList.end(); ++itr) {

					Mob* tar = GetTarget();

					if (!CheckSpellRecastTimers(itr->spellid))
						continue;
					if (spells[itr->spellid].zonetype != -1 && zone->GetZoneType() != -1 && spells[itr->spellid].zonetype != zone->GetZoneType()) // is this bit or index?
						continue;
					switch (spells[itr->spellid].targettype) {
						continue;
					}
					if (CanBuffStack(itr->spellid, GetLevel(), true) < 0)
						continue;

					castedSpell = AIDoSpellCast(itr->spellid, tar, -1);
					if (castedSpell)
						break;
				}
				break;
			}

			case SpellType_InCombatBuffSong: {
				if (GetClass() != CLASSLESS && GetClass() != BARD)
					break;
				auto buffSpellList = GetMercSpellsBySpellType(SpellType_InCombatBuffSong);
				for (auto itr = buffSpellList.begin();
					itr != buffSpellList.end(); ++itr) {

					Mob* tar = GetTarget();

					if (!CheckSpellRecastTimers(itr->spellid))
						continue;
					if (spells[itr->spellid].zonetype != -1 && zone->GetZoneType() != -1 && spells[itr->spellid].zonetype != zone->GetZoneType()) // is this bit or index?
						continue;
					switch (spells[itr->spellid].targettype) {
						continue;
					}
					if (CanBuffStack(itr->spellid, GetLevel(), true) < 0)
						continue;

					castedSpell = AIDoSpellCast(itr->spellid, tar, -1);
					if (castedSpell)
						break;
				}
				break;
			}

			case SpellType_Nuke: {
				//check for taunt
				if (CheckAETaunt()) {
					//get AE taunt
					selectedMercSpell = GetBestMercSpellForAETaunt();
					Log(Logs::General, Logs::Mercenaries, "%s AE Taunting.", GetName());
				}

				if (selectedMercSpell.spellid == 0 && CheckTaunt()) {
					//get taunt
					selectedMercSpell = GetBestMercSpellForTaunt();
				}

				//get hate disc
				if (selectedMercSpell.spellid == 0) {
					selectedMercSpell = GetBestMercSpellForHate();
				}

				Mob* tar = GetTarget();

				selectedMercSpell = GetBestMercSpellForAENuke(tar);

				if (selectedMercSpell.spellid == 0 && !tar->GetSpecialAbility(UNSTUNABLE) && !tar->IsStunned()) {
					uint8 stunChance = 15;
					if (zone->random.Roll(stunChance)) {
						selectedMercSpell = GetBestMercSpellForStun();
					}
				}

				if (selectedMercSpell.spellid == 0) {
					uint8 lureChance = 25;
					if (zone->random.Roll(lureChance)) {
						selectedMercSpell = GetBestMercSpellForNukeByTargetResists(tar);
					}
				}

				if (selectedMercSpell.spellid == 0) {
					selectedMercSpell = GetBestMercSpellForNuke();
				}

				break;

				if (selectedMercSpell.spellid > 0) {
					if (isDiscipline) {
						castedSpell = AIDoSpellCast(selectedMercSpell.spellid, this, -1);
					}
					else {
						castedSpell = AIDoSpellCast(selectedMercSpell.spellid, GetTarget(), -1);
					}
				}

				break;
			}
			case SpellType_InCombatBuff: {
				buffSpellList = GetMercSpellsBySpellType(SpellType_InCombatBuff);
				Mob* tar = this;

				for (auto itr = buffSpellList.begin();
					itr != buffSpellList.end(); ++itr) {
					MercSpell selectedMercSpell = *itr;

					if (!(spells[selectedMercSpell.spellid].targettype == ST_Self)) {
						continue;
					}

					if (spells[selectedMercSpell.spellid].skill == EQ::skills::SkillBackstab && spells[selectedMercSpell.spellid].targettype == ST_Self) {
						if (!hidden) {
							continue;
						}
					}

					if (!tar->IsImmuneToSpell(selectedMercSpell.spellid, this)
						&& (tar->CanBuffStack(selectedMercSpell.spellid, mercLevel, true) >= 0)) {

						uint32 TempDontBuffMeBeforeTime = tar->DontBuffMeBefore();

						if (selectedMercSpell.spellid > 0) {
							if (isDiscipline) {
								castedSpell = AIDoSpellCast(selectedMercSpell.spellid, this, -1);
							}
							else {
								castedSpell = AIDoSpellCast(selectedMercSpell.spellid, this, -1);
							}
						}
					}
				}
				break;
			}
			case SpellType_Cure: {
				Mob* tar = nullptr;
				for (int i = 0; i < MAX_GROUP_MEMBERS; i++) {
					if (g->members[i] && !g->members[i]->qglobal) {
						if (GetNeedsCured(g->members[i]) && (g->members[i]->DontCureMeBefore() < Timer::GetCurrentTime())) {
							tar = g->members[i];
						}
					}
				}

				if (tar && !(g->GetNumberNeedingHealedInGroup(IsEngaged() ? 25 : 40, false) > 0) && !(g->GetNumberNeedingHealedInGroup(IsEngaged() ? 40 : 60, false) > 2))
				{
					selectedMercSpell = GetBestMercSpellForCure(tar);

					if (selectedMercSpell.spellid == 0)
						break;

					uint32 TempDontCureMeBeforeTime = tar->DontCureMeBefore();

					castedSpell = AIDoSpellCast(selectedMercSpell.spellid, tar, spells[selectedMercSpell.spellid].mana, &TempDontCureMeBeforeTime);

					if (castedSpell) {
						if (IsGroupSpell(selectedMercSpell.spellid)) {

							if (this->HasGroup()) {
								Group* g = this->GetGroup();

								if (g) {
									for (int i = 0; i < MAX_GROUP_MEMBERS; i++) {
										if (g->members[i] && !g->members[i]->qglobal) {
											if (TempDontCureMeBeforeTime != tar->DontCureMeBefore())
												g->members[i]->SetDontCureMeBefore(Timer::GetCurrentTime() + 4000);
										}
									}
								}
							}
						}
						else {
							if (TempDontCureMeBeforeTime != tar->DontCureMeBefore())
								tar->SetDontCureMeBefore(Timer::GetCurrentTime() + 4000);
						}
					}
				}
				break;
			}
			case SpellType_Resurrect: {
				Corpse* corpse = GetGroupMemberCorpse();

				if (corpse) {
					selectedMercSpell = GetFirstMercSpellBySpellType(SpellType_Resurrect);

					if (selectedMercSpell.spellid == 0)
						break;

					uint32 TempDontRootMeBeforeTime = corpse->DontRootMeBefore();

					castedSpell = AIDoSpellCast(selectedMercSpell.spellid, corpse, spells[selectedMercSpell.spellid].mana, &TempDontRootMeBeforeTime);

					//CastSpell(selectedMercSpell.spellid, corpse->GetID(), 1, -1, -1, &TempDontRootMeBeforeTime);
					corpse->SetDontRootMeBefore(TempDontRootMeBeforeTime);
				}

				break;
			}
			case SpellType_Escape: {
				Mob* tar = GetTarget();
				bool mayGetAggro = false;

				if (tar) {
					mayGetAggro = HasOrMayGetAggro(); //classes have hate reducing spells

					if (mayGetAggro) {
						selectedMercSpell = GetFirstMercSpellBySpellType(SpellType_Escape);

						if (selectedMercSpell.spellid == 0)
							break;
						if (!IsBeneficialSpell(selectedMercSpell.spellid))
							castedSpell = AIDoSpellCast(selectedMercSpell.spellid, tar, -1);
						else
							castedSpell = AIDoSpellCast(selectedMercSpell.spellid, this, -1);
					}
				}

				break;
			}
			}
		}
	}
	return castedSpell;
}

void Merc::CheckHateList() {
	if(check_target_timer.Enabled())
		check_target_timer.Disable();

	if(!IsEngaged()) {
		if(GetFollowID()) {
			Group* g = GetGroup();
			if(g) {
				Mob* MercOwner = GetOwner();
				if(MercOwner && MercOwner->GetTarget() && MercOwner->GetTarget()->IsNPC() && (MercOwner->GetTarget()->GetHateAmount(MercOwner) || MercOwner->CastToClient()->AutoAttackEnabled()) && IsAttackAllowed(MercOwner->GetTarget())) {
					float range = g->HasRole(MercOwner, RolePuller) ? RuleI(Mercs, AggroRadiusPuller) : RuleI(Mercs, AggroRadius);
					range = range * range;
					if(DistanceSquaredNoZ(m_Position, MercOwner->GetTarget()->GetPosition()) < range) {
						AddToHateList(MercOwner->GetTarget(), 1);
					}
				}
				else {
					std::list<NPC*> npc_list;
					entity_list.GetNPCList(npc_list);

					for (auto itr = npc_list.begin(); itr != npc_list.end(); ++itr) {
						NPC* npc = *itr;
						float dist = DistanceSquaredNoZ(m_Position, npc->GetPosition());
						int radius = RuleI(Mercs, AggroRadius);
						radius *= radius;
						if(dist <= radius) {

							for(int counter = 0; counter < g->GroupCount(); counter++) {
								Mob* groupMember = g->members[counter];
								if(groupMember) {
									if(npc->IsOnHatelist(groupMember)) {
										if(!hate_list.IsEntOnHateList(npc)) {
											float range = g->HasRole(groupMember, RolePuller) ? RuleI(Mercs, AggroRadiusPuller) : RuleI(Mercs, AggroRadius);
											range *= range;
											if(DistanceSquaredNoZ(m_Position, npc->GetPosition()) < range) {
												hate_list.AddEntToHateList(npc, 1);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

bool Merc::HasOrMayGetAggro() {
	bool mayGetAggro = false;

	if(GetTarget() && GetTarget()->GetHateTop()) {
		Mob *topHate = GetTarget()->GetHateTop();

		if(topHate == this)
			mayGetAggro = true; //I currently have aggro
		else {
			int64 myHateAmt = GetTarget()->GetHateAmount(this);
			int64 topHateAmt = GetTarget()->GetHateAmount(topHate);

			if(myHateAmt > 0 && topHateAmt > 0 && (uint8)((myHateAmt/topHateAmt)*100) > 90) //I have 90% as much hate as top, next action may give me aggro
				mayGetAggro = true;
		}
	}

	return mayGetAggro;
}

bool Merc::CheckAENuke(Mob* tar, int32 spell_id, uint8 &numTargets) {
	std::list<NPC*> npc_list;
	entity_list.GetNPCList(npc_list);

	for (auto itr = npc_list.begin(); itr != npc_list.end(); ++itr) {
		NPC* npc = *itr;

		if(DistanceSquaredNoZ(npc->GetPosition(), tar->GetPosition()) <= spells[spell_id].aoerange * spells[spell_id].aoerange) {
			if(!npc->IsMezzed()) {
				numTargets++;
			}
			else {
				numTargets = 0;
				return false;
			}
		}
	}

	if(numTargets > 1)
		return true;

	return false;
}

int16 Merc::GetFocusEffect(focusType type, int32 spell_id) {

	int16 realTotal = 0;
	int16 realTotal2 = 0;
	int16 realTotal3 = 0;
	bool rand_effectiveness = false;

	//Improved Healing, Damage & Mana Reduction are handled differently in that some are random percentages
	//In these cases we need to find the most powerful effect, so that each piece of gear wont get its own chance
	if((type == focusManaCost || type == focusImprovedHeal || type == focusImprovedDamage)
		&& RuleB(Spells, LiveLikeFocusEffects))
	{
		rand_effectiveness = true;
	}

	//Check if item focus effect exists for the client.
	if (itembonuses.FocusEffects[type]){

		const EQ::ItemData* TempItem = nullptr;
		const EQ::ItemData* UsedItem = nullptr;
		uint16 UsedFocusID = 0;
		int16 Total = 0;
		int16 focus_max = 0;
		int16 focus_max_real = 0;

		//item focus
		for (int x = EQ::invslot::EQUIPMENT_BEGIN; x <= EQ::invslot::EQUIPMENT_END; ++x)
		{

			TempItem = nullptr;
			if (equipment[x] == 0)
				continue;
			TempItem = database.GetItem(equipment[x]);
			if (TempItem && TempItem->Focus.Effect > 0 && TempItem->Focus.Effect != SPELL_UNKNOWN) {
				if(rand_effectiveness) {
					focus_max = CalcFocusEffect(type, TempItem->Focus.Effect, spell_id, true);
					if (focus_max > 0 && focus_max_real >= 0 && focus_max > focus_max_real) {
						focus_max_real = focus_max;
						UsedItem = TempItem;
						UsedFocusID = TempItem->Focus.Effect;
					} else if (focus_max < 0 && focus_max < focus_max_real) {
						focus_max_real = focus_max;
						UsedItem = TempItem;
						UsedFocusID = TempItem->Focus.Effect;
					}
				}
				else {
					Total = CalcFocusEffect(type, TempItem->Focus.Effect, spell_id);
					if (Total > 0 && realTotal >= 0 && Total > realTotal) {
						realTotal = Total;
						UsedItem = TempItem;
						UsedFocusID = TempItem->Focus.Effect;
					} else if (Total < 0 && Total < realTotal) {
						realTotal = Total;
						UsedItem = TempItem;
						UsedFocusID = TempItem->Focus.Effect;
					}
				}
			}
		}

		if(UsedItem && rand_effectiveness && focus_max_real != 0)
			realTotal = CalcFocusEffect(type, UsedFocusID, spell_id);

		if (realTotal != 0 && UsedItem)
			MessageString(Chat::Spells, BEGINS_TO_GLOW, UsedItem->Name);
	}

	//Check if spell focus effect exists for the client.
	if (spellbonuses.FocusEffects[type]){

		//Spell Focus
		int32 Total2 = 0;
		int32 focus_max2 = 0;
		int32 focus_max_real2 = 0;

		int buff_tracker = -1;
		int buff_slot = 0;
		int32 focusspellid = 0;
		int32 focusspell_tracker = 0;
		uint32 buff_max = GetMaxTotalSlots();
		for (buff_slot = 0; buff_slot < buff_max; buff_slot++) {
			focusspellid = buffs[buff_slot].spellid;
			if (focusspellid == 0 || focusspellid >= SPDAT_RECORDS)
				continue;

			if(rand_effectiveness) {
				focus_max2 = CalcFocusEffect(type, focusspellid, spell_id, true);
				if (focus_max2 > 0 && focus_max_real2 >= 0 && focus_max2 > focus_max_real2) {
					focus_max_real2 = focus_max2;
					buff_tracker = buff_slot;
					focusspell_tracker = focusspellid;
				} else if (focus_max2 < 0 && focus_max2 < focus_max_real2) {
					focus_max_real2 = focus_max2;
					buff_tracker = buff_slot;
					focusspell_tracker = focusspellid;
				}
			}
			else {
				Total2 = CalcFocusEffect(type, focusspellid, spell_id);
				if (Total2 > 0 && realTotal2 >= 0 && Total2 > realTotal2) {
					realTotal2 = Total2;
					buff_tracker = buff_slot;
					focusspell_tracker = focusspellid;
				} else if (Total2 < 0 && Total2 < realTotal2) {
					realTotal2 = Total2;
					buff_tracker = buff_slot;
					focusspell_tracker = focusspellid;
				}
			}
		}

		if(focusspell_tracker && rand_effectiveness && focus_max_real2 != 0)
			realTotal2 = CalcFocusEffect(type, focusspell_tracker, spell_id);

		// For effects like gift of mana that only fire once, save the spellid into an array that consists of all available buff slots.
		if(buff_tracker >= 0 && buffs[buff_tracker].numhits > 0) {
			m_spellHitsLeft[buff_tracker] = focusspell_tracker;
		}
	}


	// AA Focus
	/*if (aabonuses.FocusEffects[type]){

	int16 Total3 = 0;
	uint32 slots = 0;
	uint32 aa_AA = 0;
	uint32 aa_value = 0;

	for (int i = 0; i < MAX_PP_AA_ARRAY; i++)
	{
	aa_AA = this->aa[i]->AA;
	aa_value = this->aa[i]->value;
	if (aa_AA < 1 || aa_value < 1)
	continue;

	Total3 = CalcAAFocus(type, aa_AA, spell_id);
	if (Total3 > 0 && realTotal3 >= 0 && Total3 > realTotal3) {
	realTotal3 = Total3;
	}
	else if (Total3 < 0 && Total3 < realTotal3) {
	realTotal3 = Total3;
	}
	}
	}*/

	if(type == focusReagentCost && IsSummonPetSpell(spell_id) && GetAA(aaElementalPact))
		return 100;

	if(type == focusReagentCost && (IsEffectInSpell(spell_id, SE_SummonItem) || IsSacrificeSpell(spell_id)))
		return 0;
	//Summon Spells that require reagents are typically imbue type spells, enchant metal, sacrifice and shouldn't be affected
	//by reagent conservation for obvious reasons.

	return realTotal + realTotal2 + realTotal3;
}

int32 Merc::GetActSpellCost(int32 spell_id, int32 cost)
{
	// Formula = Unknown exact, based off a random percent chance up to mana cost(after focuses) of the cast spell
	if(this->itembonuses.Clairvoyance && spells[spell_id].classes[(GetClass()%17) - 1] >= GetLevel() - 5)
	{
		int16 mana_back = this->itembonuses.Clairvoyance * zone->random.Int(1, 100) / 100;
		// Doesnt generate mana, so best case is a free spell
		if(mana_back > cost)
			mana_back = cost;

		cost -= mana_back;
	}

	// This formula was derived from the following resource:
	// http://www.eqsummoners.com/eq1/specialization-library.html
	// WildcardX
	float PercentManaReduction = 0;

	int16 focus_redux = GetFocusEffect(focusManaCost, spell_id);

	if(focus_redux > 0)
	{
		PercentManaReduction += zone->random.Real(1, (double)focus_redux);
	}

	cost -= (cost * (PercentManaReduction / 100));

	// Gift of Mana - reduces spell cost to 1 mana
	if(focus_redux >= 100) {
		uint32 buff_max = GetMaxTotalSlots();
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

int32 Merc::GetActSpellCasttime(int32 spell_id, int32 casttime)
{
	int32 cast_reducer = 0;
	cast_reducer += GetFocusEffect(focusSpellHaste, spell_id);

	if (cast_reducer > RuleI(Spells, MaxCastTimeReduction))
		cast_reducer = RuleI(Spells, MaxCastTimeReduction);

	casttime = (casttime*(100 - cast_reducer)/100);

	return casttime;
}

int8 Merc::GetChanceToCastBySpellType(SpellTypes spellType) {
	int mercStance = (int)GetStance();
	int8 mercClass = GetClass();
	int8 chance = 0;

	switch (spellType) {

	case SpellType_Buff: {
		chance = IsEngaged() ? 0 : 100;
		break;
	}
	default:
	{
		chance = 100;
		break;
	}
	}
	return chance;
}

bool Merc::CheckStance(int16 stance) {
	return true;
}

std::vector<MercSpell> Merc::GetMercSpellsBySpellType(SpellTypes spellType) {
	std::vector<MercSpell> result;

	if(AI_HasSpells()) {
		auto mercSpellList = merc_spells;

		for (auto itr = merc_spells.begin();
			itr != merc_spells.end(); ++itr) {
			if (itr->spellid <= 0 ||itr->spellid >= SPDAT_RECORDS) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if((itr->type == spellType) && CheckStance(itr->stance)) {

				MercSpell resultentry;
				resultentry.spellid = itr->spellid;
				resultentry.stance = itr->stance;
				resultentry.type = itr->type;
				resultentry.slot = itr->slot;
				resultentry.proc_chance = itr->proc_chance;
				resultentry.time_cancast = itr->time_cancast;
				result.push_back(resultentry);
			}
		}
	}

	return result;
}

MercSpell Merc::GetFirstMercSpellBySpellType(SpellTypes spellType) {

	MercSpell result;

	result.spellid = -1;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(AI_HasSpells()) {
		std::vector<MercSpell> mercSpellList = merc_spells;

		for (auto itr = merc_spells.begin();
			itr != merc_spells.end(); ++itr) {
			if (itr->spellid <= 0 || itr->spellid >= SPDAT_RECORDS) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if((itr->type & spellType)
				&& CheckStance(itr->stance)
				&& CheckSpellRecastTimers(itr->spellid)) {

				MercSpell resultentry;
				resultentry.spellid = itr->spellid;
				resultentry.stance = itr->stance;
				resultentry.type = itr->type;
				resultentry.slot = itr->slot;
				resultentry.proc_chance = itr->proc_chance;
				resultentry.time_cancast = itr->time_cancast;
				return resultentry;
			}
		}
	}

	return result;
}

MercSpell Merc::GetMercSpellBySpellID(int32 spellid) {
	
	
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(AI_HasSpells()) {
		std::vector<MercSpell> mercSpellList = merc_spells;

		for (int i = mercSpellList.size() - 1; i >= 0; i--) {
			if (mercSpellList[i].spellid <= 0 || mercSpellList[i].spellid >= SPDAT_RECORDS) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if((mercSpellList[i].spellid == spellid)
				&& CheckStance(mercSpellList[i].stance)) {
					result.spellid = mercSpellList[i].spellid;
					result.stance = mercSpellList[i].stance;
					result.type = mercSpellList[i].type;
					result.slot = mercSpellList[i].slot;
					result.proc_chance = mercSpellList[i].proc_chance;
					result.time_cancast = mercSpellList[i].time_cancast;

					break;
			}
		}
	}

	return result;
}

std::vector<MercSpell> Merc::GetMercSpellsForSpellEffect(int32 spellEffect) {
	std::vector<MercSpell> result;

	if(AI_HasSpells()) {
		std::vector<MercSpell> mercSpellList = merc_spells;

		for (int i = mercSpellList.size() - 1; i >= 0; i--) {
			if (mercSpellList[i].spellid <= 0 || mercSpellList[i].spellid >= SPDAT_RECORDS) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if(IsEffectInSpell(mercSpellList[i].spellid, spellEffect) && CheckStance(mercSpellList[i].stance)) {
				MercSpell MercSpell;
				MercSpell.spellid = mercSpellList[i].spellid;
				MercSpell.stance = mercSpellList[i].stance;
				MercSpell.type = mercSpellList[i].type;
				MercSpell.slot = mercSpellList[i].slot;
				MercSpell.proc_chance = mercSpellList[i].proc_chance;
				MercSpell.time_cancast = mercSpellList[i].time_cancast;

				result.push_back(MercSpell);
			}
		}
	}

	return result;
}

std::vector<MercSpell> Merc::GetMercSpellsForSpellEffectAndTargetType(int spellEffect, SpellTargetType targetType) {
	std::vector<MercSpell> result;

	if(AI_HasSpells()) {
		std::vector<MercSpell> mercSpellList = merc_spells;

		for (auto itr = merc_spells.begin();
			itr != merc_spells.end(); ++itr) {
			if (itr->spellid <= 0 || itr->spellid >= SPDAT_RECORDS) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if (IsEffectInSpell(itr->spellid, spellEffect) && CheckStance(itr->stance)) {
				if (spells[itr->spellid].targettype == targetType) {
					MercSpell MercSpell;
					MercSpell.spellid = itr->spellid;
					MercSpell.stance = itr->stance;
					MercSpell.type = itr->type;
					MercSpell.slot = itr->slot;
					MercSpell.proc_chance = itr->proc_chance;
					MercSpell.time_cancast = itr->time_cancast;

					result.push_back(MercSpell);
				}
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForVeryFastHeal() {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;
	std::vector<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(SE_CurrentHP);

	for (auto itr = mercSpellList.begin();
		itr != mercSpellList.end(); ++itr) {
		// Assuming all the spells have been loaded into this list by level and in descending order
		if(IsVeryFastHealSpell(itr->spellid)
			&& CheckSpellRecastTimers(itr->spellid)) {
				result.spellid = itr->spellid;
				result.stance = itr->stance;
				result.type = itr->type;
				result.slot = itr->slot;
				result.proc_chance = itr->proc_chance;
				result.time_cancast = itr->time_cancast;

				break;
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForFastHeal() {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;
		std::vector<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(SE_CurrentHP);

		for (auto itr = mercSpellList.begin();
			itr != mercSpellList.end(); ++itr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if (IsFastHealSpell(itr->spellid)
				&& CheckSpellRecastTimers(itr->spellid)) {
				result.spellid = itr->spellid;
				result.stance = itr->stance;
				result.type = itr->type;
				result.slot = itr->slot;
				result.proc_chance = itr->proc_chance;
				result.time_cancast = itr->time_cancast;

				break;
			}
		}

	return result;
}

MercSpell Merc::GetBestMercSpellForHealOverTime() {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;
		std::vector<MercSpell> mercHoTSpellList = GetMercSpellsForSpellEffect(SE_HealOverTime);

		for (auto itr = mercHoTSpellList.begin();
			itr != mercHoTSpellList.end(); ++itr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsHealOverTimeSpell(itr->spellid)) {

				if (itr->spellid <= 0 || itr->spellid >= SPDAT_RECORDS) {
					// this is both to quit early to save cpu and to avoid casting bad spells
					// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
					continue;
				}

				if(CheckSpellRecastTimers(itr->spellid)) {
					result.spellid = itr->spellid;
					result.stance = itr->stance;
					result.type = itr->type;
					result.slot = itr->slot;
					result.proc_chance = itr->proc_chance;
					result.time_cancast = itr->time_cancast;
				}

				break;
			}
		}
	return result;
}

MercSpell Merc::GetBestMercSpellForPercentageHeal() {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(AI_HasSpells()) {
		std::vector<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(SE_CurrentHP);

		for (auto itr = mercSpellList.begin();
			itr != mercSpellList.end(); ++itr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsCompleteHealSpell(itr->spellid)
				&& CheckSpellRecastTimers(itr->spellid)) {
					result.spellid = itr->spellid;
					result.stance = itr->stance;
					result.type = itr->type;
					result.slot = itr->slot;
					result.proc_chance = itr->proc_chance;
					result.time_cancast = itr->time_cancast;

					break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForRegularSingleTargetHeal() {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

		std::vector<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(SE_CurrentHP);

		for (auto itr = mercSpellList.begin();
			itr != mercSpellList.end(); ++itr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if (IsRegularSingleTargetHealSpell(itr->spellid)
				&& CheckSpellRecastTimers(itr->spellid)) {
				result.spellid = itr->spellid;
				result.stance = itr->stance;
				result.type = itr->type;
				result.slot = itr->slot;
				result.proc_chance = itr->proc_chance;
				result.time_cancast = itr->time_cancast;

				break;
			}
		}

	return result;
}

MercSpell Merc::GetFirstMercSpellForSingleTargetHeal() {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;
		std::vector<MercSpell> mercSpellList = GetMercSpellsForSpellEffect( SE_CurrentHP);

		for (auto itr = mercSpellList.begin();
			itr != mercSpellList.end(); ++itr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if((IsRegularSingleTargetHealSpell(itr->spellid)
				|| IsFastHealSpell(itr->spellid))
				&& CheckSpellRecastTimers(itr->spellid)) {
					result.spellid = itr->spellid;
					result.stance = itr->stance;
					result.type = itr->type;
					result.slot = itr->slot;
					result.proc_chance = itr->proc_chance;
					result.time_cancast = itr->time_cancast;

					break;
			}
		}

	return result;
}

MercSpell Merc::GetBestMercSpellForGroupHeal() {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;
		std::vector<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(SE_CurrentHP);

		for (auto itr = mercSpellList.begin();
			itr != mercSpellList.end(); ++itr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if (IsRegularGroupHealSpell(itr->spellid)
				&& CheckSpellRecastTimers(itr->spellid)) {
				result.spellid = itr->spellid;
				result.stance = itr->stance;
				result.type = itr->type;
				result.slot = itr->slot;
				result.proc_chance = itr->proc_chance;
				result.time_cancast = itr->time_cancast;

				break;
			}
		}

	return result;
}

MercSpell Merc::GetBestMercSpellForGroupHealOverTime() {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;
		std::vector<MercSpell> mercHoTSpellList = GetMercSpellsForSpellEffect(SE_HealOverTime);

		for (auto itr = mercHoTSpellList.begin();
			itr != mercHoTSpellList.end(); ++itr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if (IsGroupHealOverTimeSpell(itr->spellid)) {

				if (itr->spellid <= 0 || itr->spellid >= SPDAT_RECORDS) {
					// this is both to quit early to save cpu and to avoid casting bad spells
					// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
					continue;
				}

				if (CheckSpellRecastTimers(itr->spellid)) {
					result.spellid = itr->spellid;
					result.stance = itr->stance;
					result.type = itr->type;
					result.slot = itr->slot;
					result.proc_chance = itr->proc_chance;
					result.time_cancast = itr->time_cancast;
				}

				break;
			}
		}

	return result;
}

MercSpell Merc::GetBestMercSpellForGroupCompleteHeal() {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

		std::vector<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(SE_CompleteHeal);

		for (auto itr = mercSpellList.begin();
			itr != mercSpellList.end(); ++itr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsGroupCompleteHealSpell(itr->spellid)
				&& CheckSpellRecastTimers(itr->spellid)) {
					result.spellid = itr->spellid;
					result.stance = itr->stance;
					result.type = itr->type;
					result.slot = itr->slot;
					result.proc_chance = itr->proc_chance;
					result.time_cancast = itr->time_cancast;

					break;
			}
		}

	return result;
}

MercSpell Merc::GetBestMercSpellForAETaunt() {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;
		std::vector<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(SE_Taunt);

		for (auto itr = mercSpellList.begin();
			itr != mercSpellList.end(); ++itr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if ((spells[itr->spellid].targettype == ST_AECaster
				|| spells[itr->spellid].targettype == ST_AETarget
				|| spells[itr->spellid].targettype == ST_UndeadAE)
				&& CheckSpellRecastTimers(itr->spellid)) {
				result.spellid = itr->spellid;
				result.stance = itr->stance;
				result.type = itr->type;
				result.slot = itr->slot;
				result.proc_chance = itr->proc_chance;
				result.time_cancast = itr->time_cancast;

				break;
			}
		}

	return result;
}

MercSpell Merc::GetBestMercSpellForTaunt() {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;
		std::vector<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(SE_Taunt);

		for (auto itr = mercSpellList.begin();
			itr != mercSpellList.end(); ++itr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if ((spells[itr->spellid].targettype == ST_Target)
				&& CheckSpellRecastTimers(itr->spellid)) {
				result.spellid = itr->spellid;
				result.stance = itr->stance;
				result.type = itr->type;
				result.slot = itr->slot;
				result.proc_chance = itr->proc_chance;
				result.time_cancast = itr->time_cancast;

				break;
			}
		}

	return result;
}

MercSpell Merc::GetBestMercSpellForHate() {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;
		std::vector<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(SE_InstantHate);

		for (auto itr = mercSpellList.begin();
			itr != mercSpellList.end(); ++itr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if (CheckSpellRecastTimers(itr->spellid)) {
				result.spellid = itr->spellid;
				result.stance = itr->stance;
				result.type = itr->type;
				result.slot = itr->slot;
				result.proc_chance = itr->proc_chance;
				result.time_cancast = itr->time_cancast;

				break;
			}
		}

	return result;
}

MercSpell Merc::GetBestMercSpellForCure(Mob *tar) {
	MercSpell result;
	bool spellSelected = false;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(!tar)
		return result;

	int countNeedsCured = 0;
	bool isPoisoned = tar->FindType(SE_PoisonCounter);
	bool isDiseased = tar->FindType(SE_DiseaseCounter);
	bool isCursed = tar->FindType(SE_CurseCounter);
	bool isCorrupted = tar->FindType(SE_CorruptionCounter);

	if(AI_HasSpells()) {
		std::vector<MercSpell> cureList = GetMercSpellsBySpellType(SpellType_Cure);

		if(tar->HasGroup()) {
			Group *g = tar->GetGroup();

			if(g) {
				for( int i = 0; i<MAX_GROUP_MEMBERS; i++) {
					if(g->members[i] && !g->members[i]->qglobal) {
						if(GetNeedsCured(g->members[i]))
							countNeedsCured++;
					}
				}
			}
		}

		//Check for group cure first
		if(countNeedsCured > 2) {
			for (auto itr = cureList.begin(); itr != cureList.end(); ++itr) {
				MercSpell selectedMercSpell = *itr;

				if(IsGroupSpell(itr->spellid) && CheckSpellRecastTimers(itr->spellid)) {
					if(selectedMercSpell.spellid == 0)
						continue;

					if(isPoisoned && IsEffectInSpell(itr->spellid, SE_PoisonCounter)) {
						spellSelected = true;
					}
					else if(isDiseased && IsEffectInSpell(itr->spellid, SE_DiseaseCounter)) {
						spellSelected = true;
					}
					else if(isCursed && IsEffectInSpell(itr->spellid, SE_CurseCounter)) {
						spellSelected = true;
					}
					else if(isCorrupted && IsEffectInSpell(itr->spellid, SE_CorruptionCounter)) {
						spellSelected = true;
					}
					else if(IsEffectInSpell(itr->spellid, SE_DispelDetrimental)) {
						spellSelected = true;
					}

					if(spellSelected)
					{
						result.spellid = itr->spellid;
						result.stance = itr->stance;
						result.type = itr->type;
						result.slot = itr->slot;
						result.proc_chance = itr->proc_chance;
						result.time_cancast = itr->time_cancast;

						break;
					}
				}
			}
		}

		//no group cure for target- try to find single target spell
		if(!spellSelected) {
			for (auto itr = cureList.begin(); itr != cureList.end(); ++itr) {
				MercSpell selectedMercSpell = *itr;

				if(CheckSpellRecastTimers(itr->spellid)) {
					if(selectedMercSpell.spellid == 0)
						continue;

					if(isPoisoned && IsEffectInSpell(itr->spellid, SE_PoisonCounter)) {
						spellSelected = true;
					}
					else if(isDiseased && IsEffectInSpell(itr->spellid, SE_DiseaseCounter)) {
						spellSelected = true;
					}
					else if(isCursed && IsEffectInSpell(itr->spellid, SE_CurseCounter)) {
						spellSelected = true;
					}
					else if(isCorrupted && IsEffectInSpell(itr->spellid, SE_CorruptionCounter)) {
						spellSelected = true;
					}
					else if(IsEffectInSpell(itr->spellid, SE_DispelDetrimental)) {
						spellSelected = true;
					}

					if(spellSelected)
					{
						result.spellid = itr->spellid;
						result.stance = itr->stance;
						result.type = itr->type;
						result.slot = itr->slot;
						result.proc_chance = itr->proc_chance;
						result.time_cancast = itr->time_cancast;

						break;
					}
				}
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForStun() {
	MercSpell result;
	bool spellSelected = false;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;
		std::vector<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(SE_Stun);
		for (auto itr = mercSpellList.begin();
			itr != mercSpellList.end(); ++itr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if (CheckSpellRecastTimers(itr->spellid)) {
				result.spellid = itr->spellid;
				result.stance = itr->stance;
				result.type = itr->type;
				result.slot = itr->slot;
				result.proc_chance = itr->proc_chance;
				result.time_cancast = itr->time_cancast;

				break;
			}
		}

	return result;
}

MercSpell Merc::GetBestMercSpellForAENuke(Mob* tar) {
	MercSpell result;
	bool spellSelected = false;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;
		uint8 initialCastChance = 0;
		uint8 castChanceFalloff = 75;

		switch(GetStance())
		{
		case EQ::constants::stanceBurnAE:
			initialCastChance = 50;
			break;
		case EQ::constants::stanceBalanced:
			initialCastChance = 25;
			break;
		case EQ::constants::stanceBurn:
			initialCastChance = 0;
			break;
		}

		//check of we even want to cast an AE nuke
		if (zone->random.Roll(initialCastChance)) {

			result = GetBestMercSpellForAERainNuke(tar);

			//check if we have a spell & allow for other AE nuke types
			if (result.spellid == 0 && zone->random.Roll(castChanceFalloff)) {

				result = GetBestMercSpellForPBAENuke(tar);

				//check if we have a spell & allow for other AE nuke types
				if (result.spellid == 0 && zone->random.Roll(castChanceFalloff)) {

					result = GetBestMercSpellForTargetedAENuke(tar);
				}
			}
		}
	return result;
}

MercSpell Merc::GetBestMercSpellForTargetedAENuke(Mob* tar) {
	MercSpell result;
	int castChance = 50;            //used to cycle through multiple spells (first has 50% overall chance, 2nd has 25%, etc.)
	int numTargetsCheck = 1;        //used to check for min number of targets to use AE
	bool spellSelected = false;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	switch(GetStance())
	{
	case EQ::constants::stanceBurnAE:
		numTargetsCheck = 1;
		break;
	case EQ::constants::stanceBalanced:
	case EQ::constants::stanceBurn:
		numTargetsCheck = 2;
		break;
	}
		std::vector<MercSpell> mercSpellList = GetMercSpellsBySpellType(SpellType_Nuke);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
			++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if (IsAENukeSpell(mercSpellListItr->spellid) && !IsAERainNukeSpell(mercSpellListItr->spellid)
				&& !IsPBAENukeSpell(mercSpellListItr->spellid) && CheckSpellRecastTimers(mercSpellListItr->spellid)) {
				uint8 numTargets = 0;
				if (CheckAENuke(tar, mercSpellListItr->spellid, numTargets)) {
					if (numTargets >= numTargetsCheck && zone->random.Roll(castChance)) {
						result.spellid = mercSpellListItr->spellid;
						result.stance = mercSpellListItr->stance;
						result.type = mercSpellListItr->type;
						result.slot = mercSpellListItr->slot;
						result.proc_chance = mercSpellListItr->proc_chance;
						result.time_cancast = mercSpellListItr->time_cancast;
					}
				}

				break;
			}
		}

	return result;
}

MercSpell Merc::GetBestMercSpellForPBAENuke(Mob* tar) {
	MercSpell result;
	int castChance = 50;            //used to cycle through multiple spells (first has 50% overall chance, 2nd has 25%, etc.)
	int numTargetsCheck = 1;        //used to check for min number of targets to use AE
	bool spellSelected = false;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	switch(GetStance())
	{
	case EQ::constants::stanceBurnAE:
		numTargetsCheck = 2;
		break;
	case EQ::constants::stanceBalanced:
	case EQ::constants::stanceBurn:
		numTargetsCheck = 3;
		break;
	}

		std::vector<MercSpell> mercSpellList = GetMercSpellsBySpellType(SpellType_Nuke);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
			++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if (IsPBAENukeSpell(mercSpellListItr->spellid) && CheckSpellRecastTimers(mercSpellListItr->spellid)) {
				uint8 numTargets = 0;
				if (CheckAENuke(this, mercSpellListItr->spellid, numTargets)) {
					if (numTargets >= numTargetsCheck && zone->random.Roll(castChance)) {
						result.spellid = mercSpellListItr->spellid;
						result.stance = mercSpellListItr->stance;
						result.type = mercSpellListItr->type;
						result.slot = mercSpellListItr->slot;
						result.proc_chance = mercSpellListItr->proc_chance;
						result.time_cancast = mercSpellListItr->time_cancast;
					}
				}

				break;
			}
		}

	return result;
}

MercSpell Merc::GetBestMercSpellForAERainNuke(Mob* tar) {
	MercSpell result;
	int castChance = 50;            //used to cycle through multiple spells (first has 50% overall chance, 2nd has 25%, etc.)
	int numTargetsCheck = 1;        //used to check for min number of targets to use AE
	bool spellSelected = false;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	switch(GetStance())
	{
	case EQ::constants::stanceBurnAE:
		numTargetsCheck = 1;
		break;
	case EQ::constants::stanceBalanced:
	case EQ::constants::stanceBurn:
		numTargetsCheck = 2;
		break;
	}
		std::vector<MercSpell> mercSpellList = GetMercSpellsBySpellType(SpellType_Nuke);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
			++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if (IsAERainNukeSpell(mercSpellListItr->spellid) && zone->random.Roll(castChance) && CheckSpellRecastTimers(mercSpellListItr->spellid)) {
				uint8 numTargets = 0;
				if (CheckAENuke(tar, mercSpellListItr->spellid, numTargets)) {
					if (numTargets >= numTargetsCheck) {
						result.spellid = mercSpellListItr->spellid;
						result.stance = mercSpellListItr->stance;
						result.type = mercSpellListItr->type;
						result.slot = mercSpellListItr->slot;
						result.proc_chance = mercSpellListItr->proc_chance;
						result.time_cancast = mercSpellListItr->time_cancast;
					}
				}

				break;
			}
		}

	return result;
}

MercSpell Merc::GetBestMercSpellForNuke() {
	MercSpell result;
	int castChance = 50;    //used to cycle through multiple spells (first has 50% overall chance, 2nd has 25%, etc.)
	bool spellSelected = false;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;
		std::vector<MercSpell> mercSpellList = GetMercSpellsBySpellType(SpellType_Nuke);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsPureNukeSpell(mercSpellListItr->spellid) && !IsAENukeSpell(mercSpellListItr->spellid)
				&& zone->random.Roll(castChance) && CheckSpellRecastTimers(mercSpellListItr->spellid)) {
					result.spellid = mercSpellListItr->spellid;
					result.stance = mercSpellListItr->stance;
					result.type = mercSpellListItr->type;
					result.slot = mercSpellListItr->slot;
					result.proc_chance = mercSpellListItr->proc_chance;
					result.time_cancast = mercSpellListItr->time_cancast;

					break;
			}
		}

	return result;
}

MercSpell Merc::GetBestMercSpellForNukeByTargetResists(Mob* target) {
	MercSpell result;
	bool spellSelected = false;

	result.spellid = 0;
	result.stance = 0;
	result.type = SpellType_PreCombatBuffSong;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(!target)
		return result;
		const int lureResisValue = -100;
		const int maxTargetResistValue = 300;
		bool selectLureNuke = false;

		if((target->GetMR() > maxTargetResistValue) && (target->GetCR() > maxTargetResistValue) && (target->GetFR() > maxTargetResistValue))
			selectLureNuke = true;

		std::vector<MercSpell> mercSpellList = GetMercSpellsBySpellType(SpellType_Nuke);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order

			if(IsPureNukeSpell(mercSpellListItr->spellid) && !IsAENukeSpell(mercSpellListItr->spellid) && CheckSpellRecastTimers(mercSpellListItr->spellid)) {
				if(selectLureNuke && (spells[mercSpellListItr->spellid].ResistDiff < lureResisValue)) {
					spellSelected = true;
				}
				else {
					if(((target->GetMR() < target->GetCR()) || (target->GetMR() < target->GetFR())) && (GetSpellResistType(mercSpellListItr->spellid) == RESIST_MAGIC)
						&& (spells[mercSpellListItr->spellid].ResistDiff > lureResisValue))
					{
						spellSelected = true;
					}
					else if(((target->GetCR() < target->GetMR()) || (target->GetCR() < target->GetFR())) && (GetSpellResistType(mercSpellListItr->spellid) == RESIST_COLD)
						&& (spells[mercSpellListItr->spellid].ResistDiff > lureResisValue))
					{
						spellSelected = true;
					}
					else if(((target->GetFR() < target->GetCR()) || (target->GetFR() < target->GetMR())) && (GetSpellResistType(mercSpellListItr->spellid) == RESIST_FIRE)
						&& (spells[mercSpellListItr->spellid].ResistDiff > lureResisValue))
					{
						spellSelected = true;
					}
				}
			}

			if(spellSelected) {
				result.spellid = mercSpellListItr->spellid;
				result.stance = mercSpellListItr->stance;
				result.type = mercSpellListItr->type;
				result.slot = mercSpellListItr->slot;
				result.proc_chance = mercSpellListItr->proc_chance;
				result.time_cancast = mercSpellListItr->time_cancast;

				break;
			}
		}

	return result;
}

bool Merc::GetNeedsCured(Mob *tar) {
	bool needCured = false;

	if(tar) {
		if(tar->FindType(SE_PoisonCounter) || tar->FindType(SE_DiseaseCounter) || tar->FindType(SE_CurseCounter) || tar->FindType(SE_CorruptionCounter)) {
			uint32 buff_count = tar->GetMaxTotalSlots();
			int buffsWithCounters = 0;
			needCured = true;

			for (unsigned int j = 0; j < buff_count; j++) {
				if(tar->GetBuffs()[j].spellid != SPELL_UNKNOWN) {
					if(CalculateCounters(tar->GetBuffs()[j].spellid) > 0) {
						buffsWithCounters++;

						if(buffsWithCounters == 1 && (tar->GetBuffs()[j].ticsremaining < 2 || (int32)((tar->GetBuffs()[j].ticsremaining * 6) / tar->GetBuffs()[j].counters) < 2)) {
							// Spell has ticks remaining but may have too many counters to cure in the time remaining;
							// We should try to just wait it out. Could spend entire time trying to cure spell instead of healing, buffing, etc.
							// Since this is the first buff with counters, don't try to cure. Cure spell will be wasted, as cure will try to
							// remove counters from the first buff that has counters remaining.
							needCured = false;
							break;
						}
					}
				}
			}
		}
	}

	return needCured;
}

void Merc::MercGroupSay(Mob *speaker, std::string gmsg)
{

	if(speaker->HasGroup()) {
		Group *g = speaker->GetGroup();

		if(g)
			g->GroupMessage(speaker->CastToMob(), 0, 100, gmsg.c_str());
	}
}

void Merc::SetSpellRecastTimer(uint16 timer_id, int32 spellid, uint32 recast_delay) {
	if(timer_id > 0) {
		MercTimer timer;
		timer.timerid = timer_id;
		timer.timertype = 1;
		timer.spellid = spellid;
		timer.time_cancast = Timer::GetCurrentTime() + recast_delay;
		timers[timer_id] = timer;
	}
}

int32 Merc::GetSpellRecastTimer(uint16 timer_id) {
	int32 result = 0;
	if(timer_id > 0) {
		if(timers.find(timer_id) != timers.end()) {
			result = timers[timer_id].time_cancast;
		}
	}
	return result;
}

bool Merc::CheckSpellRecastTimers( int32 spell_id) {
	MercSpell mercSpell = GetMercSpellBySpellID(spell_id);
	if(mercSpell.spellid > 0 && mercSpell.time_cancast < Timer::GetCurrentTime()) { //checks spell recast
			return true; //can cast spell
	}
	return false;
}

void Merc::SetSpellTimeCanCast(int32 spellid, uint32 recast_delay) {
	for (int i = 0; i < merc_spells.size(); i++) {
		if(merc_spells[i].spellid == spellid) {
			merc_spells[i].time_cancast = Timer::GetCurrentTime() + recast_delay;
		}
	}
}

bool Merc::CheckTaunt() {
	Mob* tar = GetTarget();
	//Only taunt if we are not top on target's hate list
	//This ensures we have taunt available to regain aggro if needed
	if(tar && tar->GetHateTop() && tar->GetHateTop() != this) {
		return true;
	}
	return false;
}

bool Merc::CheckAETaunt() {
	//need to check area for mobs needing taunted
	MercSpell mercSpell = GetBestMercSpellForAETaunt();
	uint8 result = 0;

	if(mercSpell.spellid != 0) {

		std::list<NPC*> npc_list;
		entity_list.GetNPCList(npc_list);

		for (auto itr = npc_list.begin(); itr != npc_list.end(); ++itr) {
			NPC* npc = *itr;
			float dist = DistanceSquaredNoZ(m_Position, npc->GetPosition());
			int range = GetActSpellRange(mercSpell.spellid, spells[mercSpell.spellid].range);
			range *= range;

			if(dist <= range) {
				if(!npc->IsMezzed()) {
					if(HasGroup()) {
						Group* g = GetGroup();

						if(g) {
							for(int i = 0; i < g->GroupCount(); i++) {
								//if(npc->IsOnHatelist(g->members[i]) && g->members[i]->GetTarget() != npc && g->members[i]->IsEngaged()) {
								if(GetTarget() != npc && g->members[i] && g->members[i]->GetTarget() != npc && npc->IsOnHatelist(g->members[i])) {
									result++;
								}
							}
						}
					}
				}
			}
		}

		if(result >= 1) {
			Log(Logs::General, Logs::Mercenaries, "%s: Attempting AE Taunt", GetCleanName());
			return true;
		}
	}
	return false;
}

Corpse* Merc::GetGroupMemberCorpse() {
	Corpse* corpse = nullptr;

	if(HasGroup()) {
		Group* g = GetGroup();

		if(g) {
			for(int i = 0; i < g->GroupCount(); i++) {
				if(g->members[i] && g->members[i]->IsClient()) {
					corpse = entity_list.GetCorpseByOwnerWithinRange(g->members[i]->CastToClient(), this, RuleI(Mercs, ResurrectRadius));

					if(corpse && !corpse->IsRezzed()) {
						return corpse;
					}
				}
			}
		}
	}
	return 0;
}

bool Merc::TryHide() {
	if(GetClass() != MELEEDPS) {
		return false;
	}

	//don't hide if already hidden
	if(hidden == true) {
		return false;
	}

	auto outapp = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
	SpawnAppearance_Struct* sa_out = (SpawnAppearance_Struct*)outapp->pBuffer;
	sa_out->spawn_id = GetID();
	sa_out->type = 0x03;
	sa_out->parameter = 1;
	entity_list.QueueClients(this, outapp, true);
	safe_delete(outapp);
	hidden = true;

	return true;
}

//Checks if Merc still has confidence. Can be checked to begin fleeing, or to regain confidence after confidence loss - true = confident, false = confidence loss
bool Merc::CheckConfidence() {
	bool result = true;
	int ConfidenceLossChance = 0;
	float ConfidenceCheck = 0;
	int ConfidenceRating = 2 * GetProficiencyID();

	std::list<NPC*> npc_list;
	entity_list.GetNPCList(npc_list);

	for (auto itr = npc_list.begin(); itr != npc_list.end(); ++itr) {
		NPC* mob = *itr;
		float ConRating = 1.0;
		int CurrentCon = 0;

		if(!mob) continue;

		if(!mob->IsEngaged()) continue;

		if(mob->IsFeared() || mob->IsMezzed() || mob->IsStunned() || mob->IsRooted() || mob->IsCharmed()) continue;

		if(!mob->CheckAggro(this)) continue;

		float AggroRange = mob->GetAggroRange();

		// Square it because we will be using DistNoRoot

		AggroRange = AggroRange * AggroRange;

		if(DistanceSquared(m_Position, mob->GetPosition()) > AggroRange) continue;

		CurrentCon = this->GetLevelCon(mob->GetLevel());
		switch(CurrentCon) {


					case CON_GRAY: {
						ConRating = 0;
						break;
					}

					case CON_GREEN: {
						ConRating = 0.1;
						break;
									}

					case CON_LIGHTBLUE: {
						ConRating = 0.2;
						break;
										}

					case CON_BLUE: {
						ConRating = 0.6;
						break;
								   }

					case CON_WHITE: {
						ConRating = 1.0;
						break;
									}

					case CON_YELLOW: {
						ConRating = 1.2;
						break;
									 }

					case CON_RED: {
						ConRating = 1.5;
						break;
								  }

					default: {
						ConRating = 0;
						break;
							 }
		}

		ConfidenceCheck += ConRating;
	}

	if(ConfidenceRating < ConfidenceCheck) {
		ConfidenceLossChance = 25 - ( 5 * (GetTierID() - 1));
	}

	if(zone->random.Roll(ConfidenceLossChance)) {
		result = false;
	}

	return result;
}

void Merc::MercMeditate(bool isSitting) {
	return;
}


void Merc::Sit() {
	if(IsMoving()) {
		moved = false;
		// SetHeading(CalculateHeadingToTarget(GetTarget()->GetX(), GetTarget()->GetY()));
		SentPositionPacket(0.0f, 0.0f, 0.0f, 0.0f, 0);
		SetMoving(false);
	}

	SetAppearance(eaSitting);
}

void Merc::Stand() {
	SetAppearance(eaStanding);
}

bool Merc::IsSitting() {
	bool result = false;

	if(GetAppearance() == eaSitting && !IsMoving())
		result = true;

	return result;
}

bool Merc::IsStanding() {
	bool result = false;

	if(GetAppearance() == eaStanding)
		result = true;

	return result;
}

float Merc::GetMaxMeleeRangeToTarget(Mob* target) {
	float result = 0;

	if(target) {
		float size_mod = GetSize();
		float other_size_mod = target->GetSize();

		if(GetRace() == 49 || GetRace() == 158 || GetRace() == 196) //For races with a fixed size
			size_mod = 60.0f;
		else if (size_mod < 6.0)
			size_mod = 8.0f;

		if(target->GetRace() == 49 || target->GetRace() == 158 || target->GetRace() == 196) //For races with a fixed size
			other_size_mod = 60.0f;
		else if (other_size_mod < 6.0)
			other_size_mod = 8.0f;

		if (other_size_mod > size_mod) {
			size_mod = other_size_mod;
		}

		// this could still use some work, but for now it's an improvement....

		if (size_mod > 29)
			size_mod *= size_mod;
		else if (size_mod > 19)
			size_mod *= size_mod * 2;
		else
			size_mod *= size_mod * 4;

		// prevention of ridiculously sized hit boxes
		if (size_mod > 10000)
			size_mod = size_mod / 7;

		result = size_mod;
	}

	return result;
}

void Merc::DoClassAttacks(Mob *target) {
	if(target == nullptr)
		return; //gotta have a target for all these

	bool ca_time = classattack_timer.Check(false);

	//only check attack allowed if we are going to do something
	if(ca_time && !IsAttackAllowed(target))
		return;

	if(!ca_time)
		return;

	float HasteModifier = GetHaste() * 0.01f;

	int level = GetLevel();
	int reuse = TauntReuseTime * 1000;      //make this very long since if they dont use it once, they prolly never will
	bool did_attack = false;
	//class specific stuff...
	switch(GetClass()) {
			case MELEEDPS:
				if(level >= 10) {
					reuse = BackstabReuseTime * 1000;
					TryBackstab(target, reuse);
					did_attack = true;
				}
				break;
			case TANK:{
				if(level >= RuleI(Combat, NPCBashKickLevel)){
					if(zone->random.Int(0, 100) > 25) //tested on live, warrior mobs both kick and bash, kick about 75% of the time, casting doesn't seem to make a difference.
					{
						DoAnim(animKick, 0, false);
						int32 dmg = GetBaseSkillDamage(EQ::skills::SkillKick);

						if (GetWeaponDamage(target, (const EQ::ItemData*)nullptr) <= 0)
							dmg = DMG_INVULNERABLE;

						reuse = KickReuseTime * 1000;
						DoSpecialAttackDamage(target, EQ::skills::SkillKick, dmg, 1, -1, reuse);
						did_attack = true;
					}
					else
					{
						DoAnim(animTailRake, 0, false);
						int32 dmg = GetBaseSkillDamage(EQ::skills::SkillBash);

						if (GetWeaponDamage(target, (const EQ::ItemData*)nullptr) <= 0)
							dmg = DMG_INVULNERABLE;

						reuse = BashReuseTime * 1000;
						DoSpecialAttackDamage(target, EQ::skills::SkillBash, dmg, 1, -1, reuse);
						did_attack = true;
					}
				}
				break;
					  }
	}

	classattack_timer.Start(reuse / HasteModifier);
}

bool Merc::Attack(Mob* other, int Hand, bool bRiposte, bool IsStrikethrough, bool IsFromSpell, ExtraAttackOptions* opts)
{
	if (!other) {
		SetTarget(nullptr);
		LogError("A null Mob object was passed to Client::Attack() for evaluation!");
		return false;
	}

	if (!GetTarget())
		SetTarget(other);

	LogCombat("Attacking [{}] with hand [{}] [{}]", other ? other->GetName() : "(nullptr)", Hand, bRiposte ? "(this is a riposte)" : "");

	//SetAttackTimer();
	if (
		//(IsCasting() && GetClass() != BARD && !IsFromSpell)
		(IsCasting() && casting_spell_id != 43302 && !UseBardSpellLogic())
		|| other == nullptr
		|| IsDead() 
		|| (other->IsClient() && other->CastToClient()->IsDead())
		|| (GetHP() < 0)
		|| (!IsAttackAllowed(other))
		) {
		LogCombat("Attack cancelled, invalid circumstances");
		return false; // Only bards can attack while casting
	}

	if (DivineAura()) {//cant attack while invulnerable unless your a gm
		LogCombat("Attack cancelled, Divine Aura is in effect");
		MessageString(Chat::DefaultText, DIVINE_AURA_NO_ATK);	//You can't attack while invulnerable
		return false;
	}

	EQ::ItemInstance* weapon = nullptr;
	if (Hand == EQ::invslot::slotSecondary) {	// Kaiyodo - Pick weapon from the attacking hand
		weapon = GetInv().GetItem(EQ::invslot::slotSecondary);
		OffHandAtk(true);
	}
	else {
		weapon = GetInv().GetItem(EQ::invslot::slotPrimary);
		OffHandAtk(false);
	}

	if (weapon != nullptr) {
		if (!weapon->IsWeapon()) {
			LogCombat("Attack cancelled, Item [{}] ([{}]) is not a weapon", weapon->GetItem()->Name, weapon->GetID());
			return(false);
		}
		LogCombat("Attacking with weapon: [{}] ([{}])", weapon->GetItem()->Name, weapon->GetID());
	}
	else {
		LogCombat("Attacking without a weapon");
	}

	DamageHitInfo my_hit;
	// calculate attack_skill and skillinuse depending on hand and weapon
	// also send Packet to near clients
	my_hit.skill = AttackAnimation(Hand, weapon);
	LogCombat("Attacking with [{}] in slot [{}] using skill [{}]", weapon ? weapon->GetItem()->Name : "Fist", Hand, my_hit.skill);

	// Now figure out damage
	my_hit.damage_done = 1;
	my_hit.min_damage = 0;
	uint8 mylevel = GetLevel() ? GetLevel() : 1;
	int64 hate = 0;
	if (weapon)
		hate = (weapon->GetItem()->Damage + weapon->GetItem()->ElemDmgAmt);

	my_hit.base_damage = GetWeaponDamage(other, weapon, &hate);
	if (hate == 0 && my_hit.base_damage > 1)
		hate = my_hit.base_damage;

	//if weapon damage > 0 then we know we can hit the target with this weapon
	//otherwise we cannot and we set the damage to -5 later on
	if (my_hit.base_damage > 0) {
		// if we revamp this function be more general, we will have to make sure this isn't
		// executed for anything BUT normal melee damage weapons from auto attack
#ifdef TLP_STYLE
		if (Hand == EQ::invslot::slotPrimary || Hand == EQ::invslot::slotSecondary)
			my_hit.base_damage = DoDamageCaps(my_hit.base_damage);
#endif
		auto shield_inc = spellbonuses.ShieldEquipDmgMod + itembonuses.ShieldEquipDmgMod + aabonuses.ShieldEquipDmgMod;
		if (shield_inc > 0 && HasShieldEquiped() && Hand == EQ::invslot::slotPrimary) {
			my_hit.base_damage = my_hit.base_damage * (100 + shield_inc) / 100;
			hate = hate * (100 + shield_inc) / 100;
		}

		int hit_chance_bonus = 0;
		my_hit.offense = offense(my_hit.skill); // we need this a few times
		my_hit.hand = Hand;

		//my_hit.base_damage = my_hit.base_damage * (my_hit.offense / 100.0f);

		Log(Logs::Detail, Logs::Combat, "Damage calculated: base %d min damage %d skill %d", my_hit.base_damage, my_hit.min_damage, my_hit.skill);

		if (opts) {
			my_hit.base_damage *= opts->damage_percent;
			my_hit.base_damage += opts->damage_flat;
			hate *= opts->hate_percent;
			hate += opts->hate_flat;
			hit_chance_bonus += opts->hit_chance;
		}

		my_hit.tohit = GetTotalToHit(my_hit.skill, hit_chance_bonus);

		DoAttack(other, my_hit, opts);
	}
	else {
		my_hit.damage_done = DMG_INVULNERABLE;
	}

	// Hate Generation is on a per swing basis, regardless of a hit, miss, or block, its always the same.
	// If we are this far, this means we are atleast making a swing.

	other->AddToHateList(this, hate);

	///////////////////////////////////////////////////////////
	////// Send Attack Damage
	///////////////////////////////////////////////////////////
	if (my_hit.damage_done > 0 && aabonuses.SkillAttackProc[0] && aabonuses.SkillAttackProc[1] == my_hit.skill &&
		IsValidSpell(aabonuses.SkillAttackProc[2])) {
		float chance = aabonuses.SkillAttackProc[0] / 1000.0f;
		if (zone->random.Roll(chance))
			SpellFinished(aabonuses.SkillAttackProc[2], other, EQ::spells::CastingSlot::Item, 0, -1,
				spells[aabonuses.SkillAttackProc[2]].ResistDiff);
	}
	other->Damage(this, my_hit.damage_done, SPELL_UNKNOWN, my_hit.skill, true, -1, false, m_specialattacks);

	if (IsDead()) return false;

	MeleeLifeTap(my_hit.damage_done);

	if (my_hit.damage_done > 0 && HasSkillProcSuccess() && other && other->GetHP() > 0)
		TrySkillProc(other, my_hit.skill, 0, true, Hand);

	CommonBreakInvisibleFromCombat();

	if (GetTarget())
		TriggerDefensiveProcs(other, Hand, true, my_hit.damage_done);

	if (my_hit.damage_done > 0)
		return true;

	else
		return false;
}

void Merc::Damage(Mob* other, int64 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, bool avoidable, int8 buffslot, bool iBuffTic, eSpecialAttacks special)
{
	if(IsDead() || IsCorpse())
		return;

	if(spell_id==0)
		spell_id = SPELL_UNKNOWN;

	NPC::Damage(other, damage, spell_id, attack_skill, avoidable, buffslot, iBuffTic, special);

	//Not needed since we're using NPC damage.
	//CommonDamage(other, damage, spell_id, attack_skill, avoidable, buffslot, iBuffTic);
}

bool Merc::FindTarget() {
	bool found = false;
	Mob* target = GetHateTop();

	if(target) {
		found = true;
		SetTarget(target);
	}

	return found;
}

void Merc::SetTarget(Mob* mob) {
	NPC::SetTarget(mob);
}

Mob* Merc::GetOwnerOrSelf() {
	Mob* Result = nullptr;

	if(this->GetMercOwner())
		Result = GetMercOwner();
	else
		Result = this;

	return Result;
}

bool Merc::Death(Mob* killerMob, int64 damage, uint16 spell, EQ::skills::SkillType attack_skill)
{
	if(!NPC::Death(killerMob, damage, spell, attack_skill))
	{
		return false;
	}

	Save();

	//no corpse, no exp if we're a merc.
	//We'll suspend instead, since that's what live does.
	//Not actually sure live supports 'depopping' merc corpses.
	//if(entity_list.GetCorpseByID(GetID()))
	//      entity_list.GetCorpseByID(GetID())->Depop();

	// If client is in zone, suspend merc, else depop it.
	if (!Suspend())
	{
		Depop();
	}

	return true;
}

Client* Merc::GetMercOwner() {
	Client* mercOwner = nullptr;

	if(GetOwner())
	{
		if(GetOwner()->IsClient())
		{
			mercOwner = GetOwner()->CastToClient();
		}
	}

	return mercOwner;
}

Mob* Merc::GetOwner() {
	Mob* Result = nullptr;

	Result = Mob::GetOwner();

	if(!Result) {
		return Result;
	}

	return Result->CastToMob();
}

const char* Merc::GetRandomName(){
	// creates up to a 10 char name
	static char name[17];
	char vowels[18]="aeiouyaeiouaeioe";
	char cons[48]="bcdfghjklmnpqrstvwxzybcdgklmnprstvwbcdgkpstrkd";
	char rndname[17]="\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
	char paircons[33]="ngrkndstshthphsktrdrbrgrfrclcr";
	bool valid = false;

	while(!valid) {
		int rndnum=zone->random.Int(0, 75),n=1;
		bool dlc=false;
		bool vwl=false;
		bool dbl=false;
		if (rndnum>63)
		{       // rndnum is 0 - 75 where 64-75 is cons pair, 17-63 is cons, 0-16 is vowel
			rndnum=(rndnum-61)*2;   // name can't start with "ng" "nd" or "rk"
			rndname[0]=paircons[rndnum];
			rndname[1]=paircons[rndnum+1];
			n=2;
		}
		else if (rndnum>16)
		{
			rndnum-=17;
			rndname[0]=cons[rndnum];
		}
		else
		{
			rndname[0]=vowels[rndnum];
			vwl=true;
		}
		int namlen=zone->random.Int(5, 10);
		for (int i=n;i<namlen;i++)
		{
			dlc=false;
			if (vwl)        //last char was a vowel
			{                       // so pick a cons or cons pair
				rndnum=zone->random.Int(0, 62);
				if (rndnum>46)
				{       // pick a cons pair
					if (i>namlen-3) // last 2 chars in name?
					{       // name can only end in cons pair "rk" "st" "sh" "th" "ph" "sk" "nd" or "ng"
						rndnum=zone->random.Int(0, 7)*2;
					}
					else
					{       // pick any from the set
						rndnum=(rndnum-47)*2;
					}
					rndname[i]=paircons[rndnum];
					rndname[i+1]=paircons[rndnum+1];
					dlc=true;       // flag keeps second letter from being doubled below
					i+=1;
				}
				else
				{       // select a single cons
					rndname[i]=cons[rndnum];
				}
			}
			else
			{               // select a vowel
				rndname[i]=vowels[zone->random.Int(0, 16)];
			}
			vwl=!vwl;
			if (!dbl && !dlc)
			{       // one chance at double letters in name
				if (!zone->random.Int(0, i+9))     // chances decrease towards end of name
				{
					rndname[i+1]=rndname[i];
					dbl=true;
					i+=1;
				}
			}
		}

		rndname[0]=toupper(rndname[0]);

		if(!database.CheckNameFilter(rndname)) {
			valid = false;
		}
		else if(rndname[0] < 'A' && rndname[0] > 'Z') {
			//name must begin with an upper-case letter.
			valid = false;
		}
		else if (database.CheckUsedName(rndname)) {
			valid = true;
		}
		else {
			valid = false;
		}
	}

	memset(name, 0, 17);
	strcpy(name, rndname);
	return name;
}

bool Merc::LoadMercSpells() {
	// loads mercs spells into list
	merc_spells.clear();

	int idx = 1;

	for (uint32 z = 0; z < EQ::spells::SPELL_GEM_COUNT; z++) {

		auto spell_id = m_pp.mem_spells[z];

		if (!IsValidSpell(spell_id))
		{
			continue;
		}
		MercSpell mercSpell;

		mercSpell.spellid = spell_id;
		mercSpell.type = (
						(IsEffectInSpell(spell_id, SE_SummonPet) || IsEffectInSpell(spell_id, SE_NecPet) || IsEffectInSpell(spell_id, SE_SummonBSTPet)) ? SpellType_Pet : (
						IsShortDurationBuff(spell_id) && IsBeneficialSpell(spell_id) ? SpellType_InCombatBuff : (
						IsBuffSpell(spell_id) && IsBeneficialSpell(spell_id) ? SpellType_Buff : (
						IsBeneficialSpell(spell_id) && (IsHealOverTimeSpell(spell_id) || IsPercentalHealSpell(spell_id) || IsRegularSingleTargetHealSpell(spell_id) || IsRegularGroupHealSpell(spell_id) || IsCHDurationSpell(spell_id) || IsVeryFastHealSpell(spell_id)) ? SpellType_Heal : (
						IsLifetapSpell(spell_id) ? SpellType_Lifetap : (
						IsAENukeSpell(spell_id) || IsAERainNukeSpell(spell_id) || IsPBAENukeSpell(spell_id) || IsEffectInSpell(spell_id, SE_SkillAttack) || IsPureNukeSpell(spell_id) && !IsBeneficialSpell(spell_id) ? SpellType_Nuke : ( 
						IsDot(spell_id) && !IsBeneficialSpell(spell_id) ? SpellType_DOT : (
						IsDebuffSpell(spell_id) && !(IsEffectInSpell(spell_id, SE_Root)) ? SpellType_Snare : (
						IsEffectInSpell(spell_id, SE_Root)) ? SpellType_Root : (
							SpellType_PreCombatBuffSong)))))))));
		mercSpell.stance = 0;
		mercSpell.slot = idx;
		mercSpell.proc_chance = 0;
		mercSpell.time_cancast = 0;
		merc_spells.push_back(mercSpell);
		idx++;
	}
	std::sort(merc_spells.begin(), merc_spells.end(), [](const MercSpell a, const MercSpell b) {
		return a.slot > b.slot;
	});

	if (merc_spells.empty())
		AIautocastspell_timer->Disable();
	else {
		HasAISpell = true;
		AIautocastspell_timer->Trigger();
	}

	return true;
}

void Merc::AI_Event_MercSpellCastFinished(bool iCastSucceeded, int32 spell_id)
{

	if (!IsValidSpell(spell_id))
		return;

	// if the spell wasn't casted, then take back any extra mana that was given to the bot to cast that spell
	if (!iCastSucceeded) {

	}
	else { //handle spell recast and recast timers
		if (spells[spell_id].mana && GetMana() - spells[spell_id].mana >= 0)
			SetMana(GetMana() - spells[spell_id].mana);
		else
			SetMana(0);

		if (spells[spell_id].EndurCost && GetEndurance() - spells[spell_id].EndurCost >= 0)
			SetEndurance(GetEndurance() - spells[spell_id].EndurCost);
		else
			SetEndurance(0);
		SetSpellTimeCanCast(spell_id, spells[spell_id].recast_time);
	}
}

void Merc::AISpellsList(Client* c)
{
	if (!c)
		return;

	for (auto it = merc_spells.begin(); it != merc_spells.end(); ++it)
		c->Message(Chat::White, "%s (%d): Type %d, Proc Chance %d, Stance %d, Time_CanCast %d, Slot %d",
			spells[it->spellid].name, it->spellid, it->type, it->proc_chance, it->stance, it->time_cancast, it->slot);

	return;
}

bool Merc::Save() {

	if(database.SaveMerc(this)){
		return true;
	}

	return false;
}

void Merc::UpdateMercInfo(Client* c) {
	snprintf(c->GetMercInfo().merc_name, 64, "%s", name);
	c->GetMercInfo().mercid = GetID();
	c->GetMercInfo().IsSuspended = IsSuspended();
	c->GetMercInfo().hp = GetHP();
	c->GetMercInfo().mana = GetMana();
	c->GetMercInfo().endurance = GetEndurance();
}

Merc* Merc::LoadMercFromCharacterID(Client* c, uint32 character_id) {

	MercTemplate* merc_template = zone->GetMercTemplate(1);

	const auto validMercList = c->GetValidMercenaries();

	auto mercEntry = validMercList.find(character_id);

	if (mercEntry == validMercList.end() || !c)
		return nullptr;

	if (mercEntry->first == c->CharacterID())
		return nullptr;

	//get mercenary data
	if (merc_template)
	{
		//TODO: Maybe add a way of updating client merc stats in a seperate function? like, for example, on leveling up.
		const NPCType* npc_type_to_copy = database.LoadNPCTypesData(900);
		if (npc_type_to_copy != nullptr)
		{
			//This is actually a very terrible method of assigning stats, and should be changed at some point. See the comment in merc's deconstructor.
			auto npc_type = new NPCType;
			memset(npc_type, 0, sizeof(NPCType));
			memcpy(npc_type, npc_type_to_copy, sizeof(NPCType));

			npc_type->race = merc_template->RaceID;

			// Use the Gender and Size of the Merchant if possible
			uint8 tmpgender = 0;
			float tmpsize = 6.0f;
			tmpgender = c->GetMercInfo().Gender;
			tmpsize = c->GetMercInfo().MercSize;
			if (c->GetMercInfo().merc_name[0] == 0)
			{
				snprintf(c->GetMercInfo().merc_name, 64, "%s", mercEntry->second.name); //sanity check.
			}
			snprintf(npc_type->name, 64, "%s", mercEntry->second.name);
			sprintf(npc_type->lastname, "%s's Alt", c->GetName());
			npc_type->gender = tmpgender;
			npc_type->size = tmpsize;
			npc_type->loottable_id = 0; // Loottable has to be 0, otherwise we'll be leavin' some corpses!
			npc_type->npc_id = 0; //NPC ID has to be 0, otherwise db gets all confuzzled.
			npc_type->class_ = 10;
			npc_type->maxlevel = 0; //We should hard-set this to override scalerate's functionality in the NPC class when it is constructed.
			npc_type->no_target_hotkey = 1;

			auto merc = new Merc(npc_type, c->GetX(), c->GetY(), c->GetZ(), 0);
			merc->GiveNPCTypeData(npc_type); // for clean up, works a bit like pets

			if (merc)
			{
				merc->SetMercData(merc_template->MercTemplateID);
				merc->UpdateMercStats(c, true);
				database.LoadCurrentMerc(c);
				merc->SetSuspended(c->GetMercInfo().IsSuspended);
				
			}

			Log(Logs::General, Logs::Mercenaries, "LoadMerc Successful for %s (%s).", merc->GetName(), c->GetName());
			return merc;
		}
	}

	return nullptr;
}



void Merc::UpdateMercStats(Client *c, bool setmax)
{
}

void Merc::ScaleStats(int scalepercent, bool setmax) {
	return;
}

void Merc::UpdateMercAppearance() {
	// Copied from Bot Code:
	uint32 itemID = 0;
	uint8 materialFromSlot = EQ::textures::materialInvalid;
	for (int i = EQ::invslot::EQUIPMENT_BEGIN; i <= EQ::invslot::EQUIPMENT_END; ++i) {
		auto inst = m_inv[i];
		if(inst && inst->GetID() > 0) {
			materialFromSlot = EQ::InventoryProfile::CalcMaterialFromSlot(i);
			if (materialFromSlot != EQ::textures::materialInvalid)
				this->SendWearChange(materialFromSlot);
		}
	}

	if (UpdateActiveLight())
		SendAppearancePacket(AT_Light, GetActiveLightType());
}

void Merc::UpdateEquipmentLight()
{
	m_Light.Type[EQ::lightsource::LightEquipment] = 0;
	m_Light.Level[EQ::lightsource::LightEquipment] = 0;

	for (int index = EQ::invslot::EQUIPMENT_BEGIN; index <= EQ::invslot::EQUIPMENT_END; ++index) {
		if (index == EQ::invslot::slotAmmo) { continue; }

		auto itemInst = GetInv().GetItem(index);

		auto item = database.GetItem(itemInst ? itemInst->GetID() : 0);
		if (item.ID == 0) { continue; }

		if (EQ::lightsource::IsLevelGreater(item.Light, m_Light.Type[EQ::lightsource::LightEquipment])) {
			m_Light.Type[EQ::lightsource::LightEquipment] = item.Light;
			m_Light.Level[EQ::lightsource::LightEquipment] = EQ::lightsource::TypeToLevel(m_Light.Type[EQ::lightsource::LightEquipment]);
		}
	}

	uint8 general_light_type = 0;
	for (auto iter = itemlist.begin(); iter != itemlist.end(); ++iter) {
		auto item = database.GetItem((*iter)->item_id);
		if (item.ID == 0) { continue; }

		if (!item.IsClassCommon()) { continue; }
		if (item.Light < 9 || item.Light > 13) { continue; }

		if (EQ::lightsource::TypeToLevel(item.Light))
			general_light_type = item.Light;
	}

	if (EQ::lightsource::IsLevelGreater(general_light_type, m_Light.Type[EQ::lightsource::LightEquipment]))
		m_Light.Type[EQ::lightsource::LightEquipment] = general_light_type;

	m_Light.Level[EQ::lightsource::LightEquipment] = EQ::lightsource::TypeToLevel(m_Light.Type[EQ::lightsource::LightEquipment]);
}

void Merc::AddItem(uint8 slot, uint32 item_id) {

}

bool Merc::Spawn(Client *owner) {

	if (!owner)
	{
		Log(Logs::General, Logs::Mercenaries, "SpawnMerc: Fail! Owner nullptr.");
		return false;
	}

	m_pp.SetPlayerProfileVersion(EQ::versions::ClientVersion::RoF2);
	m_inv.SetInventoryVersion(EQ::versions::ClientVersion::RoF2);
	m_inv.SetGMInventory(true);

	bool fail = !database.GetInventory(owner->GetMercCharacterID(), &m_inv); /* Load Character Inventory */
	if (fail)
	{
		Log(Logs::General, Logs::Mercenaries, "SpawnMerc: Fail! No inventory for char");
		return false;
	}

	MercTemplate* merc_template = zone->GetMercTemplate(GetMercTemplateID());

	if(!merc_template)
		return false;

	_characterID = owner->GetMercCharacterID();

	database.LoadCharacterBandolier(_characterID, &m_pp); /* Load Character Bandolier */
	database.LoadCharacterBindPoint(_characterID, &m_pp); /* Load Character Bind */
	database.LoadCharacterMaterialColor(_characterID, &m_pp); /* Load Character Material */
	database.LoadCharacterPotions(_characterID, &m_pp); /* Load Character Potion Belt */
	database.LoadCharacterCurrency(_characterID, &m_pp); /* Load Character Currency into PP */
	database.LoadCharacterData(_characterID, &m_pp, &m_epp); /* Load Character Data from DB into PP as well as E_PP */
	database.LoadCharacterSkills(_characterID, &m_pp); /* Load Character Skills */
	database.LoadCharacterSpellBook(_characterID, &m_pp); /* Load Character Spell Book */
	database.LoadCharacterMemmedSpells(_characterID, &m_pp);  /* Load Character Memorized Spells */
	database.LoadCharacterDisciplines(_characterID, &m_pp); /* Load Character Disciplines */
	database.LoadCharacterLanguages(_characterID, &m_pp); /* Load Character Languages */
	database.LoadCharacterLeadershipAA(_characterID, &m_pp); /* Load Character Leadership AA's */
	database.LoadCharacterTribute(_characterID, &m_pp); /* Load CharacterTribute */

		/* Set Mob variables for spawn */
	class_ = m_pp.class_;
	level = m_pp.level;
	SetPosition(owner->GetX(), owner->GetY(), owner->GetZ());
	m_RewindLocation = glm::vec3();
	race = m_pp.race;
	base_race = m_pp.race;
	gender = m_pp.gender;
	base_gender = m_pp.gender;
	deity = m_pp.deity;
	haircolor = m_pp.haircolor;
	beardcolor = m_pp.beardcolor;
	eyecolor1 = m_pp.eyecolor1;
	eyecolor2 = m_pp.eyecolor2;
	hairstyle = m_pp.hairstyle;
	luclinface = m_pp.face;
	beard = m_pp.beard;
	drakkin_heritage = m_pp.drakkin_heritage;
	drakkin_tattoo = m_pp.drakkin_tattoo;
	drakkin_details = m_pp.drakkin_details;
	switch (race)
	{
	case OGRE:
		size = 9; break;
	case TROLL:
		size = 8; break;
	case VAHSHIR: case BARBARIAN:
		size = 7; break;
	case HUMAN: case HIGH_ELF: case ERUDITE: case IKSAR: case DRAKKIN:
		size = 6; break;
	case HALF_ELF:
		size = 5.5; break;
	case WOOD_ELF: case DARK_ELF: case FROGLOK:
		size = 5; break;
	case DWARF:
		size = 4; break;
	case HALFLING:
		size = 3.5; break;
	case GNOME:
		size = 3; break;
	default:
		size = 0;
	}

	/* Check for Invalid points */
	if (m_pp.ldon_points_guk < 0 || m_pp.ldon_points_guk > 2000000000) { m_pp.ldon_points_guk = 0; }
	if (m_pp.ldon_points_mir < 0 || m_pp.ldon_points_mir > 2000000000) { m_pp.ldon_points_mir = 0; }
	if (m_pp.ldon_points_mmc < 0 || m_pp.ldon_points_mmc > 2000000000) { m_pp.ldon_points_mmc = 0; }
	if (m_pp.ldon_points_ruj < 0 || m_pp.ldon_points_ruj > 2000000000) { m_pp.ldon_points_ruj = 0; }
	if (m_pp.ldon_points_tak < 0 || m_pp.ldon_points_tak > 2000000000) { m_pp.ldon_points_tak = 0; }
	if (m_pp.ldon_points_available < 0 || m_pp.ldon_points_available > 2000000000) { m_pp.ldon_points_available = 0; }

	if (RuleB(World, UseClientBasedExpansionSettings)) {
		m_pp.expansions = EQ::expansions::ConvertClientVersionToExpansionsMask(EQ::versions::ClientVersion::RoF2);
	}
	else {
		m_pp.expansions = (RuleI(World, ExpansionSettings) & EQ::expansions::ConvertClientVersionToExpansionsMask(EQ::versions::ClientVersion::RoF2));
	}

	if (SPDAT_RECORDS > 0) {
		for (uint32 z = 0; z < EQ::spells::SPELL_GEM_COUNT; z++) {
			if (m_pp.mem_spells[z] >= (uint32)SPDAT_RECORDS)
				m_pp.mem_spells[z] = 0xFFFFFFFF;
		}
	}

	SetHP(m_pp.cur_hp);
	Mob::SetMana(m_pp.mana); // mob function doesn't send the packet
	SetEndurance(m_pp.endurance);

	LoadMercSpells();
	p_timers.SetCharID(_characterID);
	/* Load Spell Slot Refresh from Currently Memoried Spells */
	for (unsigned int i = 0; i < EQ::spells::SPELL_GEM_COUNT; ++i)
		if (IsValidSpell(m_pp.mem_spells[i]))
			m_pp.spellSlotRefresh[i] = p_timers.GetRemainingTime(pTimerSpellStart + m_pp.mem_spells[i]) * 1000;

	/* Ability slot refresh send SK/PAL */
	if (m_pp.class_ == SHADOWKNIGHT || m_pp.class_ == PALADIN) {
		uint32 abilitynum = 0;
		if (m_pp.class_ == SHADOWKNIGHT) { abilitynum = pTimerHarmTouch; }
		else { abilitynum = pTimerLayHands; }

		uint32 remaining = p_timers.GetRemainingTime(abilitynum);
		if (remaining > 0 && remaining < 15300)
			m_pp.abilitySlotRefresh = remaining * 1000;
		else
			m_pp.abilitySlotRefresh = 0;
	}
	entity_list.AddMerc(this, true, true);

	CalcBonuses();
	AI_Start();

	SentPositionPacket(0.0f, 0.0f, 0.0f, 0.0f, 0);
	SendArmorAppearance();

	Log(Logs::General, Logs::Mercenaries, "Spawn Mercenary %s.", GetName());

	return true;
}

void Client::SendMercResponsePackets(uint32 ResponseType)
{
	switch (ResponseType)
	{
	case 0: // Mercenary Spawned Successfully?
		SendMercMerchantResponsePacket(0);
		break;
	case 1: //You do not have enough funds to make that purchase!
		SendMercMerchantResponsePacket(1);
		break;
	case 2: //Mercenary does not exist!
		SendMercMerchantResponsePacket(2);
		break;
	case 3: //Mercenary failed to spawn!
		SendMercMerchantResponsePacket(3);
		break;
	case 4: //Mercenaries are not allowed in raids!
		SendMercMerchantResponsePacket(4);
		break;
	case 5: //You already have a pending mercenary purchase!
		SendMercMerchantResponsePacket(5);
		break;
	case 6: //You have the maximum number of mercenaries.  You must dismiss one before purchasing a new one!
		SendMercMerchantResponsePacket(6);
		break;
	case 7: //You must dismiss your suspended mercenary before purchasing a new one!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(7);
		else
			//You have the maximum number of mercenaries.  You must dismiss one before purchasing a new one!
			SendMercMerchantResponsePacket(6);
		break;
	case 8: //You can not purchase a mercenary because your group is full!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(8);
		else
			SendMercMerchantResponsePacket(7);
		break;
	case 9: //You can not purchase a mercenary because you are in combat!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			//Mercenary failed to spawn!
			SendMercMerchantResponsePacket(3);
		else
			SendMercMerchantResponsePacket(8);
		break;
	case 10: //You have recently dismissed a mercenary and must wait a few more seconds before you can purchase a new one!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			//Mercenary failed to spawn!
			SendMercMerchantResponsePacket(3);
		else
			SendMercMerchantResponsePacket(9);
		break;
	case 11: //An error occurred created your mercenary!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(9);
		else
			SendMercMerchantResponsePacket(10);
		break;
	case 12: //Upkeep Charge Message
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(10);
		else
			SendMercMerchantResponsePacket(11);
		break;
	case 13: // ???
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(11);
		else
			SendMercMerchantResponsePacket(12);
		break;
	case 14: //You ran out of funds to pay for your mercenary!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(12);
		else
			SendMercMerchantResponsePacket(13);
		break;
	case 15: // ???
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(13);
		else
			SendMercMerchantResponsePacket(14);
		break;
	case 16: //Your mercenary is about to be suspended due to insufficient funds!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(14);
		else
			SendMercMerchantResponsePacket(15);
		break;
	case 17: //There is no mercenary liaison nearby!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(15);
		else
			SendMercMerchantResponsePacket(16);
		break;
	case 18: //You are too far from the liaison!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(16);
		else
			SendMercMerchantResponsePacket(17);
		break;
	case 19: //You do not meet the requirements for that mercenary!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(17);
		else
			SendMercMerchantResponsePacket(18);
		break;
	case 20: //You are unable to interact with the liaison!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			//You are too far from the liaison!
			SendMercMerchantResponsePacket(16);
		else
			SendMercMerchantResponsePacket(19);
		break;
	case 21: //You do not have a high enough membership level to purchase this mercenary!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			//You do not meet the requirements for that mercenary!
			SendMercMerchantResponsePacket(17);
		else
			SendMercMerchantResponsePacket(20);
		break;
	case 22: //Your purchase has failed because this mercenary requires a Gold membership!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			//You do not meet the requirements for that mercenary!
			SendMercMerchantResponsePacket(17);
		else
			SendMercMerchantResponsePacket(21);
		break;
	case 23: //Your purchase has failed because this mercenary requires at least a Silver membership!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			//You do not meet the requirements for that mercenary!
			SendMercMerchantResponsePacket(17);
		else
			SendMercMerchantResponsePacket(22);
		break;
	default: //Mercenary failed to spawn!
		SendMercMerchantResponsePacket(3);
		break;
	}
	Log(Logs::General, Logs::Mercenaries, "SendMercResponsePackets %i for %s.", ResponseType, GetName());

}

void Client::UpdateMercTimer()
{
	Merc *merc = GetMerc();

	if(merc && !merc->IsSuspended())
	{
		if(GetMercTimer()->Check())
		{
			uint32 upkeep = merc->CalcUpkeepCost(merc->GetMercTemplateID(), GetLevel());

			if(CheckCanRetainMerc(upkeep))
			{
				if(RuleB(Mercs, ChargeMercUpkeepCost))
				{
					TakeMoneyFromPP((upkeep * 100), true);
				}
			}
			else
			{
				merc->Suspend();
				return;
			}

			// Reset the upkeep timer
			GetMercInfo().MercTimerRemaining = RuleI(Mercs, UpkeepIntervalMS);
			SendMercTimer(merc);
			GetMercTimer()->Start(RuleI(Mercs, UpkeepIntervalMS));
			GetMercTimer()->SetTimer(GetMercInfo().MercTimerRemaining);

			Log(Logs::General, Logs::Mercenaries, "UpdateMercTimer Complete for %s.", GetName());

			// Normal upkeep charge message
			//Message(Chat::LightGray, "You have been charged a mercenary upkeep cost of %i plat, and %i gold and your mercenary upkeep cost timer has been reset to 15 minutes.", upkeep_plat, upkeep_gold, (int)(RuleI(Mercs, UpkeepIntervalMS) / 1000 / 60));

			// Message below given when too low level to be charged
			//Message(Chat::LightGray, "Your mercenary waived an upkeep cost of %i plat, and %i gold or %i %s and your mercenary upkeep cost timer has been reset to %i minutes", upkeep_plat, upkeep_gold, 1, "Bayle Marks", (int)(RuleI(Mercs, UpkeepIntervalMS) / 1000 / 60));
		}
	}
}

bool Client::CheckCanHireMerc(Mob* merchant, uint32 template_id) {

	if (!CheckCanSpawnMerc(template_id))
	{
		return false;
	}

	MercTemplate* mercTemplate = zone->GetMercTemplate(template_id);

	//check for suspended merc
	if(GetMercInfo().mercid != 0 && GetMercInfo().IsSuspended) {
		SendMercResponsePackets(6);
		return false;
	}

	//check for valid merchant
	if(!merchant) {
		SendMercResponsePackets(17);
		return false;
	}

	//check for merchant too far away
	if(DistanceSquared(m_Position, merchant->GetPosition()) > USE_NPC_RANGE2) {
		SendMercResponsePackets(18);
		return false;
	}

	//check for sufficient funds and remove them last
	if(RuleB(Mercs, ChargeMercPurchaseCost)) {
		uint32 cost = Merc::CalcPurchaseCost(template_id, GetLevel()) * 100;  // Cost is in gold
		if(cost > 0 && !HasMoney(cost)) {
			SendMercResponsePackets(1);
			return false;
		}
	}

	Log(Logs::General, Logs::Mercenaries, "CheckCanHireMerc True for %s.", GetName());

	return true;
}

bool Client::CheckCanRetainMerc(uint32 upkeep) {
	Merc* merc = GetMerc();

	//check for sufficient funds
	if(RuleB(Mercs, ChargeMercPurchaseCost)) {
		if(merc) {
			if(upkeep > 0 && !HasMoney(upkeep * 100)) {
				SendMercResponsePackets(14);
				return false;
			}
		}
	}

	return true;
}

bool Client::CheckCanSpawnMerc(uint32 template_id) {

	// Check if mercs are enabled globally
	if(!RuleB(Mercs, AllowMercs))
	{
		return false;
	}

	// Check if zone allows mercs
	if(!zone->AllowMercs())
	{
		SendMercResponsePackets(3);
		return false;
	}

	MercTemplate* mercTemplate = zone->GetMercTemplate(template_id);

	// Invalid merc data
	if(!mercTemplate)
	{
		SendMercResponsePackets(11);
		return false;
	}

	// Check client version
	if(static_cast<unsigned int>(ClientVersion()) < mercTemplate->ClientVersion)
	{
		SendMercResponsePackets(3);
		return false;
	}

	// Check for raid
	if(HasRaid())
	{
		SendMercResponsePackets(4);
		return false;
	}

	// Check group size
	if(GetGroup() &&  GetGroup()->GroupCount() >= MAX_GROUP_MEMBERS)	// database.GroupCount(GetGroup()->GetID())
	{
		SendMercResponsePackets(8);
		return false;
	}

	// Check in combat
	if(GetAggroCount() > 0)
	{
		SendMercResponsePackets(9);
		return false;
	}

	Log(Logs::General, Logs::Mercenaries, "CheckCanSpawnMerc True for %s.", GetName());

	return true;
}

bool Client::CheckCanUnsuspendMerc() {

	if (!CheckCanSpawnMerc(GetMercInfo().MercTemplateID))
	{
		return false;
	}

	MercTemplate* mercTemplate = zone->GetMercTemplate(GetMercInfo().MercTemplateID);

	if(!GetPTimers().Expired(&database, pTimerMercSuspend, false))
	{
		SendMercResponsePackets(10);
		//TODO: find this packet response and tell them properly.
		Message(0, "You must wait %i seconds before unsuspending your mercenary.", GetPTimers().GetRemainingTime(pTimerMercSuspend));
		return false;
	}

	Log(Logs::General, Logs::Mercenaries, "CheckCanUnsuspendMerc True for %s.", GetName());

	return true;
}

void Client::CheckMercSuspendTimer() {

	if(GetMercInfo().SuspendedTime != 0)
	{
		//if(time(nullptr) >= GetMercInfo().SuspendedTime)
		if (p_timers.Expired(&database, pTimerMercSuspend, false))
		{
			GetMercInfo().SuspendedTime = 0;
			SendMercResponsePackets(0);
			SendMercSuspendResponsePacket(GetMercInfo().SuspendedTime);
			Log(Logs::General, Logs::Mercenaries, "CheckMercSuspendTimer Ready for %s.", GetName());
		}
	}
}

void Client::SuspendMercCommand() {

	if(GetMercInfo().MercTemplateID != 0)
	{
		if(GetMercInfo().IsSuspended)
		{
			if(!CheckCanUnsuspendMerc())
			{
				Log(Logs::General, Logs::Mercenaries, "SuspendMercCommand Unable to Unsuspend Merc for %s.", GetName());

				return;
			}

			// Get merc, assign it to client & spawn
			Merc* merc = Merc::LoadMercFromCharacterID(this, GetMercCharacterID());
			if(merc)
			{
				SpawnMerc(merc, true);
				Log(Logs::General, Logs::Mercenaries, "SuspendMercCommand Successful Unsuspend for %s.", GetName());
			}
			else
			{
				//merc failed to spawn
				SendMercResponsePackets(3);
				Log(Logs::General, Logs::Mercenaries, "SuspendMercCommand Failed to Spawn Merc for %s.", GetName());
			}
		}
		else
		{
			Merc* CurrentMerc = GetMerc();


			if (!RuleB(Mercs, AllowMercSuspendInCombat))
			{
				if (!CheckCanSpawnMerc(GetMercInfo().MercTemplateID))
				{
					return;
				}
			}

			if(CurrentMerc && GetMercID())
			{
				CurrentMerc->Suspend();
				Log(Logs::General, Logs::Mercenaries, "SuspendMercCommand Successful Suspend for %s.", GetName());
			}
			else
			{
				// Reset Merc Suspend State
				GetMercInfo().IsSuspended = true;
				//GetMercInfo().SuspendedTime = time(nullptr) + RuleI(Mercs, SuspendIntervalS);
				//GetMercInfo().MercTimerRemaining = GetMercTimer()->GetRemainingTime();
				//GetMercInfo().Stance = GetStance();
				GetMercTimer()->Disable();
				SendMercSuspendResponsePacket(GetMercInfo().SuspendedTime);
				SendMercTimer(nullptr);
				Log(Logs::General, Logs::Mercenaries, "SuspendMercCommand Failed to Get Merc to Suspend. Resetting Suspend State for %s.", GetName());
			}
		}
	}
	else
	{
		SpawnMercOnZone();
		Log(Logs::General, Logs::Mercenaries, "SuspendMercCommand Request Failed to Load Merc for %s.  Trying SpawnMercOnZone.", GetName());
	}
}


// Handles all client zone change event
void Merc::ProcessClientZoneChange(Client* mercOwner) {

	if(mercOwner)
	{
		Zone();
	}
}

void Client::SpawnMercOnZone() {

	if(!RuleB(Mercs, AllowMercs))
		return;

	if (GetMerc())
		return;

	if(database.LoadMercInfo(this))
	{
		if(!GetMercInfo().IsSuspended)
		{
			GetMercInfo().SuspendedTime = 0;
			// Get merc, assign it to client & spawn
			Merc* merc = Merc::LoadMercFromCharacterID(this, GetMercCharacterID());
			if(merc)
			{
				SpawnMerc(merc, true);
			}
			Log(Logs::General, Logs::Mercenaries, "SpawnMercOnZone Normal Merc for %s.", GetName());
		}
		else
		{
			int32 TimeDiff = GetMercInfo().SuspendedTime - time(nullptr);
			if (TimeDiff > 0)
			{
				if (!GetPTimers().Enabled(pTimerMercSuspend))
				{
					// Start the timer to send the packet that refreshes the Unsuspend Button
					GetPTimers().Start(pTimerMercSuspend, TimeDiff);
				}
			}
			// Send Mercenary Status/Timer packet
			SendMercTimer(GetMerc());

			Log(Logs::General, Logs::Mercenaries, "SpawnMercOnZone Suspended Merc for %s.", GetName());
		}
	}
	else
	{
		// No Merc Hired
		// RoF+ displays a message from the following packet, which seems useless
		SendClearMercInfo();
		Log(Logs::General, Logs::Mercenaries, "SpawnMercOnZone Failed to load Merc Info from the Database for %s.", GetName());
	}
}

void Client::SendMercTimer(Merc* merc) {

	if (GetMercInfo().mercid == 0)
	{
		return;
	}

	if (!merc)
	{
		SendMercTimerPacket(NO_MERC_ID, MERC_STATE_SUSPENDED, NOT_SUSPENDED_TIME, GetMercInfo().MercTimerRemaining, RuleI(Mercs, SuspendIntervalMS));
		Log(Logs::General, Logs::Mercenaries, "SendMercTimer No Merc for %s.", GetName());
	}
	else if (merc->IsSuspended())
	{
		SendMercTimerPacket(NO_MERC_ID, MERC_STATE_SUSPENDED, NOT_SUSPENDED_TIME, GetMercInfo().MercTimerRemaining, RuleI(Mercs, SuspendIntervalMS));
		Log(Logs::General, Logs::Mercenaries, "SendMercTimer Suspended Merc for %s.", GetName());
	}
	else
	{
		SendMercTimerPacket(merc->GetID(), MERC_STATE_NORMAL, NOT_SUSPENDED_TIME, GetMercInfo().MercTimerRemaining, RuleI(Mercs, SuspendIntervalMS));
		Log(Logs::General, Logs::Mercenaries, "SendMercTimer Normal Merc for %s.", GetName());
	}

}

void Client::SpawnMerc(Merc* merc, bool setMaxStats) {

	if (!merc || !CheckCanSpawnMerc(merc->GetMercTemplateID()))
	{
		if (merc)
		{
			merc->Suspend();
		}
		return;
	}

	merc->Spawn(this);
	merc->SetSuspended(false);
	SetMerc(merc);
	merc->Unsuspend(setMaxStats);
	merc->SetStance((EQ::constants::StanceType)GetMercInfo().Stance);

	Log(Logs::General, Logs::Mercenaries, "SpawnMerc Success for %s.", GetName());

	return;

}

bool Merc::Suspend() {

	Client* mercOwner = GetMercOwner();

	if(!mercOwner)
		return false;

	SetSuspended(true);

	mercOwner->GetMercInfo().IsSuspended = true;
	mercOwner->GetMercInfo().SuspendedTime = time(nullptr) + RuleI(Mercs, SuspendIntervalS);
	mercOwner->GetMercInfo().MercTimerRemaining = mercOwner->GetMercTimer()->GetRemainingTime();
	mercOwner->GetMercInfo().Stance = GetStance();
	Save();
	mercOwner->GetMercTimer()->Disable();
	mercOwner->SendMercSuspendResponsePacket(mercOwner->GetMercInfo().SuspendedTime);
	mercOwner->SendMercTimer(this);

	Depop();

	// Start the timer to send the packet that refreshes the Unsuspend Button
	mercOwner->GetPTimers().Start(pTimerMercSuspend, RuleI(Mercs, SuspendIntervalS));

	Log(Logs::General, Logs::Mercenaries, "Suspend Complete for %s.", mercOwner->GetName());

	return true;
}

bool Client::MercOnlyOrNoGroup() {

	if (!GetGroup())
	{
		return true;
	}
	if (GetMerc())
	{
		if (GetMerc()->GetGroup() == GetGroup())
		{
			if (GetGroup()->GroupCount() < 3)
			{
				return true;
			}
		}
	}
	return false;
}

bool Merc::Unsuspend(bool setMaxStats) {

	Client* mercOwner = nullptr;

	if(GetMercOwner()) {
		mercOwner = GetMercOwner();
	}

	if(!mercOwner)
		return false;

	if(GetID())
	{
		// Set time remaining to max on unsuspend - there is a charge for unsuspending as well
		SetSuspended(false);

		mercOwner->GetMercInfo().mercid = GetID();
		mercOwner->SetMercID(mercOwner->GetID());
		mercOwner->GetMercInfo().IsSuspended = false;

		mercOwner->SendMercenaryUnsuspendPacket(0);
		mercOwner->SendMercenaryUnknownPacket(1);
		mercOwner->GetMercInfo().SuspendedTime = 0;
		// Reset the upkeep timer
		mercOwner->GetMercInfo().MercTimerRemaining = RuleI(Mercs, UpkeepIntervalMS);
		mercOwner->GetMercTimer()->Start(RuleI(Mercs, UpkeepIntervalMS));
		//mercOwner->GetMercTimer()->SetTimer(mercOwner->GetMercInfo().MercTimerRemaining);
		mercOwner->SendMercTimer(this);
		if(!mercOwner->GetPTimers().Expired(&database, pTimerMercSuspend, false))
			mercOwner->GetPTimers().Clear(&database, pTimerMercSuspend);

		CalcBonuses();

		if (MercJoinClientGroup())
		{
			if(setMaxStats)
			{
				SetHP(GetMaxHP());
				SetMana(GetMaxMana());
				SetEndurance(GetMaxEndurance());
			}

			//check for sufficient funds and remove them last
			if(RuleB(Mercs, ChargeMercUpkeepCost))
			{
				uint32 cost = CalcUpkeepCost(GetMercTemplateID(), GetLevel()) * 100;    // Cost is in gold
				if(cost > 0 && !mercOwner->HasMoney(cost))
				{
					mercOwner->SendMercResponsePackets(1);
					Suspend();
					return false;
				}
			}
			Save();
		}
	}

	return true;
}

bool Client::DismissMerc(uint32 MercID) {

	bool Dismissed = true;
	if (!database.DeleteMerc(MercID))
	{
		Log(Logs::General, Logs::Mercenaries, "Dismiss Failed Database Query for MercID: %i, Client: %s.", MercID, GetName());
		Dismissed = false;
	}
	else
	{
		m_epp.edge_merc_character_id = 0;
		Log(Logs::General, Logs::Mercenaries, "Dismiss Successful for %s.", GetName());
		if (GetMerc())
		{
			GetMerc()->Depop();
		}
		SendClearMercInfo();
		SetMerc(nullptr);
	}

	return Dismissed;
}

void Merc::Zone() {
	Save();
	Depop();
}

void Merc::Depop() {

	WipeHateList();

	if(IsCasting())
	{
		InterruptSpell();
	}

	entity_list.RemoveFromHateLists(this);

	if(GetGroup())
	{
		RemoveMercFromGroup(this, GetGroup());
	}

	SetOwner(nullptr);

	p_depop = true;

	NPC::Depop(false);
}

bool Merc::RemoveMercFromGroup(Merc* merc, Group* group) {

	bool Result = false;

	if(merc && group)
	{
		uint32 groupID = group->GetID();
		if(merc->HasGroup())
		{
			if(!group->IsLeader(merc))
			{
				merc->SetFollowID(0);

				if (group->GroupCount() <= 2 && merc->GetGroup() == group && is_zone_loaded)
				{
					group->DisbandGroup();
				}
				else if(group->DelMember(merc, true))
				{
					if(merc->GetMercCharacterID() != 0)
					{
						database.SetGroupID(merc->GetName(), 0, merc->GetMercCharacterID(), true);
					}
				}
			}
			else
			{
				// A merc is group leader - Disband and re-group each member with their mercs
				for(int i = 0; i < MAX_GROUP_MEMBERS; i++)
				{
					if(!group->members[i])
						continue;

					if(!group->members[i]->IsClient())
						continue;

					Client *groupMember = group->members[i]->CastToClient();
					groupMember->LeaveGroup();
					if (groupMember->GetMerc())
					{
						groupMember->GetMerc()->MercJoinClientGroup();
					}
				}
				// Group should be removed by now, but just in case:
				Group *oldGroup = entity_list.GetGroupByID(groupID);
				if (oldGroup != nullptr)
				{
					oldGroup->DisbandGroup();
				}
			}

			Result = true;
		}
	}

	return Result;
}


bool Merc::MercJoinClientGroup() {

	Client* mercOwner = nullptr;

	if(GetMercOwner())
	{
		mercOwner = GetMercOwner();
	}

	if(!mercOwner)
	{
		Suspend();
		return false;
	}

	if(GetID())
	{
		if (HasGroup())
		{
			RemoveMercFromGroup(this, GetGroup());
		}

		Group* g = entity_list.GetGroupByClient(mercOwner);

		//nobody from our group is here... start a new group
		if(!g)
		{
			g = new Group(mercOwner);

			if(!g)
			{
				delete g;
				g = nullptr;
				return false;
			}

			entity_list.AddGroup(g);

			if(g->GetID() == 0)
			{

				delete g;
				g = nullptr;
				return false;
			}

			mercOwner->SendGroupCreatePacket();
			mercOwner->SendGroupLeaderChangePacket(mercOwner->GetName());
			mercOwner->SendGroupJoinAcknowledge();

			if (AddMercToGroup(this, g))
			{
				database.SetGroupID(mercOwner->GetName(), g->GetID(), mercOwner->CharacterID(), false);
				database.SetGroupLeaderName(g->GetID(), mercOwner->GetName());
				database.RefreshGroupFromDB(mercOwner);
				mercOwner->CalcBonuses();
				g->SaveGroupLeaderAA();
				Log(Logs::General, Logs::Mercenaries, "Mercenary joined new group: %s (%s).", GetName(), mercOwner->GetName());
			}
			else
			{
				g->DisbandGroup();
				Suspend();
				Log(Logs::General, Logs::Mercenaries, "Mercenary disbanded new group: %s (%s).", GetName(), mercOwner->GetName());
			}

		}
		else if (AddMercToGroup(this, mercOwner->GetGroup()))
		{
			// Group already exists
			database.RefreshGroupFromDB(mercOwner);
			// Update members that are out of zone
			GetGroup()->SendGroupJoinOOZ(this);
			mercOwner->CalcBonuses();
			Log(Logs::General, Logs::Mercenaries, "Mercenary %s joined existing group with %s.", GetName(), mercOwner->GetName());
		}
		else
		{
			Suspend();
			Log(Logs::General, Logs::Mercenaries, "Mercenary failed to join the group - Suspending %s for (%s).", GetName(), mercOwner->GetName());
		}
	}

	return true;
}

bool Merc::AddMercToGroup(Merc* merc, Group* group) {
	bool Result = false;

	if(merc && group) {
		// Remove merc from current group if it's not the destination group
		if(merc->HasGroup())
		{
			if(merc->GetGroup() == group && merc->GetMercOwner())
			{
				// Merc is already in the destination group
				merc->SetFollowID(merc->GetMercOwner()->GetID());
				return true;
			}
			merc->RemoveMercFromGroup(merc, merc->GetGroup());
		}
		//Try and add the member, followed by checking if the merc owner exists.
		if(merc->GetMercOwner() && group->AddMember(merc))
		{
			merc->SetFollowID(merc->GetMercOwner()->GetID());
			Result = true;
		}
		else
		{
			//Suspend it if the member is not added or the merc's owner is not valid.
			merc->Suspend();
		}
	}

	return Result;
}

void Client::InitializeMercInfo() {

	for(int i=0; i<MAXMERCS; i++)
	{
		m_mercinfo[i] = MercInfo();
	}

}

Merc* Client::GetMerc() {

	if(GetMercID() == 0)
	{
		Log(Logs::Detail, Logs::Mercenaries, "GetMerc - GetMercID: 0 for %s.", GetName());
		return (nullptr);
	}

	Merc* tmp = entity_list.GetMercByID(GetMercID());
	if(tmp == nullptr)
	{
		SetMercID(0);
		Log(Logs::Detail, Logs::Mercenaries, "GetMerc No Merc for %s.", GetName());
		return (nullptr);
	}

	if(tmp->GetOwner() != this)
	{
		SetMercID(0);
		//Log(Logs::Detail, Logs::Mercenaries, "GetMerc Owner Mismatch - OwnerID: %d, ClientID: %d, Client: %s.", tmp->GetOwnerID(), GetID(), GetName());
		return (nullptr);
	}

	return (tmp);
}

int8 Client::GetMercIndexByCharacterID(uint32 character_id) {

	auto entry = validMercCharacters.find(character_id);

	if (entry == validMercCharacters.end())
	{
		return -1;
	}

	return entry->second.slot;
}

void Merc::SetMercData( uint32 template_id ) {

	MercTemplate* merc_template = zone->GetMercTemplate(template_id);
	SetMercTemplateID( merc_template->MercTemplateID );
	SetMercType( merc_template->MercType );
	SetMercSubType( merc_template->MercSubType );
	SetProficiencyID( merc_template->ProficiencyID );
	SetTierID( merc_template->TierID );
	SetCostFormula( merc_template->CostFormula );
	SetMercNameType( merc_template->MercNameType );

}

MercTemplate* Zone::GetMercTemplate( uint32 template_id ) {
	return &merc_templates[template_id];
}

void Client::SetMerc(Merc* newmerc) {

	Merc* oldmerc = GetMerc();
	if (oldmerc)
	{
		oldmerc->Depop();
	}

	if (!newmerc)
	{
		SetMercID(0);
		GetMercInfo().mercid = 0;
		GetMercInfo().MercTemplateID = 0;
		GetMercInfo().myTemplate = nullptr;
		GetMercInfo().IsSuspended = false;
		GetMercInfo().SuspendedTime = 0;
		GetMercInfo().Gender = 0;
		GetMercInfo().State = 0;
		memset(GetMercInfo().merc_name, 0, 64);
		Log(Logs::General, Logs::Mercenaries, "SetMerc No Merc for %s.", GetName());
	}
	else
	{
		//Client* oldowner = entity_list.GetClientByID(newmerc->GetOwnerID());
		newmerc->SetOwner(this);
		newmerc->SetMercCharacterID(this->CharacterID());
		newmerc->SetMercID(GetID());
		newmerc->SetClientVersion((uint8)this->ClientVersion());
		GetMercInfo().mercid = newmerc->GetID();
		GetMercInfo().MercTemplateID = newmerc->GetMercTemplateID();
		GetMercInfo().myTemplate = zone->GetMercTemplate(GetMercInfo().MercTemplateID);
		GetMercInfo().IsSuspended = newmerc->IsSuspended();
		GetMercInfo().SuspendedTime = 0;
		GetMercInfo().Gender = newmerc->GetGender();
		GetMercInfo().State = newmerc->IsSuspended() ? MERC_STATE_SUSPENDED : MERC_STATE_NORMAL;
		snprintf(GetMercInfo().merc_name, 64, "%s", newmerc->GetName());
		Log(Logs::General, Logs::Mercenaries, "SetMerc New Merc for %s.", GetName());
	}
}

void Client::UpdateMercLevel() {
	Merc* merc = GetMerc();
	if (merc)
	{
		merc->UpdateMercStats(this, false);
		merc->SendAppearancePacket(AT_WhoLevel, GetLevel(), true, true);
	}
}

void Client::SendMercMerchantResponsePacket(int32 response_type) {
	// This response packet brings up the Mercenary Manager window
	if (ClientVersion() >= EQ::versions::ClientVersion::SoD)
	{
		auto outapp = new EQApplicationPacket(OP_MercenaryHire, sizeof(MercenaryMerchantResponse_Struct));
		MercenaryMerchantResponse_Struct* mmr = (MercenaryMerchantResponse_Struct*)outapp->pBuffer;
		mmr->ResponseType = response_type;              // send specified response type
		FastQueuePacket(&outapp);
		Log(Logs::Moderate, Logs::Mercenaries, "Sent SendMercMerchantResponsePacket ResponseType: %i, Client: %s.", response_type, GetName());
	}
}

void Client::SendMercenaryUnknownPacket(uint8 type) {

	auto outapp = new EQApplicationPacket(OP_MercenaryUnknown1, 1);
	outapp->WriteUInt8(type);
	FastQueuePacket(&outapp);
	Log(Logs::Moderate, Logs::Mercenaries, "Sent SendMercenaryUnknownPacket Type: %i, Client: %s.", type, GetName());

}

void Client::SendMercenaryUnsuspendPacket(uint8 type) {

	auto outapp = new EQApplicationPacket(OP_MercenaryUnsuspendResponse, 1);
	outapp->WriteUInt8(type);
	FastQueuePacket(&outapp);
	Log(Logs::Moderate, Logs::Mercenaries, "Sent SendMercenaryUnsuspendPacket Type: %i, Client: %s.", type, GetName());

}

void Client::SendMercSuspendResponsePacket(uint32 suspended_time) {

	auto outapp = new EQApplicationPacket(OP_MercenarySuspendResponse, sizeof(SuspendMercenaryResponse_Struct));
	SuspendMercenaryResponse_Struct* smr = (SuspendMercenaryResponse_Struct*)outapp->pBuffer;
	smr->SuspendTime = suspended_time;              // Seen 0 (not suspended) or c9 c2 64 4f (suspended on Sat Mar 17 11:58:49 2012) - Unix Timestamp
	FastQueuePacket(&outapp);
	Log(Logs::Moderate, Logs::Mercenaries, "Sent SendMercSuspendResponsePacket Time: %i, Client: %s.", suspended_time, GetName());

}

void Client::SendMercTimerPacket(int32 entity_id, int32 merc_state, int32 suspended_time, int32 update_interval, int32 unk01) {

	// Send Mercenary Status/Timer packet
	auto outapp = new EQApplicationPacket(OP_MercenaryTimer, sizeof(MercenaryStatus_Struct));
	MercenaryStatus_Struct* mss = (MercenaryStatus_Struct*)outapp->pBuffer;
	mss->MercEntityID = entity_id; // Seen 0 (no merc spawned) or unknown value when merc is spawned
	mss->MercState = merc_state; // Seen 5 (normal) or 1 (suspended)
	mss->SuspendedTime = suspended_time; // Seen 0 for not suspended or Unix Timestamp for suspended merc
	mss->UpdateInterval = update_interval; // Seen 900000 - 15 minutes in ms
	mss->MercUnk01 = unk01; // Seen 180000 - 3 minutes in ms - Used for the unsuspend button refresh timer
	FastQueuePacket(&outapp);
	Log(Logs::Moderate, Logs::Mercenaries, "Sent SendMercTimerPacket EndID: %i, State: %i, SuspendTime: %i, Interval: %i, Unk1: %i, Client: %s.", entity_id, merc_state, suspended_time, update_interval, unk01, GetName());

}

void Client::SendMercAssignPacket(uint32 entityID, uint32 unk01, uint32 unk02) {
	auto outapp = new EQApplicationPacket(OP_MercenaryAssign, sizeof(MercenaryAssign_Struct));
	MercenaryAssign_Struct* mas = (MercenaryAssign_Struct*)outapp->pBuffer;
	mas->MercEntityID = entityID;
	mas->MercUnk01 = unk01;
	mas->MercUnk02 = unk02;
	FastQueuePacket(&outapp);
	Log(Logs::Moderate, Logs::Mercenaries, "Sent SendMercAssignPacket EndID: %i, Unk1: %i, Unk2: %i, Client: %s.", entityID, unk01, unk02, GetName());
}

void NPC::LoadMercTypes() {

	std::string query = StringFormat("SELECT DISTINCT MTyp.dbstring, MTyp.clientversion "
		"FROM merc_merchant_entries MME, merc_merchant_template_entries MMTE, "
		"merc_types MTyp, merc_templates MTem "
		"WHERE MME.merchant_id = %i "
		"AND MME.merc_merchant_template_id = MMTE.merc_merchant_template_id "
		"AND MMTE.merc_template_id = MTem.merc_template_id "
		"AND MTem.merc_type_id = MTyp.merc_type_id;", GetNPCTypeID());
	auto results = database.QueryDatabase(query);
	if (!results.Success())
	{
		LogError("Error in NPC::LoadMercTypes()");
		return;
	}

	for (auto row = results.begin(); row != results.end(); ++row)
	{
		MercType tempMercType;

		tempMercType.Type = atoi(row[0]);
		tempMercType.ClientVersion = atoi(row[1]);

		mercTypeList.push_back(tempMercType);
	}

}

void NPC::LoadMercs() {

	std::string query = StringFormat("SELECT DISTINCT MTem.merc_template_id, MTyp.dbstring AS merc_type_id, "
		"MTem.dbstring AS merc_subtype_id, 0 AS CostFormula, "
		"CASE WHEN MTem.clientversion > MTyp.clientversion "
		"THEN MTem.clientversion "
		"ELSE MTyp.clientversion END AS clientversion, MTem.merc_npc_type_id "
		"FROM merc_merchant_entries MME, merc_merchant_template_entries MMTE, "
		"merc_types MTyp, merc_templates MTem "
		"WHERE MME.merchant_id = %i AND "
		"MME.merc_merchant_template_id = MMTE.merc_merchant_template_id "
		"AND MMTE.merc_template_id = MTem.merc_template_id "
		"AND MTem.merc_type_id = MTyp.merc_type_id;", GetNPCTypeID());
	auto results = database.QueryDatabase(query);

	if (!results.Success())
	{
		LogError("Error in NPC::LoadMercTypes()");
		return;
	}

	for (auto row = results.begin(); row != results.end(); ++row)
	{
		MercData tempMerc;

		tempMerc.MercTemplateID = atoi(row[0]);
		tempMerc.MercType = atoi(row[1]);
		tempMerc.MercSubType = atoi(row[2]);
		tempMerc.CostFormula = atoi(row[3]);
		tempMerc.ClientVersion = atoi(row[4]);
		tempMerc.NPCID = atoi(row[5]);

		mercDataList.push_back(tempMerc);
	}

}

int NPC::GetNumMercTypes(uint32 clientVersion) {

	int count = 0;
	std::list<MercType> mercTypeList = GetMercTypesList();

	for (auto mercTypeListItr = mercTypeList.begin(); mercTypeListItr != mercTypeList.end(); ++mercTypeListItr) {
		if(mercTypeListItr->ClientVersion <= clientVersion)
			count++;
	}

	return count;
}

int NPC::GetNumMercs(uint32 clientVersion) {

	int count = 0;
	std::list<MercData> mercDataList = GetMercsList();

	for (auto mercListItr = mercDataList.begin(); mercListItr != mercDataList.end(); ++mercListItr) {
		if(mercListItr->ClientVersion <= clientVersion)
			count++;
	}

	return count;
}

std::list<MercType> NPC::GetMercTypesList(uint32 clientVersion) {

	std::list<MercType> result;

	if(GetNumMercTypes() > 0)
	{
		for (auto mercTypeListItr = mercTypeList.begin(); mercTypeListItr != mercTypeList.end();
		     ++mercTypeListItr) {
			if(mercTypeListItr->ClientVersion <= clientVersion)
			{
				MercType mercType;
				mercType.Type = mercTypeListItr->Type;
				mercType.ClientVersion = mercTypeListItr->ClientVersion;
				result.push_back(mercType);
			}
		}
	}

	return result;
}

std::list<MercData> NPC::GetMercsList(uint32 clientVersion) {

	std::list<MercData> result;

	if(GetNumMercs() > 0)
	{
		for (auto mercListItr = mercDataList.begin(); mercListItr != mercDataList.end(); ++mercListItr) {
			if(mercListItr->ClientVersion <= clientVersion)
			{
				MercTemplate *merc_template = zone->GetMercTemplate(mercListItr->MercTemplateID);

				if(merc_template)
				{
					MercData mercData;
					mercData.MercTemplateID = mercListItr->MercTemplateID;
					mercData.MercType = merc_template->MercType;
					mercData.MercSubType = merc_template->MercSubType;
					mercData.CostFormula = merc_template->CostFormula;
					mercData.ClientVersion = merc_template->ClientVersion;
					mercData.NPCID = merc_template->MercNPCID;
					result.push_back(mercData);
				}
			}
		}
	}

	return result;
}

uint32 Merc::CalcPurchaseCost(uint32 templateID , uint8 level, uint8 currency_type) {

	uint32 cost = 0;

	MercTemplate *mercData = zone->GetMercTemplate(templateID);

	if(mercData)
	{
		//calculate cost in coin - cost in gold
		if(currency_type == 0)
		{
			int levels_above_cutoff;
			switch (mercData->CostFormula)
			{
			case 0:
				levels_above_cutoff = level > 10 ? (level - 10) : 0;
				cost = levels_above_cutoff * 300;
				cost += level >= 10 ? 100 : 0;
				cost /= 100;
				break;
			default:
				break;
			}
		}
		else if(currency_type == 19)
		{
			// cost in Bayle Marks
			cost = 1;
		}
	}

	return cost;
}

uint32 Merc::CalcUpkeepCost(uint32 templateID , uint8 level, uint8 currency_type) {

	uint32 cost = 0;

	MercTemplate *mercData = zone->GetMercTemplate(templateID);

	if(mercData)
	{
		//calculate cost in coin - cost in gold
		if(currency_type == 0)
		{
			int levels_above_cutoff;
			switch (mercData->CostFormula)
			{
			case 0:
				levels_above_cutoff = level > 10 ? (level - 10) : 0;
				cost = levels_above_cutoff * 300;
				cost += level >= 10 ? 100 : 0;
				cost /= 100;
				break;
			default:
				break;
			}
		}
		else if(currency_type == 19)
		{
			// cost in Bayle Marks
			cost = 1;
		}
	}

	return cost;
}
