# Third-party components

## JPEXS Free Flash Decompiler 26.3.0

Used as an independent command-line process to inspect and render SWF pages.
Its binaries are downloaded during packaging, not committed to this repository.

- License: GNU GPL v3; bundled dependencies retain their respective licenses.
- Release: https://github.com/jindrapetrik/jpexs-decompiler/releases/tag/version26.3.0
- Corresponding source: https://github.com/jindrapetrik/jpexs-decompiler/tree/version26.3.0
- Source archive: https://github.com/jindrapetrik/jpexs-decompiler/archive/refs/tags/version26.3.0.zip

Packaging preserves the renderer's `license.txt` and the `lib/` license notices.
If distributing a package, preserve these notices and provide access to the
corresponding renderer sources in accordance with its license.

## Eclipse Temurin 17.0.20.1

Downloaded on first conversion, not committed or included in the initial package.
The setup script verifies the pinned Windows x64 JRE archive before extracting it.
The runtime archive includes its own `legal/` license notices.

- Release: https://github.com/adoptium/temurin17-binaries/releases/tag/jdk-17.0.20.1%2B1
- Project: https://github.com/adoptium/temurin17-binaries
