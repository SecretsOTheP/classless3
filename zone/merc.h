#ifndef MERC_H
#define MERC_H

#include "npc.h"
#include "../common/eq_packet_structs.h"
#include "../common/extprofile.h"
#include "../common/extprofile.h"
#include "../common/ptimer.h"

class Client;
class Corpse;
class Group;
class Mob;
class Raid;
struct MercTemplate;
struct NPCType;
struct NewSpawn_Struct;
struct ExtendedProfile_Struct;
struct PlayerProfile_Struct;
struct MercEquip : EQ::textures::Texture_Struct, EQ::textures::Tint_Struct {};
namespace EQ
{
	struct ItemData;
}

#define MAXMERCS 1
#define TANK 1
#define HEALER 2
#define MELEEDPS 9
#define CASTERDPS 12

#define NO_MERC_ID 0
#define MERC_STATE_NORMAL 5
#define MERC_STATE_SUSPENDED 1
#define NOT_SUSPENDED_TIME 0

const int MercAISpellRange = 100; // TODO: Write a method that calcs what the merc's spell range is based on spell, equipment, AA, whatever and replace this

struct MercSpell {
	int32 spellid; // <= 0 = no spell
	SpellTypes type; // 0 = never, must be one (and only one) of the defined values
	int16 stance; // 0 = all, + = only this stance, - = all except this stance
	int16 slot;
	uint16 proc_chance;
	uint32 time_cancast; // when we can cast this spell next
};

struct MercTimer {
	uint16 timerid; // EndurTimerIndex
	uint8 timertype; // 1 = spell, 2 = disc
	int32 spellid; // <= 0 = no spell
	uint32 time_cancast; // when we can cast this spell next
};

class Merc : public NPC {
public:
	Merc(const NPCType* d, float x, float y, float z, float heading);
	virtual ~Merc();

	//abstract virtual function implementations requird by base abstract class
	virtual bool Death(Mob* killerMob, int64 damage, uint16 spell_id, EQ::skills::SkillType attack_skill);
	virtual void Damage(Mob* from, int64 damage, uint16 spell_id, EQ::skills::SkillType attack_skill, bool avoidable = true, int8 buffslot = -1, bool iBuffTic = false, eSpecialAttacks special = eSpecialAttacks::None);
	virtual bool Attack(Mob* other, int Hand = EQ::invslot::slotPrimary, bool FromRiposte = false, bool IsStrikethrough = false,
	bool IsFromSpell = false, ExtraAttackOptions *opts = nullptr);
	virtual bool HasRaid() { return false; }
	virtual bool HasGroup() { return (GetGroup() ? true : false); }
	virtual Raid* GetRaid() { return 0; }
	virtual Group* GetGroup() { return entity_list.GetGroupByMob(this); }
	virtual void	SetAttackTimer();
	int GetQuiverHaste(int delay);
	int64 CalcEnduranceRegen(bool bCombat);

	// Mob AI Virtual Override Methods
	virtual void AI_Start(int32 iMoveDelay = 0);
	virtual void AI_Process();

	//virtual bool AICastSpell(Mob* tar, int8 iChance, uint32 iSpellTypes);
	virtual bool AICastSpell(int8 iChance, uint32 iSpellTypes);
	virtual bool AIDoSpellCast(int32 spellid, Mob* tar, int32 mana_cost, uint32* oDontDoAgainBefore = 0);
	virtual bool AI_EngagedCastCheck();
	void AI_Event_MercSpellCastFinished(bool iCastSucceeded, int32 spell_id);
	//virtual bool AI_PursueCastCheck();
	virtual bool AI_IdleCastCheck();
	virtual uint32 GetEquippedItemFromTextureSlot(uint8 material_slot) const; // returns item id
	virtual int32 GetEquipmentMaterial(uint8 material_slot) const;
	virtual uint32 GetEquipmentColor(uint8 material_slot) const;

	virtual bool Process();

	// Static Merc Group Methods
	static bool AddMercToGroup(Merc* merc, Group* group);
	static bool RemoveMercFromGroup(Merc* merc, Group* group);
	void ProcessClientZoneChange(Client* mercOwner);
	static void MercGroupSay(Mob *speaker, std::string gmsg);
	Corpse* GetGroupMemberCorpse();

	// Merc Spell Casting Methods
	virtual int32 GetActSpellCasttime(int32 spell_id, int32 casttime);
	virtual int32 GetActSpellCost(int32 spell_id, int32 cost);
	virtual float GetActSpellSynergy(uint16 spell_id, bool usecache = true);
	int8 GetChanceToCastBySpellType(SpellTypes spellType);
	void SetSpellRecastTimer(uint16 timer_id, int32 spellid, uint32 recast_delay);
	void SetSpellTimeCanCast(int32 spellid, uint32 recast_delay);
	 int32 GetSpellRecastTimer(uint16 timer_id);
	 bool CheckSpellRecastTimers(int32 spellid);
	 std::vector<MercSpell> GetMercSpellsForSpellEffect(int32 spellEffect);
	 std::vector<MercSpell> GetMercSpellsForSpellEffectAndTargetType(int spellEffect, SpellTargetType targetType);
	 std::vector<MercSpell> GetMercSpellsBySpellType(SpellTypes spellType);
	 MercSpell GetFirstMercSpellBySpellType(SpellTypes spellType);
	 MercSpell GetFirstMercSpellForSingleTargetHeal();
	 MercSpell GetMercSpellBySpellID(int32 spellid);
	 MercSpell GetBestMercSpellForVeryFastHeal();
	 MercSpell GetBestMercSpellForFastHeal();
	 MercSpell GetBestMercSpellForHealOverTime();
	 MercSpell GetBestMercSpellForPercentageHeal();
	 MercSpell GetBestMercSpellForRegularSingleTargetHeal();
	 MercSpell GetBestMercSpellForGroupHealOverTime();
	 MercSpell GetBestMercSpellForGroupCompleteHeal();
	 MercSpell GetBestMercSpellForGroupHeal();
	 MercSpell GetBestMercSpellForAETaunt();
	 MercSpell GetBestMercSpellForTaunt();
	 MercSpell GetBestMercSpellForHate();
	 MercSpell GetBestMercSpellForCure(Mob* target);
	 MercSpell GetBestMercSpellForStun();
	 MercSpell GetBestMercSpellForAENuke(Mob* target);
	 MercSpell GetBestMercSpellForTargetedAENuke(Mob* target);
	 MercSpell GetBestMercSpellForPBAENuke(Mob* target);
	 MercSpell GetBestMercSpellForAERainNuke(Mob* target);
	 MercSpell GetBestMercSpellForNuke();
	 MercSpell GetBestMercSpellForNukeByTargetResists(Mob* target);
	 bool CheckAENuke(Mob* tar, int32 spell_id, uint8 &numTargets);
	 bool GetNeedsCured(Mob *tar);
	bool HasOrMayGetAggro();
	virtual void AISpellsList(Client* c);

	virtual bool IsMerc() const { return true; }

	virtual void FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho);
	static Merc* LoadMercFromCharacterID(Client* c, uint32 character_id);
	void UpdateMercInfo(Client *c);
	void UpdateMercStats(Client *c, bool setmax = false);
	void UpdateMercAppearance();
	virtual void UpdateEquipmentLight();
	void AddItem(uint8 slot, uint32 item_id);
	static const char *GetRandomName();
	bool Spawn(Client *owner);
	bool Suspend();
	bool Unsuspend(bool setMaxStats);
	bool MercJoinClientGroup();
	void Zone();
	virtual void Depop();
	virtual bool Save();
	bool GetDepop() { return p_depop; }

	bool IsDead() { return GetHP() < 0;};
	bool IsMedding() { return _medding; };
	bool IsSuspended() { return _suspended; };

	static uint32 CalcPurchaseCost( uint32 templateID , uint8 level, uint8 currency_type = 0);
	static uint32 CalcUpkeepCost( uint32 templateID , uint8 level, uint8 currency_type = 0);

	// "GET" Class Methods
	virtual Mob* GetOwner();
	Client* GetMercOwner();
	virtual Mob* GetOwnerOrSelf();
	uint32 GetMercID() { return _MercID; }
	uint32 GetMercCharacterID( ) { return owner_char_id; }
	uint32 GetMercTemplateID() { return _MercTemplateID; }
	uint32 GetMercType() { return _MercType; }
	uint32 GetMercSubType() { return _MercSubType; }
	uint8 GetProficiencyID() { return _ProficiencyID; }
	uint8 GetTierID() { return _TierID; }
	uint32 GetCostFormula() { return _CostFormula; }
	uint32 GetMercNameType() { return _NameType; }
	EQ::constants::StanceType GetStance() { return _currentStance; }
	int GetHatedCount() { return _hatedCount; }

	inline const uint8 GetClientVersion() const { return _OwnerClientVersion; }

	virtual void SetTarget(Mob* mob);
	bool HasSkill(EQ::skills::SkillType skill_id) const;
	bool CanHaveSkill(EQ::skills::SkillType skill_id) const;
	uint16 MaxSkill(EQ::skills::SkillType skillid, uint16 class_, uint16 level) const;
	inline uint16 MaxSkill(EQ::skills::SkillType skillid) const { return MaxSkill(skillid, GetClass(), GetLevel()); }
	virtual void DoClassAttacks(Mob *target);
	void CheckHateList();
	bool CheckTaunt();
	bool CheckAETaunt();
	bool CheckConfidence();
	bool TryHide();

	// stat functions
	inline virtual int64 GetSTR() const { return STR; }
	inline virtual int64 GetSTA() const { return STA; }
	inline virtual int64 GetDEX() const { return DEX; }
	inline virtual int64 GetAGI() const { return AGI; }
	inline virtual int64 GetINT() const { return INT; }
	inline virtual int64 GetWIS() const { return WIS; }
	inline virtual int64 GetCHA() const { return CHA; }
	inline virtual int32 GetMR() const { return MR; }
	inline virtual int32 GetFR() const { return FR; }
	inline virtual int32 GetDR() const { return DR; }
	inline virtual int32 GetPR() const { return PR; }
	inline virtual int32 GetCR() const { return CR; }
	inline virtual int32 GetCorrup() const { return Corrup; }
	inline virtual int32 GetPhR() const { return PhR; }

	virtual void ScaleStats(int scalepercent, bool setmax = false);
	virtual void CalcBonuses();
	void CalcSynergy();
	int64 GetEndurance() const {return Mob::GetEndurance();} //This gets our current endurance
	inline uint8 GetEndurancePercent() { return (uint8)((float)GetEndurance() / (float)GetMaxEndurance() * 100.0f); }
	inline virtual int32 GetATKBonus() const { return itembonuses.ATK + spellbonuses.ATK; }
	// Mod2
	inline virtual int32 GetShielding() const { return itembonuses.MeleeMitigation; }
	inline virtual int32 GetSpellShield() const { return itembonuses.SpellShield; }
	inline virtual int32 GetDoTShield() const { return itembonuses.DoTShielding; }
	inline virtual int32 GetStunResist() const { return itembonuses.StunResist; }
	inline virtual int32 GetStrikeThrough() const { return itembonuses.StrikeThrough; }
	inline virtual int32 GetAvoidance() const { return itembonuses.AvoidMeleeChance; }
	inline virtual int32 GetAccuracy() const { return itembonuses.HitChance; }
	inline virtual int32 GetCombatEffects() const { return itembonuses.ProcChance; }
	inline virtual int32 GetDS() const { return itembonuses.DamageShield; }
	// Mod3
	inline virtual int32 GetHealAmt() const { return itembonuses.HealAmt; }
	inline virtual int32 GetSpellDmg() const { return itembonuses.SpellDmg; }
	inline virtual int32 GetClair() const { return itembonuses.Clairvoyance; }
	inline virtual int32 GetDSMit() const { return itembonuses.DSMitigation; }

	inline virtual int32 GetSingMod() const { return itembonuses.singingMod; }
	inline virtual int32 GetBrassMod() const { return itembonuses.brassMod; }
	inline virtual int32 GetPercMod() const { return itembonuses.percussionMod; }
	inline virtual int32 GetStringMod() const { return itembonuses.stringedMod; }
	inline virtual int32 GetWindMod() const { return itembonuses.windMod; }

	inline virtual int32 GetDelayDeath() const { return aabonuses.DelayDeath + spellbonuses.DelayDeath + itembonuses.DelayDeath; }

	// "SET" Class Methods
	void SetMercData (uint32 templateID );
	void SetMercID( uint32 mercID ) { _MercID = mercID; }
	void SetMercCharacterID( uint32 ownerID ) { owner_char_id = ownerID; }
	void SetMercTemplateID( uint32 templateID ) { _MercTemplateID = templateID; }
	void SetMercType( uint32 type ) { _MercType = type; }
	void SetMercSubType( uint32 subtype ) { _MercSubType = subtype; }
	void SetProficiencyID( uint8 proficiency_id ) { _ProficiencyID = proficiency_id; }
	void SetTierID( uint8 tier_id ) { _TierID = tier_id; }
	void SetCostFormula( uint8 costformula ) { _CostFormula = costformula; }
	void SetMercNameType( uint8 nametype ) { _NameType = nametype; }
	void SetClientVersion(uint8 clientVersion) { _OwnerClientVersion = clientVersion; }
	void SetSuspended(bool suspended) { _suspended = suspended; }
	void SetStance( EQ::constants::StanceType stance ) { _currentStance = stance; }
	void SetHatedCount( int count ) { _hatedCount = count; }

	inline EQ::InventoryProfile& GetInv() { return m_inv; }
	inline const EQ::InventoryProfile& GetInv() const { return m_inv; }

	void Sit();
	void Stand();
	bool IsSitting();
	bool IsStanding();

	// Merc-specific functions
	bool IsMercCaster() { return (GetClass() == HEALER || GetClass() == CASTERDPS); }
	virtual float GetMaxMeleeRangeToTarget(Mob* target);
	virtual void MercMeditate(bool isSitting);
	bool FindTarget();

protected:
	void CalcItemBonuses(StatBonuses* newbon);
	void AddItemBonuses(const EQ::ItemInstance* inst, StatBonuses* newbon, bool isAug = false, bool isTribute = false, int rec_override = 0, bool ammo_slot_item = false);
	void AddFakeItemBonuses(const EQ::ItemInstance* inst, StatBonuses* newbon, bool isAug = false, const EQ::ItemInstance* baseinst = nullptr);
	void AdditiveWornBonuses(const EQ::ItemInstance* inst, StatBonuses* newbon, bool isAug = false);
	int CalcRecommendedLevelBonus(uint8 level, uint8 reclevel, int basestat);

	int16 GetFocusEffect(focusType type, int32 spell_id);

	std::vector<MercSpell> merc_spells;
	std::map<uint32,MercTimer> timers;

	Timer evade_timer; // can be moved to pTimers at some point

	uint16 skills[EQ::skills::HIGHEST_SKILL + 1];
	EQ::InventoryProfile inv[EQ::invslot::EQUIPMENT_COUNT]; //this is an array of item IDs
	uint16 d_melee_texture1; //this is an item Material value
	uint16 d_melee_texture2; //this is an item Material value (offhand)
	uint8 prim_melee_type; //Sets the Primary Weapon attack message and animation
	uint8 sec_melee_type; //Sets the Secondary Weapon attack message and animation

private:
	int64 CalcAlcoholPhysicalEffect();
	int64 CalcSTR();
	int64 CalcSTA();
	int64 CalcDEX();
	int64 CalcAGI();
	int64 CalcINT();
	int64 CalcWIS();
	int64 CalcCHA();
	float CalcPOWER();
	float CalcNORMAL();
	float CalcNORMAL_Legacy();
	int32 CalcMR();
	int32 CalcFR();
	int32 CalcDR();
	int32 CalcPR();
	int32 CalcCR();
	int64 CalcMaxHP();
	int64 CalcBaseHP();
	int64 CalcMaxMana();
	int64 CalcBaseMana();
	int64 CalcBaseEndurance(); //Calculates Base End
	int64 GetMaxEndurance() const {return max_end;} //This gets our endurance from the last CalcMaxEndurance() call
	void DoHPRegen();
	int32 GetCharmEffectID(int16 slot);
	void DoManaRegen();
	void DoEnduranceRegen(); //This Regenerates endurance
	void SetEndurance(int64 newEnd); //This sets the current endurance to the new value

	int GroupLeadershipAAHealthEnhancement();
	int GroupLeadershipAAManaEnhancement();
	int GroupLeadershipAAHealthRegeneration();
	int GroupLeadershipAAOffenseEnhancement();

	float GetDefaultSize();

	bool LoadMercSpells();
	bool CheckStance(int16 stance);
	std::vector<MercSpell> GetMercSpells() { return merc_spells; }

	// Private "base stats" Members
	int64 _characterID;


	uint32 _MercID;
	uint32 _MercTemplateID;
	uint32 _MercType;
	uint32 _MercSubType;
	uint8 _ProficiencyID;
	uint8 _TierID;
	uint8 _CostFormula;
	uint8 _NameType;
	uint8 _OwnerClientVersion;
	EQ::constants::StanceType _currentStance;
	EQ::InventoryProfile m_inv;
	PlayerProfile_Struct m_pp;
	ExtendedProfile_Struct m_epp;
	inline PlayerProfile_Struct& GetPP() { return m_pp; }
	PTimerList p_timers;
	inline ExtendedProfile_Struct& GetEPP() { return m_epp; }

	bool _medding;
	bool _suspended;
	bool p_depop;
	bool _check_confidence;
	bool _lost_confidence;
	int _hatedCount;
	uint32 owner_char_id;
	Timer check_target_timer;
};

#endif // MERC_H
