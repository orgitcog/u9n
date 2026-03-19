# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys
from pathlib import Path

#-------------------------------------------------------------------------------
def _enable_vt100():
    if os.name != "nt":
        return

    import ctypes
    win_dll = ctypes.LibraryLoader(ctypes.WinDLL)
    get_std_handle = win_dll.kernel32.GetStdHandle
    get_console_mode = win_dll.kernel32.GetConsoleMode
    set_console_mode = win_dll.kernel32.SetConsoleMode

    con_mode = ctypes.c_int()
    stdout_handle = get_std_handle(-11)
    get_console_mode(stdout_handle, ctypes.byref(con_mode))
    con_mode = con_mode.value | 4
    set_console_mode(stdout_handle, con_mode)

#-------------------------------------------------------------------------------
def _main():
    # Get all the channels ready and aggregated
    lib_dir = Path(__file__).resolve().parent / "lib"
    sys.path.append(str(lib_dir))
    import bootstrap
    inst_dir = bootstrap.impl()

    # Run the '$boot' command
    import run
    ret = run.main((f"{inst_dir}/manifest", "$boot", *sys.argv[1:]))
    raise SystemExit(ret)



if __name__ == "__main__":
    _enable_vt100()
    _main()
