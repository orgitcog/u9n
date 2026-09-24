import shutil
import flow.cmd
from pathlib import Path

#-------------------------------------------------------------------------------
def _find_ushell_dir(start_path):
    start_path = (Path(start_path) / "x") or Path(__file__)
    for item in start_path.parents:
        if (item / ("ushell.bat")).is_file():
            return item;

#-------------------------------------------------------------------------------
class Gather(flow.cmd.Cmd):
    """ Gathers an Engine-deployed version of ushell into a standalone version. """
    destdir     = flow.cmd.Arg(Path, "Destination directory to gather into")
    srcdir      = flow.cmd.Arg("", "Source to gather (default is this ushell instance)")
    overwrite   = flow.cmd.Opt(False, "Overwrite destination if it exists")

    def main(self):
        self.print_info("Src/dest paths")
        src_dir = _find_ushell_dir(self.args.srcdir)
        if not src_dir:
            raise ValueError("Unable to locate '<src_dir>/ushell.bat'")
        print("src:", src_dir)

        # Validate source
        for item in src_dir.parents:
            candidate = item / "GenerateProjectFiles.bat"
            print("    ", candidate, end="\r")
            if candidate.is_file():
                print("  !")
                break
            print("  -")
        else:
            raise ValueError(f"'{src_dir}' does not appear to be located in a UE branch")

        # Validate destination
        print("dest:", self.args.destdir)
        dest_dir = self.args.destdir
        if dest_dir.exists():
            if not self.args.overwrite:
                raise ValueError(f"'{dest_dir}' already exists")
            else:
                if not (dest_dir / "ushell.bat").is_file():
                    raise ValueError("'{dest_dir}/ushell.bat' expected - not overwriting")
                shutil.rmtree(dest_dir)

        dest_dir.mkdir(parents=True, exist_ok=True)

        # Build copy rota
        self.print_info("Collecting source")

        def is_src_file(item):
            if not item.is_file():          return
            if item.suffix == ".pyc":       return
            if item.parent.name == "tps":   return
            return item

        print(src_dir / "**")
        rota = [(x, x.parent.relative_to(src_dir)) for x in src_dir.rglob("*") if is_src_file(x)]

        p_dest = Path("channels/unreal/core/pylib/unreal/platforms/win64")
        for p_dir in (src_dir.parent.parent / "Platforms").glob("*"):
            p_dir /= "Extras/ushell"
            if not p_dir.is_dir():
                continue
            print(p_dir / "*")
            rota += [(x, p_dest) for x in p_dir.glob("*") if is_src_file(x)]

        print("Source files:", len(rota))

        # Do the copy
        self.print_info("Gathering")
        for i, (item, item_dest_dir) in enumerate(rota):
            dest_suffix = item_dest_dir / item.name
            dest_item = dest_dir / dest_suffix
            if dest_item.name.startswith("platform_"):
                dest_item = dest_item.parent / dest_item.name[9:]
            print("%3d" % i, dest_suffix)
            dest_item.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(item, dest_item)
        print("... done!")
