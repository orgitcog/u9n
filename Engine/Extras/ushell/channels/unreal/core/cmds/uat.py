# Copyright Epic Games, Inc. All Rights Reserved.

import os
import unreal
import unrealcmd

#-------------------------------------------------------------------------------
class Uat(unrealcmd.MultiPlatformCmd):
    """ Runs an UnrealAutomationTool command """
    command     = unrealcmd.Arg(str, "The UnrealAutomationTool command to run")
    uatargs     = unrealcmd.Arg([str], "Arguments to pass to the UAT command")
    unprojected = unrealcmd.Opt(False, "No implicit '-project=' please")
    allscripts  = unrealcmd.Opt(False, "Ask UAT to compile all scripts (default is project-only)");
    debug       = unrealcmd.Opt(False, "Use the debug variant of AutomationTool");
    attach      = unrealcmd.Opt(False, "Open VS to debug UAT (not exactly 'attach')");

    def complete_command(self, prefix):
        import re

        def _read_foreach_subdir(dir, func):
            for entry in (x for x in os.scandir(dir) if x.is_dir()):
                yield from func(entry.path)

        def _read_buildcommand_types(dir, depth=0):
            for entry in os.scandir(dir):
                if entry.name.endswith(".cs"):
                    with open(entry.path, "rt") as lines:
                        for line in lines:
                            m = re.search(r"(\w+) : BuildCommand", line)
                            if m:
                                yield m.group(1)
                                break
                elif entry.is_dir() and entry.name != "obj" and depth < 2:
                    yield from _read_buildcommand_types(entry.path, depth + 1)
                    continue

        def _find_automation_rules(dir):
            for entry in os.scandir(dir):
                if entry.name.endswith(".Automation.csproj"):
                    yield from _read_buildcommand_types(dir)
                    return

        ue_context = self.get_unreal_context()

        uat_script_roots = (
            ue_context.get_engine().get_dir() / "Source/Programs/AutomationTool",
        )

        if project := ue_context.get_project():
            uat_script_roots = (
                *uat_script_roots,
                project.get_dir() / "Build",
            )

        for script_root in uat_script_roots:
            yield from _read_foreach_subdir(script_root, _find_automation_rules)

    def _get_build_script(self, name="RunUAT"):
        ue_context = self.get_unreal_context()
        script = ue_context.get_engine().get_dir() / "Build/BatchFiles"
        script /= name + (".bat" if os.name == "nt" else ".sh")
        return script

    def _compile(self):
        exec_context = self.get_exec_context()
        cmd = self._get_build_script()
        cmd = exec_context.create_runnable(cmd, "-Compile")
        return cmd.run()

    def _build_debug(self):
        self.print_info("Building debug UAT")

        import subprocess as sp
        from pathlib import Path
        ue_context = self.get_unreal_context()
        cmd = (
            self._get_build_script("GetDotnetPath"),
            "&&"
            "where",
            "dotnet",
        )
        dotnet_path = None
        with sp.Popen(cmd, stdout=sp.PIPE) as proc:
            for line in proc.stdout:
                path = Path(line.decode().strip())
                if path.stem == "dotnet" and path.is_file():
                    dotnet_path = dotnet_path or path
        dotnet_path or dotnet_path or "dotnet"

        cmd = (
            dotnet_path,
            "build",
            str(ue_context.get_engine().get_dir() / "Source/Programs/AutomationTool/AutomationTool.csproj"),
            "-c", "Debug"
        )
        exec_context = self.get_exec_context()
        cmd = exec_context.create_runnable(*cmd)
        for line in cmd:
            print(line)

        return cmd.get_return_code()

    def _build(self):
        if self.args.debug:
            return self._build_debug()

        exec_context = self.get_exec_context()
        cmd = self._get_build_script("BuildUAT")
        cmd = exec_context.create_runnable(cmd)
        return cmd.run()

    def _get_binary_path(self, *, variant=None):
        unconventional = self.args.debug or variant
        if not unconventional:
            return self._get_build_script()

        variant = variant or "Debug"
        ue_context = self.get_unreal_context()
        engine_dir = ue_context.get_engine().get_dir()
        return engine_dir / f"Source/Programs/AutomationTool/bin/{variant}/AutomationTool.exe"

    def _launch_debugger(self, *args):
        self.print_info("Launching Visual Studio")

        variant = "Debug" if self.args.debug else "Development"
        cmd = self._get_binary_path(variant=variant)

        if self.get_os_name() != "nt":
            self.print_warning("Debugging unavailable. Printing command instead")
            print(cmd, *args)
            return False

        exec_context = self.get_exec_context()
        cmd = exec_context.create_runnable("devenv.exe", "/debugexe", cmd, *args)
        cmd.launch()
        print("\nRemember to change debugger type to '.NET Core'")

    def main(self):
        self.use_all_platforms()

        if self.get_os_name() == "nt":
            # POSIX's BuildUAT.sh script isn't self-contained and doesn't run alone
            if ret := self._build():
                return ret

        args = (self.args.command,)

        ue_context = self.get_unreal_context()
        if project := ue_context.get_project():
            if args[0] and not self.args.unprojected:
                args = (*args, "-project=" + str(project.get_path()))
            if ue_context.get_type() != unreal.ContextType.FOREIGN:
                if not self.args.allscripts:
                    args = (*args, "-ScriptsForProject=" + project.get_name())

        args = (*args, *self.args.uatargs)

        if self.args.attach:
            return self._launch_debugger(*args)

        exec_context = self.get_exec_context()
        cmd = self._get_binary_path()
        cmd = exec_context.create_runnable(cmd, *args)
        return cmd.run(stdin=False)
