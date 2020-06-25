//
//  PluginZeroConf.mm
//  TemplateApp
//
//  Copyright (c) 2016 Corona Labs. All rights reserved.
//

#import "PluginZeroConf.h"
#import "Foundation/Foundation.h"

#include "CoronaRuntime.h"
#include "CoronaEvent.h"


#include <sys/socket.h>
//#include <netdb.h>
#include <arpa/inet.h>



NSString * const kDefaultDomain = @"local";
NSString * const kDefaultType = @"_corona._tcp";

// ----------------------------------------------------------------------------

class PluginZeroConf;

@interface CoronaNetServiceDelegateBase : NSObject {
	PluginZeroConf *plugin;
	lua_State *L;
}

-(instancetype)initWithPlugin:(PluginZeroConf*)lib andLuaState:(lua_State*)luaState;
-(void)dispatchService:(NSNetService*)service phase:(const char*)phase errorCode:(NSNumber*)errCode browser:(void*)browser publisher:(void*)pubpisher;

@end


@interface CoronaNetServiceDelegatePublish : CoronaNetServiceDelegateBase < NSNetServiceDelegate >
@end

@interface CoronaNetServiceDelegateBrowser : CoronaNetServiceDelegateBase < NSNetServiceBrowserDelegate >
@end

@interface CoronaNetServiceDelegateResolve : CoronaNetServiceDelegateBase < NSNetServiceDelegate > {
	void *browser;
}
-(instancetype)initWithPlugin:(PluginZeroConf*)lib luaState:(lua_State*)luaState service:(NSNetService*)service andBrowser:(void*)browser;

@property (nonatomic, retain) NSNetService* service;

@end


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
	static int Open( lua_State *L );

protected:
	static int Finalizer( lua_State *L );

public:
	static Self *ToPlugin( lua_State *L );

public:
	static int init( lua_State *L );
	static int publish( lua_State *L );
	static int unpublish( lua_State *L );
	static int unpublishAll( lua_State *L );

	static int browse( lua_State *L );
	static int stopBrowse( lua_State *L );
	static int stopBrowseAll( lua_State *L );

	void stopBrowseAll();
	void unpublishAll();

public:
	NSMutableSet<CoronaNetServiceDelegateResolve*>* ResolvingDelegates();

	NSMutableDictionary<NSValue*, NSNetService*>* Publishers();
	NSMutableDictionary<NSValue*, NSNetServiceBrowser*>* Browsers();

private:

	CoronaNetServiceDelegatePublish* PublishDelegate(lua_State*L);
	CoronaNetServiceDelegatePublish* fPublishDelegate;
	NSMutableDictionary<NSValue*, NSNetService*>* fPublishers;


	CoronaNetServiceDelegateBrowser* BrowserDelegate(lua_State*L);
	CoronaNetServiceDelegateBrowser* fBrowserDelegate;
	NSMutableDictionary<NSValue*, NSNetServiceBrowser*>* fBrowsers;

	NSMutableSet<CoronaNetServiceDelegateResolve*>* fResolvingDelegates;

private:
	CoronaLuaRef fListener;
};

@implementation CoronaNetServiceDelegateBase


- (instancetype)initWithPlugin:(PluginZeroConf*)lib andLuaState:(lua_State*)luaState {
	self = [super init];
	if (self) {
		plugin = lib;
		L = luaState;
	}
	return self;
}



-(void)dispatchService:(NSNetService*)service phase:(const char*)phase errorCode:(NSNumber*)errCode browser:(void*)browser publisher:(void*)pubpisher {
	if(plugin->GetListener())
	{
		dispatch_async(dispatch_get_main_queue(), ^(void) {
			if(!plugin->GetListener())
				return;

			CoronaLuaNewEvent( L, PluginZeroConf::kEvent);

			lua_pushboolean(L, errCode!=nil);
			lua_setfield(L, -2, CoronaEventIsErrorKey());

			lua_pushstring( L, phase);
			lua_setfield(L, -2, CoronaEventPhaseKey());

			if (errCode)
			{
			   lua_pushinteger(L, [errCode intValue]);
			   lua_setfield(L, -2, CoronaEventErrorCodeKey());
			}

			if (browser)
			{
				lua_pushlightuserdata(L, browser);
				lua_setfield(L, -2, "browser");
			}

			if (pubpisher)
			{
				lua_pushlightuserdata(L, pubpisher);
				lua_setfield(L, -2, "publisher");
			}

			if(service)
			{
				if(service.name)
				{
					lua_pushstring(L, [service.name UTF8String]);
					lua_setfield(L, -2, "serviceName");
				}

				if(service.type)
				{
					lua_pushstring(L, [service.type UTF8String]);
					lua_setfield(L, -2, "type");
				}

				if(service.port != -1)
				{
					lua_pushinteger(L, service.port);
					lua_setfield(L, -2, "port");
				}

				if (service.hostName)
				{
					lua_pushstring(L, [service.hostName UTF8String]);
					lua_setfield(L, -2, "hostname");
				}

				if (service.addresses)
				{
					lua_createtable(L, (int)service.addresses.count, 0);
					int index = 1;
					for (NSData* addr in service.addresses) {
						const struct sockaddr * sa = (const struct sockaddr *)[addr bytes];
						char buff[INET6_ADDRSTRLEN];
						switch (sa->sa_family)
						{
							case AF_INET:
								inet_ntop(AF_INET, &(((sockaddr_in*)sa)->sin_addr), buff, INET6_ADDRSTRLEN);
								break;
							case AF_INET6:
								inet_ntop(AF_INET6, &(((sockaddr_in6*)sa)->sin6_addr), buff, INET6_ADDRSTRLEN);
								break;
							default:
								buff[0] = 0;
						}
						if(buff[0])
						{
							lua_pushstring(L, buff);
							lua_rawseti(L, -2, index++);
						}
					}
					lua_setfield(L, -2, "addresses");
				}

				if(service.TXTRecordData)
				{
					NSDictionary * data = [NSNetService dictionaryFromTXTRecordData:service.TXTRecordData];
					lua_createtable(L, 0, (int)data.count);
					for (NSString*k in data) {
						NSData *v = [data objectForKey:k];
						lua_pushlstring(L, (const char*)v.bytes, v.length);
						lua_setfield(L, -2, [k UTF8String]);
					}
					lua_setfield(L, -2, "data");
				}
			}

			CoronaLuaDispatchEvent(L, plugin->GetListener(), 0);
		});
	}
}

@end

@implementation CoronaNetServiceDelegatePublish

-(void)netServiceDidPublish:(NSNetService *)sender {
	[self dispatchService:sender phase:"published" errorCode:nil browser:nil publisher:sender];
}

-(void)netService:(NSNetService *)sender didNotPublish:(NSDictionary<NSString *,NSNumber *> *)errorDict {
	[self dispatchService:sender phase:"published" errorCode:[errorDict objectForKey:NSNetServicesErrorCode] browser:nil publisher:sender];

	[sender setDelegate:nil];
	[sender stop];
	[plugin->Publishers() removeObjectForKey:[NSValue valueWithPointer:sender]];
}

@end

@implementation CoronaNetServiceDelegateBrowser


-(void)netServiceBrowser:(NSNetServiceBrowser *)browser didNotSearch:(NSDictionary<NSString *,NSNumber *> *)errorDict {
	[self dispatchService:nil phase:"browseError" errorCode:[errorDict objectForKey:NSNetServicesErrorCode] browser:nil publisher:nil];

	[browser setDelegate:nil];
	[browser stop];
	[plugin->Browsers() removeObjectForKey:[NSValue valueWithPointer:browser]];
}

-(void)netServiceBrowser:(NSNetServiceBrowser *)browser didRemoveService:(NSNetService *)service moreComing:(BOOL)moreComing {
	[self dispatchService:service phase:"lost" errorCode:nil browser:browser publisher:nil];
}

-(void)netServiceBrowser:(NSNetServiceBrowser *)browser didFindService:(NSNetService *)service moreComing:(BOOL)moreComing {
	service.delegate = [[CoronaNetServiceDelegateResolve alloc] initWithPlugin:plugin luaState:L service:service andBrowser:browser];
	[service resolveWithTimeout:5.0];
}

@end


@implementation CoronaNetServiceDelegateResolve

-(instancetype)initWithPlugin:(PluginZeroConf*)lib luaState:(lua_State*)luaState service:(NSNetService*)service andBrowser:(void*)_browser
{
	self = [super initWithPlugin:lib andLuaState:luaState];
	if (self) {
		self.service = service;
		browser = _browser;
		[plugin->ResolvingDelegates() addObject:self];
	}
	return self;
}

-(void)finishAndDeregister:(BOOL)dereg
{
	if(dereg)
	{
		[plugin->ResolvingDelegates() removeObject:self];
	}
	self.service.delegate = nil;
	[self.service stop];
	[self release];
}

-(void)netServiceDidResolveAddress:(NSNetService *)sender {
	[self dispatchService:sender phase:"found" errorCode:nil browser:browser publisher:nil];
	[self finishAndDeregister:YES];
}

-(void)netService:(NSNetService *)sender didNotResolve:(NSDictionary<NSString *,NSNumber *> *)errorDict {
	NSString *err = [NSString stringWithFormat:@"Error while resolving service %@: %@", sender.name, [errorDict objectForKey:NSNetServicesErrorCode]];
	CoronaLog("%s", [err UTF8String]);
	[self finishAndDeregister:YES];
}

@end

// ----------------------------------------------------------------------------

const char PluginZeroConf::kName[] = "plugin.zeroconf";

// This corresponds to the event name, e.g. [Lua] event.name
const char PluginZeroConf::kEvent[] = "PluginZeroConfEvent";

PluginZeroConf::PluginZeroConf()
:	fListener( NULL )
,	fPublishDelegate( nil )
,	fBrowserDelegate( nil )
,	fPublishers( nil )
,	fBrowsers( nil )
,	fResolvingDelegates( nil )
{
}

PluginZeroConf::~PluginZeroConf()
{
	unpublishAll();
	[fPublishDelegate release];

	stopBrowseAll();
	[fBrowserDelegate release];

	[fPublishers release];
	[fBrowsers release];

	[fResolvingDelegates enumerateObjectsUsingBlock:^(CoronaNetServiceDelegateResolve *r, BOOL *stop) {
		[r finishAndDeregister:NO];
	}];
	[fResolvingDelegates release];
}


CoronaNetServiceDelegatePublish*
PluginZeroConf::PublishDelegate(lua_State*L)
{
	if(fPublishDelegate == nil)
	{
		fPublishDelegate = [[CoronaNetServiceDelegatePublish alloc] initWithPlugin:this andLuaState:L];
	}
	return fPublishDelegate;
}

NSMutableDictionary<NSValue*, NSNetService*>*
PluginZeroConf::Publishers()
{
	if(fPublishers == nil)
	{
		fPublishers = [[NSMutableDictionary alloc] init];
	}
	return fPublishers;
}

CoronaNetServiceDelegateBrowser*
PluginZeroConf::BrowserDelegate(lua_State*L)
{
	if(fBrowserDelegate == nil)
	{
		fBrowserDelegate = [[CoronaNetServiceDelegateBrowser alloc] initWithPlugin:this andLuaState:L];
	}
	return fBrowserDelegate;
}

NSMutableDictionary<NSValue*, NSNetServiceBrowser*>*
PluginZeroConf::Browsers()
{
	if(fBrowsers == nil)
	{
		fBrowsers = [[NSMutableDictionary alloc] init];
	}
	return fBrowsers;
}

NSMutableSet<CoronaNetServiceDelegateResolve*>*
PluginZeroConf::ResolvingDelegates()
{
	if(fResolvingDelegates == nil)
	{
		fResolvingDelegates = [[NSMutableSet alloc] init];
	}
	return fResolvingDelegates;
}



int
PluginZeroConf::Open( lua_State *L )
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
PluginZeroConf::Finalizer( lua_State *L )
{
	Self *plugin = (Self *)CoronaLuaToUserdata( L, 1 );

	CoronaLuaDeleteRef( L, plugin->GetListener() );
	plugin->fListener = NULL;

	delete plugin;
	return 0;
}

PluginZeroConf *
PluginZeroConf::ToPlugin( lua_State *L )
{
	// plugin is pushed as part of the closure
	Self *plugin = (Self *)CoronaLuaToUserdata( L, lua_upvalueindex( 1 ) );
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
	PluginZeroConf *plugin = ToPlugin(L);

	NSString *name = @"";
	NSString *type = kDefaultType;
	NSString *domain = kDefaultDomain;
	NSNumber *port = nil;
	NSNetService * service = nil;
	NSData* data = nil;

	if(lua_istable(L, 1))
	{
		lua_getfield(L, idx, "name");
		if( lua_type(L, -1) == LUA_TSTRING )
		{
			name = [NSString stringWithUTF8String:lua_tostring(L, -1)];
		}
		lua_pop(L, 1);

		lua_getfield(L, idx, "serviceName");
		if( lua_type(L, -1) == LUA_TSTRING )
		{
			name = [NSString stringWithUTF8String:lua_tostring(L, -1)];
		}
		lua_pop(L, 1);


		lua_getfield(L, idx, "type");
		if( lua_type(L, -1) == LUA_TSTRING )
		{
			type = [NSString stringWithUTF8String:lua_tostring(L, -1)];
		}
		lua_pop(L, 1);

		lua_getfield(L, idx, "domain");
		if( lua_type(L, -1) == LUA_TSTRING )
		{
			domain = [NSString stringWithUTF8String:lua_tostring(L, -1)];
		}
		lua_pop(L, 1);

		lua_getfield(L, -1, "data");
		if(lua_istable(L, -1))
		{
			NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithCapacity:0];
			lua_pushnil(L);
			while (lua_next(L, -2) != 0) {
				if(lua_type(L, -2) == LUA_TSTRING && lua_isstring(L, -1))
				{
					NSString *key = [NSString stringWithUTF8String:lua_tostring(L, -2)];
					size_t len=0;
					const char* szdata = lua_tolstring(L, -1, &len);
					NSData* value = [NSData dataWithBytes:szdata length:len];
					[dict setObject:value forKey:key];
				}
				lua_pop(L, 1);
			}
			data = [NSNetService dataFromTXTRecordDictionary:dict];
		}
		lua_pop(L, 1);

		lua_getfield(L, idx, "port");
		if( lua_type(L, -1) == LUA_TNUMBER )
		{
			port = [NSNumber numberWithInt:(int)lua_tointeger(L, -1)];
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

	if(port)
	{
		service = [[NSNetService alloc] initWithDomain:domain type:type name:name port:port.intValue];
		service.TXTRecordData = data;
	}

	if(service)
	{
		[service setDelegate:plugin->PublishDelegate(L)];

		NSValue *key = [NSValue valueWithPointer:service];
		[plugin->Publishers() setObject:service forKey:key];

		[service publish];

		lua_pushlightuserdata(L, (void*)service);
		[service release];
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
	PluginZeroConf *plugin = ToPlugin(L);
	NSNetService *service = nil;

	NSValue *key = nil;
	if(lua_type(L, idx) == LUA_TLIGHTUSERDATA)
	{
		id v = (id)lua_touserdata(L, idx);
		key = [NSValue valueWithPointer:v];
		service = [plugin->Publishers() objectForKey:key];
	}
	else
	{
		CoronaLuaError(L, "zeroconf.unpublish(): did not receive publised service as first parameter");
	}


	if(service)
	{
		[service stop];
		[plugin->Publishers() removeObjectForKey:key];
	}
	else
	{
		CoronaLuaWarning(L, "zeroconf.unpublish(): unable to find specified service!" );
	}

	return 0;
}

void
PluginZeroConf::unpublishAll()
{
	[Publishers() enumerateKeysAndObjectsUsingBlock:^(NSValue * key, NSNetService * obj, BOOL * stop) {
		[obj stop];
	}];
	[Publishers() removeAllObjects];
}

int
PluginZeroConf::unpublishAll( lua_State *L )
{
	ToPlugin(L)->unpublishAll();
	return 0;
}

// [Lua] zeroconf.browse( params )
int
PluginZeroConf::browse( lua_State *L )
{
	int idx = 1;
	PluginZeroConf *plugin = ToPlugin(L);

	NSString *type = kDefaultType;
	NSString *domain = kDefaultDomain;


	if(lua_istable(L, 1))
	{
		lua_getfield(L, idx, "type");
		if( lua_type(L, -1) == LUA_TSTRING )
		{
			type = [NSString stringWithUTF8String:lua_tostring(L, -1)];
		}
		lua_pop(L, 1);

		lua_getfield(L, idx, "domain");
		if( lua_type(L, -1) == LUA_TSTRING )
		{
			domain = [NSString stringWithUTF8String:lua_tostring(L, -1)];
		}
		lua_pop(L, 1);
	}


	NSNetServiceBrowser* browser = [[NSNetServiceBrowser alloc] init];
	[browser setDelegate:plugin->BrowserDelegate(L)];

	NSValue *key = [NSValue valueWithPointer:browser];
	[plugin->Browsers() setObject:browser forKey:key];

	[browser searchForServicesOfType:type inDomain:domain];

	lua_pushlightuserdata(L, (void*)browser);
	[browser release];

	return 1;
}


int
PluginZeroConf::stopBrowse( lua_State *L )
{
	int idx = 1;
	PluginZeroConf *plugin = ToPlugin(L);
	NSNetServiceBrowser *browser = nil;

	NSValue *key = nil;
	if(lua_type(L, idx) == LUA_TLIGHTUSERDATA)
	{
		id v = (id)lua_touserdata(L, idx);
		key = [NSValue valueWithPointer:v];
		browser = [plugin->Browsers() objectForKey:key];
	}
	else
	{
		CoronaLuaError(L, "zeroconf.stopBrowse(): did not receive browser id as a parameter");
	}

	if(browser)
	{
		[browser stop];
		[plugin->Browsers() removeObjectForKey:key];
	}
	else
	{
		CoronaLuaWarning(L, "zeroconf.stopBrowse(): unable to find specified browser!" );
	}

	return 0;
}

void
PluginZeroConf::stopBrowseAll()
{
	[Browsers() enumerateKeysAndObjectsUsingBlock:^(NSValue * key, NSNetServiceBrowser * obj, BOOL * stop) {
		[obj stop];
	}];
	[Browsers()  removeAllObjects];
}

int
PluginZeroConf::stopBrowseAll( lua_State *L )
{
	ToPlugin(L)->stopBrowseAll();
	return 0;
}


// ----------------------------------------------------------------------------

CORONA_EXPORT int luaopen_plugin_zeroconf( lua_State *L )
{
	return PluginZeroConf::Open( L );
}
