#ifndef LUA_MAIN_HHH
#define LUA_MAIN_HHH

extern bool xtech_lua_init();
extern bool xtech_lua_quit();

extern void xtech_lua_load();
extern void xtech_lua_loop();
extern void xtech_lua_reset();
extern void xtech_lua_renderStart();
extern void xtech_lua_renderEnd();
extern void xtech_lua_render(int screenZ);
extern void xtech_lua_renderHud(int screenZ, int numScreens);

struct lua_State;
extern lua_State* xtech_lua_getState();

#endif // LUA_MAIN_HHH
