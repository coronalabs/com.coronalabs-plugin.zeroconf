#pragma once

#include "DnsWrapper.h"
#include <windows.h>
#include <unordered_map>

class DNSWindowsEventLoop :
	public BaseDNSEventLoop
{
public:
	DNSWindowsEventLoop();
	virtual ~DNSWindowsEventLoop();
	virtual void RegisterRef(DNSServiceRef ref) override;
	virtual void TerminateRef(DNSServiceRef ref) override;
private:
	static LRESULT CALLBACK OnProcessMessage(HWND windowHandle, UINT messageId, WPARAM wParam, LPARAM lParam);
	HWND m_hWnd;
	std::unordered_map<SOCKET, DNSServiceRef> mapping;
};

