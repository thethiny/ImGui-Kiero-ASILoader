# ImGui-Kiero-ASILoader
This code allows you to share ImGui between multiple ASIs (Dlls), makes you able to use the same ImGui menu across different implementations. All you need is to provide an interface to your Draw() functionality.

## Usage
Add the following code to your dll along with the required library `imgui`
```
extern "C" __declspec(dllexport) ImGuiContext * __stdcall SharedPresent(ImGuiContext* ctx)
{
	ImGui::SetCurrentContext(ctx);
	
	//
		Im Gui Commands Here
	//
	return ImGui::GetCurrentContext();

}
```
You also need to compile the solution using MSVC, and load the resulting ASI.

## How it works
This ASI works by searching for all ASIs in the game's binaries directory and waiting for them to load (max 2 seconds), then creates a TAB for them in the generic menu. The function will use Kiero to hook into D3D's GetDisplay function, which in turn will replace the `Present Function` in the form of `HRESULT hkPresent`. Refer to `include.h` for more information about `HRESULT`.

## Note
This version has been tested on the DX11 Win32 implementation. The code relies on WinAPI therefore it is not cross-platform. At the time of writing, this code was implemented for the game Ultimate Mortal Kombat 11, but should work on DX11 games.

To use other backends, you should utilize other Kiero hooks (such as DX9 Kiero Hook, see credits below) or manually write an implementation for your DX version. Concept should remain the same. The premise of this entire code is to share an ImGui context across all DLLs automatically in a standarized manner.

## Credits
[Kiero](https://github.com/Rebzzel/kiero) - Enabling ImGui in D3D Apps by hooking
[Kiero DX11 Hook](https://github.com/rdbo/ImGui-DirectX-11-Kiero-Hook) - DX11 Implementation
[Dear ImGui](https://github.com/ocornut/imgui) - D3D Gui
[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) - ASI Loader
[MKXHook](https://github.com/ermaccer/MKXHook) - ASI Implementation

