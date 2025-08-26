# Cursory

**Cursory** is a lightweight and fast Windows cursor manager.  
Apply full cursor themes with one click, convert Linux XCursor themes to `.cur` and `.ani`, and manage your pointer experience without a hassle.

## Features

### One-Click Cursor Theme Switching

Apply complete Windows cursor themes instantly. Cursory updates all system cursors in a single operation - no need to change each pointer manually.

- Automatically modifies the Windows Registry to apply the new scheme  
- Handles all standard roles: Arrow, Wait, IBeam, Hand, Resize, etc.  
- Optionally reset to the default Windows theme at any time

### Full Support for `.cur` and `.ani` Formats

Cursory fully supports native Windows cursor formats:
- `.cur`: Static cursors (with alpha transparency)  
- `.ani`: Animated cursors with multiple frames

It correctly handles:
- Frame delays and looping for `.ani`  
- Proper structure and metadata  
- Compatibility with all major Windows versions

### XCursor to Windows Cursor Conversion

Convert Linux/X11 cursor themes into Windows compatible cursor sets.

- Import PNG-based XCursor themes or `.xcursor` files  
- Automatically extract cursor images and map metadata  
- Convert standard XCursor names (like `left_ptr`, `xterm`, `hand2`) to Windows cursor roles (`Arrow`, `IBeam`, `Link`, etc.)  
- Batch convert entire themes with consistent format

Supports themes from:
- KDE, GNOME, Xfce, and other Linux desktops  
- Popular cursor packs like Breeze, Adwaita, and Phinger

## Installation

No installer required - just run the executable.  
Cursory does not modify system files and only updates cursor-related registry keys.

> [!WARNING]
> Administrator privileges may be required to apply system-wide changes.

## License

MIT License


## Credits
- *[`libbzip2`](https://sourceware.org/bzip2/)* Implements decompression of `.bz2` files using **libbzip2** with dynamic memory handling.

- *[`lzma`](https://www.7-zip.org/sdk.html)* Contains code using the **7-Zip LZMA SDK** for `.xz` stream decoding.

- *[`miniz`](https://github.com/richgel999/miniz)* Used for `.zip` archive extraction.

- *[`zlib`](https://zlib.net/)* Zlib dependency folder (for gzip support).

- *[`inih`](https://github.com/benhoyt/inih)* INI Parser for c.
