#include "DNSWindowsEventLoop.h"
#include <dns_sd.h>

#define WM_DNS_SD_EVENT ( WM_USER + 0x101 )


DNSWindowsEventLoop::DNSWindowsEventLoop()
	: m_hWnd(NULL)
{
}

DNSWindowsEventLoop::~DNSWindowsEventLoop()
{
	for (auto &e : mapping)
	{
		WSAAsyncSelect(e.first, m_hWnd, 0, 0);
		DNSServiceRefDeallocate(e.second);
	}
	mapping.clear();

	if (m_hWnd)
	{
		DestroyWindow(m_hWnd);
		::UnregisterClassW(L"DNSEventLoopWindow", 0);
	}
}

LRESULT DNSWindowsEventLoop::OnProcessMessage(HWND windowHandle, UINT messageId, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = -1;

	switch (messageId)
	{
	case WM_DNS_SD_EVENT:
		{
			SOCKET sock = (SOCKET)wParam;

			if (WSAGETSELECTERROR(lParam) && !(HIWORD(lParam)))
				break;

			DNSWindowsEventLoop *e = (DNSWindowsEventLoop *)::GetWindowLongPtr(windowHandle, GWLP_USERDATA);
			if (e)
			{
				const auto &it = e->mapping.find(sock);
				if (it != e->mapping.end())
				{
					DNSServiceRef ref = it->second;
					if (DNSServiceProcessResult(ref) != kDNSServiceErr_NoError)
						result = -1;
					else
						result = 0;
				}
				break;
			}
		}
	default:
		result = ::DefWindowProc(windowHandle, messageId, wParam, lParam);
		break;
	}

	return result;
}

void DNSWindowsEventLoop::RegisterRef(DNSServiceRef ref)
{
	if (!ref)
		return;

	if (m_hWnd == NULL)
	{
		static const wchar_t *sClassName = nullptr;
		if (!sClassName)
		{
			HMODULE moduleHandle = nullptr;
			DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
			::GetModuleHandleExW(flags, (LPCWSTR)sClassName, &moduleHandle);

			WNDCLASSEXW settings{};
			settings.cbSize = sizeof(settings);
			settings.lpszClassName = L"DNSEventLoopWindow";
			settings.hInstance = moduleHandle;
			settings.lpfnWndProc = &DNSWindowsEventLoop::OnProcessMessage;
			if (::RegisterClassExW(&settings))
			{
				sClassName = settings.lpszClassName;
				m_hWnd = ::CreateWindowEx(0, sClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, 0, 0);
				if (m_hWnd)
				{
					SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);
				}
			}
		}
	}
	if (m_hWnd)
	{
		SOCKET sock = DNSServiceRefSockFD(ref);
		mapping[sock] = ref;
		WSAAsyncSelect((SOCKET)DNSServiceRefSockFD(ref), m_hWnd, WM_DNS_SD_EVENT, FD_READ | FD_CLOSE);
	}
}

void DNSWindowsEventLoop::TerminateRef(DNSServiceRef ref)
{
	if (!ref)
		return;
	if (m_hWnd)
	{
		SOCKET sock = DNSServiceRefSockFD(ref);
		mapping.erase(sock);
		WSAAsyncSelect(sock, m_hWnd, 0, 0);
	}
	DNSServiceRefDeallocate(ref);
}

