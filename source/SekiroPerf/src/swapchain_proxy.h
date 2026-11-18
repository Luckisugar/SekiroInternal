#pragma once

#include <windows.h>
#include <dxgi1_5.h>

#include <functional>

namespace sp::upscale {

struct Settings {
    float sharpness = 0.8f;
    bool disableVSync = false;
};

// Sizes `hwnd` to a borderless window covering its monitor before any swap chain is created.
// Writes that monitor size to `width` and `height`. Weapon Wheel latches its UI to the first
// window size it sees, so this has to happen first; a later resize leaves the wheel invisible.
// Returns false when the window cannot be sized.
bool PrepareBorderless(HWND hwnd, UINT& width, UINT& height);

// Creates the game's swap chain (always windowed, via `createReal`) behind a proxy. The game
// keeps rendering at `gameWidth` x `gameHeight` into a texture the proxy hands out as the back
// buffer; on Present the proxy upscales that with FSR to the borderless window. Pass 0, 0 to
// use the real swap chain's size. Fullscreen requests from the game are honoured virtually.
//
// Returns nullptr if upscaling can't be used here; the caller then creates the swap chain normally.
IDXGISwapChain4* CreateProxy(IUnknown* device, HWND hwnd, bool gameFullscreen, UINT sampleCount,
                             UINT gameWidth, UINT gameHeight, const Settings& settings,
                             const std::function<HRESULT(IDXGISwapChain**)>& createReal);

// Called with the real output chain after the upscale and before that chain is presented.
// Read each frame, so it can be installed after the proxy already exists.
using FrameFn = void(__stdcall*)(IDXGISwapChain* realChain);
void SetFrameCallback(FrameFn fn);

}  // namespace sp::upscale
