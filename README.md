# Yogeshwari Encrypter (multimedia steganography)

Single-file C++ steganography demo and toolkit.

Features
- Render text to a 24-bit BMP (monospace 8x8 font).
- Embed a BMP payload into a WAV file by setting per-sample LSBs (audible carrier preserved).
- Generate a waveform image (BMP/PNG) from WAV and copy payload bits into pixel LSBs.
- Decode the payload from a waveform image and (if the payload is a BMP) extract the rendered text.

Goals for this repo
- Keep the project self-contained and easy to build on Windows (PowerShell) and Unix (make/g++).
- Provide clear docs and a reproducible simple build/run flow for testers and contributors.

Prerequisites
- A C++17-capable compiler (g++ recommended).
- On Windows: PowerShell (ship includes a PowerShell build script).

Build (Windows PowerShell)

Open PowerShell (or a shell) in the repository root and build using g++ (example):

```powershell
# build executable (output named after the source file)
g++ -std=c++17 -O2 "yogeshwari_encrypter_kavi.cpp" -o yogeshwari_encrypter_kavi.exe
```

Or use the helper script:

```powershell
.\build.ps1
```

Build (Unix / Make)

```bash
make
```

Run (interactive)

```powershell
.\yogeshwari_encrypter_kavi.exe
```

Default credentials for demo login (DEMO ONLY):
- username: `abyss`
- password: `B16`

Quick scripted smoke test (PowerShell):

```powershell
$in = "abyss`nB16`n5`n"; $in | .\yogeshwari_encrypter_kavi.exe
```

Full pipeline smoke test (non-interactive)

PowerShell (Windows):

```powershell
$in = "abyss`nB16`n1`nHello, this is a test message.`nn`nmessage1.bmp`n2`nmessage1.bmp`ncarrier1.wav`n3`ncarrier1.wav`nwaveform1.bmp`n4`nwaveform1.bmp`ndecoded1.txt`n5`n"
$in | .\yogeshwari_encrypter_kavi.exe
```

Bash (Linux / macOS):

```bash
printf "abyss\nB16\n5\n" | ./yogeshwari_encrypter_kavi
```

Project layout
- `yogeshwari_encrypter_kavi.cpp` — main single-file program
- `README.md` — this file
- `build.ps1` — PowerShell build helper
- `Makefile` — Unix make helper
- `.gitignore` — ignore binaries and generated files
- `SAMPLES/` — sample input files (message.txt)

Contributing
- See `CONTRIBUTING.md` for a minimal guide.

License
- MIT (see `LICENSE`).

Notes
- The program intentionally attempts to enable ANSI VT on Windows at runtime; some older consoles may not fully support color/formatting.
- For CI/release we suggest adding a GitHub Actions workflow to build on push and run the smoke test.
