#include <windows.h>

#define JOLT_API extern "C" __declspec(dllexport)

static LONG g_joltActive = 0;
static const char* kJoltVersion = "jolt/0.1";

JOLT_API void __stdcall JoltInit(float gravity) {
	(void)gravity;
	InterlockedExchange(&g_joltActive, 1);
}

JOLT_API void __stdcall JoltShutdown(void) {
	InterlockedExchange(&g_joltActive, 0);
}

JOLT_API int __stdcall JoltIsActive(void) {
	return (int)g_joltActive;
}

JOLT_API const char* __stdcall JoltVersion(void) {
	return kJoltVersion;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_DETACH) {
		InterlockedExchange(&g_joltActive, 0);
	}
	return TRUE;
}
