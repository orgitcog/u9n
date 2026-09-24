# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys
import shutil
from pathlib import Path
from typing import Iterable

import fsutils
import marshal
import flow.describe
import subprocess as sp
import os
import stat

#-------------------------------------------------------------------------------
class _Log(object):
    def __init__(self, log_path):
        self._indent = 0
        try: self._log_file = open(log_path, "wt")
        except: self._log_file = None

    def __del__(self):
        if self._log_file:
            self._log_file.close()

    def indent(self, *name):
        self.write("Section:", *name)
        self._indent += 1

    def unindent(self):
        self._indent -= 1

    def write(self, *args):
        if self._log_file:
            print(*args, file=self._log_file)

    def print(self, *args, **kwargs):
        print("  " * (self._indent * (1 if kwargs.get("end") == None else 0)), end="")
        print(*args, **kwargs, file=sys.stdout)

    def print_write(self, *args, **kwargs):
        self.print(*args, **kwargs)
        self.write(*args)

_log = None



#-------------------------------------------------------------------------------
def _http_get(url, dest_dir, progress_cb=None):
    import re
    from urllib.request import urlopen

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

    if progress_cb:
        progress_cb("", 0.0)

    # Get the download's file name
    content_header = client.headers.get("content-disposition", "")
    m = re.search(r'filename="?([^;"]+)"?(;|$)', content_header)
    if m: file_name = m.group(1)
    else: file_name = os.path.basename(client.url)
    dest_path = dest_dir + file_name
    assert not os.path.exists(dest_path), f"Directory '{dest_path}' unexpectedly exists"

    # Do the download. If there's no content-length we are probably receiving
    # chunked transfer-encoding. So we'll cheat.
    chunk_size = 128 << 10
    if chunk_count := client.headers.get("content-length"):
        chunk_count = int(chunk_count)
    else:
        chunk_count = 1 << 48;
    chunk_count = (chunk_count + chunk_size - 1) // chunk_size
    chunk_index = 0
    with open(dest_path, "wb") as out:
        while chunk := client.read(chunk_size):
            out.write(chunk)
            out.flush()
            chunk_index += 1
            if progress_cb:
                progress_cb(file_name, chunk_index / chunk_count)

    return dest_path



#-------------------------------------------------------------------------------
def _extract_tar(payload, dest_dir):
    cmd = ("tar", "-x", "-C", dest_dir, "-f", payload)
    _, ext = os.path.splitext(payload)
    if ext == ".zip" and os.name == "posix":
        # Tar on Linux cannot open .zip files
        cmd = ("unzip", payload, '-d', dest_dir)
    ret = sp.run(cmd, stdout=sp.DEVNULL)
    assert ret.returncode == 0, "Extract failed; " + " ".join(cmd)

#-------------------------------------------------------------------------------
def _tool_extract_method(src_path, dest_dir):
    extractors = {
        ".tar.gz"   : _extract_tar,
        ".tgz"      : _extract_tar,
        ".zip"      : _extract_tar, # Windows comes with tar as standard now
    }

    was_archive = False
    for file_name in (x.lower() for x in os.listdir(src_path)):
        for suffix, extract_func in extractors.items():
            if not file_name.endswith(suffix):
                continue

            _log.write("extracting", file_name, "to", dest_dir, "with", extract_func)
            extract_func(src_path + file_name, dest_dir)
            was_archive = True
            break

    if was_archive:
        return

    for file_name in os.listdir(src_path):
        os.rename(src_path + file_name, dest_dir + file_name)



#-------------------------------------------------------------------------------
def _validate_tool(name, tool):
    assert tool._version, f"No version set on tool '{name}'"
    assert tool._bundles, f"No payloads added for tool '{name}'"

#-------------------------------------------------------------------------------
def _acquire_tool(name, tool, manifest, target_dir, progress_cb):
    import hashlib

    bundles = {k:v for k,v in manifest["bundles"].items() if v["enabled"]}
    if not bundles:
        return

    # Some boiler plate to provide a progress update
    payload_i = 0
    payload_n = sum(len(x["payloads"]) for x in bundles.values())
    def inner_progress_cb(file_name, value):
        value = int(((float(payload_i) + value) * 100) / payload_n)
        progress_cb(f"{value:3}%")

    # Acquire the tool's bundles/payloads
    temp_dir = fsutils.WorkPath(target_dir[:-1] + ".work/")
    for bundle_name, bundle in bundles.items():
        _log.write("Bundle", bundle_name)
        fetch_dir = temp_dir + bundle_name + "/"
        os.mkdir(fetch_dir)
        for payload, ftype in bundle["payloads"]:
            _log.write("Payload", payload, ftype)
            payload_dest = _http_get(payload, fetch_dir, inner_progress_cb)
            payload_i += 1
            if ftype:
                os.rename(payload_dest, payload_dest + "." + ftype)

    # Hash the downloaded content to confirm it is what is expected
    progress_cb("sha1")
    for bundle_name, bundle in bundles.items():
        sha1 = hashlib.sha1()
        for item in fsutils.read_files(temp_dir + bundle_name):
            with open(item.path, "rb") as in_file:
                while True:
                    data = in_file.read(16384)
                    if len(data) <= 0:
                        break
                    sha1.update(data)
        sha1 = sha1.hexdigest()
        _log.write("SHA1", bundle_name, sha1)

        if expected_sha1 := bundle["sha1"]:
            expected_sha1 = expected_sha1.lower()
            if sha1 != expected_sha1:
                assert false, f"Unexpected content bundle data for tool '{name}' [sha1:{sha1}]"

        bundle["sha1"] = sha1

    # Extract the payloads.
    progress_cb("uzip")
    extract_func = getattr(tool, "extract", _tool_extract_method)
    extract_dir = temp_dir + "$extract/"
    os.mkdir(extract_dir)
    for bundle_name, bundle in bundles.items():
        _log.write("Extract", bundle_name, "from", fetch_dir, "to", extract_dir, "with", extract_func)
        fetch_dir = temp_dir + bundle_name + "/"
        extract_func(fetch_dir, extract_dir)

    # Collapse
    source_dir = str(extract_dir)
    while True:
        dir_iter = iter(fsutils.read_files(source_dir))
        x = next(dir_iter, None)
        if x and not next(dir_iter, None) and x.is_dir():
            source_dir = x.path
            continue
        break

    if tool._root_dir:
        source_dir += "/" + tool._root_dir
        assert os.path.isdir(source_dir), "Given root f'{tool._root_dir}' for tool '{name}' is not valid"

    # Windows can take its time moving/deleting a directory causing the rename
    # to spuriously fail. Or it AV interfering with source. So retry a few times.
    for i in range(6):
        try:
            os.rename(source_dir, target_dir)
            break
        except OSError:
            if i == 5:
                raise
            import time
            time.sleep(0.1)
            continue

    progress_cb("done")

#-------------------------------------------------------------------------------
def _manifest_tool(name, tool):
    def is_enabled(bundle):
        bundle_platform = getattr(bundle, "_platform", None)
        if not bundle_platform:
            return True

        host_platform = sys.platform
        if "-" in bundle_platform:
            import platform
            arch = platform.machine()
            arch = "amd64" if arch in ("x86_64", "AMD64") else "arm64"
            host_platform += "-" + arch

        return bundle_platform == host_platform

    bundles = {}
    for bundle_name, bundle in tool._bundles.items():
        payloads = ((u.replace("$VERSION", tool._version),ft) for u,ft in bundle._payloads)
        bundles[bundle_name] = {
            "enabled"   : is_enabled(bundle),
            "payloads"  : tuple(payloads),
            "bin_paths" : tuple(bundle._bin_paths),
            "sha1"      : getattr(bundle, "_sha1", 0),
        }

    return {
        "version"   : tool._version,
        "double"    : f"{name}-{tool._version}",
        "source"    : tool._source,
        "bundles"   : bundles,
        "bin_paths" : tuple(),
    }

#-------------------------------------------------------------------------------
def _install_tool(name, tool, cleaner):
    manifest = _manifest_tool(name, tool)

    tool_double = f"{name}-{tool._version}/"
    tool_dir = "../tools/" + tool_double
    manifest_path = tool_dir + "manifest.1.flow"
    _log.write("Installing tool", tool_double)

    # If the tool exists then reuse the cached manifest
    if os.path.isfile(manifest_path):
        _log.write("Tool already acquired", tool_dir)
        with open(manifest_path, "rb") as inp:
            return marshal.load(inp)

    # Clean up any legacy deployments
    if os.path.isdir(tool_dir):
        cleaner.delete(Path(tool_dir))

    # Acquire the tool
    log_line = (13 * " ") + tool_double[:-1]
    log_line += "\b" * (len(log_line) - 1)
    _log.print(log_line, end="\r      [    ]\b")

    def progress_cb(info):
        _log.print("\b" * 4, info[:4], sep="", end="")

    try:
        _log.write("Acquiring to", tool_dir)
        _acquire_tool(name, tool, manifest, tool_dir, progress_cb)
    except KeyboardInterrupt:
        raise
    except Exception as e:
        _log.write("ERROR", str(e))
        _log.print("\b" * 4, "fail", sep="", end="\n")
        return manifest

    # Maybe this tool doen't deploy for this platform?
    if not os.path.isdir(tool_dir):
        _log.print("\b" * 4, " na ", sep="", end="\n")
        return manifest

    # Post install
    if hasattr(tool, "post_install"):
        progress_cb("post")
        _log.write("Post-install")
        tool.post_install(tool_dir)

    # Validate that the required binaries exist.
    bin_paths = set()
    for bundle in manifest["bundles"].values():
        if not bundle["enabled"]:
            continue
        for bin_path in bundle["bin_paths"]:
            if not os.path.exists(tool_dir + bin_path):
                _log.write(f"Unable to find '{bin_path}' in '{tool_dir}'")
                continue

            bin_paths.add(bin_path)

            # Confirm we have a binary that is executable.
            if os.name == "nt":
                continue
            binary = os.path.join(tool_dir, bin_path)
            if not (stat.S_IXUSR & os.stat(binary)[stat.ST_MODE]):
                st = os.stat(binary)
                os.chmod(binary, st.st_mode | stat.S_IEXEC)
    manifest["bin_paths"] = tuple(bin_paths)

    with open(manifest_path, "wb") as out:
        marshal.dump(manifest, out)

    _log.print("\b" * 4, " ok ", sep="", end="\n")
    return manifest



#-------------------------------------------------------------------------------
def _validate_command(channel_dir, name, command):
    assert command._path, f"Command '{name}' missing call to invoke/composite()"
    assert command._py_path, f"Command '{name}' missing call to source()"
    if not (channel_dir / command._py_path).is_file():
        assert False, f"Command '{name}' missing source '{command._py_path}'"

#-------------------------------------------------------------------------------
def _build_channel(channel_name:str, channel_dir:Path, cleaner:"_Cleaner") -> dict[str]:
    _log.print(f"Updating channel '{channel_name}'")
    _log.indent("Channel", channel_name)

    # Load the describe.flow.py as a module.
    channel_py = channel_dir / "describe.flow.py"
    module = fsutils.import_script(channel_py)

    def read_items_by_type(type):
        for name, value in module.__dict__.items():
            if isinstance(value, type):
                yield name, value

    # Get the channel's description.
    name, channel = next(read_items_by_type(flow.describe.Channel), (None, None))
    assert channel, f"Channel() instance missing from channel '{channel_name}'"
    assert channel._version != None, f"Channel '{channel_name}' has no version"

    # Install channel's pips.
    if channel._pips:
        _log.print("Pips:")
        _log.indent("Pips")
        for pip_name in channel._pips:
            _log.print_write(pip_name)
            cmd = (sys.executable, "-Xutf8", "-Esum", "pip", "install", pip_name)
            result = sp.run(cmd, stdout=sp.DEVNULL, stderr=sp.DEVNULL)
            if result.returncode:
                _log.print(" ...failed");
        _log.unindent()

    # Tools
    tools_manifest = {}
    tools = [x for x in read_items_by_type(flow.describe.Tool)]
    if tools:
        _log.print("Tools:")
        _log.indent("Tools")
        for name, tool in tools:
            _validate_tool(name, tool)

        for name, tool in tools:
            tools_manifest[name] = _install_tool(name, tool, cleaner)
        _log.unindent()

    # Commands
    commands = [x for x in read_items_by_type(flow.describe.Command)]
    for name, command in commands:
        _validate_command(channel_dir, name, command)
        command._path = command._path or (name)
        command._path = (command._prefix, *command._path)

    # Build manifest
    manifest = {}
    manifest["name"] = channel_name
    manifest["path"] = str(channel_dir) + "/"
    manifest["parent"] = channel._parent or "flow.core"
    manifest["tools"] = tools_manifest
    manifest["commands"] = tuple(v.__dict__ for k,v in commands)

    _log.print("Done\n")
    _log.unindent()
    return manifest



#-------------------------------------------------------------------------------
def _plant_cmd_tree(channels):
    # Build the command tree
    cmd_tree = {}
    for channel in channels:
        for cmd in channel["commands"]:
            node = cmd_tree
            prefix, *cmd_path = cmd["_path"]
            for piece in (prefix + cmd_path[0], *cmd_path[1:]):
                node = node.setdefault(piece, {})

            cmd_desc = (
                channel["index"],
                cmd["_py_path"],
                cmd["_py_class"],
            )

            leaf = node.setdefault(493, [channel["index"]])
            leaf.insert(1, cmd_desc)

    return cmd_tree

#-------------------------------------------------------------------------------
def _carve_channels(channels):
    out = [None] * len(channels)
    for channel in channels:
        index = channel["index"]
        parent_index = channel["parent"]["index"] if channel["parent"] else -1
        out[index] = {
            "name" : channel["name"],
            "path" : channel["path"],
            "parent" : parent_index,
            "tools" : channel["tools"],
        }

    return out

#-------------------------------------------------------------------------------
def _create_shims(manifest):
    shims_dir = "shims/"
    _log.write("Shims dir;", shims_dir)

    import shims
    builder = shims.Builder(shims_dir)
    builder.clean()

    manifest_path = Path().resolve()
    manifest_path = manifest_path.parent / manifest_path.name.split("$")[0]
    manifest_path /= "manifest"
    manifest_path = str(manifest_path)

    # Create command shims
    run_py = os.path.abspath(__file__ + "/../../run.py")
    args_prefix = ("-Xutf8", "-Esu", run_py, manifest_path)
    for name in (x for x in manifest["cmd_tree"] if isinstance(x, str)):
        builder.create_python_shim(name, *args_prefix, name)

    # Create tool shims
    tool_root = os.path.abspath("../tools") + "/"
    for channel in reversed(manifest["channels"]):
        for name, tool in channel["tools"].items():
            version = tool["version"]
            bin_paths = tool["bin_paths"]
            tool_dir = tool_root + tool["double"] + "/"
            for bin_path in bin_paths:
                name = os.path.basename(bin_path)
                tool_path = tool_dir + bin_path
                if os.path.isfile(tool_path):
                    builder.create_shim(name, tool_path)
                else:
                    _log.write("Binary not found;", tool_path)

    builder.commit()

#-------------------------------------------------------------------------------
def _finalise(manifests):
    # Assign each manifest an index
    for i in range(len(manifests)):
        manifests[i]["index"] = i

    # Connect children to their parents
    manifests = {x["name"]:x for x in manifests}
    for manifest in manifests.values():
        parent = manifest["parent"]
        assert parent in manifests, f"Invalid parent '{parent}' for channel '{manifest['name']}'"
        manifest["parent"] = manifests[parent] if parent != manifest["name"] else None

    # Sort the manifests topologically
    topo_manifests = {}

    def topo_insert(manifest):
        if manifest:
            topo_insert(manifest["parent"])
            topo_manifests[manifest["name"]] = manifest

    for manifest in manifests.values():
        topo_insert(manifest)

    manifests = topo_manifests
    _log.write("Channels;", *(x["name"] for x in manifests.values()))

    # Collect various paths for each channel
    pylib_dirs = []
    for manifest in manifests.values():
        pylib_dir = manifest["path"] + "pylib/"
        if os.path.isdir(pylib_dir):
            pylib_dirs.append(pylib_dir)
    _log.write("Pylib dirs;", *(x for x in pylib_dirs))

    # Build the primary manifest.
    primary = {
        "channels"   : _carve_channels(manifests.values()),
        "pylib_dirs" : tuple(pylib_dirs),
        "tools_dir"  : os.path.abspath("../tools") + "/",
        "cmd_tree"   : _plant_cmd_tree(manifests.values()),
    }

    # Create shims
    _create_shims(primary)

    # Write the primary manifest out.
    with open("manifest", "wb") as x:
        marshal.dump(primary, x)



#-------------------------------------------------------------------------------
class _Cleaner(object):
    def __init__(self, working_dir:Path) -> None:
        self._dir = working_dir / f"$cleaner/{os.getpid()}"
        self._dir.mkdir(parents=True, exist_ok=True)
        self._count = 0

    def __del__(self) -> None:
        shutil.rmtree(self._dir.parent, ignore_errors=True)

    def delete(self, path:Path) -> bool:
        try: path.rename(self._dir / str(self._count))
        except: pass
        self._count += 1



#-------------------------------------------------------------------------------
def _hashen(x:str) -> int:
    ret = 5381
    for c in x:
        ret = ((ret << 5) + ret) + ord(c)
        ret &= 0x7fffffff
    return ret



#-------------------------------------------------------------------------------
class _StateDir(object):
    _dir:Path

    def __init__(self, root:Path):
        loc = str(Path(__file__).resolve())
        if host := os.getenv("SHELL"):
            loc += host
        loc_hash = _hashen(loc)
        self._dir = root / ("flow_%08x" % loc_hash)

    def get_dir(self) -> Path:
        return self._dir

    def get_age(self) -> int:
        try: return (self._dir / "manifest").stat().st_mtime
        except FileNotFoundError: return 0



#-------------------------------------------------------------------------------
class _Sources(object):
    _sources    :dict[str, str]
    _key        :int

    def __init__(self) -> None:
        self._sources = {}
        self._key = 0

    def __iter__(self) -> Iterable[[str, Path]]:
        yield from self._sources.items()

    def add_source(self, name:str, flow_py_path:Path) -> None:
        self._sources[name] = flow_py_path
        self._key ^= _hashen(name);

    def get_key(self) -> str:
        return f"sources_{self._key:08x}"

    def get_age(self) -> float:
        deps = (
            Path(sys.executable),
            Path(__file__).parent.parent / "version",
            *(self._sources.values()),
        )
        return max(x.stat().st_mtime for x in deps)



#-------------------------------------------------------------------------------
def _get_sources() -> _Sources:
    sources = _Sources()

    def _add_source(source_dir:Path, group_name:str="") -> int:
        count = 0
        for item in source_dir.glob("*"):
            if not item.is_dir():
                continue

            candidate = item / "describe.flow.py"
            if candidate.is_file():
                sources.add_source(group_name + item.name, candidate)
                count += 1
            elif not group_name:
                count += _add_source(item, item.name + ".")

        return count

    for item in Path(__file__).parents:
        if item.name == "channels":
            _add_source(item)
            break

    from_home = Path.home() / ".ushell/channels"
    if from_home.is_dir():
        _add_source(from_home, "$home.")

    if from_env := os.getenv("FLOW_CHANNELS_DIR"):
        import shlex
        for item in shlex.split(from_env):
            item = Path(item).resolve()
            if item.is_dir():
                _add_source(item, "$env.")

    return sources

#-------------------------------------------------------------------------------
def _update(working_dir:Path, sources:_Sources, cleaner:"_Cleaner") -> None:
    class _CwdScope(object):
        def __init__(self, dir:Path): self._prev_dir = os.getcwd(); os.chdir(dir)
        def __del__(self) -> None:    os.chdir(self._prev_dir)
    cwd_scope = _CwdScope(working_dir)

    # Fire up our log. This needs to be deleted on the way out to not lock dirs
    global _log
    _log = _Log(working_dir / "setup.log")
    _log.write("Working dir;", working_dir)
    class _LogScope(object):
        def __del__(self): global _log; _log = None
    log_scope = _LogScope()

    try:
        # Build all the channels
        manifests = []
        for name, flow_py_path in sources:
            manifest = _build_channel(name, flow_py_path.parent, cleaner)
            manifests.append(manifest)

        # Aggregate all channels into a manifest and create command shims
        _finalise(manifests)

        # Add the marker to detect new/removed channels
        with (working_dir / sources.get_key()).open("wb"):
            pass
    except:
        import traceback
        _log.write(traceback.format_exc())
        raise
    finally:
        del cwd_scope
        del log_scope

#-------------------------------------------------------------------------------
def impl() -> Path:
    # Assume we've our own Python (explicit or a venv) and use that to determine
    # where the working directory is.
    py_bin_path = Path(sys.executable)
    for item in py_bin_path.parents:
        if item.parent.name == "python":
            working_dir = item.parent.parent
            break
    else:
        working_dir = Path.home() / ".ushell"

    sources = _get_sources()

    state_dir = _StateDir(working_dir)
    if sources.get_age() <= state_dir.get_age():
        if (state_dir.get_dir() / sources.get_key()).is_file():
            return state_dir.get_dir()

    dest_dir = state_dir.get_dir()

    cleaner = _Cleaner(working_dir)
    cleaner.delete(dest_dir)

    temp_dir = dest_dir.parent / (dest_dir.name + f"${os.getpid()}")
    try:
        temp_dir.mkdir(parents=True)
        _update(temp_dir, sources, cleaner)
        dest_dir = temp_dir.rename(dest_dir)
    except:
        cleaner.delete(temp_dir)
        raise

    return dest_dir
