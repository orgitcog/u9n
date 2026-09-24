# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys
import shutil
import unreal
import unrealcmd
import unreal.cmdline
import http.client
import json
import hashlib
import tempfile
import socket

#-------------------------------------------------------------------------------
def _get_ips_socket(_target_platform):
    import socket
    def _get_ip(family, ip_addr):
        with socket.socket(family, socket.SOCK_DGRAM) as s:
            try:
                s.connect((ip_addr, 1))
                return s.getsockname()[0]
            except Exception as e:
                return
    ret = (_get_ip(socket.AF_INET6, "fe80::0"), _get_ip(socket.AF_INET, "172.31.255.255"))
    return [x for x in ret if x]

def _get_ips_osx(target_platform):
    ret = _get_ips_socket(target_platform)
    try:
        import subprocess
        import re
        # since grep will return an exit code of 1 if not found, force it to return 0, otherwise it throws an exception
        segments = re.split("\\s+", subprocess.check_output('ifconfig | grep 169.254. || true', shell=True).decode('utf-8').strip())
        print(segments)
        # if you have multiple devices plugged in, this will iterate over the segments (every 6th item is the IP)
        # ie: ['inet', '169.254.12.24', 'netmask', '0xffff0000', 'broadcast', '169.254.255.255', 'inet', '169.254.110.30', 'netmask', '0xffff0000', 'broadcast', '169.254.255.255']
        ret.extend(segments[1::6])

        # Can not rely on a stable server link local address above as each time USB cable is plugged in,
        # the IP changes. So use the mac server's hostname for the client to connect against as a fallback.
        # Note: only do this for members of the iOS family - and there's no guarantee the mac has a bonjour name
        if any (x in target_platform.lower() for x in ["ios", "tvos", "vision"]):
            if bonjourName := subprocess.check_output('scutil --get ComputerName', shell=True).decode('utf-8').strip():
                ret.append("macserver://" + bonjourName)
    except:
        print("Exception thrown in _get_ips_osx")
        pass
    return ret

def _get_ips_powershell():
    import subprocess as sp
    ps_cmd = (
        "powershell.exe",
        "-NoProfile",
        "-NoLogo",
        "-Command",
        r'(Get-CimInstance -Query "SELECT * FROM Win32_NetworkAdapterConfiguration WHERE IPEnabled=true"|Sort -Property IPConnectionMetric).IPAddress',
    )
    startupinfo = sp.STARTUPINFO()
    startupinfo.dwFlags = sp.STARTF_USESHOWWINDOW
    startupinfo.wShowWindow = sp.SW_HIDE
    with sp.Popen(ps_cmd, stdout=sp.PIPE, stderr=sp.DEVNULL, stdin=sp.DEVNULL, startupinfo=startupinfo, creationflags=sp.CREATE_NEW_CONSOLE) as proc:
        ret = [x.strip().decode() for x in proc.stdout]
        proc.stdout.close()
    return ret if proc.returncode == 0 else None

def _get_ips_wmic():
    import subprocess as sp
    ret = []
    current_ips = None
    wmic_cmd = ("wmic", "nicconfig", "where", "ipenabled=true", "get", "ipaddress,ipconnectionmetric", "/format:value")
    with sp.Popen(wmic_cmd, stdout=sp.PIPE, stderr=sp.DEVNULL) as proc:
        for line in proc.stdout:
            if line.startswith(b"IPAddress="):
                if line := line[10:].strip():
                    current_ips = line
            elif line.startswith(b"IPConnectionMetric="):
                if line := line[19:].strip():
                    connection_metric = int(line)
                    for ip in eval(current_ips):
                        ret.append((connection_metric, ip))
    ret.sort(key=lambda p: p[0])
    return [item[1] for item in ret]

def _get_ips_nt(target_platform):
    try:
        ret = _get_ips_powershell()
        if not ret:
            ret = _get_ips_wmic()
    except:
        ret = _get_ips_socket(target_platform)
    ret.sort(key=lambda x: "." in x) # sort ipv4 last
    return ret

_get_ips = _get_ips_nt if os.name == "nt" else (_get_ips_osx if sys.platform == "darwin" else _get_ips_socket)

#-------------------------------------------------------------------------------
class ZenBaseCmd(unrealcmd.Cmd):
    cloudauthservice = unrealcmd.Opt("", "Name of the service to authorize with when importing from a cloud source")

    def _build(self, target, variant, platform):
        if target.get_type() == unreal.TargetType.EDITOR:
            build_cmd = ("_build", "editor", variant)
        else:
            platform = platform or unreal.Platform.get_host()
            build_cmd = ("_build", "target", target.get_name(), platform, variant)

        import subprocess
        ret = subprocess.run(build_cmd)
        if ret.returncode:
            raise SystemExit(ret.returncode)

    def get_binary_path(self, target, variant, platform, build):
        ue_context = self.get_unreal_context()

        if isinstance(target, str):
            target = ue_context.get_target_by_name(target)
        else:
            target = ue_context.get_target_by_type(target)

        if build:
            self._build(target, variant, platform)

        variant = unreal.Variant.parse(variant)
        if build := target.get_build(variant, platform):
            if build := build.get_binary_path():
                return build

        raise EnvironmentError(f"No {variant} build found for target '{target.get_name()}'")

    def run_ue_program_target(self, target, variant, build, cmdargs, runargs):
        binary_path = self.get_binary_path(target, variant, None, build)

        final_args = []
        if cmdargs: final_args += cmdargs
        if runargs: final_args += (*unreal.cmdline.read_ueified(*runargs),)

        launch_kwargs = {}
        if not sys.stdout.isatty():
            launch_kwargs = { "silent" : True }
        exec_context = self.get_exec_context()
        runnable = exec_context.create_runnable(str(binary_path), *final_args);
        runnable.launch(**launch_kwargs)
        return True if runnable.is_gui() else runnable.wait()

    def launch_zenserver(self):
        args = ["-SponsorProcessID=" + str(os.getppid())]
        self.run_ue_program_target("ZenLaunch", "development", True, args, None)

    def launch_zenserver_if_not_running(self):
        if not self.is_zenserver_running():
            self.launch_zenserver()

    def is_zenserver_running(self, hostname="localhost", port=8558):
        headers = {"Accept":"application/json"}
        conn = http.client.HTTPConnection(hostname, port=port, timeout=0.1)
        try:
            conn.request("GET", "/health/ready", headers=headers)
            response = conn.getresponse()
            return response.read().decode().lower() == "ok!"
        except OSError:
            return False

        return False

    def find_project_by_id(self, projectid, hostname="localhost", port=8558):
        self.launch_zenserver_if_not_running()
        headers = {"Accept":"application/json"}
        conn = http.client.HTTPConnection(hostname, port=port, timeout=0.1)
        projects = []
        try:
            conn.request("GET", "/prj", headers=headers)
            response = conn.getresponse()
            projects = json.loads(response.read().decode())
        except OSError:
            return None

        for project in projects:
            if project['Id'] == projectid:
                return project
        return None

    def find_oplog_by_id(self, projectid, oplogid, hostname="localhost", port=8558):
        self.launch_zenserver_if_not_running()
        headers = {"Accept":"application/json"}
        conn = http.client.HTTPConnection(hostname, port=port, timeout=0.1)
        oplogs = []
        try:
            conn.request("GET", f"/prj/{projectid}/oplog/{oplogid}", headers=headers)
            response = conn.getresponse()
            oplog = json.loads(response.read().decode())
        except:
            return None

        return oplog

    def perform_project_completion(self, prefix, hostname="localhost", port=8558):
        headers = {"Accept":"application/json"}
        conn = http.client.HTTPConnection(hostname, port=port, timeout=0.1)
        projects = []
        try:
            conn.request("GET", "/prj", headers=headers)
            response = conn.getresponse()
            projects = json.loads(response.read().decode())
        except OSError:
            self.launch_zenserver()
            return

        for project in projects:
            yield project['Id']

    def perform_oplog_completion(self, prefix, project_filter, hostname="localhost", port=8558):
        headers = {"Accept":"application/json"}
        conn = http.client.HTTPConnection(hostname, port=port, timeout=0.1)
        projects = []
        try:
            if project_filter:
                conn.request("GET", f"/prj/{project_filter}", headers=headers)
                response = conn.getresponse()
                projects.append(json.loads(response.read().decode()))
            else:
                conn.request("GET", "/prj", headers=headers)
                response = conn.getresponse()
                projects = json.loads(response.read().decode())
        except OSError:
            self.launch_zenserver()
            return

        for project in projects:
            for oplog in project['oplogs']:
                yield oplog['id']

    def get_project_id_from_project(self, project):
        if not project:
            self.print_error("An active project is required for this operation")

        normalized_project_path = str(project.get_path()).replace("\\","/")
        project_id = project.get_name() + "." + hashlib.md5(normalized_project_path.encode('utf-8')).hexdigest()[:8]
        return project_id;


    def get_project_or_default(self, explicit_project):
        if explicit_project:
            return explicit_project

        ue_context = self.get_unreal_context()
        return self.get_project_id_from_project(ue_context.get_project())

    def _lookup_service_name(self):
        if getattr(self, "_service_name", None):
            return

        config = self.get_unreal_context().get_config()
        default = config.get("Engine", "StorageServers", "Default")
        if value := default.OAuthProviderIdentifier:
            self._service_name = value
        else:
            cloud = config.get("Engine", "StorageServers", "Cloud")
            if value := cloud.OAuthProviderIdentifier:
                self._service_name = value

    def get_exec_context(self):
        context = super().get_exec_context()
        if hasattr(self, '_AccessToken') and self._AccessToken:
            if sys.platform == 'win32':
                context.get_env()["UE-CloudDataCacheAccessToken"] = self._AccessToken
            else:
                context.get_env()["UE_CloudDataCacheAccessToken"] = self._AccessToken
        return context

    def refresh_zen_token(self):
        ue_context = self.get_unreal_context()
        engine = ue_context.get_engine()

        bin_type = "win-x64"
        if sys.platform == "darwin": bin_type = "osx-x64"
        elif sys.platform == "linux": bin_type = "linux-x64"

        bin_ext = ".exe"
        if sys.platform == "darwin": bin_ext = ""
        elif sys.platform == "linux": bin_ext = ""

        bin_path = engine.get_dir()
        bin_path /= f"Binaries/DotNET/OidcToken/{bin_type}/OidcToken{bin_ext}"
        if not bin_path.is_file():
            raise FileNotFoundError(f"Unable to find '{bin_path}'")

        self._service_name = self.args.cloudauthservice
        self._lookup_service_name()
        if not self._service_name:
            raise ValueError("Unable to discover service name")

        token_dir = tempfile.TemporaryDirectory()
        token_file_path = f"{token_dir.name}/oidctoken.json"
        oidcargs = (
            "--ResultToConsole=true",
            "--Service=" + str(self._service_name),
            f"--OutFile={token_file_path}"
        )

        if project := ue_context.get_project():
            oidcargs = (*oidcargs, "--Project=" + str(project.get_path()))

        cmd = self.get_exec_context().create_runnable(bin_path, *oidcargs)
        ret, output = cmd.run2()
        if ret != 0:
            return False

        tokenresponse = None
        with open(token_file_path, "r") as token_file:
            tokenresponse = json.load(token_file)

        self._AccessToken = tokenresponse['Token']
        self.print_info("Cloud access token obtained for import operation.")

        return True

    def conform_to_cook_forms(self, oplogid):
        ue_context = self.get_unreal_context()
        platforms = ue_context.get_platform_provider()
        for platform_name in platforms.read_platform_names():
            platform = platforms.get_platform(platform_name)
            for form_name in ("client", "game", "server"):
                try:
                    cook_form = platform.get_cook_form(form_name)
                    if cook_form.lower() == oplogid.lower():
                        return cook_form
                except:
                    pass
        return oplogid

    def perform_target_creation(self, snapshot, projectid = None, oplogid = None):
        target_projectid = self.get_project_or_default(projectid)
        if self.find_project_by_id(target_projectid) is None:
            ue_context = self.get_unreal_context()
            args = [
            "project-create",
            target_projectid,
            ue_context.get_branch().get_dir(),
            ue_context.get_engine().get_dir(),
            ue_context.get_project().get_dir(),
            ue_context.get_project().get_path()
            ]
            self.run_zen_utility(args)

        target_oplogid = oplogid or snapshot['targetplatform']

        ue_context = self.get_unreal_context()
        target_gcmarker = ue_context.get_project().get_dir() / f"Saved/Cooked/{self.conform_to_cook_forms(target_oplogid)}/ue.projectstore"

        target_gcmarker_dir = os.path.dirname(target_gcmarker)
        if not os.path.exists(target_gcmarker_dir):
            os.makedirs(target_gcmarker_dir)
        remotehostnames = _get_ips(snapshot['targetplatform'])
        remotehostnames.append("hostname://" + socket.getfqdn())
        projectstore = {
            "zenserver" : {
                "islocalhost" : True,
                "hostname" : "[::1]",
                "remotehostnames" : remotehostnames,
                "hostport" : 8558,
                "projectid" : target_projectid,
                "oplogid" : target_oplogid
            }
        }
        with open(target_gcmarker, "w") as target_gcmarker_outfile:
            json.dump(projectstore, target_gcmarker_outfile, indent='\t')

        args = [
        "oplog-create",
        target_projectid,
        target_oplogid,
        target_gcmarker,
        "--force-update"
        ]
        self.run_zen_utility(args)
        return True

    def add_args_from_descriptor(self, snapshot, sourcehost, args):
        snapshot_type = snapshot['type']

        if snapshot_type in ['cloud', 'zen', 'builds'] and not sourcehost:
            sourcehost = snapshot['host']

        if snapshot_type == 'cloud':
            args.append('--cloud')
            args.append(sourcehost)
            args.append('--namespace')
            args.append(snapshot['namespace'])
            args.append('--bucket')
            args.append(snapshot['bucket'])
            args.append('--key')
            args.append(snapshot['key'])
        elif snapshot_type == 'builds':
            args.append('--builds')
            args.append(sourcehost)
            args.append('--namespace')
            args.append(snapshot['namespace'])
            args.append('--bucket')
            args.append(snapshot['bucket'])
            args.append('--builds-id')
            args.append(snapshot['builds-id'])
        elif snapshot_type == 'zen':
            args.append('--zen')
            args.append(sourcehost)
            args.append('--source-project')
            args.append(snapshot['projectid'])
            args.append('--source-oplog')
            args.append(snapshot['oplogid'])
        elif snapshot_type == 'file':
            args.append('--file')
            args.append(snapshot['directory'])
            args.append('--name')
            args.append(snapshot['filename'])
        else:
            self.print_error(f"Unsupported snapshot type {snapshot_type}")
            return False

        return True

    def perform_import(self, snapshot, sourcehost, projectid, oplogid, asyncimport, forceimport):
        args = [
        "oplog-import",
        self.get_project_or_default(projectid),
        oplogid or snapshot['targetplatform'],
        "--ignore-missing-attachments",
        "--clean"
        ]

        if asyncimport:
            args.append("--async")

        if forceimport:
            args.append("--force")

        if not self.add_args_from_descriptor(snapshot, sourcehost, args):
            return False

        if snapshot['type'] in ['cloud', 'builds']:
            if not self.refresh_zen_token():
                return False

        return self.run_zen_utility(args)

    def perform_clean_post_import(self, snapshot, oplogid):
        target_oplogid = oplogid or snapshot['targetplatform']

        ue_context = self.get_unreal_context()
        target_dir = ue_context.get_project().get_dir() / f"Saved/Cooked/{self.conform_to_cook_forms(target_oplogid)}"

        for fsentry in os.scandir(target_dir):
            if fsentry.is_dir(follow_symlinks=False):
                shutil.rmtree(fsentry.path)
            else:
                if fsentry.name != 'ue.projectstore':
                    os.unlink(fsentry.path)

    def import_snapshot(self, snapshotdescriptor, sourcehost, projectid, oplogid, asyncimport, forceimport):
        if not self.perform_target_creation(snapshotdescriptor, projectid, oplogid):
            self.print_error(f"Error creating import target location")
            return False

        import_result = self.perform_import(snapshotdescriptor, sourcehost, projectid, oplogid, asyncimport, forceimport)
        if import_result == 0:
            self.perform_clean_post_import(snapshotdescriptor, oplogid)
        return import_result

    def get_workspaces(self):
        conn = http.client.HTTPConnection(host="localhost", port=8558, timeout=0.1)
        headers = {"Accept":"application/json"}
        workspaces_obj = {}
        try:
            conn.request('GET', '/ws', headers=headers)
            response = conn.getresponse()
            workspaces_obj = json.loads(response.read().decode())
        except OSError:
            return None

        if workspaces_obj:
            return workspaces_obj["workspaces"]
        return None

    def get_workspace(self, workspaces, base_dir):
        for workspace in workspaces:
            if workspace['root_path'] == base_dir:
                return workspace
        return None

    def get_workspaceshare(self, workspace, share_dir):
        for share in workspace.get('shares', []):
            if share['share_path'] == share_dir:
                return share
        return None

    def create_workspace(self, workspacepath, dynamic):
        workspaces = self.get_workspaces()
        workspace_exists = False
        for workspace in workspaces:
            if workspace['root_path'] == workspacepath:
                workspace_exists = True
                break

        if not workspace_exists:
            args = [
                "workspace",
                "create",
                workspacepath,
                "--allow-share-create-from-http" if dynamic else None
            ]
            if self.run_zen_utility(args) != 0:
                return None

        workspaces = self.get_workspaces()
        for workspace in workspaces:
            if workspace['root_path'] == workspacepath:
                return workspace

        return None

    def create_workspaceshare(self, workspacepath, sharepath, dynamic):
        existing_workspace = None
        workspaces = self.get_workspaces()
        for workspace in workspaces:
            if workspace['root_path'] == workspacepath:
                existing_workspace = workspace
                break

        if existing_workspace is None:
            existing_workspace = self.create_workspace(workspacepath, dynamic)
            if existing_workspace is None:
                return False

        existing_share = None
        for share in existing_workspace.get('shares', []):
            if share["share_path"] == sharepath:
                existing_share = share
                break

        if existing_share is None:
            args = [
                "workspace-share",
                "create",
                existing_workspace["id"],
                sharepath
            ]
            return self.run_zen_utility(args) == 0

        return True

#-------------------------------------------------------------------------------
class ZenUETargetBaseCmd(ZenBaseCmd):
    variant = unrealcmd.Arg("development", "Build variant to launch")
    runargs  = unrealcmd.Arg([str], "Additional arguments to pass to the process being launched")
    build    = unrealcmd.Opt(True, "Build the target prior to running it")

    def run_ue_program_target(self, target, cmdargs = None):
        return super().run_ue_program_target(target, self.args.variant, self.args.build, cmdargs, self.args.runargs)


#-------------------------------------------------------------------------------
class ZenUtilityBaseCmd(ZenBaseCmd):
    zenargs  = unrealcmd.Arg([str], "Additional arguments to pass to zen utility")

    def get_zen_utility_command(self, cmdargs):
        ue_context = self.get_unreal_context()
        engine = ue_context.get_engine()

        platform = unreal.Platform.get_host()
        exe_filename = "zen"
        if platform == "Win64":
            exe_filename = exe_filename + ".exe"

        bin_path = engine.get_dir()
        bin_path /= f"Binaries/{platform}/{exe_filename}"
        if not bin_path.is_file():
            raise FileNotFoundError(f"Unable to find '{bin_path}'")

        final_args = []
        if cmdargs: final_args += cmdargs
        if self.args.zenargs: final_args += self.args.zenargs

        return bin_path, final_args


    def run_zen_utility(self, cmdargs):
        bin_path, final_args = self.get_zen_utility_command(cmdargs)

        cmd = self.get_exec_context().create_runnable(bin_path, *final_args)
        return cmd.run()