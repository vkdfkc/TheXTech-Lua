#ifndef XTECH_LUA_DATA_H
#define XTECH_LUA_DATA_H

#include <string>

struct lua_State;

// Serialize a Lua table at the given stack index to a JSON string.
// Supports: nil, boolean, number, string, nested table.
std::string lua_table_to_json(lua_State* L, int tableIndex);

// Deserialize a JSON string, pushing the resulting Lua table onto the stack.
void json_to_lua_table(lua_State* L, const std::string& json);

#endif // XTECH_LUA_DATA_H
