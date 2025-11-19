#include <TiltedOnlinePCH.h>
#include "TiltedOnlineApp.h"

extern std::unique_ptr<TiltedOnlineApp> g_appInstance;

#include <windows.h>
using EngramPreTickFn = void(__cdecl*)();

static EngramPreTickFn pEngramPreTick = nullptr;
static bool g_EngramChecked = false;

static constexpr const wchar_t* kEngramDllName = L"engram_skyrim.dll";

static void TryInitEngram()
{
    if (g_EngramChecked)
        return;
    g_EngramChecked = true;
    HMODULE hEngram = GetModuleHandleW(kEngramDllName);
    if (hEngram)
    {
        pEngramPreTick = reinterpret_cast<EngramPreTickFn>(GetProcAddress(hEngram, "engram_pre_tick"));
        if (pEngramPreTick)
        {
            spdlog::info("[TiltedOnline] Engram detected, enabling integration.");
        }
    }
}


extern "C" void engram_pre_tick();

#include <GameVM.h>

struct Main;
struct VMContext
{
    char pad[0x680];
    uint8_t inactive; // 0x680
};

TP_THIS_FUNCTION(TVMUpdate, int, VMContext, float);
TP_THIS_FUNCTION(TMainLoop, short, Main);
TP_THIS_FUNCTION(TVMDestructor, uintptr_t, void);

static TVMUpdate* VMUpdate = nullptr;
static TMainLoop* MainLoop = nullptr;
static TVMDestructor* VMDestructor = nullptr;

int TP_MAKE_THISCALL(HookVMUpdate, VMContext, float a2)
{
    if (apThis->inactive == 0)
        g_appInstance->Update();

    return TiltedPhoques::ThisCall(VMUpdate, apThis, a2);
}

short TP_MAKE_THISCALL(HookMainLoop, Main)
{
    TP_EMPTY_HOOK_PLACEHOLDER

    if (!g_EngramChecked)
    {
        TryInitEngram();
    }
    if (pEngramPreTick)
    {
        pEngramPreTick();
    }

    return TiltedPhoques::ThisCall(MainLoop, apThis);
}

uintptr_t TP_MAKE_THISCALL(HookVMDestructor, void)
{
    TP_EMPTY_HOOK_PLACEHOLDER

    return TiltedPhoques::ThisCall(VMDestructor, apThis);
}

static TiltedPhoques::Initializer s_mainHooks(
    []()
    {
        POINTER_SKYRIMSE(TMainLoop, cMainLoop, 36564);
        POINTER_SKYRIMSE(TVMUpdate, cVMUpdate, 53926);
        POINTER_SKYRIMSE(TVMDestructor, cVMDestructor, 40412);

        VMUpdate = cVMUpdate.Get();
        MainLoop = cMainLoop.Get();
        VMDestructor = cVMDestructor.Get();

        TP_HOOK(&VMUpdate, HookVMUpdate);
        TP_HOOK(&MainLoop, HookMainLoop);
        TP_HOOK(&VMDestructor, HookVMDestructor);
    });

