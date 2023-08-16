#include <windows.h>
#include <iostream>

typedef struct _UNICODE_STR
{
    USHORT Length;
    USHORT MaximumLength;
    PWSTR pBuffer;
} UNICODE_STR, * PUNICODE_STR;

// structures and definitions taken from:
// https://modexp.wordpress.com/2020/08/06/windows-data-structures-and-callbacks-part-1/

typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA {
    ULONG           Flags;             // Reserved.
    PUNICODE_STR FullDllName;       // The full path name of the DLL module.
    PUNICODE_STR BaseDllName;       // The base file name of the DLL module.
    PVOID           DllBase;           // A pointer to the base address for the DLL in memory.
    ULONG           SizeOfImage;       // The size of the DLL image, in bytes.
} LDR_DLL_LOADED_NOTIFICATION_DATA, * PLDR_DLL_LOADED_NOTIFICATION_DATA;

typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA {
    ULONG           Flags;             // Reserved.
    PUNICODE_STR FullDllName;       // The full path name of the DLL module.
    PUNICODE_STR BaseDllName;       // The base file name of the DLL module.
    PVOID           DllBase;           // A pointer to the base address for the DLL in memory.
    ULONG           SizeOfImage;       // The size of the DLL image, in bytes.
} LDR_DLL_UNLOADED_NOTIFICATION_DATA, * PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

typedef union _LDR_DLL_NOTIFICATION_DATA {
    LDR_DLL_LOADED_NOTIFICATION_DATA   Loaded;
    LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
} LDR_DLL_NOTIFICATION_DATA, * PLDR_DLL_NOTIFICATION_DATA;

typedef VOID(CALLBACK* PLDR_DLL_NOTIFICATION_FUNCTION)(
    ULONG                       NotificationReason,
    PLDR_DLL_NOTIFICATION_DATA  NotificationData,
    PVOID                       Context);

typedef struct _LDR_DLL_NOTIFICATION_ENTRY {
    LIST_ENTRY                     List;
    PLDR_DLL_NOTIFICATION_FUNCTION Callback;
    PVOID                          Context;
} LDR_DLL_NOTIFICATION_ENTRY, * PLDR_DLL_NOTIFICATION_ENTRY;

typedef NTSTATUS(NTAPI* _LdrRegisterDllNotification) (
    ULONG                          Flags,
    PLDR_DLL_NOTIFICATION_FUNCTION NotificationFunction,
    PVOID                          Context,
    PVOID* Cookie);

typedef NTSTATUS(NTAPI* _LdrUnregisterDllNotification)(PVOID Cookie);
typedef NTSTATUS(NTAPI* pLdrUnloadDll)(HMODULE hModule);

BOOLEAN UNICODE_STR_CMP(PUNICODE_STR String1, const wchar_t* String2) {
    int str2len = wcslen(String2) * sizeof(wchar_t);
    if (String1->Length != str2len) {
        return FALSE;
    }

    for (int i = 0; i < (String1->Length / sizeof(WCHAR)); i++) {
        if (String1->pBuffer[i] != String2[i]) {
            return FALSE;
        }
    }

    return TRUE;
}


// Our callback function
VOID DllCallback(ULONG NotificationReason, const PLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context)
{
    const wchar_t* targetDll = L"nonexistent.dll";
    printf("Dll loaded: %Z", NotificationData->Loaded.BaseDllName);

    // Imagine this is an integrity check possibly checking against whitelisted signatures
    if (UNICODE_STR_CMP(NotificationData->Loaded.BaseDllName, targetDll)) {
        printf(" - Integrity check: FAIL!\n");
        HMODULE hTargetDll = GetModuleHandle(targetDll);
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        pLdrUnloadDll LdrUnloadDll = (pLdrUnloadDll)GetProcAddress(hNtdll, "LdrUnloadDll");
        LdrUnloadDll(hTargetDll);
        
        return;
    }

    printf(" - Integrity check: PASS!\n");
    
    return;
}

void loadDll() {

    // Load the DLL
    HMODULE hModule = LoadLibrary(TEXT("nonexistent.dll"));

    if (hModule == NULL) {
        DWORD error = GetLastError();
        std::cout << "Failed to load DLL! Error code: " << error << std::endl;
        return;
    }

    // Unload the DLL
    if (hModule)
        FreeLibrary(hModule);

    return;
}

int main() {

    // Get handle of ntdll
    HMODULE hNtdll = GetModuleHandleA("NTDLL.dll");

    if (hNtdll != NULL) {

        // find the LdrRegisterDllNotification function
        _LdrRegisterDllNotification pLdrRegisterDllNotification = (_LdrRegisterDllNotification)GetProcAddress(hNtdll, "LdrRegisterDllNotification");

        // Register our function MyCallback as a DLL Notification Callback
        PVOID cookie;
        NTSTATUS status = pLdrRegisterDllNotification(0, (PLDR_DLL_NOTIFICATION_FUNCTION)DllCallback, NULL, &cookie);
        if (status == 0) {
            printf("[+] Successfully registered callback\n");
        }
    }

    // Load supposedly non-existent dll
    loadDll();
    Sleep(5000);

    return 0;
}
