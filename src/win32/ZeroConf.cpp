//
//  ZeroConf.cpp
//  ZeroConf plugin for Corona
//
//  Created by Vlad Shcherban on 2016-08-18.
//  Copyright © 2016 Corona Labs. All rights reserved.
//


#include "CoronaAssert.h"
#include "ZeroConf.h"
#include "CoronaEvent.h"

#include "DnsWrapper.h"

// ----------------------------------------------------------------------------


class PluginZeroConf
{
public:
	typedef PluginZeroConf Self;

public:
	static const char kName[];
	static const char kEvent[];

protected:
	PluginZeroConf();
	~PluginZeroConf();

public:
	CoronaLuaRef GetListener() const { return fListener; }

public:

public:
	static int Open(lua_State *L);

protected:
	static int Finalizer(lua_State *L);

public:
	static Self *ToPlugin(lua_State *L);
	static DNSServiceManager *ToManager(lua_State *L);

public:
	static int init(lua_State *L);
	static int publish(lua_State *L);
	static int unpublish(lua_State *L);
	static int unpublishAll(lua_State *L);

	static int browse(lua_State *L);
	static int stopBrowse(lua_State *L);
	static int stopBrowseAll(lua_State *L);

private:
	DNSServiceManager *Manager(lua_State *L);

private:
	CoronaLuaRef fListener;
	DSNMessageBusBase *fMessanger;
	DNSServiceManager *fManager;
};

class LuaMessenger : public DSNMessageBusBase
{
	lua_State *L;
	PluginZeroConf *plugin;
public:
	LuaMessenger(lua_State *L, PluginZeroConf *plugin);
	virtual void Message(const ServiceInfo &srv, int errorCode, const char* phase) override;
	virtual ~LuaMessenger();
};

// ----------------------------------------------------------------------------

const char PluginZeroConf::kName[] = "plugin.zeroconf";
const char PluginZeroConf::kEvent[] = "PluginZeroConfEvent";

LuaMessenger::LuaMessenger(lua_State *L, PluginZeroConf *plugin)
: plugin(plugin)
, L(L)
{

}

void LuaMessenger::Message(const ServiceInfo &info, int errorCode, const char* phase)
{
	if(plugin->GetListener())
	{
		CoronaLuaNewEvent( L, PluginZeroConf::kEvent);

		lua_pushboolean(L, errorCode!=0);
		lua_setfield(L, -2, CoronaEventIsErrorKey());

		lua_pushstring( L, phase);
		lua_setfield(L, -2, CoronaEventPhaseKey());

		if (errorCode)
		{
			lua_pushinteger(L, errorCode);
			lua_setfield(L, -2, CoronaEventErrorCodeKey());
		}

		if (info.browser)
		{
			lua_pushlightuserdata(L, info.browser);
			lua_setfield(L, -2, "browser");
		}

		if (info.publisher)
		{
			lua_pushlightuserdata(L, info.publisher);
			lua_setfield(L, -2, "publisher");
		}

		if(info.name.length())
		{
			lua_pushstring(L, info.name.c_str());
			lua_setfield(L, -2, "serviceName");
		}

		if(info.type.length())
		{
			lua_pushstring(L, info.name.c_str());
			lua_setfield(L, -2, "type");
		}

		if(info.port != -1)
		{
			lua_pushinteger(L, info.port);
			lua_setfield(L, -2, "port");
		}

		if (info.hostname.length())
		{
			lua_pushstring(L, info.hostname.c_str());
			lua_setfield(L, -2, "hostname");
		}

		lua_createtable(L, (int)info.addresses.size(), 0);
		int index = 1;
		for(auto &addr : info.addresses)
		{
			lua_pushstring(L, addr.c_str());
			lua_rawseti(L, -2, index++);
		}
		lua_setfield(L, -2, "addresses");

		lua_createtable(L, 0, (int)info.data.size());
		for(auto &dataEntry : info.data)
		{
			lua_pushstring(L, dataEntry.second.c_str());
			lua_setfield(L, -2, dataEntry.first.c_str());
		}
		lua_setfield(L, -2, "data");

		CoronaLuaDispatchEvent(L, plugin->GetListener(), 0);
	}
}

LuaMessenger::~LuaMessenger()
{

}



PluginZeroConf::PluginZeroConf()
: fListener(NULL)
, fMessanger(nullptr)
, fManager(nullptr)
{
}

PluginZeroConf::~PluginZeroConf()
{
	delete fManager;
	delete fMessanger;
}

DNSServiceManager *
PluginZeroConf::Manager(lua_State *L)
{
	if(fMessanger == nullptr)
		fMessanger = new LuaMessenger(L, this);
	if(fManager == nullptr)
		fManager = new DNSServiceManager(fMessanger);

	return fManager;
}

DNSServiceManager *
PluginZeroConf::ToManager(lua_State *L)
{
	return ToPlugin(L)->Manager(L);
}



int
PluginZeroConf::Open(lua_State *L)
{
	// Register __gc callback
	const char kMetatableName[] = __FILE__; // Globally unique string to prevent collision
	CoronaLuaInitializeGCMetatable( L, kMetatableName, Finalizer );

	const luaL_Reg kVTable[] =
	{
		{ "init", init },

		{ "publish", publish },
		{ "unpublish", unpublish },
		{ "unpublishAll", unpublishAll },

		{ "browse", browse },
		{ "stopBrowse", stopBrowse },
		{ "stopBrowseAll", stopBrowseAll },

		{ NULL, NULL }
	};


	CoronaLuaPushUserdata( L, new Self, kMetatableName );

	luaL_openlib( L, kName, kVTable, 1 ); // leave Self on top of stack

	return 1;
}

int
PluginZeroConf::Finalizer(lua_State *L)
{
	Self *plugin = (Self *)CoronaLuaToUserdata(L, 1);

	CoronaLuaDeleteRef(L, plugin->GetListener());

	delete plugin;
	return 0;
}

PluginZeroConf *
PluginZeroConf::ToPlugin(lua_State *L)
{
	// plugin is pushed as part of the closure
	Self *plugin = (Self *)CoronaLuaToUserdata(L, lua_upvalueindex(1));
	return plugin;
}


int
PluginZeroConf::init( lua_State *L )
{
	int listenerIndex = 1;

	if ( CoronaLuaIsListener( L, listenerIndex, kEvent ) )
	{
		Self *plugin = ToPlugin( L );
		CoronaLuaDeleteRef(L, plugin->fListener);
		plugin->fListener = CoronaLuaNewRef( L, listenerIndex );
	}

	return 0;
}

int
PluginZeroConf::publish( lua_State *L )
{
	int idx = 1;

	ServiceInfo si;

	if(lua_istable(L, 1))
	{
		lua_getfield(L, idx, "name");
		if( lua_type(L, -1) == LUA_TSTRING )
		{
			si.name = lua_tostring(L, -1);
		}
		lua_pop(L, 1);

		lua_getfield(L, idx, "serviceName");
		if( lua_type(L, -1) == LUA_TSTRING )
		{
			si.name = lua_tostring(L, -1);
		}
		lua_pop(L, 1);


		lua_getfield(L, idx, "type");
		if( lua_type(L, -1) == LUA_TSTRING )
		{
			si.type = lua_tostring(L, -1);
		}
		lua_pop(L, 1);

		lua_getfield(L, idx, "domain");
		if( lua_type(L, -1) == LUA_TSTRING )
		{
			si.domain = lua_tostring(L, -1);
		}
		lua_pop(L, 1);

		lua_getfield(L, -1, "data");
		if(lua_istable(L, -1))
		{
			lua_pushnil(L);
			while (lua_next(L, -2) != 0) {
				if(lua_type(L, -2) == LUA_TSTRING && lua_isstring(L, -1))
				{
					const char* key = lua_tostring(L, -2);
					const char* value = lua_tostring(L, -1);
					si.setData(key, value);
				}
				lua_pop(L, 1);
			}
		}
		lua_pop(L, 1);

		lua_getfield(L, idx, "port");
		if( lua_type(L, -1) == LUA_TNUMBER )
		{
			si.port = (int)lua_tointeger(L, -1);
		}
		else
		{
			CoronaLuaError(L, "zeroconf.publish(): parameters table does not contain 'port' field");
		}
		lua_pop(L, 1);
	}
	else
	{
		CoronaLuaError(L, "zeroconf.publish(): did not receive parameters table" );
	}

	PublisherHandle publisher = ToManager(L)->publish(si);
	
	if(publisher)
	{
		lua_pushlightuserdata(L, publisher);
	}
	else
	{
		CoronaLuaWarning(L, "zeroconf.publish(): failed to create service!" );
		lua_pushnil( L );
	}

	return 1;
}


int
PluginZeroConf::unpublish( lua_State *L )
{
	int idx = 1;
	PublisherHandle publisher = nullptr;

	if(lua_type(L, idx) == LUA_TLIGHTUSERDATA)
	{
		publisher = lua_touserdata(L, -1);
	}
	else
	{
		CoronaLuaError(L, "zeroconf.unpublish(): did not receive publised service as first parameter");
	}

	if(!ToManager(L)->unpublish(publisher))
	{
		CoronaLuaWarning(L, "zeroconf.unpublish(): unable to find specified service!" );
	}

	return 0;
}


int
PluginZeroConf::unpublishAll( lua_State *L )
{
	ToManager(L)->unpublishAll();
	return 0;
}

// [Lua] zeroconf.browse( params )
int
PluginZeroConf::browse( lua_State *L )
{
	int idx = 1;

	ServiceInfo si;

	if(lua_istable(L, 1))
	{
		lua_getfield(L, idx, "type");
		if( lua_type(L, -1) == LUA_TSTRING )
		{
			si.type = lua_tostring(L, -1);
		}
		lua_pop(L, 1);

		lua_getfield(L, idx, "domain");
		if( lua_type(L, -1) == LUA_TSTRING )
		{
			si.domain = lua_tostring(L, -1);
		}
		lua_pop(L, 1);
	}

	ToManager(L)->browse(si);

	return 1;
}


int
PluginZeroConf::stopBrowse( lua_State *L )
{
	int idx = 1;
	PublisherHandle browser = nullptr;

	if(lua_type(L, idx) == LUA_TLIGHTUSERDATA)
	{
		browser = lua_touserdata(L, -1);
	}
	else
	{
		CoronaLuaError(L, "zeroconf.stopBrowse(): did not receive browser type as first parameter");
	}

	if(!ToManager(L)->stopBrowser(browser))
	{
		CoronaLuaWarning(L, "zeroconf.stopBrowse(): unable to find specified browser!" );
	}


	return 0;
}

int
PluginZeroConf::stopBrowseAll( lua_State *L )
{
	ToManager(L)->stopAllBrowsers();
	return 0;
}



// ----------------------------------------------------------------------------

CORONA_EXPORT int luaopen_plugin_zeroconf(lua_State *L)
{
	return PluginZeroConf::Open(L);
}
