#ifndef EQEMU_LUA_LOOTLOCKOUT_H
#define EQEMU_LUA_LOOTLOCKOUT_H
#ifdef LUA_EQEMU

#include "zonedump.h"
#include "lua_ptr.h"

struct LootLockout;

namespace luabind {
	struct scope;
}

luabind::scope lua_register_lootlockout();

class Lua_LootLockout : public Lua_Struct<LootLockout>
{
	typedef LootLockout NativeType;
public:
	Lua_LootLockout() { SetLuaStructData(LootLockout()); }
	Lua_LootLockout(LootLockout& d) { SetLuaStructData(d); }
	virtual ~Lua_LootLockout() { }

	operator LootLockout()& {
			return d_;
	}
	
	uint32 GetNPCTypeID();
	uint32 GetCharacterID();
	uint32 GetZoneID();
	uint32 GetExpiryTime();
	const char* GetNPCName();
};

#endif
#endif
