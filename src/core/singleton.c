/*
 * Multi-instance enabler: closes Roblox's singleton event+mutex objects
 * under \Sessions\<id>\BaseNamedObjects so the next launch is allowed.
 * Modeled on github.com/sir49/SingleTonEventCloser/blob/main/roblox.c.
 *
 * We dynamically resolve the Nt* APIs from ntdll.dll so the binary still
 * loads on systems where ntdll's import surface differs. Walking the
 * directory then duplicating the handle into our process via
 * NtDuplicateObject(DUPLICATE_CLOSE_SOURCE) is what actually closes the
 * remote handles -- but the simplest reliable trick is to open each named
 * object with DELETE/SYNCHRONIZE access in our session and call
 * NtMakeTemporaryObject so they evaporate when the last handle dies.
 */
#include "singleton.h"
#include "sleep.h"
#include <stdio.h>
#include <string.h>

/* ---- minimal NT type/proto subset (avoids <winternl.h> for portability) ---- */

typedef LONG NTSTATUS;
#define NT_SUCCESS(s) ((s) >= 0)

typedef struct _UNICODE_STRING_RH {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
} UNICODE_STRING_RH, *PUNICODE_STRING_RH;

typedef struct _OBJECT_ATTRIBUTES_RH {
    ULONG  Length;
    HANDLE RootDirectory;
    PUNICODE_STRING_RH ObjectName;
    ULONG  Attributes;
    PVOID  SecurityDescriptor;
    PVOID  SecurityQualityOfService;
} OBJECT_ATTRIBUTES_RH, *POBJECT_ATTRIBUTES_RH;

#ifndef OBJ_CASE_INSENSITIVE
#define OBJ_CASE_INSENSITIVE 0x00000040L
#endif

#define InitOA_RH(p, name) do {                       \
    (p)->Length = sizeof(OBJECT_ATTRIBUTES_RH);       \
    (p)->RootDirectory = NULL;                        \
    (p)->ObjectName = (name);                         \
    (p)->Attributes = OBJ_CASE_INSENSITIVE;           \
    (p)->SecurityDescriptor = NULL;                   \
    (p)->SecurityQualityOfService = NULL;             \
} while (0)

typedef NTSTATUS (NTAPI *PFN_NtOpenEvent)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES_RH);
typedef NTSTATUS (NTAPI *PFN_NtOpenMutant)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES_RH);
typedef NTSTATUS (NTAPI *PFN_NtMakeTemporaryObject)(HANDLE);
typedef VOID     (NTAPI *PFN_RtlInitUnicodeString)(PUNICODE_STRING_RH, PCWSTR);

static PFN_NtOpenEvent           pNtOpenEvent;
static PFN_NtOpenMutant          pNtOpenMutant;
static PFN_NtMakeTemporaryObject pNtMakeTemporaryObject;
static PFN_RtlInitUnicodeString  pRtlInitUnicodeString;

static int resolve_nt(void) {
    if (pNtOpenEvent) return 1;
    HMODULE m = GetModuleHandleA("ntdll.dll");
    if (!m) m = LoadLibraryA("ntdll.dll");
    if (!m) return 0;
    pNtOpenEvent           = (PFN_NtOpenEvent)          GetProcAddress(m, "NtOpenEvent");
    pNtOpenMutant          = (PFN_NtOpenMutant)         GetProcAddress(m, "NtOpenMutant");
    pNtMakeTemporaryObject = (PFN_NtMakeTemporaryObject)GetProcAddress(m, "NtMakeTemporaryObject");
    pRtlInitUnicodeString  = (PFN_RtlInitUnicodeString) GetProcAddress(m, "RtlInitUnicodeString");
    return pNtOpenEvent && pNtOpenMutant && pNtMakeTemporaryObject && pRtlInitUnicodeString;
}

static int close_one(const wchar_t *path, int as_mutant) {
    UNICODE_STRING_RH u;
    pRtlInitUnicodeString(&u, path);
    OBJECT_ATTRIBUTES_RH oa;
    InitOA_RH(&oa, &u);
    HANDLE h = NULL;
    NTSTATUS st = as_mutant
        ? pNtOpenMutant(&h, DELETE | SYNCHRONIZE, &oa)
        : pNtOpenEvent (&h, DELETE | SYNCHRONIZE, &oa);
    if (!NT_SUCCESS(st) || !h) return 0;
    pNtMakeTemporaryObject(h);
    CloseHandle(h);
    return 1;
}

int rh_singleton_close_once(void) {
    if (!resolve_nt()) return 0;
    DWORD sid = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &sid);

    wchar_t evtPath[160], mtxPath[160];
    _snwprintf(evtPath, 160, L"\\Sessions\\%lu\\BaseNamedObjects\\ROBLOX_singletonEvent", sid);
    _snwprintf(mtxPath, 160, L"\\Sessions\\%lu\\BaseNamedObjects\\ROBLOX_singletonMutex", sid);

    int n = 0;
    n += close_one(evtPath, 0);
    n += close_one(mtxPath, 1);
    return n;
}

static DWORD WINAPI singleton_worker(LPVOID p) {
    rh_singleton_ctx *c = (rh_singleton_ctx *)p;
    while (InterlockedCompareExchange(&c->running, 1, 1) == 1) {
        rh_singleton_close_once();
        if (!rh_interruptible_sleep(&c->running, c->interval_ms)) break;
    }
    return 0;
}

void rh_singleton_init(rh_singleton_ctx *c) {
    if (!c) return;
    memset(c, 0, sizeof(*c));
}

int rh_singleton_start(rh_singleton_ctx *c, int interval_ms) {
    if (!c) return 0;
    if (InterlockedCompareExchange(&c->running, 1, 0) != 0) return 0;
    c->interval_ms = interval_ms < 500 ? 500 : interval_ms;
    DWORD tid;
    c->thread = CreateThread(NULL, 0, singleton_worker, c, 0, &tid);
    return c->thread != NULL;
}

void rh_singleton_stop(rh_singleton_ctx *c) {
    if (!c) return;
    if (InterlockedCompareExchange(&c->running, 0, 1) != 1) return;
    if (c->thread) {
        WaitForSingleObject(c->thread, 3000);
        CloseHandle(c->thread);
        c->thread = NULL;
    }
}
