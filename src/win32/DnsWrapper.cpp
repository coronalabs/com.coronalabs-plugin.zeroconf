//
//  DnsWrapper.cpp
//  ZeroConf plugin for Corona
//
//  Created by Vlad Shcherban on 2016-08-18.
//  Copyright © 2016 Corona Labs. All rights reserved.
//

#include "DnsWrapper.h"

#if _WINDOWS
	#include <Winsock2.h>
	#include <ws2ipdef.h>
	#include <WS2tcpip.h>
	#pragma comment(lib, "Ws2_32.lib")
	#pragma comment(lib, "dnssdStatic.lib")
#else
	#include <arpa/inet.h>
#endif

#include <dns_sd.h>

#ifndef _WINDOWS

class MacEventLoop : public BaseDNSEventLoop
{
public:
	virtual void RegisterRef(DNSServiceRef ref) override
	{
		DNSServiceSetDispatchQueue(ref, dispatch_get_main_queue());
	}

	virtual void TerminateRef(DNSServiceRef ref) override
	{
		DNSServiceRefDeallocate(ref);
	}
	virtual ~MacEventLoop(){}
};

typedef MacEventLoop PlatformEventLoop;

#else
#include "DNSWindowsEventLoop.h"

typedef DNSWindowsEventLoop PlatformEventLoop;

#endif




using namespace std;

class ServiceBrowser
{
public:
	typedef ServiceBrowser Self;

	list< shared_ptr<ServiceInfo> > resolving;

	string type;
	string domain;
	DSNMessageBusBase *bus;
	DNSServiceManager *owner;

	DNSServiceRef browserRef;

	bool browse();
	ServiceBrowser(DSNMessageBusBase *bus, DNSServiceManager *owner);
	void stop();

	static void DNSSD_API callbackBrowse(DNSServiceRef sdRef,
								   DNSServiceFlags flags,
								   uint32_t interfaceIndex,
								   DNSServiceErrorType errorCode,
								   const char *serviceName,
								   const char *regtype,
								   const char *replyDomain,
								   void *context
								   );


	static void DNSSD_API callbackResolve(DNSServiceRef sdRef,
										  DNSServiceFlags flags,
										  uint32_t interfaceIndex,
										  DNSServiceErrorType errorCode,
										  const char *fullname,
										  const char *hosttarget,
										  uint16_t port,
										  uint16_t txtLen,
										  const unsigned char *txtRecord,
										  void *context
										  );

	static void DNSSD_API callbackAddr(DNSServiceRef sdRef,
									   DNSServiceFlags flags,
									   uint32_t interfaceIndex,
									   DNSServiceErrorType errorCode,
									   const char *hostname,
									   const struct sockaddr *address,
									   uint32_t ttl,
									   void *context
									   );

};

class ServicePublisher
{

public:

	typedef ServicePublisher Self;

	DSNMessageBusBase *bus;
	ServiceInfo info;
	DNSServiceManager *owner;

	ServicePublisher(DSNMessageBusBase* bus, DNSServiceManager *owner);

	bool publish();
	void unpublish();

	static void DNSSD_API callbackRegister(DNSServiceRef sdRef,
										   DNSServiceFlags flags,
										   DNSServiceErrorType errorCode,
										   const char *name,
										   const char *regtype,
										   const char *domain,
										   void *context);
};


const char *ServiceInfo::kDefaultType = "_corona._tcp";
const char *ServiceInfo::kDefaultDomain = "local";

ServiceInfo::ServiceInfo()
: port(-1)
, ref(0)
, domain(kDefaultDomain)
, type(kDefaultType)
, browser(nullptr)
, publisher(nullptr)
{

}


void ServiceInfo::setData(const char *key, const char *value)
{
	if(strlen(key) > 0)
	{
		data[key] = value;
	}
}

vector<uint8_t> ServiceInfo::TXTData() const
{

	vector<uint8_t> ret;
	if (data.size() > 0)
	{

		int txtLen = 0;
		for(const auto &k: data)
			txtLen += k.first.length() + k.second.size() + 2;

		ret.reserve(txtLen);

		for(const auto &k: data)
		{
			if(k.second.length()>0)
			{
				size_t l = k.first.length() + 1 + k.second.length();
				if (l>=255)
					continue;

				ret.push_back(l);

				copy(k.first.c_str(), k.first.c_str()+k.first.length(), back_inserter(ret));

				ret.push_back('=');

				copy(k.second.c_str(), k.second.c_str()+k.second.length(), back_inserter(ret));
			}
			else
			{
				size_t l = k.first.length();
				if (l>=255)
					continue;

				ret.push_back(l);

				copy(k.first.c_str(), k.first.c_str()+k.first.length(), back_inserter(ret));
			}
		}
	}
	else
	{
		ret.push_back(0);
	}
	return ret;
}

void ServiceInfo::ReadTXT(const unsigned char *sz, int len)
{
	data.empty();
	if( sz == nullptr || len == 0 )
		return;

	int c = 0;
	while(c<len)
	{
		int pairLen = sz[c];
		c+=1;
		if (pairLen > 0 && len>=c+pairLen)
		{
			string pair(sz+c, sz+c+pairLen);

			string::size_type pos = pair.find('=');
			if(pos != pair.npos)
			{
				string key = pair.substr(0, pos);
				string val = pair.substr(pos+1);
				data[key] = val;
			}
			else
			{
				data[pair] = "";
			}

			c+=pairLen;
		}
	}
}



ServicePublisher::ServicePublisher(DSNMessageBusBase *bus, DNSServiceManager *owner)
: bus(bus)
, owner(owner)
{

}


bool ServicePublisher::publish()
{
	const char* cName = NULL;
	if(!info.name.empty())
		cName = info.name.c_str();

	const char* cDomain = NULL;
	if(!info.domain.empty())
		cDomain = info.domain.c_str();

	vector<uint8_t> data = info.TXTData();

	info.publisher = this;

	DNSServiceErrorType ret = DNSServiceRegister(&info.ref, 0, 0, cName, info.type.c_str(), cDomain, 0, info.port, data.size(), data.data(), &Self::callbackRegister, this);

	if(ret == kDNSServiceErr_NoError)
	{
		owner->EventLoop().RegisterRef(info.ref);
	}
	else if(bus)
	{
		bus->Message(info, ret, "published");
	}

	return (ret == kDNSServiceErr_NoError);
}

void ServicePublisher::unpublish()
{
	owner->EventLoop().TerminateRef(info.ref);
}

void DNSSD_API ServicePublisher::callbackRegister(DNSServiceRef sdRef,
												  DNSServiceFlags flags,
												  DNSServiceErrorType errorCode,
												  const char *name,
												  const char *regtype,
												  const char *domain,
												  void *context)
{
	Self *t = (Self*)context;
	if(name)
		t->info.name = name;
	if(t->bus)
		t->bus->Message(t->info, errorCode, "published");

	if(errorCode != kDNSServiceErr_NoError && t->owner)
	{
		t->owner->publishFailed(t);
	}
}



ServiceBrowser::ServiceBrowser(DSNMessageBusBase* bus, DNSServiceManager *owner)
: domain(ServiceInfo::kDefaultDomain)
, type(ServiceInfo::kDefaultType)
, browserRef(0)
, bus(bus)
, owner(owner)
{

}

bool ServiceBrowser::browse()
{
	const char* cDomain = NULL;
	if(!domain.empty())
		cDomain = domain.c_str();

	DNSServiceErrorType ret = DNSServiceBrowse(&browserRef, 0, 0, type.c_str(), cDomain, &Self::callbackBrowse, this);

	if(ret == kDNSServiceErr_NoError)
	{
		owner->EventLoop().RegisterRef(browserRef);
	}

	return (ret == kDNSServiceErr_NoError);
}

void ServiceBrowser::stop()
{
	for(auto &info : resolving)
	{
		owner->EventLoop().TerminateRef(info->ref);
	}
	resolving.clear();

	owner->EventLoop().TerminateRef(browserRef);
}

void ServiceBrowser::callbackBrowse(DNSServiceRef sdRef,
									DNSServiceFlags flags,
									uint32_t interfaceIndex,
									DNSServiceErrorType errorCode,
									const char *serviceName,
									const char *regtype,
									const char *replyDomain,
									void *context
									)
{

	Self *browser = (ServiceBrowser*)context;
	shared_ptr<ServiceInfo> toResolve = make_shared<ServiceInfo>();

	if(serviceName)
		toResolve->name = serviceName;
	if(regtype)
		toResolve->type = regtype;
	if(replyDomain)
		toResolve->domain = replyDomain;

	toResolve->browser = browser;

	if (errorCode!=kDNSServiceErr_NoError)
	{
		if(browser->bus)
			browser->bus->Message(*toResolve, errorCode, "browseError");
		if(browser->owner)
			browser->owner->browseFailed(browser);
	}
	else
		if(flags & kDNSServiceFlagsAdd)
		{
			DNSServiceErrorType ret = DNSServiceResolve(&toResolve->ref, 0, 0, serviceName, regtype, replyDomain, &Self::callbackResolve, toResolve.get());

			if(ret == kDNSServiceErr_NoError)
			{
				browser->resolving.push_back(toResolve);
				browser->owner->EventLoop().RegisterRef(toResolve->ref);
			}
		}
		else
		{
			if(browser->bus)
				browser->bus->Message(*toResolve, errorCode, "lost");
		}
}

void DNSSD_API ServiceBrowser::callbackResolve(DNSServiceRef sdRef,
											   DNSServiceFlags flags,
											   uint32_t interfaceIndex,
											   DNSServiceErrorType errorCode,
											   const char *fullname,
											   const char *hosttarget,
											   uint16_t port,
											   uint16_t txtLen,
											   const unsigned char *txtRecord,
											   void *context
											   )
{
	ServiceInfo *info = (ServiceInfo*)context;
	ServiceBrowser *browser = (ServiceBrowser*)info->browser;
	info->ReadTXT(txtRecord, txtLen);
	info->hostname = hosttarget;
	info->port = port;

	if(errorCode == kDNSServiceErr_NoError)
	{
		DNSServiceRef addRef = 0;
		DNSServiceGetAddrInfo(&addRef, 0, 0, 0, hosttarget, &Self::callbackAddr, info);

		int sock = DNSServiceRefSockFD(addRef);
		fd_set read_set;
		FD_ZERO(&read_set);
		FD_SET(sock, &read_set);
		timeval ti = {0,500000};
		int r = select(sock+1, &read_set, 0, 0, &ti);
		if(r>0) {
			DNSServiceProcessResult(addRef);
		}
		DNSServiceRefDeallocate(addRef);
	}

	if(browser && browser->bus)
	{
		browser->bus->Message(*info, errorCode, "found");
	}
	if(browser)
	{
		browser->owner->EventLoop().TerminateRef(info->ref);
	}
	info->ref = 0;
	if(browser)
	{
		browser->resolving.remove_if([info](shared_ptr<ServiceInfo> &i){
			return i.get()==info;
		});
	}
}

void DNSSD_API ServiceBrowser::callbackAddr(DNSServiceRef sdRef,
								   DNSServiceFlags flags,
								   uint32_t interfaceIndex,
								   DNSServiceErrorType errorCode,
								   const char *hostname,
								   const struct sockaddr *address,
								   uint32_t ttl,
								   void *context
								   )
{
	ServiceInfo *info = (ServiceInfo*)context;
//	char host[NI_MAXHOST], serv[NI_MAXSERV];
//	if(0==getnameinfo(address, 0, host, NI_MAXHOST, serv, NI_MAXSERV, 0)) {
//		info->addresses.push_back(host);
//	}
	char buff[70];
	switch (address->sa_family)
	{
		case AF_INET:
			inet_ntop(AF_INET, &(((sockaddr_in*)address)->sin_addr), buff, 70);
			break;
		case AF_INET6:
			inet_ntop(AF_INET6, &(((sockaddr_in6*)address)->sin6_addr), buff, INET6_ADDRSTRLEN);
			break;
		default:
			buff[0] = 0;
	}
	if(buff[0])
	{
		info->addresses.push_back(buff);
	}

}


DNSServiceManager::DNSServiceManager(DSNMessageBusBase *m)
: bus(m)
, eventLoop(nullptr)
{

}

DNSServiceManager::~DNSServiceManager()
{
	stop();
	delete eventLoop;
}

BaseDNSEventLoop&
DNSServiceManager::EventLoop()
{
	if (eventLoop == nullptr) {
		eventLoop = new PlatformEventLoop();
	}
	return *eventLoop;
}

PublisherHandle
DNSServiceManager::publish(const ServiceInfo &info)
{
	shared_ptr<ServicePublisher> pub = make_shared<ServicePublisher>(bus, this);
	pub->info = info;
	if(pub->publish())
	{
		publishers.push_back(pub);
		return pub.get();
	}
	else
	{
		return nullptr;
	}
}

bool
DNSServiceManager::unpublish(PublisherHandle publisherHandle)
{
	if(publisherHandle == nullptr)
		return false;

	bool found = false;
	for(auto &pub : publishers)
	{
		if(pub.get() == publisherHandle)
		{
			pub->bus = nullptr;
			pub->unpublish();
			found = true;
		}
	}

	publishers.remove_if([publisherHandle](const shared_ptr<ServicePublisher> &i){
		return i.get()==publisherHandle;
	});

	return found;
}


void
DNSServiceManager::unpublishAll()
{
	for(auto &pub : publishers)
	{
		pub->bus = nullptr;
		pub->unpublish();
	}

	publishers.clear();
}


PublisherHandle
DNSServiceManager::browse(const ServiceInfo &info)
{
	shared_ptr<ServiceBrowser> browser = make_shared<ServiceBrowser>(bus, this);
	browser->domain = info.domain;
	browser->type = info.type;
	if(browser->browse())
	{
		browsers.push_back(browser);
		return browser.get();
	}
	else
	{
		return nullptr;
	}
}

bool
DNSServiceManager::stopBrowser(BrowserHandle browserHandle)
{
	if(browserHandle == nullptr)
		return false;

	bool found = false;
	for(auto &browser : browsers)
	{
		if(browser.get() == browserHandle)
		{
			browser->bus = nullptr;
			browser->stop();
			found = true;
		}
	}

	browsers.remove_if([browserHandle](const shared_ptr<ServiceBrowser> &i){
		return i.get()==browserHandle;
	});

	return found;
}

void
DNSServiceManager::stopAllBrowsers()
{
	for(auto &browser : browsers)
	{
		browser->bus = nullptr;
		browser->stop();
	}

	browsers.clear();
}

void
DNSServiceManager::browseFailed(BrowserHandle browser)
{
	stopBrowser(browser);
}

void
DNSServiceManager::publishFailed(PublisherHandle publisher)
{
	unpublish(publisher);
}

void
DNSServiceManager::stop()
{
	stopAllBrowsers();
	unpublishAll();
}

