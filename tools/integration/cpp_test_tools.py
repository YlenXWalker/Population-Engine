"""Visual Studio discovery shared by standalone Windows regression runners."""
from pathlib import Path
import os
import subprocess


def visual_studio_dev_cmd():
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    install = subprocess.check_output([
        str(vswhere), "-latest", "-products", "*", "-requires",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath",
    ], text=True).strip()
    if not install:
        raise RuntimeError("Visual Studio C++ tools are required")
    return Path(install) / "Common7/Tools/VsDevCmd.bat"
