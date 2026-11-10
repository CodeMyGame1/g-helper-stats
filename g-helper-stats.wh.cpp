// ==WindhawkMod==
// @id              g-helper-stats
// @name            G-Helper Stats
// @description     Surfaces G-Helper info directly in the taskbar!
// @version         0.1
// @author          CodeMyGame1
// @github          https://github.com/CodeMyGame1
// @homepage        https://codemygame.dev
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -lpdh
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# G-Helper Stats
Instead of an overlay, uses the empty space in the taskbar to display CPU, GPU, RAM, and
other relevant system information!
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- ShowSeconds: true
  $name: Show seconds 
*/
// ==/WindhawkModSettings==

// INCLUDES
#include "windhawk_utils.h"

// DEFINITIONS
using RunFromWindowThreadProc_t = void(WINAPI*)(void* parameter);

// Fetches the taskbar process.
bool HookTaskbarDllSymbols() {
    HMODULE module = LoadLibraryEx(L"taskbar.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (!module) {
        Wh_Log(L"Failed to load taskbar.dll");
        return false;
    }

    WindhawkUtils::SYMBOL_HOOK taskbarDllHooks[] = {};

    return WindhawkUtils::HookSymbols(module, taskbarDllHooks, ARRAYSIZE(taskbarDllHooks));
}

// Finds the taskbar window.
HWND FindCurrentProcessTaskbarWnd() {
    HWND hTaskbarWnd = nullptr;

    EnumWindows(
        [](HWND hWnd, LPARAM lParam) -> BOOL {
            DWORD dwProcessId;
            WCHAR className[32];

            if (GetWindowThreadProcessId(hWnd, &dwProcessId) &&
                // Is the window we're looking at `explorer.exe`?
                dwProcessId == GetCurrentProcessId() &&
                // Is the window we're looking at the main taskbar?
                GetClassName(hWnd, className, ARRAYSIZE(className)) &&
                _wcsicmp(className, L"Shell_TrayWnd") == 0
            ) {
                // Save this window ID.
                *reinterpret_cast<HWND*>(lParam) = hWnd;
                return FALSE;
            }
            return TRUE;
        },
    reinterpret_cast<LPARAM>(&hTaskbarWnd));

    return hTaskbarWnd;
}

bool RunFromWindowThread(HWND hWnd, RunFromWindowThreadProc_t proc, void* procParam) {
    static const UINT registeredMsg =
        RegisterWindowMessage(L"Windhawk_RunFromWindowThread_" WH_MOD_ID);
    
    struct THREAD_PROC_PARAM {
        RunFromWindowThreadProc_t proc;
        void* procParam;
    };

    // Attempts to fetch the process that owns the window.
    DWORD dwThreadId = GetWindowThreadProcessId(hWnd, nullptr);
    if (dwThreadId == 0) {
        return false;
    }

    // No point in sending a message, invoke process directly.
    if (dwThreadId == GetCurrentThreadId()) {
        proc(procParam);
        return true;
    }

    // Set a hook to intercept the message we will send.
    HHOOK hook = SetWindowsHookEx(
        WH_CALLWNDPROC,
        [](int nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
            // If we're allowed to handle the message, handle it.
            if (nCode == HC_ACTION) {
                const CWPSTRUCT* cwp = (const CWPSTRUCT*)lParam;
                if (cwp->message == registeredMsg) {
                    auto* param = (THREAD_PROC_PARAM*)cwp->lParam;
                    // Invoke process from within correct thread
                    // so Windows doesn't cry and crash.
                    param->proc(param->procParam);
                }
            }
            return CallNextHookEx(nullptr, nCode, wParam, lParam);
        },
        nullptr, dwThreadId
    );

    if (!hook) return false;

    THREAD_PROC_PARAM param{proc, procParam};
    // Blocks until message has (ideally) been handled by hook,
    // meaning process was run.
    SendMessage(hWnd, registeredMsg, 0, (LPARAM)&param);

    UnhookWindowsHookEx(hook);
    return true;
}

// The mod is being initialized, load settings, hook functions, and do other
// initialization stuff if required.
BOOL Wh_ModInit() {
    Wh_Log(L"Init");

    if (!HookTaskbarDllSymbols()) {
        return FALSE;
    }
    return TRUE;
}

// After initialization.
void Wh_ModAfterInit() {
    HWND hTaskbarWnd = FindCurrentProcessTaskbarWnd();
    if (!hTaskbarWnd) return;

    Wh_Log(L"Windhawk thread: %u", GetCurrentThreadId());

    RunFromWindowThread(hTaskbarWnd, [](void*) {
        Wh_Log(L"Running on thread: %u", GetCurrentThreadId());
    }, nullptr);
}

// The mod is being unloaded, free all allocated resources.
void Wh_ModUninit() {
    Wh_Log(L"Uninit");
}

// The mod setting were changed, reload them.
void Wh_ModSettingsChanged() {
    Wh_Log(L"SettingsChanged");
}
