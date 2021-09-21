#ifdef LUA_EQEMU

#include "lua.hpp"
#include <luabind/luabind.hpp>

#include "zonedump.h"
#include "zonedb.h"
#include "../common/string_util.h"
#include "lua_lootlockout.h"

extern ZoneDatabase database;

uint32 Lua_LootLockout::GetNPCTypeID() 
{
	LootLockout llo = GetLuaStructData();

	return llo.npctype_id;
}
uint32 Lua_LootLockout::GetCharacterID()
{
	LootLockout llo = GetLuaStructData();

	return llo.character_id;
}
uint32 Lua_LootLockout::GetZoneID()
{
	LootLockout llo = GetLuaStructData();

	return llo.zone_id;

}
uint32 Lua_LootLockout::GetExpiryTime() 
{
	LootLockout llo = GetLuaStructData();
	return llo.expirydate;

}
const char* Lua_LootLockout::GetNPCName()
{
	LootLockout llo = GetLuaStructData();
	auto tmp = database.LoadNPCTypesData(llo.npctype_id);
	if (tmp && tmp->name[0])
	{
		char clean_name[64];
		return CleanMobName(tmp->name, clean_name);
	}
	return "INVALID NPC";
}

luabind::scope lua_register_lootlockout() {
	return luabind::class_<Lua_LootLockout>("LootLockout")
		.def(luabind::constructor<>())
		.property("null", &Lua_LootLockout::Null)
		.property("valid", &Lua_LootLockout::Valid)
		.def("GetNPCTypeID", (uint32(Lua_LootLockout::*)(void)) & Lua_LootLockout::GetNPCTypeID)
		.def("GetNPCName", (const char* (Lua_LootLockout::*)(void)) & Lua_LootLockout::GetNPCName)
		.def("GetExpiryTime", (uint32(Lua_LootLockout::*)(void)) & Lua_LootLockout::GetExpiryTime)
		.def("GetZoneID", (uint32(Lua_LootLockout::*)(void)) & Lua_LootLockout::GetZoneID)
		.def("GetCharacterID", (uint32(Lua_LootLockout::*)(void)) & Lua_LootLockout::GetCharacterID);
}

#endif
