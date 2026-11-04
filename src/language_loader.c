#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static unsigned char* g_enc_data = NULL;
static DWORD g_enc_size = 0;

#pragma pack(push, 1)
typedef struct {
    unsigned char original_code[5];
    unsigned char jmp_back[5];
} Trampoline;
#pragma pack(pop)

static Trampoline g_tramp_FindResourceA;
static Trampoline g_tramp_LoadResource;
static Trampoline g_tramp_LockResource;
static Trampoline g_tramp_FreeResource;
static Trampoline g_tramp_SizeofResource;

static void LogMsg(const char* fmt, ...) {
    FILE* f = fopen("sacred_lang.log", "a");
    if (f) {
        va_list args;
        va_start(args, fmt);
        vfprintf(f, fmt, args);
        va_end(args);
        fprintf(f, "\n");
        fclose(f);
    }
}

/*
 * Hooked Win32 Resource API functions.
 * Sacred.exe loads its text strings via FindResourceA(NULL, 107, "BINARY").
 * By intercepting resource ID 107, we feed our decrypted/encrypted translation buffer directly to the engine.
 */

static HRSRC WINAPI Hook_FindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType) {
    if ((DWORD_PTR)lpName == 107 && g_enc_data != NULL) {
        LogMsg("Hook_FindResourceA: Intercepted resource 107 -> returning custom handle");
        return (HRSRC)0x1337;
    }
    return ((HRSRC (WINAPI*)(HMODULE, LPCSTR, LPCSTR))&g_tramp_FindResourceA)(hModule, lpName, lpType);
}

static HGLOBAL WINAPI Hook_LoadResource(HMODULE hModule, HRSRC hResInfo) {
    if (hResInfo == (HRSRC)0x1337) {
        return (HGLOBAL)0x1337;
    }
    return ((HGLOBAL (WINAPI*)(HMODULE, HRSRC))&g_tramp_LoadResource)(hModule, hResInfo);
}

static LPVOID WINAPI Hook_LockResource(HGLOBAL hResData) {
    if (hResData == (HGLOBAL)0x1337) {
        return g_enc_data;
    }
    return ((LPVOID (WINAPI*)(HGLOBAL))&g_tramp_LockResource)(hResData);
}

static BOOL WINAPI Hook_FreeResource(HGLOBAL hResData) {
    if (hResData == (HGLOBAL)0x1337) {
        return TRUE;
    }
    return ((BOOL (WINAPI*)(HGLOBAL))&g_tramp_FreeResource)(hResData);
}

static DWORD WINAPI Hook_SizeofResource(HMODULE hModule, HRSRC hResInfo) {
    if (hResInfo == (HRSRC)0x1337) {
        return g_enc_size;
    }
    return ((DWORD (WINAPI*)(HMODULE, HRSRC))&g_tramp_SizeofResource)(hModule, hResInfo);
}

static void InstallInlineHook(void* target_func, void* hook_func, Trampoline* tramp) {
    if (!target_func || !hook_func || !tramp) return;

    DWORD oldProtect;
    if (!VirtualProtect(target_func, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) return;

    memcpy(tramp->original_code, target_func, 5);
    tramp->jmp_back[0] = 0xE9;
    *(DWORD*)(&tramp->jmp_back[1]) = (DWORD)((DWORD_PTR)target_func + 5) - (DWORD)((DWORD_PTR)&tramp->jmp_back[0] + 5);

    DWORD trampProtect;
    VirtualProtect(tramp, sizeof(Trampoline), PAGE_EXECUTE_READWRITE, &trampProtect);

    unsigned char jmp_hook[5];
    jmp_hook[0] = 0xE9;
    *(DWORD*)(&jmp_hook[1]) = (DWORD)((DWORD_PTR)hook_func) - (DWORD)((DWORD_PTR)target_func + 5);
    memcpy(target_func, jmp_hook, 5);

    VirtualProtect(target_func, 5, oldProtect, &oldProtect);
}

static void TrimString(char* s) {
    char* p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static void LoadAndEncryptTranslation(void) {
    char lang[32] = "us";
    FILE* f_cfg = fopen("Settings.cfg", "r");
    if (f_cfg) {
        char line[256];
        while (fgets(line, sizeof(line), f_cfg)) {
            char* colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                char* k = line;
                char* v = colon + 1;
                TrimString(k);
                TrimString(v);
                if (_stricmp(k, "LANGUAGE") == 0 && strlen(v) > 0) {
                    strncpy(lang, v, sizeof(lang) - 1);
                    lang[sizeof(lang) - 1] = '\0';
                    break;
                }
            }
        }
        fclose(f_cfg);
    }
    LogMsg("Language configured in Settings.cfg: '%s'", lang);

    char path[MAX_PATH];
    FILE* f_res = NULL;

    sprintf(path, "scripts\\%s\\global.res", lang);
    f_res = fopen(path, "rb");

    if (!f_res) {
        char lower_lang[32];
        strncpy(lower_lang, lang, sizeof(lower_lang) - 1);
        lower_lang[sizeof(lower_lang) - 1] = '\0';
        for (int i = 0; lower_lang[i]; i++) lower_lang[i] = (char)tolower(lower_lang[i]);
        sprintf(path, "scripts\\%s\\global.res", lower_lang);
        f_res = fopen(path, "rb");
    }

    if (!f_res) {
        char upper_lang[32];
        strncpy(upper_lang, lang, sizeof(upper_lang) - 1);
        upper_lang[sizeof(upper_lang) - 1] = '\0';
        for (int i = 0; upper_lang[i]; i++) upper_lang[i] = (char)toupper(upper_lang[i]);
        sprintf(path, "scripts\\%s\\global.res", upper_lang);
        f_res = fopen(path, "rb");
    }

    if (!f_res) {
        LogMsg("No external translation file found for language '%s'. Game will use embedded fallback.", lang);
        return;
    }

    fseek(f_res, 0, SEEK_END);
    long sz = ftell(f_res);
    fseek(f_res, 0, SEEK_SET);

    if (sz <= 4 || (sz % 2) != 0) {
        LogMsg("Invalid translation file size: %ld", sz);
        fclose(f_res);
        return;
    }

    unsigned char* plain = (unsigned char*)malloc(sz);
    if (!plain) {
        LogMsg("Memory allocation failed for plain file (%ld bytes)", sz);
        fclose(f_res);
        return;
    }
    fread(plain, 1, sz, f_res);
    fclose(f_res);

    unsigned short* words = (unsigned short*)plain;
    DWORD num_words = (DWORD)(sz / 2);
    unsigned short* enc = (unsigned short*)malloc(sz);
    if (!enc) {
        LogMsg("Memory allocation failed for encrypted buffer (%ld bytes)", sz);
        free(plain);
        return;
    }

    // Ascaron rolling XOR cipher:
    // dec[0] = enc[0] ^ 0x45ad; dec[i] = enc[i-1] ^ enc[i]
    // Inverse (encryption):
    // enc[0] = dec[0] ^ 0x45ad; enc[i] = enc[i-1] ^ dec[i]
    enc[0] = words[0] ^ 0x45ad;
    for (DWORD i = 1; i < num_words; i++) {
        enc[i] = enc[i - 1] ^ words[i];
    }
    free(plain);

    g_enc_data = (unsigned char*)enc;
    g_enc_size = (DWORD)sz;
    LogMsg("Successfully loaded and encrypted '%s' (%u bytes)", path, g_enc_size);

    // Patch Sacred.exe internal language tables in memory at runtime (slot 10)
    // Table 1: 0x89201c, Table 2: 0x8961e0, Table 3: 0x899434
    DWORD oldProtect;
    if (VirtualProtect((LPVOID)0x891f00, 0x9000, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        char upper_lang[16];
        strncpy(upper_lang, lang, sizeof(upper_lang) - 1);
        upper_lang[sizeof(upper_lang) - 1] = '\0';
        for (int i = 0; upper_lang[i]; i++) upper_lang[i] = (char)toupper(upper_lang[i]);

        memset((void*)0x89201c, 0, 16);
        strncpy((char*)0x89201c, upper_lang, 15);

        memset((void*)0x8961e0, 0, 16);
        strncpy((char*)0x8961e0, upper_lang, 15);

        memset((void*)0x899434, 0, 16);
        strncpy((char*)0x899434, upper_lang, 15);

        VirtualProtect((LPVOID)0x891f00, 0x9000, oldProtect, &oldProtect);
        LogMsg("Patched in-memory language tables for '%s'", upper_lang);
    }
}

static void InstallHooks(void) {
    if (!g_enc_data) return;

    HMODULE hK32 = GetModuleHandleA("kernel32.dll");
    if (!hK32) return;

    void* pFindResourceA  = (void*)GetProcAddress(hK32, "FindResourceA");
    void* pLoadResource   = (void*)GetProcAddress(hK32, "LoadResource");
    void* pLockResource   = (void*)GetProcAddress(hK32, "LockResource");
    void* pFreeResource   = (void*)GetProcAddress(hK32, "FreeResource");
    void* pSizeofResource = (void*)GetProcAddress(hK32, "SizeofResource");

    if (pFindResourceA)  InstallInlineHook(pFindResourceA, Hook_FindResourceA, &g_tramp_FindResourceA);
    if (pLoadResource)   InstallInlineHook(pLoadResource, Hook_LoadResource, &g_tramp_LoadResource);
    if (pLockResource)   InstallInlineHook(pLockResource, Hook_LockResource, &g_tramp_LockResource);
    if (pFreeResource)   InstallInlineHook(pFreeResource, Hook_FreeResource, &g_tramp_FreeResource);
    if (pSizeofResource) InstallInlineHook(pSizeofResource, Hook_SizeofResource, &g_tramp_SizeofResource);

    LogMsg("Installed inline hooks in kernel32.dll");
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        DeleteFileA("sacred_lang.log");
        LogMsg("Sacred Language Loader plugin attached to PID %u", GetCurrentProcessId());
        LoadAndEncryptTranslation();
        InstallHooks();
    }
    return TRUE;
}
