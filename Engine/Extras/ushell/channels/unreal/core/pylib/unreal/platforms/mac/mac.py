# Copyright Epic Games, Inc. All Rights Reserved.

import os
import unreal
import subprocess as sp
from pathlib import Path

#-------------------------------------------------------------------------------
def _is_sandboxed(binary_path):
    cs_cmd = (
        "codesign",
        "-d",
        "--xml",
        "--entitlements=-",
        binary_path,
    )
    with sp.Popen(cs_cmd, stdout=sp.PIPE, stderr=sp.DEVNULL) as proc:
        it = iter(proc.stdout)
        xml_data = b"".join(it)

    import xml.etree.ElementTree as et
    xml = et.fromstring(xml_data)
    it = (x for x in xml.findall(".//dict/*"))
    for item in it:
        if item.text != "com.apple.security.app-sandbox":
            continue

        item = next(it)
        return item.tag == "true"

#-------------------------------------------------------------------------------
class Platform(unreal.Platform):
    name = "Mac"

    def _read_env(self):
        yield from ()

    def _get_version_ue4(self):
        dot_cs = self.get_unreal_context().get_engine().get_dir()
        dot_cs /= "Source/Programs/UnrealBuildTool/Platform/Mac/MacToolChain.cs"
        return Platform._get_version_helper_ue4(dot_cs, "MinMacOSVersion")

    def _get_version_ue5(self):
        dot_cs = "Source/Programs/UnrealBuildTool/Platform/Mac/ApplePlatformSDK"
        version = self._get_version_helper_ue5(dot_cs + ".Versions.cs")
        return version or self._get_version_helper_ue5(dot_cs + ".cs")

    def _get_cook_form(self, target):
        if target == "game":   return "Mac" if self.get_unreal_context().get_engine().get_version_major() > 4 else "MacNoEditor"
        if target == "client": return "MacClient"
        if target == "server": return "MacServer"

    def _launch(self, exec_context, stage_dir, binary_path, args):
        print("-- Checking entitlements")
        constrained = False
        try:
            if not os.getenv("USHELL_NO_BASEDIR", None):
                constrained = _is_sandboxed(binary_path)
        except IOError:
            print("NOTE: failed calling 'codesign' to check for sandbox entitlement")

        if constrained:
            lhs = Path(stage_dir)
            rhs = Path(binary_path).parent.parent
            if not lhs.is_relative_to(rhs):
                print("\x1b[91m!!")
                print(f"!! Binary '{binary_path}' is sandboxed")
                print(f"!! It cannot access dataset '{stage_dir}'")
                print(f"!!")
                print("!! BUILD BINARIES WITH '.build [...] -- -NoEntitlements'")
                print("!!\x1b[0m")
                import time
                time.sleep(2)
                stage_dir = None

        if os.getenv("USHELL_NO_BASEDIR", None):
            stage_dir = None

        if stage_dir:
            midfix = "Engine";
            if project := self.get_unreal_context().get_project():
                midfix = project.get_name()
            base_dir = stage_dir + midfix + "/Binaries/Mac"
            if not os.path.isdir(base_dir):
                raise EnvironmentError(f"Failed to find base directory '{base_dir}'")
            args = (*args, "-basedir=" + base_dir)

        cmd = exec_context.create_runnable(binary_path, *args)
        cmd.launch()
        return True
