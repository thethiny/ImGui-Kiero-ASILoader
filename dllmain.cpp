#include "includes.h"
#include <iostream>
#include <windows.h>
#include <vector>
#include <chrono>
#include "code/eSettingsManager.h"

void CreateConsole();
std::vector<std::string> FindASIs(HMODULE hDllModule);
void OnInitializeHook(HMODULE);

std::vector<std::string> ASIs;

typedef ImGuiContext* (__stdcall ASIPresent)(ImGuiContext* ctx);

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Present oPresent;
HWND window = NULL;
WNDPROC oWndProc;
ID3D11Device* pDevice = NULL;
ID3D11DeviceContext* pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView;

struct ASIStruct {
	std::string name;
	ASIPresent* PresentFunction;
	bool bInit = false;
};

ASIStruct* sASIStruct;
ASIStruct* sActiveASI = NULL;

bool isActive = false;

void InitImGui()
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(pDevice, pContext);
}

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}


bool init = false;

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (!init)
	{
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice)))
		{
			pDevice->GetImmediateContext(&pContext);
			DXGI_SWAP_CHAIN_DESC sd;
			pSwapChain->GetDesc(&sd);
			window = sd.OutputWindow;

			// Set Windows
			/*for (int i = 0; i < ASIs.size(); i++)
			{
				if (!sASIStruct[i].bInit)
					continue;

				sASIStruct[i].SetGlobWindowFunction(window);
			}*/
			// End Set Windows

			ID3D11Texture2D* pBackBuffer;
			pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
			pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
			oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
			InitImGui();
			init = true;
		}
		else
			return oPresent(pSwapChain, SyncInterval, Flags);
	}

	if (!isActive)
		return oPresent(pSwapChain, SyncInterval, Flags);

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::GetIO().MouseDrawCursor = true;


	// Multi Stuff Here
	auto ASI_count = ASIs.size();
	if (ASI_count == 1)
	{
		ImGui::Begin(sASIStruct[0].name.c_str());
		ImGui::SetCurrentContext(sASIStruct[0].PresentFunction(ImGui::GetCurrentContext())); // Draw me only
	}
	else if (ASI_count > 1)
	{
		ImGui::Begin("All in One ImGui");
		ImGui::Text("Hooks:");
		for (int i = 0; i < ASIs.size(); i++)
		{
			if (!sASIStruct[i].bInit)
				continue;

			ImGui::SameLine();

			if (ImGui::Button(sASIStruct[i].name.c_str()))
				sActiveASI = &sASIStruct[i];

			

		}
		ImGui::Separator();

		for (int i = 0; i < ASIs.size(); i++)
		{
			if (sActiveASI == &sASIStruct[i])
				ImGui::SetCurrentContext(sASIStruct[i].PresentFunction(ImGui::GetCurrentContext())); // Draw me if I'm active
		}
	}
	else
	{
		ImGui::TextWrapped("There are no ASIs loaded compatible with ImGuiPresent");
		ImGui::Spacing();
	}
	
	// End Multi Stuff
	ImGui::EndFrame();
	ImGui::Render();
	

	pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	

	return oPresent(pSwapChain, SyncInterval, Flags);
}


DWORD WINAPI MainThread(LPVOID lpReserved)
{
	bool init_hook = false;
	do
	{
		if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success)
		{
			kiero::bind(8, (void**)&oPresent, hkPresent);
			init_hook = true;
		}
	} while (!init_hook);
	return TRUE;
}

HMODULE AwaitHModule(const char* name, uint64_t timeout)
{
	HMODULE toAwait = NULL;
	auto start = std::chrono::system_clock::now();
	while (!toAwait)
	{
		if (timeout > 0)
		{
			std::chrono::duration<double> now = std::chrono::system_clock::now() - start;
			if (now.count() * 1000 > timeout)
			{
				std::cout << "No Handle " << name << std::endl;
				return NULL;
			}
		}
		toAwait = GetModuleHandle(name);
	}
	std::cout << "Obtained Handle for " << name << std::endl;
	return toAwait;
}


void CreateConsole()
{
	AllocConsole();
	FILE* fNull;
	freopen_s(&fNull, "CONOUT$", "w", stdout);
	freopen_s(&fNull, "CONOUT$", "w", stderr);

	std::cout.clear();
	std::cerr.clear();
	std::clog.clear();
	std::string consoleName = "ImGui Console";
	SetConsoleTitleA(consoleName.c_str());
}

void main()
{
	while (1)
	{
		if (GetAsyncKeyState(VK_F1))
		{
			isActive = !isActive;
			while (GetAsyncKeyState(VK_F1)); // Wait until Negative Edge
		}
	}
}

void OnInitializeHook(HMODULE hModule)
{
	SettingsMgr->Init();
	CreateConsole();
	std::cout << "Console Created!" << std::endl;

	ASIs = FindASIs(hModule);
	sASIStruct = new ASIStruct[ASIs.size()];
	uint32_t ctr = 0;
	for (auto i = ASIs.begin(); i != ASIs.end(); i++)
	{
		ctr++;
		std::cout << "Initializing " << *i << std::endl;
		sASIStruct[ctr-1].name = i->substr(0, i->length() - 4);

		HMODULE hDLL = AwaitHModule(i->c_str(), 2000);
		if (hDLL == NULL)
			continue;


		sASIStruct[ctr-1].PresentFunction = (ASIPresent*)GetProcAddress(hDLL, "SharedPresent");
		if (sASIStruct[ctr-1].PresentFunction == 0)
			continue;

		sASIStruct[ctr-1].bInit = true;
		std::cout << "Initializing " << *i << " Success!" << std::endl;
	}

	main();
}

std::vector<std::string> FindASIs(HMODULE hDllModule)
{
	WIN32_FIND_DATA w32Data;
	std::vector<std::string> ASIs;
	HANDLE hFind = FindFirstFile("*.asi", &w32Data);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		FindClose(hFind);
		return ASIs;
	}


	char lpAsiName[MAX_PATH];
	DWORD chars = GetModuleFileName(hDllModule, lpAsiName, MAX_PATH + 1);
	std::string szAsiName(lpAsiName);
	if (chars)
	{
		std::cout << "Dll: " << lpAsiName << std::endl;
	}

	do {
		std::string szFileName(w32Data.cFileName);
		if (szAsiName.substr(szAsiName.length() - szFileName.length(), szFileName.length()) != szFileName) // Avoid Myself
		{
			ASIs.push_back(szFileName);
		}

	} while (FindNextFile(hFind, &w32Data));

	FindClose(hFind);
	return ASIs;
}

bool APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpRes)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		//OnInitializeHook(hModule);
		CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)OnInitializeHook, hModule, 0, nullptr);
		CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return true;
}

