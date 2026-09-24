from dataclasses import dataclass
import argparse
import os
import platform
import re
import shutil
import subprocess
import sys

activate_print_verbose = False

def print_verbose(*args, **kwargs):
    if activate_print_verbose:
        print("VERBOSE:", *args, **kwargs)

def print_warning(*args, **kwargs):
    print("WARNING:", *args, **kwargs)

def decode_string(string):
    if type(string) is str:
        return string

    try:
        result = string.decode("utf-8")
    except UnicodeDecodeError:
        try:
            result = string.decode("cp1252")
        except UnicodeDecodeError:
            print_warning("Failed to decode so returning original string \"{0}\"".format(string))
            return string
    
    return result

def run_subprocess(command, **kwargs):
    environment = os.environ.copy()

    # Add additional paths on the Mac so it can find p4 etc.
    if platform.system() == "Darwin":
        additional_paths = ["/usr/local/bin", "/usr/bin", "/opt/homebrew/bin"]
        new_path = os.environ.get("PATH", "")
        for path in additional_paths:
            new_path = os.pathsep.join([new_path, path])

        environment = environment | {"PATH": new_path}

    return subprocess.run(command, capture_output=True, env=environment, **kwargs)

@dataclass
class P4_ztag():
    file_path: str = None
    depot_file: str = None
    is_add: bool = False
    is_open: bool = True

def p4_ztag(path):
    command = ["p4", "-ztag", "fstat", "-Ro", path]
    result = run_subprocess(command)
    stdout_string = decode_string(result.stdout)
    stderr_string = decode_string(result.stderr)
    
    to_return = P4_ztag(file_path = path)
    
    if "not opened on this client" in stderr_string:
        to_return.is_open = False
        return to_return
    
    lines = stdout_string.splitlines()

    depot_file_regex = re.compile(r'^\.\.\. depotFile (.*)')
    type_regex = re.compile(r'^\.\.\. type (.*)')
    action_regex = re.compile(r'^\.\.\. action (.*)')

    for line in lines:
        if match := depot_file_regex.match(line):
            to_return.depot_file = match.group(1)
        if match := type_regex.match(line):
            file_type = match.group(1)
            if file_type.strip() != "text":
                return None
        if match := action_regex.match(line):
            action = match.group(1)
            if action.strip() == "add":
                to_return.is_add = True

    return to_return

def clang_format(exe_path, config_path, file_path, verify=False):
    """Returns True if the diff needed formatting, False otherwise."""

    command = [exe_path, "-style=file:%s" % config_path, "-i", file_path]
    if verify:
        command.append("--dry-run")
        command.append("-Werror")

    result = run_subprocess(command)
    if result.returncode == 1:
        return True
    if result.returncode != 0:
        print("Non-zero return code {0} from {1}".format(result.returncode, " ".join(command)))
        print("stderr:", result.stderr)
        return
    print(decode_string(result.stdout))

    return False

def p4_diff(path):
    command = ["p4", "diff", "-du0", path]
    result = run_subprocess(command)
    if result.returncode != 0:
        raise Exception("p4 diff failed: %s" % result.stderr)
    stdout_string = decode_string(result.stdout)
    return stdout_string

def p4_open(path):
    command = ["p4", "open", path]
    result = run_subprocess(command)
    if result.returncode != 0:
        raise Exception("p4 open failed: %s" % result.stderr)

def clang_format_diff(clang_format_diff_path, clang_format_path, config_path, diff, verify=False):
    """Returns True if the diff needed formatting, False otherwise."""

    # Figure out which python executable to call.
    python_command = None
    if shutil.which("python") is not None:
        python_command = "python"
    elif shutil.which("python3") is not None:
        python_command = "python3"
    else:
        raise Exception("Could not run clang-format-diff.py: could not find \"python\" or \"python3\" executable to run it with.")

    print_verbose("Using python command \"%s\"." % python_command)

    command = [python_command, clang_format_diff_path, "-binary=%s" % clang_format_path, "-style=file:%s" % config_path, "-v"]

    if not verify:
        # Write the formatted code back to the files being formatted.
        command.append("-i")

    result = run_subprocess(command, text=True, input=diff)
    # clang-format-diff returns 1 when the file needs to be formatted.
    if result.returncode == 1:
        return True
    elif result.returncode != 0:
        print("Non-zero return code {0} from {1}".format(result.returncode, " ".join(command)))
        print("stderr:", result.stderr)

    return False

def main():
    args_parser = argparse.ArgumentParser(
        prog="perforce-clang-format",
        description="Uses Perforce to run clang-format on the diff of files you've changed",
    )
    args_parser.add_argument("paths", nargs="*")
    args_parser.add_argument("-v", "--verbose", action="store_true")
    args_parser.add_argument("--clang-format-path")
    args_parser.add_argument("--clang-format-config-path")
    args_parser.add_argument("--clang-format-diff-path")
    args_parser.add_argument("--verify", action="store_true")
    args = args_parser.parse_args()

    global activate_print_verbose
    activate_print_verbose = args.verbose

    print_verbose("args.verify=", args.verify)
    print_verbose("args.paths=", args.paths)

    script_dir = os.path.dirname(os.path.realpath(__file__))

    clang_format_path = args.clang_format_path
    if clang_format_path is None:
        if platform.system() == "Darwin":
            clang_format_path = os.path.join(script_dir, "Mac-arm64", "clang-format")
        elif platform.system() == "Windows":
            clang_format_path = os.path.join(script_dir, "Win64", "clang-format.exe")
        elif platform.system() == "Linux":
            raise Exception("Linux is not yet supported.")
        else:
            # Support the old clang-format.exe location for now.
            clang_format_path = os.path.join(script_dir, "clang-format.exe")

    config_path = args.clang_format_config_path
    if config_path is None:
        config_path = os.path.join(script_dir, "experimental.clang-format")

    clang_format_diff_path = args.clang_format_diff_path
    if clang_format_diff_path is None:
        clang_format_diff_path = os.path.join(script_dir, "clang-format-diff.py")

    files_that_need_formatting = []

    for path in args.paths:
        ztag = p4_ztag(path)
        if ztag is None:
            continue

        if ztag.is_add:
            if args.verify:
                print_verbose("Checking formatting of new file \"%s\"." % ztag.file_path)
            else:
                print_verbose("Fully formatting new file \"%s\"." % ztag.file_path)

            if clang_format(clang_format_path, config_path, ztag.file_path, args.verify):
                if args.verify:
                    files_that_need_formatting.append(ztag.file_path)

        elif not ztag.is_open:
            if not args.verify:
                print_verbose("Opening unopened file \"%s\"." % ztag.file_path)
                p4_open(ztag.file_path)
            diff = p4_diff(ztag.file_path)

            if args.verify:
                print_verbose("Checking formatting of diff of file \"%s\"." % ztag.file_path)
            else:
                print_verbose("Formatting diff of file \"%s\"." % ztag.file_path)

            if clang_format_diff(clang_format_diff_path, clang_format_path, config_path, diff, args.verify):
                if args.verify:
                    files_that_need_formatting.append(ztag.file_path)

        else:
            diff = p4_diff(ztag.file_path)
            print_verbose("Formatting diff of file \"%s\"." % ztag.file_path)

            if clang_format_diff(clang_format_diff_path, clang_format_path, config_path, diff, args.verify):
                if args.verify:
                    files_that_need_formatting.append(ztag.file_path)

    if len(files_that_need_formatting) > 0:
        for file_path in files_that_need_formatting:
            print("File needs formatting: \"%s\"." % file_path)
        sys.exit(1)

if __name__ == "__main__":
    main()
