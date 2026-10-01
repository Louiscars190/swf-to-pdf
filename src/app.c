#define COBJMACROS
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdint.h>

#define ID_BROWSE 101
#define ID_CONVERT 102
#define ID_CANCEL 103
#define ID_QUALITY 104
#define ID_OPEN 105
#define MSG_PROGRESS (WM_APP + 1)
#define MSG_DONE (WM_APP + 2)
#define PATH_CAP 32768
static HWND win, browse, convert, cancel, quality, bar, openPdf;
static HFONT normalFont, smallFont, boldFont, titleFont;
static wchar_t base[PATH_CAP], source[PATH_CAP], destination[PATH_CAP],
    status[256] = L"Choose a document to get started.";
static volatile LONG busy = 0, cancelled = 0;
static int percent = 0, highQuality = 0, dpi = 96;
static HANDLE engineProcess;
static CRITICAL_SECTION processLock;
static int saved = 0;
static const COLORREF ink = RGB(24, 36, 56), muted = RGB(102, 116, 136), blue = RGB(46, 100, 231),
                      page = RGB(246, 248, 252);
typedef struct {
    int percent;
    wchar_t text[256];
} Progress;
static int S(int n) { return MulDiv(n, dpi, 96); }
static void report(int value, const wchar_t *text) {
    Progress *p = calloc(1, sizeof(*p));
    if (!p)
        return;
    p->percent = value;
    wcsncpy(p->text, text, 255);
    if (!PostMessageW(win, MSG_PROGRESS, 0, (LPARAM)p))
        free(p);
}
static void systemError(const wchar_t *message) {
    MessageBoxW(win, message, L"SWF to PDF", MB_OK | MB_ICONERROR);
}
static void join(wchar_t *out, const wchar_t *root, const wchar_t *leaf) {
    swprintf(out, PATH_CAP, L"%ls\\%ls", root, leaf);
}
static void layout() {
    RECT r;
    GetClientRect(win, &r);
    int width = r.right;
    MoveWindow(browse, width - S(181), S(173), S(128), S(42), TRUE);
    MoveWindow(quality, width - S(220), S(303), S(165), S(220), TRUE);
    MoveWindow(convert, S(32), S(392), S(210), S(46), TRUE);
    MoveWindow(cancel, width - S(128), S(394), S(96), S(42), TRUE);
    MoveWindow(openPdf, width - S(250), S(394), S(110), S(42), TRUE);
    MoveWindow(bar, S(32), S(495), width - S(64), S(8), TRUE);
}
static void drawText(HDC dc, const wchar_t *text, RECT rect, HFONT font, COLORREF color,
                     UINT flags) {
    SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, text, -1, &rect, flags);
}
static RECT box(int x, int y, int width, int height) {
    RECT r = {S(x), S(y), S(x + width), S(y + height)};
    return r;
}
static void card(HDC dc, RECT r) {
    HBRUSH fill = CreateSolidBrush(RGB(255, 255, 255));
    HPEN border = CreatePen(PS_SOLID, 1, RGB(226, 232, 242));
    HGDIOBJ oldBrush = SelectObject(dc, fill), oldPen = SelectObject(dc, border);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, S(18), S(18));
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(fill);
    DeleteObject(border);
}
static void paint(HDC dc) {
    RECT client;
    GetClientRect(win, &client);
    int width = client.right;
    HBRUSH bg = CreateSolidBrush(page);
    FillRect(dc, &client, bg);
    DeleteObject(bg);
    RECT r = box(32, 27, 490, 43);
    drawText(dc, L"SWF to PDF", r, titleFont, ink, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    r = box(32, 76, 540, 23);
    drawText(dc, L"Turn your Flash documents into clean, ready-to-use PDFs.", r, normalFont, muted,
             DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    r = (RECT){width - S(185), S(36), width - S(32), S(80)};
    drawText(dc, L"WINDOWS NATIVE", r, smallFont, muted, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    r = (RECT){S(32), S(129), width - S(32), S(253)};
    card(dc, r);
    r = box(54, 147, 300, 18);
    drawText(dc, L"SOURCE DOCUMENT", r, smallFont, muted, DT_SINGLELINE | DT_VCENTER);
    HBRUSH iconBg = CreateSolidBrush(RGB(234, 240, 255));
    HPEN iconPen = CreatePen(PS_SOLID, S(2), blue);
    HGDIOBJ oldBrush = SelectObject(dc, iconBg),
            oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, S(54), S(179), S(106), S(231), S(14), S(14));
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    SelectObject(dc, iconPen);
    POINT pts[] = {{S(70), S(190)}, {S(83), S(190)}, {S(91), S(198)},
                   {S(91), S(218)}, {S(70), S(218)}, {S(70), S(190)}};
    Polyline(dc, pts, 6);
    MoveToEx(dc, S(83), S(190), NULL);
    LineTo(dc, S(83), S(198));
    LineTo(dc, S(91), S(198));
    MoveToEx(dc, S(75), S(205), NULL);
    LineTo(dc, S(86), S(205));
    MoveToEx(dc, S(75), S(211), NULL);
    LineTo(dc, S(86), S(211));
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(iconBg);
    DeleteObject(iconPen);
    const wchar_t *name = source[0] ? PathFindFileNameW(source) : L"No document selected";
    r = (RECT){S(120), S(178), width - S(198), S(204)};
    drawText(dc, name, r, boldFont, ink, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    wchar_t folder[PATH_CAP];
    if (source[0]) {
        wcscpy(folder, source);
        PathRemoveFileSpecW(folder);
    } else
        wcscpy(folder, L"Choose a .swf or .php file");
    r = (RECT){S(120), S(207), width - S(198), S(230)};
    drawText(dc, folder, r, smallFont, muted, DT_SINGLELINE | DT_VCENTER | DT_PATH_ELLIPSIS);
    r = (RECT){S(32), S(273), width - S(32), S(371)};
    card(dc, r);
    r = box(54, 290, 390, 24);
    drawText(dc, L"Output quality", r, boldFont, ink, DT_SINGLELINE | DT_VCENTER);
    r = (RECT){S(54), S(324), width - S(242), S(350)};
    drawText(dc,
             highQuality ? L"Sharper text and diagrams. Takes a little longer."
                         : L"Clear pages with a smaller file size.",
             r, normalFont, muted, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    r = (RECT){S(32), S(461), width - S(90), S(490)};
    drawText(dc, status, r, normalFont, saved ? RGB(26, 128, 87) : muted,
             DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    wchar_t number[24];
    swprintf(number, 24, L"%d%%", percent);
    r = (RECT){width - S(82), S(461), width - S(32), S(490)};
    drawText(dc, number, r, smallFont, muted, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    r = (RECT){S(32), S(518), width - S(32), S(545)};
    drawText(dc, L"SWF & PHP supported  /  Your files stay on your computer", r, smallFont, muted,
             DT_SINGLELINE | DT_VCENTER);
}
static void buttonDraw(DRAWITEMSTRUCT *d) {
    RECT r = d->rcItem;
    BOOL enabled = IsWindowEnabled(d->hwndItem);
    COLORREF fill =
        enabled ? ((d->itemState & ODS_SELECTED) ? RGB(33, 78, 190) : blue) : RGB(235, 238, 243);
    HBRUSH b = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, enabled ? fill : RGB(214, 219, 227));
    HGDIOBJ ob = SelectObject(d->hDC, b), op = SelectObject(d->hDC, pen);
    RoundRect(d->hDC, r.left, r.top, r.right, r.bottom, S(14), S(14));
    SelectObject(d->hDC, ob);
    SelectObject(d->hDC, op);
    DeleteObject(b);
    DeleteObject(pen);
    drawText(d->hDC, L"Convert to PDF", r, boldFont,
             enabled ? RGB(255, 255, 255) : RGB(145, 153, 165),
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (d->itemState & ODS_FOCUS) {
        InflateRect(&r, -S(4), -S(4));
        DrawFocusRect(d->hDC, &r);
    }
}
static void setBusy(BOOL value) {
    EnableWindow(browse, !value);
    EnableWindow(quality, !value);
    EnableWindow(convert, !value && source[0]);
    EnableWindow(cancel, value);
    if (value)
        ShowWindow(openPdf, SW_HIDE);
    InvalidateRect(win, NULL, FALSE);
}

static void theme() {
    typedef LONG(WINAPI * VersionFn)(OSVERSIONINFOW *);
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    VersionFn version = (VersionFn)GetProcAddress(nt, "RtlGetVersion");
    OSVERSIONINFOW v = {0};
    v.dwOSVersionInfoSize = sizeof(v);
    if (!version || version(&v) != 0 || v.dwMajorVersion < 10 || v.dwBuildNumber < 18362)
        return;
    HMODULE ux = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!ux)
        return;
    typedef int(WINAPI * PreferredFn)(int);
    PreferredFn preferred = (PreferredFn)GetProcAddress(ux, MAKEINTRESOURCEA(135));
    DWORD light = 1, bytes = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER,
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &light, &bytes);
    if (preferred)
        preferred(light ? 1 : 2);
}
static BOOL capturePicker(BOOL save, wchar_t *out) {
    IFileDialog *dialog = NULL;
    IShellItem *folderItem = NULL, *selected = NULL;
    PWSTR selectedPath = NULL;
    BOOL ok = FALSE;
    wchar_t folder[PATH_CAP] = {0}, name[PATH_CAP] = {0};
    out[0] = 0;
    theme();
    HRESULT hr = CoCreateInstance(save ? &CLSID_FileSaveDialog : &CLSID_FileOpenDialog, NULL,
                                  CLSCTX_INPROC_SERVER, &IID_IFileDialog, (void **)&dialog);
    if (FAILED(hr))
        goto failure;
    FILEOPENDIALOGOPTIONS options = 0;
    hr = IFileDialog_GetOptions(dialog, &options);
    if (FAILED(hr))
        goto failure;
    options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
    if (save)
        options |= FOS_OVERWRITEPROMPT | FOS_STRICTFILETYPES;
    else
        options |= FOS_FILEMUSTEXIST;
    hr = IFileDialog_SetOptions(dialog, options);
    if (FAILED(hr))
        goto failure;
    COMDLG_FILTERSPEC openTypes[] = {{L"Flash documents (*.swf; *.php)", L"*.swf;*.php"},
                                     {L"All files", L"*.*"}};
    COMDLG_FILTERSPEC saveTypes[] = {{L"PDF document (*.pdf)", L"*.pdf"}};
    hr = IFileDialog_SetFileTypes(dialog, save ? 1 : 2, save ? saveTypes : openTypes);
    if (FAILED(hr))
        goto failure;
    IFileDialog_SetFileTypeIndex(dialog, 1);
    IFileDialog_SetTitle(dialog, save ? L"Save converted PDF" : L"Choose a SWF or PHP document");
    if (source[0]) {
        wcscpy(folder, source);
        PathRemoveFileSpecW(folder);
    } else
        GetEnvironmentVariableW(L"USERPROFILE", folder, PATH_CAP);
    if (folder[0] &&
        SUCCEEDED(SHCreateItemFromParsingName(folder, NULL, &IID_IShellItem, (void **)&folderItem)))
        IFileDialog_SetFolder(dialog, folderItem);
    if (save) {
        wcscpy(name, PathFindFileNameW(source));
        PathRemoveExtensionW(name);
        wcscat(name, L".pdf");
        IFileDialog_SetFileName(dialog, name);
        IFileDialog_SetDefaultExtension(dialog, L"pdf");
    }
    hr = IFileDialog_Show(dialog, win);
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        goto cleanup;
    if (FAILED(hr))
        goto failure;
    hr = IFileDialog_GetResult(dialog, &selected);
    if (FAILED(hr))
        goto failure;
    hr = IShellItem_GetDisplayName(selected, SIGDN_FILESYSPATH, &selectedPath);
    if (FAILED(hr))
        goto failure;
    if (!selectedPath || wcslen(selectedPath) >= PATH_CAP) {
        hr = HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
        goto failure;
    }
    wcscpy(out, selectedPath);
    ok = TRUE;
    goto cleanup;
failure:
    {
        wchar_t error[256];
        swprintf(error, 256, L"Windows could not open the file picker (0x%08lX). Please try again.",
                 (unsigned long)hr);
        systemError(error);
    }
cleanup:
    if (selectedPath)
        CoTaskMemFree(selectedPath);
    if (selected)
        IShellItem_Release(selected);
    if (folderItem)
        IShellItem_Release(folderItem);
    if (dialog)
        IFileDialog_Release(dialog);
    return ok;
}
static int spriteId = 0, pageCount = 1;
static void parseLine(const char *line, BOOL inspect) {
    if (inspect) {
        const char *p = strstr(line, "DefineSprite (chid: ");
        if (p) {
            int id = 0, a, b, c, d;
            const char *len = strstr(p, "len=");
            if (sscanf(p, "DefineSprite (chid: %d", &id) == 1 && len &&
                sscanf(len, "len= %*u %x %x %x %x", &a, &b, &c, &d) == 4) {
                int frames = c | (d << 8);
                if (frames > pageCount) {
                    spriteId = id;
                    pageCount = frames;
                }
            }
        }
    } else {
        const char *p = strstr(line, "Exported frame ");
        int current, total;
        if (p && sscanf(p, "Exported frame %d/%d", &current, &total) == 2 && total > 0) {
            wchar_t text[128];
            swprintf(text, 128, L"Converting page %d of %d...", current, total);
            report(current * 90 / total, text);
        }
    }
}
static BOOL runJava(const wchar_t *arguments, BOOL inspect) {
    wchar_t java[PATH_CAP], jar[PATH_CAP], cmd[PATH_CAP * 5];
    join(java, base, L"runtime\\bin\\java.exe");
    join(jar, base, L"engine\\ffdec.jar");
    swprintf(cmd, PATH_CAP * 5,
             L"\"%ls\" -Xmx1536m -Djava.awt.headless=true -Dfile.encoding=UTF-8 -jar \"%ls\" %ls",
             java, jar, arguments);
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE readPipe, writePipe;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
        return FALSE;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi = {0};
    BOOL started =
        CreateProcessW(java, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, base, &si, &pi);
    CloseHandle(writePipe);
    if (!started) {
        CloseHandle(readPipe);
        return FALSE;
    }
    CloseHandle(pi.hThread);
    EnterCriticalSection(&processLock);
    engineProcess = pi.hProcess;
    if (cancelled)
        TerminateProcess(engineProcess, 1);
    LeaveCriticalSection(&processLock);
    char buf[4096], line[16384];
    DWORD got;
    int used = 0;
    while (ReadFile(readPipe, buf, sizeof(buf), &got, NULL) && got) {
        for (DWORD i = 0; i < got; i++) {
            if (buf[i] == '\n') {
                line[used] = 0;
                parseLine(line, inspect);
                used = 0;
            } else if (used < (int)sizeof(line) - 1)
                line[used++] = buf[i];
        }
    }
    if (used) {
        line[used] = 0;
        parseLine(line, inspect);
    }
    CloseHandle(readPipe);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit;
    GetExitCodeProcess(pi.hProcess, &exit);
    EnterCriticalSection(&processLock);
    engineProcess = NULL;
    CloseHandle(pi.hProcess);
    LeaveCriticalSection(&processLock);
    return exit == 0 && !cancelled;
}
static void removeTree(const wchar_t *folder) {
    wchar_t pattern[PATH_CAP], path[PATH_CAP];
    join(pattern, folder, L"*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
                continue;
            join(path, folder, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                removeTree(path);
            else
                DeleteFileW(path);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(folder);
}

static BOOL jpegPage(IWICImagingFactory *factory, const wchar_t *file, BYTE **bytes, DWORD *length,
                     UINT *width, UINT *height) {
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *format = NULL;
    IWICBitmapEncoder *encoder = NULL;
    IWICBitmapFrameEncode *encoded = NULL;
    IPropertyBag2 *properties = NULL;
    IStream *stream = NULL;
    HGLOBAL memory = NULL;
    BOOL result = FALSE;
    HRESULT hr;
    hr = IWICImagingFactory_CreateDecoderFromFilename(factory, file, NULL, GENERIC_READ,
                                                      WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr))
        goto done;
    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    if (FAILED(hr))
        goto done;
    IWICBitmapFrameDecode_GetSize(frame, width, height);
    hr = IWICImagingFactory_CreateFormatConverter(factory, &format);
    if (FAILED(hr))
        goto done;
    hr = IWICFormatConverter_Initialize(format, (IWICBitmapSource *)frame,
                                        &GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone, NULL,
                                        0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
        goto done;
    hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
    if (FAILED(hr))
        goto done;
    hr = IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatJpeg, NULL, &encoder);
    if (FAILED(hr))
        goto done;
    hr = IWICBitmapEncoder_Initialize(encoder, stream, WICBitmapEncoderNoCache);
    if (FAILED(hr))
        goto done;
    hr = IWICBitmapEncoder_CreateNewFrame(encoder, &encoded, &properties);
    if (FAILED(hr))
        goto done;
    PROPBAG2 option = {0};
    option.pstrName = L"ImageQuality";
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_R4;
    value.fltVal = .96f;
    IPropertyBag2_Write(properties, 1, &option, &value);
    hr = IWICBitmapFrameEncode_Initialize(encoded, properties);
    if (FAILED(hr))
        goto done;
    hr = IWICBitmapFrameEncode_SetSize(encoded, *width, *height);
    if (FAILED(hr))
        goto done;
    WICPixelFormatGUID pf = GUID_WICPixelFormat24bppBGR;
    hr = IWICBitmapFrameEncode_SetPixelFormat(encoded, &pf);
    if (FAILED(hr))
        goto done;
    hr = IWICBitmapFrameEncode_WriteSource(encoded, (IWICBitmapSource *)format, NULL);
    if (FAILED(hr))
        goto done;
    hr = IWICBitmapFrameEncode_Commit(encoded);
    if (FAILED(hr))
        goto done;
    hr = IWICBitmapEncoder_Commit(encoder);
    if (FAILED(hr))
        goto done;
    STATSTG stat;
    hr = IStream_Stat(stream, &stat, STATFLAG_NONAME);
    if (FAILED(hr) || stat.cbSize.QuadPart > 0xffffffff)
        goto done;
    *length = (DWORD)stat.cbSize.QuadPart;
    hr = GetHGlobalFromStream(stream, &memory);
    if (FAILED(hr))
        goto done;
    void *data = GlobalLock(memory);
    if (!data)
        goto done;
    *bytes = malloc(*length);
    if (*bytes) {
        memcpy(*bytes, data, *length);
        result = TRUE;
    }
    GlobalUnlock(memory);
done:
    if (properties)
        IPropertyBag2_Release(properties);
    if (encoded)
        IWICBitmapFrameEncode_Release(encoded);
    if (encoder)
        IWICBitmapEncoder_Release(encoder);
    if (stream)
        IStream_Release(stream);
    if (format)
        IWICFormatConverter_Release(format);
    if (frame)
        IWICBitmapFrameDecode_Release(frame);
    if (decoder)
        IWICBitmapDecoder_Release(decoder);
    return result;
}
static BOOL makePdf(const wchar_t *folder, const wchar_t *out) {
    IWICImagingFactory *factory = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                  &IID_IWICImagingFactory, (void **)&factory);
    if (FAILED(hr))
        return FALSE;
    FILE *f = _wfopen(out, L"wb");
    if (!f) {
        IWICImagingFactory_Release(factory);
        return FALSE;
    }
    int count = 2 + pageCount * 3;
    int64_t *offset = calloc(count + 1, sizeof(*offset));
    BOOL ok = FALSE;
    if (!offset)
        goto done;
    fprintf(f, "%%PDF-1.4\n%%\342\343\317\323\n");
    offset[1] = _ftelli64(f);
    fprintf(f, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    offset[2] = _ftelli64(f);
    fprintf(f, "2 0 obj\n<< /Type /Pages /Count %d /Kids [", pageCount);
    for (int i = 0; i < pageCount; i++)
        fprintf(f, "%d 0 R ", 3 + i * 3);
    fprintf(f, "] >>\nendobj\n");
    for (int i = 0; i < pageCount; i++) {
        if (cancelled)
            goto done;
        wchar_t image[PATH_CAP], leaf[64];
        swprintf(leaf, 64, L"%d.png", i + 1);
        join(image, folder, leaf);
        BYTE *jpeg = NULL;
        DWORD length = 0;
        UINT w = 0, h = 0;
        if (!jpegPage(factory, image, &jpeg, &length, &w, &h))
            goto done;
        int id = 3 + i * 3;
        double scale = 612.0 / w;
        if (792.0 / h < scale)
            scale = 792.0 / h;
        double pw = w * scale, ph = h * scale;
        char content[256];
        int contentLength =
            snprintf(content, sizeof(content), "q %.4f 0 0 %.4f %.4f %.4f cm /Im0 Do Q\n", pw, ph,
                     (612 - pw) / 2, (792 - ph) / 2);
        offset[id] = _ftelli64(f);
        fprintf(f,
                "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << "
                "/XObject << /Im0 %d 0 R >> >> /Contents %d 0 R >>\nendobj\n",
                id, id + 1, id + 2);
        offset[id + 1] = _ftelli64(f);
        fprintf(f,
                "%d 0 obj\n<< /Type /XObject /Subtype /Image /Width %u /Height %u /ColorSpace "
                "/DeviceRGB /BitsPerComponent 8 /Filter /DCTDecode /Length %lu >>\nstream\n",
                id + 1, w, h, (unsigned long)length);
        size_t written = fwrite(jpeg, 1, length, f);
        free(jpeg);
        if (written != length)
            goto done;
        fprintf(f, "\nendstream\nendobj\n");
        offset[id + 2] = _ftelli64(f);
        fprintf(f, "%d 0 obj\n<< /Length %d >>\nstream\n%sendstream\nendobj\n", id + 2,
                contentLength, content);
        wchar_t text[128];
        swprintf(text, 128, L"Finishing PDF, page %d of %d...", i + 1, pageCount);
        report(90 + (i + 1) * 9 / pageCount, text);
    }
    int64_t xref = _ftelli64(f);
    fprintf(f, "xref\n0 %d\n0000000000 65535 f \n", count + 1);
    for (int i = 1; i <= count; i++)
        fprintf(f, "%010lld 00000 n \n", (long long)offset[i]);
    fprintf(f, "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%lld\n%%%%EOF\n", count + 1,
            (long long)xref);
    ok = !ferror(f);
done:
    free(offset);
    if (fclose(f) != 0)
        ok = FALSE;
    IWICImagingFactory_Release(factory);
    return ok && !cancelled;
}
static DWORD WINAPI worker(void *unused) {
    wchar_t temp[PATH_CAP] = {0}, pending[PATH_CAP] = {0}, args[PATH_CAP * 3], pageFolder[PATH_CAP],
            outputFolder[PATH_CAP];
    BOOL ok = FALSE;
    const wchar_t *error = L"Conversion failed. Please check the input file and try again.";
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    FILE *input = _wfopen(source, L"rb");
    char signature[3];
    if (!input || fread(signature, 1, 3, input) != 3) {
        if (input)
            fclose(input);
        error = L"The input file could not be read.";
        goto done;
    }
    fclose(input);
    if (memcmp(signature, "CWS", 3) && memcmp(signature, "FWS", 3)) {
        error = L"This file is not a supported SWF. A downloaded PHP file must contain SWF data, "
                L"not a saved player webpage.";
        goto done;
    }
    report(0, L"Reading your document...");
    spriteId = 0;
    pageCount = 1;
    swprintf(args, PATH_CAP * 3, L"-dumpSWF \"%ls\"", source);
    if (!runJava(args, TRUE))
        goto done;
    wchar_t root[PATH_CAP];
    GetTempPathW(PATH_CAP, root);
    if (!GetTempFileNameW(root, L"swf", 0, temp))
        goto done;
    DeleteFileW(temp);
    if (!CreateDirectoryW(temp, NULL))
        goto done;
    if (spriteId) {
        swprintf(args, PATH_CAP * 3,
                 L"-selectid %d -zoom %d -format sprite:png -export sprite \"%ls\" \"%ls\"",
                 spriteId, highQuality ? 3 : 2, temp, source);
        wchar_t leaf[64];
        swprintf(leaf, 64, L"DefineSprite_%d", spriteId);
        join(pageFolder, temp, leaf);
    } else {
        swprintf(args, PATH_CAP * 3, L"-zoom %d -format frame:png -export frame \"%ls\" \"%ls\"",
                 highQuality ? 3 : 2, temp, source);
        join(pageFolder, temp, L"frames");
    }
    if (!runJava(args, FALSE))
        goto done;
    if (cancelled)
        goto done;
    report(90, L"Building your PDF...");
    wcscpy(outputFolder, destination);
    PathRemoveFileSpecW(outputFolder);
    if (!GetTempFileNameW(outputFolder, L"pdf", 0, pending)) {
        error = L"Cannot save in that folder. Choose a writable folder.";
        goto done;
    }
    if (!makePdf(pageFolder, pending))
        goto done;
    if (cancelled)
        goto done;
    if (!MoveFileExW(pending, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = L"Cannot replace the PDF. Close it in your PDF viewer and try again.";
        goto done;
    }
    pending[0] = 0;
    ok = TRUE;
done:
    if (pending[0])
        DeleteFileW(pending);
    if (temp[0])
        removeTree(temp);
    CoUninitialize();
    wchar_t *message = _wcsdup(cancelled ? L"Conversion cancelled."
                               : ok      ? L"PDF saved. You're ready to go."
                                         : error);
    PostMessageW(win, MSG_DONE, ok, (LPARAM)message);
    return 0;
}
static void begin() {
    wchar_t java[PATH_CAP];
    join(java, base, L"runtime\\bin\\java.exe");
    if (GetFileAttributesW(java) == INVALID_FILE_ATTRIBUTES) {
        wchar_t setup[PATH_CAP], ps[PATH_CAP], params[PATH_CAP + 128];
        join(setup, base, L"setup-runtime.ps1");
        GetSystemDirectoryW(ps, PATH_CAP);
        wcscat(ps, L"\\WindowsPowerShell\\v1.0\\powershell.exe");
        swprintf(params, PATH_CAP + 128, L"-NoProfile -ExecutionPolicy Bypass -File \"%ls\"",
                 setup);
        MessageBoxW(win,
                    L"Installing the conversion components. This only happens once and needs an "
                    L"internet connection.\n\nThe app will reopen when setup finishes.",
                    L"First-time setup", MB_OK | MB_ICONINFORMATION);
        ShellExecuteW(win, L"open", ps, params, base, SW_SHOWNORMAL);
        DestroyWindow(win);
        return;
    }
    destination[0] = 0;
    if (!capturePicker(TRUE, destination))
        return;
    saved = 0;
    percent = 0;
    InterlockedExchange(&cancelled, 0);
    InterlockedExchange(&busy, 1);
    wcscpy(status, L"Reading your document...");
    SendMessageW(bar, PBM_SETPOS, 0, 0);
    setBusy(TRUE);
    HANDLE thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (thread)
        CloseHandle(thread);
    else {
        busy = 0;
        setBusy(FALSE);
        systemError(L"Could not start conversion.");
    }
}
static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        win = hwnd;
        browse = CreateWindowW(L"BUTTON", L"Browse files",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd,
                               (HMENU)ID_BROWSE, NULL, NULL);
        convert = CreateWindowW(L"BUTTON", L"Convert to PDF",
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0, hwnd,
                                (HMENU)ID_CONVERT, NULL, NULL);
        cancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0,
                               hwnd, (HMENU)ID_CANCEL, NULL, NULL);
        openPdf = CreateWindowW(L"BUTTON", L"Open PDF", WS_CHILD | WS_TABSTOP, 0, 0, 0, 0, hwnd,
                                (HMENU)ID_OPEN, NULL, NULL);
        quality =
            CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                          0, 0, 0, 0, hwnd, (HMENU)ID_QUALITY, NULL, NULL);
        SendMessageW(quality, CB_ADDSTRING, 0, (LPARAM)L"Standard");
        SendMessageW(quality, CB_ADDSTRING, 0, (LPARAM)L"High");
        SendMessageW(quality, CB_SETCURSEL, 0, 0);
        SendMessageW(quality, CB_SETITEMHEIGHT, (WPARAM)-1, S(30));
        bar = CreateWindowW(PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 0, 0, 0, 0,
                            hwnd, NULL, NULL, NULL);
        SendMessageW(bar, PBM_SETRANGE32, 0, 100);
        SendMessageW(bar, PBM_SETBARCOLOR, 0, blue);
        HWND controls[] = {browse, convert, cancel, openPdf, quality};
        for (int i = 0; i < 5; i++)
            SendMessageW(controls[i], WM_SETFONT, (WPARAM)normalFont, TRUE);
        EnableWindow(cancel, FALSE);
        EnableWindow(convert, FALSE);
        layout();
        return 0;
    case WM_SIZE:
        layout();
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lp)->ptMinTrackSize.x = S(720);
        ((MINMAXINFO *)lp)->ptMinTrackSize.y = S(600);
        return 0;
    case WM_COMMAND:
        if (HIWORD(wp) == CBN_SELCHANGE && LOWORD(wp) == ID_QUALITY) {
            highQuality = SendMessageW(quality, CB_GETCURSEL, 0, 0) == 1;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        switch (LOWORD(wp)) {
        case ID_BROWSE: {
            wchar_t selected[PATH_CAP] = {0};
            if (capturePicker(FALSE, selected)) {
                wcscpy(source, selected);
                percent = 0;
                saved = 0;
                wcscpy(status, L"Ready to convert.");
                SendMessageW(bar, PBM_SETPOS, 0, 0);
                EnableWindow(convert, TRUE);
                ShowWindow(openPdf, SW_HIDE);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        } break;
        case ID_CONVERT:
            begin();
            break;
        case ID_CANCEL:
            InterlockedExchange(&cancelled, 1);
            EnableWindow(cancel, FALSE);
            wcscpy(status, L"Cancelling...");
            EnterCriticalSection(&processLock);
            if (engineProcess)
                TerminateProcess(engineProcess, 1);
            LeaveCriticalSection(&processLock);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case ID_OPEN:
            ShellExecuteW(hwnd, L"open", destination, NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        return 0;
    case MSG_PROGRESS: {
        Progress *p = (Progress *)lp;
        percent = p->percent;
        wcscpy(status, p->text);
        SendMessageW(bar, PBM_SETPOS, percent, 0);
        free(p);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case MSG_DONE:
        busy = 0;
        saved = wp ? 1 : 0;
        percent = saved ? 100 : 0;
        wcscpy(status, (wchar_t *)lp);
        if (!saved && !cancelled)
            systemError((wchar_t *)lp);
        free((void *)lp);
        SendMessageW(bar, PBM_SETPOS, percent, 0);
        setBusy(FALSE);
        ShowWindow(openPdf, saved ? SW_SHOW : SW_HIDE);
        return 0;
    case WM_DRAWITEM:
        if (wp == ID_CONVERT) {
            buttonDraw((DRAWITEMSTRUCT *)lp);
            return TRUE;
        }
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT r;
        GetClientRect(hwnd, &r);
        HDC buffer = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, r.right, r.bottom);
        HGDIOBJ old = SelectObject(buffer, bitmap);
        paint(buffer);
        BitBlt(dc, 0, 0, r.right, r.bottom, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, old);
        DeleteObject(bitmap);
        DeleteDC(buffer);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        if (busy) {
            MessageBoxW(hwnd, L"Cancel the conversion before closing.", L"SWF to PDF",
                        MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR args, int show) {
    HRESULT comInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(comInit)) {
        MessageBoxW(NULL, L"Windows could not initialize the application.", L"SWF to PDF",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    GetModuleFileNameW(NULL, base, PATH_CAP);
    PathRemoveFileSpecW(base);
    InitializeCriticalSection(&processLock);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    dpi = GetDpiForSystem();
    normalFont = CreateFontW(-S(13), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                             CLEARTYPE_QUALITY, 0, L"Segoe UI");
    smallFont = CreateFontW(-S(11), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                            CLEARTYPE_QUALITY, 0, L"Segoe UI");
    boldFont = CreateFontW(-S(15), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                           CLEARTYPE_QUALITY, 0, L"Segoe UI");
    titleFont = CreateFontW(-S(30), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                            CLEARTYPE_QUALITY, 0, L"Segoe UI");
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    WNDCLASSW cls = {0};
    cls.hInstance = instance;
    cls.lpfnWndProc = wndProc;
    cls.lpszClassName = L"NativeSwfPdfWindow";
    cls.hCursor = LoadCursor(NULL, IDC_ARROW);
    cls.hIcon = LoadIcon(instance, MAKEINTRESOURCE(1));
    RegisterClassW(&cls);
    HWND hwnd = CreateWindowW(cls.lpszClassName, L"SWF to PDF", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                              CW_USEDEFAULT, S(760), S(610), NULL, NULL, instance, NULL);
    if (!hwnd)
        return 1;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    DeleteObject(normalFont);
    DeleteObject(smallFont);
    DeleteObject(boldFont);
    DeleteObject(titleFont);
    DeleteCriticalSection(&processLock);
    CoUninitialize();
    return 0;
}
