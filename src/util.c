#include "common.h"
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void EnsureDir(const WCHAR *dir)
{
    if (dir && *dir && !CreateDirectoryW(dir, NULL)) {
        /* ERROR_ALREADY_EXISTS is fine; anything else we silently tolerate
           because the caller will fail later with a clearer error. */
    }
}

void ResolvePaths(AppPaths *p)
{
    if (GetModuleFileNameW(NULL, p->exeDir, MAX_PATH) == 0)
        p->exeDir[0] = L'\0';
    PathRemoveFileSpecW(p->exeDir);

    /* Legacy stores (pre-2.0 layout: components beside the exe) keep being
     * used in place so an update never triggers a ~2.3 GB re-download.
     * Fresh installs use the per-user %LOCALAPPDATA%\Scribely store, which
     * survives moving or reinstalling the exe. */
    WCHAR legacyMark[MAX_PATH], legacyFfmpeg[MAX_PATH];
    PathCombineW(legacyMark,   p->exeDir, L"installed.cfg");
    PathCombineW(legacyFfmpeg, p->exeDir, L"bin\\ffmpeg.exe");

    WCHAR appData[MAX_PATH];
    if (FileExists(legacyMark) || FileExists(legacyFfmpeg)) {
        lstrcpynW(p->dataDir, p->exeDir, MAX_PATH);
    } else if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL,
                                          SHGFP_TYPE_CURRENT, appData))) {
        PathCombineW(p->dataDir, appData, L"Scribely");
        EnsureDir(p->dataDir);
    } else {
        lstrcpynW(p->dataDir, p->exeDir, MAX_PATH);
    }

    PathCombineW(p->bin,     p->dataDir, L"bin");
    PathCombineW(p->whisper, p->bin,     L"whisper");
    PathCombineW(p->llama,   p->bin,     L"llama");
    PathCombineW(p->models,  p->dataDir, L"models");
    PathCombineW(p->temp,    p->dataDir, L"temp");
    PathCombineW(p->output,  p->dataDir, L"output");

    EnsureDir(p->bin);
    EnsureDir(p->whisper);
    EnsureDir(p->llama);
    EnsureDir(p->models);
    EnsureDir(p->temp);
    EnsureDir(p->output);

    /* Default tool resolution: the bundled copies.  The bootstrap swaps in
     * a validated system ffmpeg / yt-dlp when the bundled one is absent. */
    PathCombineW(p->ffmpegExe, p->bin, L"ffmpeg.exe");
    PathCombineW(p->ytdlpExe,  p->bin, L"yt-dlp.exe");
}

void PathUnder(const AppPaths *p, WCHAR *out, const WCHAR *rel)
{
    if (!PathCombineW(out, p->dataDir, rel))
        out[0] = L'\0';
}

/* Strict gate: only http(s) URLs whose host is youtu.be or *.youtube.com,
   and with no whitespace / shell-hostile characters embedded. */
BOOL IsPlausibleYouTubeUrl(const WCHAR *u)
{
    if (!u) return FALSE;
    while (*u == L' ' || *u == L'\t') u++;

    size_t len = wcslen(u);
    if (len < 11 || len >= 2048) return FALSE;

    for (const WCHAR *q = u; *q; ++q) {
        WCHAR c = *q;
        if (c < 0x20 || c == L' ' || c == L'"' || c == L'\'' ||
            c == L'<'  || c == L'>' || c == L'|' || c == L'^' || c == L'`')
            return FALSE;
    }

    const WCHAR *p;
    if (_wcsnicmp(u, L"https://", 8) == 0)      p = u + 8;
    else if (_wcsnicmp(u, L"http://", 7) == 0)  p = u + 7;
    else return FALSE;

    WCHAR host[256];
    size_t hi = 0;
    while (*p && *p != L'/' && *p != L'?' && *p != L'#' && *p != L':' && hi < 255)
        host[hi++] = *p++;
    host[hi] = L'\0';

    if (_wcsicmp(host, L"youtu.be") == 0) return TRUE;

    const WCHAR *suf = L"youtube.com";
    size_t sl = wcslen(suf);
    size_t hl = wcslen(host);
    if (hl >= sl && _wcsnicmp(host + (hl - sl), suf, sl) == 0 &&
        (hl == sl || host[hl - sl - 1] == L'.'))
        return TRUE;

    return FALSE;
}

/* Local media files accepted for transcription: anything ffmpeg can turn
   into 16 kHz WAV.  The path must exist as a real file and carry one of
   these audio/video extensions. */
BOOL IsSupportedMediaFile(const WCHAR *path)
{
    if (!path || !*path) return FALSE;

    static const WCHAR *const exts[] = {
        L".mp3", L".wav", L".flac", L".ogg", L".oga", L".opus", L".m4a",
        L".aac", L".wma", L".mka", L".aif", L".aiff", L".amr",
        L".mp4", L".m4v", L".mkv", L".webm", L".mov", L".avi", L".wmv",
        L".ts", L".mts", L".m2ts", L".3gp", L".flv", L".mpg", L".mpeg"
    };

    const WCHAR *ext = PathFindExtensionW(path);
    if (!ext || !*ext) return FALSE;

    for (size_t i = 0; i < ARRAYSIZE(exts); ++i)
        if (_wcsicmp(ext, exts[i]) == 0)
            return FileExists(path);
    return FALSE;
}

char *WideToUtf8(const WCHAR *w)
{
    if (!w) return NULL;
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return NULL;
    char *s = (char *)malloc((size_t)n);
    if (!s) return NULL;
    if (WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL) <= 0) {
        free(s);
        return NULL;
    }
    return s;
}

WCHAR *Utf8ToWide(const char *s)
{
    if (!s) return NULL;
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) return NULL;
    WCHAR *w = (WCHAR *)malloc((size_t)n * sizeof(WCHAR));
    if (!w) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n) <= 0) {
        free(w);
        return NULL;
    }
    return w;
}

char *ReadFileUtf8(const WCHAR *path)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size) || size.QuadPart < 0 ||
        size.QuadPart > 0x7FFFFFF0) {
        CloseHandle(h);
        return NULL;
    }

    size_t total = (size_t)size.QuadPart;
    char *buf = (char *)malloc(total + 1);
    if (!buf) { CloseHandle(h); return NULL; }

    size_t got = 0;
    while (got < total) {
        DWORD chunk = (DWORD)((total - got > 0x10000000) ? 0x10000000 : (total - got));
        DWORD rd = 0;
        if (!ReadFile(h, buf + got, chunk, &rd, NULL) || rd == 0) break;
        got += rd;
    }
    CloseHandle(h);
    buf[got] = '\0';

    /* Strip a UTF-8 BOM if present. */
    if (got >= 3 && (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) {
        memmove(buf, buf + 3, got - 3 + 1);
    }
    return buf;
}

BOOL WriteFileUtf8(const WCHAR *path, const char *utf8)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;

    BOOL ok = TRUE;
    size_t total = utf8 ? strlen(utf8) : 0;
    size_t put = 0;
    while (put < total) {
        DWORD chunk = (DWORD)((total - put > 0x10000000) ? 0x10000000 : (total - put));
        DWORD wr = 0;
        if (!WriteFile(h, utf8 + put, chunk, &wr, NULL) || wr == 0) { ok = FALSE; break; }
        put += wr;
    }
    CloseHandle(h);
    return ok;
}

BOOL FileExists(const WCHAR *path)
{
    DWORD a = GetFileAttributesW(path);
    return (a != INVALID_FILE_ATTRIBUTES) &&
           !(a & FILE_ATTRIBUTE_DIRECTORY);
}

int CpuThreads(void)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int n = (int)si.dwNumberOfProcessors;
    if (n < 1) n = 1;
    if (n > 16) n = 16;
    return n;
}
