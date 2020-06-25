//
//  DnsWrapper.h
//  ZeroConf plugin for Corona
//
//  Created by Vlad Shcherban on 2016-08-18.
//  Copyright © 2016 Corona Labs. All rights reserved.
//


#ifndef DnsWrapper_h
#define DnsWrapper_h

#include <string>
#include <unordered_map>
#include <list>
#include <vector>
#include <memory>



typedef void* PublisherHandle;
typedef void* BrowserHandle;

typedef struct _DNSServiceRef_t *DNSServiceRef;

class ServiceInfo
{
public:
	static const char * kDefaultType;
	static const char * kDefaultDomain;
public:
	int port;
	std::string type;
	std::string name;
	std::string domain;
	std::string hostname;
	std::unordered_map<std::string, std::string> data;
	std::list<std::string> addresses;

	DNSServiceRef ref;

	BrowserHandle browser;
	PublisherHandle publisher;

	ServiceInfo();
	
	void setData(const char *key, const char *value);

	std::vector<uint8_t> TXTData() const;

	void ReadTXT(const unsigned char *sz, int len);
};



class DSNMessageBusBase
{
public:
	virtual void Message(const ServiceInfo &srv, int errorCode, const char* phase) = 0;
	virtual ~DSNMessageBusBase(){};
};


class ServiceBrowser;
class ServicePublisher;

class BaseDNSEventLoop
{
public:
	virtual void RegisterRef(DNSServiceRef ref) = 0;
	virtual void TerminateRef(DNSServiceRef ref) = 0;
	virtual ~BaseDNSEventLoop(){};
};

class DNSServiceManager
{
private:
	DSNMessageBusBase *bus;

	std::list< std::shared_ptr<ServicePublisher> > publishers;
	std::list< std::shared_ptr<ServiceBrowser> > browsers;

	BaseDNSEventLoop *eventLoop;
public:

	DNSServiceManager(DSNMessageBusBase *m);
	~DNSServiceManager();

	PublisherHandle publish(const ServiceInfo &info);
	bool unpublish(PublisherHandle publisher);
	void unpublishAll();

	BrowserHandle browse(const ServiceInfo &info);
	bool stopBrowser(BrowserHandle browser);
	void stopAllBrowsers();

	void stop();

	void publishFailed(PublisherHandle publisher);
	void browseFailed(BrowserHandle browser);

	BaseDNSEventLoop &EventLoop();

};







#endif /* DnsWrapper_h */
