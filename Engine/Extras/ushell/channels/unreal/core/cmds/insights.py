import unreal
import unrealcmd
import subprocess as sp
from pathlib import Path

#-------------------------------------------------------------------------------
class Insights(unrealcmd.Cmd):
    """ Conveninent access to UnrealInsights """
    trace       = unrealcmd.Arg("", "Trace ident to open from server or .utrace file")
    uiargs      = unrealcmd.Arg([str], "Additional arguments to pass to UnrealInsights")
    build       = unrealcmd.Opt(False, "Build UnrealInsights before launching")
    attach      = unrealcmd.Opt(False, "Attach the debugger")
    debug       = unrealcmd.Opt(False, "Use debug variant binaries")
    noautobuild = unrealcmd.Opt(False, "Skip check for binaries and inference of --build")

    def _get_lastest_trace(self):
        # todo: we shall create a UTS client!
        import os
        if os.name != "nt":
            self.print_warning("The status of 'latest'; not yet")
            return

        from pathlib import Path
        zerozeroone = Path(os.getenv("localappdata"))
        zerozeroone /= "UnrealEngine/Common/UnrealTrace/Store/001"

        items = list(zerozeroone.glob("*.utrace"))
        if not items:
            return
        items.sort(key=lambda x: x.stat().st_mtime)
        return items[-1]

    def _has_binary(self, *, debug=False):
        ue_context = self.get_unreal_context()
        if not (target := ue_context.get_target_by_name("UnrealInsights")):
            return False
        if not (build := target.get_build()):
            return False
        return build.get_binary_path() is not None

    def main(self):
        # Infer --build if there doesn't appear to be binaries.
        if not self.args.noautobuild:
            if not self._has_binary(debug=self.args.debug):
                self.args.build = True

        # Inject the file to the latest trace if requested
        pass_through = self.args.uiargs
        if self.args.trace == "latest":
            if latest := self._get_lastest_trace():
                pass_through = (latest, *pass_through)
        elif Path(self.args.trace).is_file():
            pass_through = (self.args.trace, *pass_through)

        # Yup, this is mostly a shim around other ushell commands!
        ui_args = (
            "_run",
            "program",
            "UnrealInsights",
            "debug" if self.args.debug else "development",
        )
        if self.args.build:     ui_args = (*ui_args, "--build")
        if self.args.attach:    ui_args = (*ui_args, "--attach")
        if pass_through:        ui_args = (*ui_args, "--", *pass_through)

        sp.run(ui_args)
