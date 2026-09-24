# Copyright Epic Games, Inc. All Rights Reserved.

import flow.describe

#-------------------------------------------------------------------------------
clink = flow.describe.Tool()
clink.version("1.0.0a6")
#clink.payload("https://github.com/mridgers/clink/releases/download/$VERSION/clink-$VERSION.zip")
#clink.sha1("70289e92e3313a2b0e8dee801901eae61f8992a3")
clink.payload("https://www.dropbox.com/scl/fi/umxccnk4irsucu71z9keu/clink-1.0.0a6.7dc5c7.zip?rlkey=1l1f0u1gnt0mqertbpwc5zy2a&st=z6b9078t&dl=1")
clink.sha1("6dd8010ed2118ce032e5ab4ca3697aecdfdc600a")
clink.platform("win32")
clink.bin("clink_x64.exe")

#-------------------------------------------------------------------------------
fd = flow.describe.Tool()
fd.version("8.7.1")
fd.payload("https://github.com/sharkdp/fd/releases/download/v$VERSION/fd-v$VERSION-x86_64-pc-windows-msvc.zip")
fd.sha1("7e8f5d0a9d1fed75b12483809f9619c04b39b60c")
fd.platform("win32")
fd.bin("fd.exe")
fd.source("https://github.com/sharkdp/fd/releases/latest", "fd-v([0-9.]+)-x86_64")

#-------------------------------------------------------------------------------
fzf = flow.describe.Tool()
fzf.version("0.56.3")
fzf.source("https://github.com/junegunn/fzf/releases/latest", r"fzf-(\d+\.\d+\.\d+)-windows")
if bundle := fzf.bundle(platform="win32"):
    bundle.payload("https://github.com/junegunn/fzf/releases/download/v$VERSION/fzf-$VERSION-windows_amd64.zip")
    bundle.bin("fzf.exe")
    bundle.sha1("9dc22afb3a687b10eac57fe27f1a0e4d65c52944")

#-------------------------------------------------------------------------------
shell_cmd = flow.describe.Command()
shell_cmd.source("shell_cmd.py", "Cmd")
shell_cmd.invoke("boot")
shell_cmd.prefix("$")

#-------------------------------------------------------------------------------
shell_pwsh = flow.describe.Command()
shell_pwsh.source("shell_pwsh.py", "Pwsh")
shell_pwsh.invoke("boot")
shell_pwsh.prefix("$")

#-------------------------------------------------------------------------------
channel = flow.describe.Channel()
channel.version("1")
channel.parent("flow.core")
