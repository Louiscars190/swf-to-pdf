# SWF to PDF

A small native Windows app for turning page-based Flash documents into PDFs.
Built because old course solution sheets should not require an old Flash player.

## Use it

1. Extract the Windows package into a folder.
2. Open **SWF-to-PDF.exe**.
3. Choose your `.swf` file, or a downloaded `.php` file containing SWF data.
4. Select **Standard** or **High** quality and click **Convert to PDF**.

The app uses the modern Explorer file picker and produces Letter-sized PDFs.
Conversion runs locally. Cancelling leaves your original document and existing
output PDF intact.

## What's native?

The interface, Windows file dialogs, progress/cancellation handling, image encoding,
and PDF assembly are compiled Windows code. There is no Swing or browser UI.

Legacy SWF inspection and rendering use **JPEXS Free Flash Decompiler** as a hidden
command-line process. That engine requires Java. On the first conversion, the app
downloads a private **Eclipse Temurin 17** runtime, verifies its SHA-256 checksum,
and stores it inside the app folder. Java is not installed globally. Subsequent
conversions work offline.

## Build on Windows

Requirements: Windows 10/11 x64, PowerShell, and a MinGW-w64 compiler. With MSYS2,
install the `mingw-w64-x86_64-gcc` toolchain and add its `mingw64/bin` directory
to your PowerShell `PATH`.

```powershell
.\scripts\build.ps1
.\scripts\package.ps1
```

The executable is built into `dist/SWF-to-PDF`. Packaging downloads the pinned
JPEXS release, verifies its checksum, preserves the vendor licenses, and creates
`dist/SWF-to-PDF-Native-Windows.zip`.

The GitHub Actions workflow builds the same package on Windows. It uploads a
build artifact; it does not automatically publish a release.

## Limitations

- Designed for page-based educational SWFs, especially documents containing a
  multipage sprite. Arbitrary interactive Flash apps are not equivalent to PDFs.
- Supports FWS/CWS input; saved HTML player pages and ZWS files are rejected.
- The largest multipage sprite is treated as the document. Other SWF structures
  may need a different page-selection strategy.
- PDF pages are images, so text is not selectable or searchable.
- The app is unsigned, so Windows may show a SmartScreen prompt.

## Validation

The Windows x64 build compiles with warnings treated as errors. Page detection
has been checked on 16-page and 35-page input documents. The native PDF writer's
cross-references, embedded images, and Letter page dimensions have been checked.

The code was developed in a Linux cross-build environment. Windows UI operation,
Windows image-codec execution, and the full Windows conversion workflow need
Windows testing. The file picker is now opened directly on the COM-initialized
UI thread; the earlier blocking helper-process approach has been removed.

## License

The custom application code is MIT licensed. JPEXS and Eclipse Temurin have their
own licenses; see [THIRD_PARTY.md](THIRD_PARTY.md). Course materials and converted
PDFs are not included in this repository.
