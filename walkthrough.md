# Walkthrough - click.exe Windows CLI Utility

We have successfully built, compiled, and tested `click.exe`, a command-line file opener for Windows.

## Project Details
- **Project Directory**: [click-cli](file:///C:/Users/scilook/.gemini/antigravity/scratch/click-cli)
- **Source Code**: [main.cpp](file:///C:/Users/scilook/.gemini/antigravity/scratch/click-cli/main.cpp)
- **Build Script**: [build.bat](file:///C:/Users/scilook/.gemini/antigravity/scratch/click-cli/build.bat)
- **Compiled Binary**: [click.exe](file:///C:/Users/scilook/.gemini/antigravity/scratch/click-cli/click.exe)

> [!TIP]
> Since we compiled it using the available `g++` with the `-static` flag, `click.exe` is a standalone native binary with zero external runtime DLL dependencies, starting instantly and taking up very little disk space.

---

## Usage Guide

| Command | Action | Description |
| :--- | :--- | :--- |
| `click <file_path>` | Default Open | Mimics double-clicking a file in the GUI. Opens the file with its default registered handler. |
| `click -a <app_path> <file_path>` | Direct Mapping | Opens the file using the specified application executable path directly. |
| `click -e <extension> <file_path>` | Indirect Mapping | Looks up the registry association for the specified extension (e.g. `txt`), and opens the file using that application. |
| `click --install` | System PATH Install | Appends the current folder of `click.exe` to your User `PATH` environment variable and broadcasts system changes. |
| `click --uninstall` | System PATH Uninstall | Removes the folder of `click.exe` from your User `PATH` environment variable. |
| `click -h` / `--help` | Help Menu | Prints usage guidelines. |

---

## Verification Results

We verified every feature of `click.exe`:

### 1. Default Open Test
We created a sample text file [test.txt](file:///C:/Users/scilook/.gemini/antigravity/scratch/click-cli/test.txt) and ran:
```powershell
.\click.exe test.txt
```
**Output**:
```
Opening C:\Users\scilook\.gemini\antigravity\scratch\click-cli\test.txt...
```
*(Opened the system default text editor successfully).*

### 2. Direct Specification Test
We forced opening the text file with standard Windows Notepad:
```powershell
.\click.exe -a C:\Windows\System32\notepad.exe test.txt
```
**Output**:
```
Opening C:\Users\scilook\.gemini\antigravity\scratch\click-cli\test.txt with C:\Windows\System32\notepad.exe...
```
*(Opened the file using Notepad successfully).*

### 3. Indirect Specification Test
We forced opening a JSON file [test.json](file:///C:/Users/scilook/.gemini/antigravity/scratch/click-cli/test.json) using the editor associated with `.txt` files in the registry:
```powershell
.\click.exe -e txt test.json
```
**Output**:
```
Opening C:\Users\scilook\.gemini\antigravity\scratch\click-cli\test.json with C:\Program Files\WindowsApps\Microsoft.WindowsNotepad_11.2604.5.0_x64__8wekyb3d8bbwe\Notepad\Notepad.exe...
```
*(Resolved the default editor for `.txt` as the modern Notepad UWP package, and successfully launched it to edit the JSON file).*

### 4. Installer / Uninstaller Registry Verification
We tested installation:
```powershell
.\click.exe --install
```
**Output**:
```
Successfully added click directory to User PATH environment variable:
  C:\Users\scilook\.gemini\antigravity\scratch\click-cli
Please restart your shell (PowerShell/CMD) to apply changes.
```
- Querying the User `PATH` via registry confirmed the folder was successfully appended.
- Running `.\click.exe --uninstall` removed the directory cleanly.
- We re-ran `.\click.exe --install` to leave it ready for global use.
