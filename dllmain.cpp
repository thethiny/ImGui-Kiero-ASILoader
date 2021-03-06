#include "includes.h"
#include <iostream>
#include <windows.h>
#include <vector>
#include <chrono>
#include <filesystem>
#include "code/eSettingsManager.h"

char* sMenuTitle = "MK11 Hooks";

void CreateConsole();
std::vector<std::string> FindASIs(HMODULE hDllModule);
void OnInitializeHook(HMODULE);

std::vector<std::string> ASIs;

typedef ImGuiContext* (__stdcall ASIPresent)(ImGuiContext* ctx);
typedef ImGuiContext* (__fastcall ASIStyle)(ImGuiContext* ctx);
typedef void* (__fastcall ASIInit)();

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
	ASIStyle* StyleFunction;
	ASIInit* InitFunction;
	bool bInit = false;
};

ASIStruct* sASIStruct;
ASIStruct* sActiveASI = NULL;

bool bIsGuiActive = false;

void InitImGui()
{
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(pDevice, pContext);
}

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_CLOSE || uMsg == WM_ENDSESSION || uMsg == WM_QUERYENDSESSION) // If Any Sort of shutdown occurs, handle it to the game so that it properly shuts down.
	{
		return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
	}

	if (uMsg == WM_KEYDOWN)
	{
		switch (wParam)
		{
		case VK_F1:
			bIsGuiActive = !bIsGuiActive;
			return true;
		case VK_ESCAPE:
			if (bIsGuiActive)
			{
				bIsGuiActive = false;
				return true;
			}
			break;
		}
	}

	if (bIsGuiActive)
	{
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
		return DefWindowProc(hWnd, uMsg, wParam, lParam); // Handle Default Events
	}
	
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

	if (!bIsGuiActive)
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
		if (sASIStruct[0].bInit)
			ImGui::SetCurrentContext(sASIStruct[0].PresentFunction(ImGui::GetCurrentContext())); // Draw me only if initialized
	}
	else if (ASI_count > 1)
	{
		ImGui::Begin(sMenuTitle);
		ImGui::Text("Hooks:");
		for (int i = 0; i < ASIs.size(); i++)
		{
			if (!sASIStruct[i].bInit)
				continue;

			ImGui::SameLine();

			if (ImGui::Button(sASIStruct[i].name.c_str()))
			{
				sActiveASI = &sASIStruct[i];
				if (sASIStruct[i].StyleFunction)
					ImGui::SetCurrentContext(sASIStruct[i].StyleFunction(ImGui::GetCurrentContext()));
			}

		}
		ImGui::Separator();

		if (sActiveASI)
			ImGui::SetCurrentContext(sActiveASI->PresentFunction(ImGui::GetCurrentContext())); // Draw active
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

//void main()
//{
//	while (1)
//	{
//		if (GetAsyncKeyState(VK_F1))
//		{
//			if (bIsGuiActive)
//			{
//				while (GetAsyncKeyState(VK_F1)); // Wait until Negative Edge to avoid Pressing by accident
//				bIsGuiActive = !bIsGuiActive;
//			}
//			else
//			{
//				bIsGuiActive = !bIsGuiActive;
//				while (GetAsyncKeyState(VK_F1)); // Wait until Negative Edge to avoid wait
//			}
//			
//			
//		}
//		if (GetAsyncKeyState(VK_ESCAPE))
//		{
//			while (GetAsyncKeyState(VK_ESCAPE)); // Wait until Negative Edge to avoid Pressing Back
//			bIsGuiActive = false;
//		}
//	}
//}

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

		// Send a message to ASI that there's a loader
		sASIStruct[ctr - 1].InitFunction = (ASIInit*)GetProcAddress(hDLL, "InitShared");
		if (sASIStruct[ctr - 1].InitFunction == 0)
			continue;
		sASIStruct[ctr - 1].InitFunction();
		std::cout << "Initialize::" << sASIStruct[ctr - 1].name << "::InitShared" << std::endl;

		// The Draw Function of the ASI
		sASIStruct[ctr-1].PresentFunction = (ASIPresent*)GetProcAddress(hDLL, "SharedPresent");
		if (sASIStruct[ctr-1].PresentFunction == 0)
			continue;
		std::cout << "Initialize::" << sASIStruct[ctr - 1].name << "::SharedPresent" << std::endl;

		// The Styling function of the ASI in case there's one
		sASIStruct[ctr - 1].StyleFunction = (ASIStyle*)GetProcAddress(hDLL, "SharedStyle");
		std::cout << "Initialize::" << sASIStruct[ctr - 1].name << "::SharedStyle" << std::endl;

		sASIStruct[ctr-1].bInit = true;
		std::cout << "Initializing " << *i << " Success!" << std::endl;
	}

	//main();
}

std::string GetDirName()
{
	CHAR fileName[MAX_PATH + 1];
	DWORD chars = GetModuleFileNameA(NULL, fileName, MAX_PATH + 1);
	if (chars)
	{
		std::string basename;
		std::string filename = std::string(fileName);
		size_t pos = filename.find_last_of('\\');
		if (pos != -1)
		{
			basename = filename.substr(0, pos);
			return basename;
		}
		return filename;
	}
	return "";
}

std::string toLower(std::string s)
{
	std::string new_string("");
	for (int i = 0; i < s.length(); i++)
	{
		new_string += std::tolower(s[i]);
	}
	return new_string;
}

inline bool FolderCompare(std::string FullPath, std::string Basename)
{
	return (FullPath.substr(FullPath.length() - Basename.length(), Basename.length()) == Basename);
}

std::vector<std::string> FindASIs(HMODULE hDllModule)
{
	std::vector<std::string> vFoundASI;
	char lpAsiName[MAX_PATH];
	DWORD chars = GetModuleFileName(hDllModule, lpAsiName, MAX_PATH + 1);
	std::string szAsiName(lpAsiName);

	if (chars)
	{
		std::cout << "ImGuiPresent::MyFile = " << lpAsiName << std::endl;
	}

	std::string dirName = GetDirName();
	std::cout << "ImGuiPresent::Search Dir = " << dirName << std::endl;
	for (const auto& file : std::filesystem::directory_iterator(dirName.c_str()))
	{

		if (file.is_directory())
			continue;
		if (file.path().has_extension())
		{
			if (toLower(file.path().extension().string()) == ".asi")
			{
				std::string szFileName = file.path().filename().string();
				if (!FolderCompare(szAsiName, szFileName)) // Avoid Myself
				{
					std::cout << "ImGuiPresent::Found ASI " << szFileName << std::endl;
					vFoundASI.push_back(szFileName);
				}
					
			}
				
		}
	}

	return vFoundASI;
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

