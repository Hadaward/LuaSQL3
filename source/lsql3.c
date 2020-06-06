/*
* lsql3.c v1.0
* A sqlite3 library specially developed for Lua5.1 that brings good performance and easy connection to local databases.
* Eduardo Gimenez <eduardo.gimenez07@gmail.com>
* 13 Mai 2020 20:36
*/

// Includes
#include <sqlite3.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h> 
#include <stdio.h>
#include <time.h>

#include "lua.h"
#include "lauxlib.h"

// Defss
#define uint unsigned int

// Lua 5.1 compatibility
#if LUA_VERSION_NUM < 502
// From Lua 5.3 lauxlib.c
LUALIB_API void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
  luaL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    int i;
    for (i = 0; i < nup; i++)  /* copy upvalues to the top */
      lua_pushvalue(L, -nup);
    lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    lua_setfield(L, -(nup + 2), l->name);
  }
  lua_pop(L, nup);  /* remove upvalues */
}
LUALIB_API void *luaL_testudata (lua_State *L, int ud, const char *tname) {
  void *p = lua_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (lua_getmetatable(L, ud)) {  /* does it have a metatable? */
      luaL_getmetatable(L, tname);  /* get correct metatable */
      if (!lua_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      lua_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}
#endif

// lsql3_asudata get any value from stack as userdata
static sqlite3 *lsql3_asudata(lua_State *l, int ud) {
	if (lua_islightuserdata(l, ud)) {
		return lua_touserdata(l, ud);
	}
	return (sqlite3 *)luaL_checkudata(l, ud, "sqlite3");
}
// lsql3.close(); > close sqlite database
static int lsql3_close(lua_State *L) {
	lua_getfield(L, 1, "__CONNECTION");
	sqlite3 *db = (sqlite3 *)lsql3_asudata(L, -1);
	sqlite3_close(db);
	return 0;
}

// lsql3.database.statemente:close() > finalize statement
static int lsql3_STMT_close(lua_State *L) {
	lua_getfield(L, 1, "__STATEMENT");
	sqlite3_stmt *stmt = (sqlite3_stmt *)lsql3_asudata(L, -1);
	sqlite3_finalize(stmt);
	return 0;
}

// lsql3.database.statement:fetchall() > return all remaining values on stack
static int lsql3_STMT_fetchall(lua_State *L) {
	lua_getfield(L, 1, "__STATEMENT");
	sqlite3_stmt *stmt = (sqlite3_stmt *)lsql3_asudata(L, -1);
	
	lua_createtable(L, 0, 0);
	
	int index = 0;
	
	while (sqlite3_step(stmt) != SQLITE_DONE) {
		index++;
		int i;
		int num_cols = sqlite3_column_count(stmt);
		
		lua_createtable(L, 0, 0);
		
		for (i = 0; i < num_cols; i++)
		{
			lua_pushinteger(L, i+1);
			switch (sqlite3_column_type(stmt, i))
			{
			case (SQLITE3_TEXT):
				lua_pushstring(L, sqlite3_column_text(stmt, i));
				break;
			case (SQLITE_INTEGER):
				lua_pushinteger(L, sqlite3_column_int(stmt, i));
				break;
			case (SQLITE_FLOAT):
				lua_pushnumber(L, sqlite3_column_double(stmt, i));
				break;
			default:
				break;
			}
			lua_settable(L, -3);
		}
		
		lua_pushinteger(L, index);
		lua_insert(L, -2);
		lua_settable(L, -3);
	}
	
	return 1;
}

// lsql3.database.statement:fetchone() > return the first value on stack
static int lsql3_STMT_fetchone(lua_State *L) {
	lua_getfield(L, 1, "__STATEMENT");
	sqlite3_stmt *stmt = (sqlite3_stmt *)lsql3_asudata(L, -1);
	
	lua_createtable(L, 0, 0);

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		int i;
		int num_cols = sqlite3_column_count(stmt);
		
		lua_createtable(L, 0, 0);
		
		for (i = 0; i < num_cols; i++)
		{
			lua_pushinteger(L, i+1);
			switch (sqlite3_column_type(stmt, i))
			{
			case (SQLITE3_TEXT):
				lua_pushstring(L, sqlite3_column_text(stmt, i));
				break;
			case (SQLITE_INTEGER):
				lua_pushinteger(L, sqlite3_column_int(stmt, i));
				break;
			case (SQLITE_FLOAT):
				lua_pushnumber(L, sqlite3_column_double(stmt, i));
				break;
			default:
				break;
			}
			lua_settable(L, -3);
		}
		
		lua_pushinteger(L, 1);
		lua_insert(L, -2);
		lua_settable(L, -3);
	}
	
	return 1;
}

// lsql3.database:execute(string) > Execute directly any statement
static int lsql3_execute(lua_State *L) {
	lua_getfield(L, 1, "__CONNECTION");
	sqlite3 *db = (sqlite3 *)lsql3_asudata(L, -1);
	const char *sql = luaL_checkstring(L, 2);
	
	char *err_msg = 0;
	
	int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
	
	if (rc != SQLITE_OK ) {
		if(err_msg == NULL) {
			err_msg = "Database was closed!";
		}
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
    } 
	
	return 0;
}

// lsql3.database:prepare(string) > Prepare a SQL Statement to fetch values
static int lsql3_prepare(lua_State *L) {
	lua_getfield(L, 1, "__CONNECTION");
	sqlite3 *db = (sqlite3 *)lsql3_asudata(L, -1);
	const char *sql = luaL_checkstring(L, 2);
	
	sqlite3_stmt *stmt;
	
	sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	
	luaL_newmetatable(L, "SQLStatement");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	
	lua_pushstring(L, "__STATEMENT");
	lua_pushlightuserdata(L, stmt);
	lua_settable(L, -3);
	
	lua_pushstring(L, "fetchone");
	lua_pushcfunction(L, lsql3_STMT_fetchone);
	lua_settable(L, -3);
	
	lua_pushstring(L, "fetchall");
	lua_pushcfunction(L, lsql3_STMT_fetchall);
	lua_settable(L, -3);
	
	lua_pushstring(L, "close");
	lua_pushcfunction(L, lsql3_STMT_close);
	lua_settable(L, -3);
	
	return 1;
}

// lsql3.connect(string) > Connect to a file or memory database
static int lsql3_connect(lua_State *L) {
    sqlite3 *db;
	
	const char *filepath = luaL_checkstring(L, 1);
	
    int rc = sqlite3_open(filepath, &db);
	
	 if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
    } else {
		luaL_newmetatable(L, "SQLConnection");
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
		
		lua_pushstring(L, "__CONNECTION");
		lua_pushlightuserdata(L, db);
		lua_settable(L, -3);
		
		lua_pushstring(L, "close");
		lua_pushcfunction(L, lsql3_close);
		lua_settable(L, -3);
		
		lua_pushstring(L, "execute");
		lua_pushcfunction(L, lsql3_execute);
		lua_settable(L, -3);
		
		lua_pushstring(L, "prepare");
		lua_pushcfunction(L, lsql3_prepare);
		lua_settable(L, -3);
	}
	
	return 1;
}

// Garbage collector
static int lsql3_gc(lua_State *l) {
	return 0;
}

__declspec(dllexport)
LUALIB_API int luaopen_lsql3(lua_State *L)
{
	luaL_newmetatable(L, "lsql3");
	lua_pushstring(L, "__gc");
	lua_pushcfunction(L, lsql3_gc);
	lua_settable(L, -3);

	luaL_Reg reg[] = {
		{"connect", lsql3_connect},
		{ NULL, NULL }
	};
	lua_newtable(L);
	luaL_setfuncs(L, reg, 0);
	lua_pushliteral(L, "Lua SQLite3");
	lua_setfield(L, -2, "_NAME");
	lua_pushliteral(L, "1.0");
	lua_setfield(L, -2, "_VERSION");
	return 1;
}
