#!/usr/bin/env python3
"""Сборка PvZ TBG Mod. Работает на Windows, Linux и macOS: нужен только Python 3 и `pip install ziglang`
(zig кросс-компилирует 32-битные Windows-файлы; MSVC и MinGW не нужны).

  python scripts/build.py test      # прогнать тесты логики (на подделанной памяти игры)
  python scripts/build.py build     # собрать build/pvzmod.dll и build/PvZModLauncher.exe
  python scripts/build.py package   # собрать и упаковать dist/PvZ-TBG-Mod-<версия>.zip
  python scripts/build.py all       # test + build + package
"""
import os, re, shutil, subprocess, sys, zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SRC, TESTS, IMGUI = (os.path.join(ROOT, p) for p in ("src", "tests", os.path.join("third_party", "imgui")))
BUILD, DIST = os.path.join(ROOT, "build"), os.path.join(ROOT, "dist")
ZIG = [sys.executable, "-m", "ziglang"]


def version():
    text = open(os.path.join(SRC, "version.h"), encoding="utf-8").read()
    return re.search(r'TBG_VERSION\s+"([^"]+)"', text).group(1)


def run(cmd):
    print("+", " ".join(os.path.relpath(c, ROOT) if os.path.isabs(c) and c.startswith(ROOT) else c for c in cmd), flush=True)
    subprocess.run(cmd, check=True, cwd=ROOT)


def make_rc(kind):
    """Ресурсы: иконка (id 1) и сведения о версии."""
    v = version(); nums = (v.split(".") + ["0", "0", "0", "0"])[:4]
    comma = ",".join(nums)
    fname = "pvzmod.dll" if kind == "dll" else "PvZModLauncher.exe"
    desc = "PvZ TBG Mod - mod menu" if kind == "dll" else "PvZ TBG Mod - launcher"
    ico = "../assets/icon.ico"            # путь относительно build/app_*.rc
    rc = f'''1 ICON "{ico}"
1 VERSIONINFO
FILEVERSION {comma}
PRODUCTVERSION {comma}
FILEOS 0x4
FILETYPE {"0x2" if kind == "dll" else "0x1"}
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904B0"
    BEGIN
      VALUE "ProductName", "PvZ TBG Mod"
      VALUE "FileDescription", "{desc}"
      VALUE "FileVersion", "{v}"
      VALUE "ProductVersion", "{v}"
      VALUE "OriginalFilename", "{fname}"
      VALUE "LegalCopyright", "MIT License"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x409, 1200
  END
END
'''
    os.makedirs(BUILD, exist_ok=True)
    path = os.path.join(BUILD, f"app_{kind}.rc")
    with open(path, "w", encoding="utf-8") as f:
        f.write(rc)
    return path


def cmd_test():
    os.makedirs(BUILD, exist_ok=True)
    exe = os.path.join(BUILD, "test_logic" + (".exe" if os.name == "nt" else ""))
    # тест собирается под 32-битный Linux (musl, статически); на Windows запускайте через WSL или смотрите CI
    run(ZIG + ["c++", "-target", "x86-linux-musl", "-static", "-std=c++17", "-I" + SRC, "-I" + TESTS,
               os.path.join(TESTS, "test_logic.cpp"), "-o", exe])
    run([exe])


def cmd_build():
    os.makedirs(BUILD, exist_ok=True)
    cxx = ZIG + ["c++", "-target", "x86-windows-gnu", "-O2", "-std=c++17", "-fno-rtti"]
    inc = ["-I" + IMGUI, "-I" + os.path.join(IMGUI, "backends"), "-I" + SRC]
    imgui = [os.path.join(IMGUI, f) for f in ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")] + \
            [os.path.join(IMGUI, "backends", f) for f in ("imgui_impl_win32.cpp", "imgui_impl_dx11.cpp")]
    run(cxx + ["-shared", "-s"] + inc + [os.path.join(SRC, "mod.cpp"), os.path.join(SRC, "ui.cpp")] + imgui + [make_rc("dll"),
        "-o", os.path.join(BUILD, "pvzmod.dll"),
        "-ld3d11", "-ldxgi", "-ld3dcompiler_47", "-ldwmapi", "-limm32", "-lgdi32", "-luser32", "-lole32", "-ladvapi32"])
    run(cxx + ["-s", "-Wl,--subsystem,windows", "-municode", "-I" + SRC, os.path.join(SRC, "launcher.cpp"), make_rc("exe"),
        "-o", os.path.join(BUILD, "PvZModLauncher.exe"), "-ladvapi32", "-luser32", "-lshell32"])
    for n in ("pvzmod.dll", "PvZModLauncher.exe"):
        print(f"  {n}: {os.path.getsize(os.path.join(BUILD, n)):,} байт")


def cmd_package():
    v = version()
    os.makedirs(DIST, exist_ok=True)
    out = os.path.join(DIST, f"PvZ-TBG-Mod-{v}.zip")
    top = "PvZ-TBG-Mod"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for n in ("pvzmod.dll", "PvZModLauncher.exe"):
            z.write(os.path.join(BUILD, n), f"{top}/{n}")
        z.write(os.path.join(ROOT, "docs", "INSTALL.ru.md"), f"{top}/УСТАНОВКА.md")
        z.write(os.path.join(ROOT, "docs", "INSTALL.en.md"), f"{top}/INSTALL.md")
        z.write(os.path.join(ROOT, "LICENSE"), f"{top}/LICENSE")
        z.write(os.path.join(ROOT, "THIRD_PARTY_NOTICES.md"), f"{top}/THIRD_PARTY_NOTICES.md")
        # весь исходный код проекта (то, что выкладывается на GitHub) — в подпапке source/, чтобы архив был единым
        skip = {".git", "build", "dist", "__pycache__"}
        for r, dirs, files in os.walk(ROOT):
            dirs[:] = [d for d in dirs if d not in skip]
            for n in files:
                if n.endswith((".pyc", ".zip")):
                    continue
                full = os.path.join(r, n)
                z.write(full, f"{top}/source/" + os.path.relpath(full, ROOT).replace(os.sep, "/"))
    print("архив:", out)


def main():
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    steps = {"test": [cmd_test], "build": [cmd_build], "package": [cmd_build, cmd_package], "all": [cmd_test, cmd_build, cmd_package]}
    if what not in steps:
        print(__doc__); return 2
    for f in steps[what]:
        f()
    return 0


if __name__ == "__main__":
    sys.exit(main())
