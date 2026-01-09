# Copyright Epic Games, Inc. All Rights Reserved.

import os
import re
import sys
import pprint
import hashlib
import marshal
import flow.cmd
import flow.describe
from pathlib import Path
from urllib.request import urlopen, URLError

#-------------------------------------------------------------------------------
def _http_get(url, on_data):
    # Try and get the certifi CA file
    try:
        import certifi
        cafile_path = certifi.where()

        import ssl
        ssl_context = ssl.SSLContext()
        ssl_context.load_verify_locations(cafile=cafile_path)
    except ImportError:
        ssl_context = None

    # Fire up an http client to get the url.
    client = urlopen(url, context=ssl_context)
    if (client.status // 100) != 2:
        assert False, f"Error creating HTTP client for {url}"

    # Get the download's file name
    content_header = client.headers.get("content-disposition", "")
    m = re.search(r'filename="?([^;"]+)"?(;|$)', content_header)
    if m: file_name = m.group(1)
    else: file_name = os.path.basename(client.url)

    # Do the download.
    recv_size = 0
    total_size = int(client.headers.get("content-length"))
    while chunk := client.read(128 << 10):
        recv_size += len(chunk)
        on_data(file_name, recv_size, total_size, chunk)



#-------------------------------------------------------------------------------
class Debug(flow.cmd.Cmd):
    """ Shows debug information about the internal state of the session """

    def main(self):
        channel = self.get_channel()
        system = channel.get_system()

        with open(system.get_working_dir() + "manifest", "rb") as x:
            manifest = marshal.load(x)

        cmd_tree = manifest["cmd_tree"]
        pprint.pprint(manifest)

        print()
        session = self.get_noticeboard(self.Noticeboard.SESSION)
        session.debug(pprint.pprint)

#-------------------------------------------------------------------------------
class Invalidate(flow.cmd.Cmd):
    """ Invalidates cached state such that initialisation runs again the next
    time the shell is started up."""

    def main(self):
        from pathlib import Path

        channel = self.get_channel()
        system = channel.get_system()
        working_dir = Path(system.get_working_dir())
        print(working_dir)

        for manifest_path in working_dir.glob("**/manifest"):
            if not manifest_path.parent.samefile(working_dir):
                print("Removing:", manifest_path.relative_to(working_dir))
                manifest_path.unlink(missing_ok=True)

        (working_dir / "manifest").unlink(missing_ok=True)

#-------------------------------------------------------------------------------
class Wipe(flow.cmd.Cmd):
    """ Wipes everything. """

    def main(self):
        import shutil
        from pathlib import Path

        channel = self.get_channel()
        system = channel.get_system()
        for item in (system.get_temp_dir(), system.get_working_dir()):
            print("Removing:", item, end="")
            shutil.rmtree(item, ignore_errors=True)
            print("")

        del_dir = Path(system.get_working_dir()) / f"$cleaner/{os.getpid()}"
        del_dir.mkdir(parents=True, exist_ok=True)

        tool_dir = Path(system.get_tools_dir())
        print("Tools dir:", tool_dir)
        for i, item in enumerate(tool_dir.glob("*")):
            if not item.is_dir():
                continue

            print("Removing tool:", item.name, end="")
            item = item.rename(del_dir / str(i))
            shutil.rmtree(item, ignore_errors=True)
            print("...pending" if item.is_dir() else "")

#-------------------------------------------------------------------------------
class Paths(flow.cmd.Cmd):
    """ Prints behind-the-scenes paths. """

    def gather(self):
        for item in Path(__file__).parents:
            if (item / "ushell.bat").is_file():
                deploy_dir = item
                break
        else:
            deploy_path = "?unknown?"

        channel = self.get_channel()
        system = channel.get_system()
        condition = lambda x: Path(x).resolve()
        return {
            "deploy"    : condition(deploy_dir),
            "working"   : condition(system.get_working_dir()),
            "shims"     : condition(system.get_working_dir()) / "shims",
            "tools"     : condition(system.get_tools_dir()),
            "temp"      : condition(system.get_temp_dir()),
        }

    def main(self):
        for name, dir in self.gather().items():
            print("%-8s:" % name, dir)

#-------------------------------------------------------------------------------
class Browse(Paths):
    """ Opens internal support directories in a file browser. """
    what = flow.cmd.Arg(str, "Supporting component's path to explore")
    complete_what = ("deploy", "working", "tools", "temp", "shims")

    def main(self):
        paths = self.gather()
        what = self.args.what.lower()
        if what not in paths:
            self.print_error(f"Not sure how to browse '{what}'")
            return False

        dest = paths[what]
        print(what, "=", dest)

        if os.name != "nt":
            self.print_warning("...fancy a shot at an implementation for this platform?")
            return False

        explorer_arg = ("explorer.exe", "/e,/expand,", dest)
        print("Launching:", *explorer_arg)
        import subprocess as sp
        sp.run(explorer_arg)

#-------------------------------------------------------------------------------
class _Channels(flow.cmd.Cmd):
    def main(self):
        channel = self.get_channel()
        system = channel.get_system()

        with open(system.get_working_dir() + "manifest", "rb") as x:
            manifest = marshal.load(x)

        self.channels = {}
        for item in (x for x in manifest["channels"] if x["tools"]):
            self.channels[item["name"]] = {
                "tools" : item["tools"],
                "path" : item["path"],
            }

#-------------------------------------------------------------------------------
class Tools(_Channels):
    """ Prints debug information about tools. """

    def main(self):
        super().main()
        pprint.pprint(self.channels)

#-------------------------------------------------------------------------------
class Sha1s(_Channels):
    """ Shows channels' tools SHA1s """

    def _impl(self, name, manifest):
        def import_script(script_path):
            import importlib.util as import_util
            spec = import_util.spec_from_file_location("", script_path)
            module = import_util.module_from_spec(spec)
            spec.loader.exec_module(module)
            return module

        tools = {}
        module = import_script(Path(manifest["path"]) / "describe.flow.py")
        for item_name in dir(module):
            item = getattr(module, item_name)
            if isinstance(item, flow.describe.Tool):
                tools[item_name] = item

        print("--", name)
        for tool_name, tool in tools.items():
            def on_data(file_name, recv_size, total_size, data):
                if sys.stdout.isatty():
                    print("%.02f%%" % ((recv_size * 100) / total_size), end="\r")
                digest.update(data)

            for bundle_name, bundle in tool._bundles.items():
                digest = hashlib.sha1()
                for url, file_type in bundle._payloads:
                    url = url.replace("$VERSION", tool._version)
                    try: _http_get(url, on_data)
                    except URLError: pass
                sha1 = digest.hexdigest()

                print(rf'{tool_name}{bundle_name}.sha1("{sha1}")')

    def main(self):
        super().main()
        for name, manifest in self.channels.items():
            self._impl(name, manifest)

#-------------------------------------------------------------------------------
class Arguments(flow.cmd.Cmd):
    """ Prints the given arguments as they would be received by commands """
    arguments = flow.cmd.Arg([str], "Arguments to print")

    def main(self):
        for i, arg in enumerate(self.args.arguments):
            print("%-2d:" % i, arg)
