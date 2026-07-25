/*
 * TheXTech - A platform game engine ported from old source code for VB6
 *
 * Copyright (c) 2009-2011 Andrew Spinks, original VB6 code
 * Copyright (c) 2020-2026 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string>
#include <cstring>
#include <cstdlib>
#include <cmath>

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
}

#include "xtech_lua_data.h"

// ============================================================================
// Lua table → JSON (recursive)
// ============================================================================

static void table_to_json_recursive(lua_State* L, int idx, std::string& out)
{
    int t = lua_type(L, idx);

    switch(t)
    {
    case LUA_TNIL:
        out += "null";
        return;

    case LUA_TBOOLEAN:
        out += lua_toboolean(L, idx) ? "true" : "false";
        return;

    case LUA_TNUMBER:
    {
        double d = lua_tonumber(L, idx);
        if(std::isnan(d) || std::isinf(d))
            out += "0";
        else if(d == std::floor(d) && std::abs(d) < 1e15)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.0f", d);
            out += buf;
        }
        else
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "%.14g", d);
            out += buf;
        }
        return;
    }

    case LUA_TSTRING:
    {
        out += '\"';
        size_t len;
        const char* s = lua_tolstring(L, idx, &len);
        for(size_t i = 0; i < len; i++)
        {
            char c = s[i];
            switch(c)
            {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if((unsigned char)c < 0x20)
                {
                    char esc[8];
                    snprintf(esc, sizeof(esc), "\\u%04x", (unsigned char)c);
                    out += esc;
                }
                else
                    out += c;
                break;
            }
        }
        out += '\"';
        return;
    }

    case LUA_TTABLE:
    {
        out += '{';
        bool first = true;

        // Push the table to the top for lua_next
        lua_pushvalue(L, idx); // [table]

        // Check if array-like: key 1 exists
        lua_pushinteger(L, 1);
        lua_gettable(L, -2);
        bool isArray = !lua_isnil(L, -1);
        lua_pop(L, 1);

        if(isArray)
        {
            // Array: iterate 1, 2, 3, ...
            for(int i = 1; ; i++)
            {
                lua_pushinteger(L, i);
                lua_gettable(L, -2);
                if(lua_isnil(L, -1))
                {
                    lua_pop(L, 1);
                    break;
                }
                if(!first) out += ',';
                first = false;
                table_to_json_recursive(L, lua_gettop(L), out);
                lua_pop(L, 1); // pop value
            }
        }
        else
        {
            // Object: iterate all key-value pairs
            lua_pushnil(L); // [table][nil] — first key
            while(lua_next(L, -2) != 0)
            {
                // [table][key][value]
                if(!first) out += ',';
                first = false;

                // Key (always a string since lua_next with string keys)
                // But could also be number for mixed tables
                int kt = lua_type(L, -2);
                if(kt == LUA_TSTRING)
                {
                    table_to_json_recursive(L, -2, out);
                }
                else if(kt == LUA_TNUMBER)
                {
                    out += '\"';
                    char buf[64];
                    double dk = lua_tonumber(L, -2);
                    if(dk == std::floor(dk) && std::abs(dk) < 1e15)
                        snprintf(buf, sizeof(buf), "%.0f", dk);
                    else
                        snprintf(buf, sizeof(buf), "%.14g", dk);
                    out += buf;
                    out += '\"';
                }
                else
                {
                    // Skip non-string/number keys
                    lua_pop(L, 1);
                    continue;
                }

                out += ':';
                table_to_json_recursive(L, lua_gettop(L), out);
                lua_pop(L, 1); // pop value, keep key for next iteration
            }
        }

        lua_pop(L, 1); // pop table copy
        out += '}';
        return;
    }

    default:
        out += "null";
        return;
    }
}


std::string lua_table_to_json(lua_State* L, int tableIndex)
{
    std::string result;
    if(lua_istable(L, tableIndex))
        table_to_json_recursive(L, tableIndex, result);
    else
        result = "null";
    return result;
}


// ============================================================================
// JSON → Lua table (recursive descent parser)
// ============================================================================

static const char* json_skip_ws(const char* p)
{
    while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    return p;
}

static const char* json_parse_value(lua_State* L, const char* p);

static const char* json_parse_string(lua_State* L, const char* p, std::string& out)
{
    if(*p != '\"') return nullptr;
    p++;
    while(*p && *p != '\"')
    {
        if(*p == '\\')
        {
            p++;
            switch(*p)
            {
            case '\"': out += '\"'; break;
            case '\\': out += '\\'; break;
            case '/':  out += '/';  break;
            case 'n':  out += '\n'; break;
            case 'r':  out += '\r'; break;
            case 't':  out += '\t'; break;
            case 'u':
            {
                char hex[5] = {0};
                for(int i = 0; i < 4 && p[1+i]; i++)
                    hex[i] = p[1+i];
                out += (char)strtol(hex, nullptr, 16);
                p += 4;
                break;
            }
            default:
                out += *p;
                break;
            }
        }
        else
            out += *p;
        p++;
    }
    if(*p != '\"') return nullptr;
    p++;
    return p;
}

static const char* json_parse_object(lua_State* L, const char* p)
{
    // p points past '{'
    lua_newtable(L);
    p = json_skip_ws(p);
    if(*p == '}')
    {
        p++;
        return p;
    }

    while(true)
    {
        p = json_skip_ws(p);
        // Parse key (must be a string)
        std::string key;
        p = json_parse_string(L, p, key);
        if(!p) return nullptr;

        p = json_skip_ws(p);
        if(*p != ':') return nullptr;
        p++;

        p = json_skip_ws(p);
        // Parse value
        p = json_parse_value(L, p);
        if(!p) return nullptr;

        // Set key-value: value is at stack top
        lua_pushstring(L, key.c_str());
        lua_pushvalue(L, -2);
        lua_settable(L, -3);
        lua_pop(L, 1); // pop value

        p = json_skip_ws(p);
        if(*p == ',') { p++; continue; }
        if(*p == '}') { p++; break; }
        return nullptr;
    }

    return p;
}

static const char* json_parse_array(lua_State* L, const char* p)
{
    // p points past '['
    lua_newtable(L);
    int idx = 1;
    p = json_skip_ws(p);
    if(*p == ']')
    {
        p++;
        return p;
    }

    while(true)
    {
        p = json_skip_ws(p);
        p = json_parse_value(L, p);
        if(!p) return nullptr;

        lua_pushinteger(L, idx++);
        lua_pushvalue(L, -2);
        lua_settable(L, -3);
        lua_pop(L, 1);

        p = json_skip_ws(p);
        if(*p == ',') { p++; continue; }
        if(*p == ']') { p++; break; }
        return nullptr;
    }

    return p;
}

static const char* json_parse_value(lua_State* L, const char* p)
{
    p = json_skip_ws(p);

    if(!*p) return nullptr;

    // String
    if(*p == '\"')
    {
        std::string s;
        p = json_parse_string(L, p, s);
        if(!p) return nullptr;
        lua_pushstring(L, s.c_str());
        return p;
    }

    // Number
    if((*p >= '0' && *p <= '9') || *p == '-')
    {
        char* end;
        double d = strtod(p, &end);
        if(end == p) return nullptr;
        lua_pushnumber(L, d);
        return end;
    }

    // true
    if(strncmp(p, "true", 4) == 0)
    {
        lua_pushboolean(L, 1);
        return p + 4;
    }

    // false
    if(strncmp(p, "false", 5) == 0)
    {
        lua_pushboolean(L, 0);
        return p + 5;
    }

    // null
    if(strncmp(p, "null", 4) == 0)
    {
        lua_pushnil(L);
        return p + 4;
    }

    // Object
    if(*p == '{')
    {
        return json_parse_object(L, p + 1);
    }

    // Array (treated as integer-indexed table)
    if(*p == '[')
    {
        return json_parse_array(L, p + 1);
    }

    return nullptr;
}


void json_to_lua_table(lua_State* L, const std::string& json)
{
    const char* p = json.c_str();
    p = json_skip_ws(p);
    if(*p == '{')
        p = json_parse_object(L, p + 1);
    else if(*p == '[')
        p = json_parse_array(L, p + 1);
    else
        lua_newtable(L); // fallback: empty table

    if(!p)
    {
        // Parse error: clear stack completely, push empty table
        lua_settop(L, 0);
        lua_newtable(L);
    }
}
