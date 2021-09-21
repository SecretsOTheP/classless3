#ifndef PETS_H
#define PETS_H

class Mob;
struct NPCType;

class Pet : public NPC {
	public:
		Pet(NPCType *type_data, Mob *owner, PetType type, uint16 spell_id, int16 power);
		virtual bool CheckSpellLevelRestriction(uint16 spell_id);
		virtual bool IsTypePet()			const { return true; }
		virtual int _GetWalkSpeed();
		virtual int _GetRunSpeed();
		virtual int _GetFearSpeed();
	};

#endif

