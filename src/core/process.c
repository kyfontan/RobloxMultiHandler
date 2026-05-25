#include "process.h"
#include <tlhelp32.h>
#include <string.h>

BOOL rh_is_roblox_running(void) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return FALSE;
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    BOOL found = FALSE;
    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, RH_ROBLOX_EXE) == 0) {
                found = TRUE;
                break;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

int rh_enum_roblox_pids(DWORD *out, int cap) {
    if (!out || cap <= 0) return 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    int n = 0;
    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, RH_ROBLOX_EXE) == 0) {
                if (n < cap) out[n++] = pe.th32ProcessID;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return n;
}

int rh_kill_roblox(void) {
    DWORD pids[64];
    int n = rh_enum_roblox_pids(pids, 64);
    int killed = 0;
    for (int i = 0; i < n; i++) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pids[i]);
        if (h) {
            if (TerminateProcess(h, 1)) killed++;
            CloseHandle(h);
        }
    }
    return killed;
}
