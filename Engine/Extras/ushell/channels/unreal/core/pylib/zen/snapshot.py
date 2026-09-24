# Copyright Epic Games, Inc. All Rights Reserved.

import bisect
import flow.cmd
import json
import os
import pathlib
import unreal
import zen.cmd
import unrealcmd
import urllib.request

from abc import ABC, abstractmethod

#-------------------------------------------------------------------------------
class BuildIndexException(Exception):
    pass

#-------------------------------------------------------------------------------
class BuildIndex(ABC):
    @abstractmethod
    def get_available_changelists(self, project, platform, branch, runtime, flavor):
        pass

    @abstractmethod
    def find_build(self, project, platform, branch, runtime, flavor, buildtype, changelist = 0):
        pass

    @abstractmethod
    def get_snapshot_descriptor(self, project, platform, branch, runtime, flavor, buildtype, changelist):
        pass

#-------------------------------------------------------------------------------
class UnrealCloudDDCBuildIndex(BuildIndex):
    def __init__(self, host, access_token, context):
        self.host = host
        self.access_token = access_token
        self.context = context

    # Transform the escaped branch into a string that's valid for use in a cloud namespace
    def sanitize_branch(self, branch):
        return branch.lower().replace("++", "").replace("+", "-").replace(".", "-")

    def get_namespace(self, branch):
        config = self.context.get_config().get("Engine", "StorageServers", "Cloud")
        if config.BuildsNamespace:
            return str(config.BuildsNamespace)

        raise BuildIndexException("Unable to determine builds namespace from [StorageServers] config section")

    def get_bucket(self, project, branch, platform, runtime, flavor):
        branch = self.sanitize_branch(branch)
        if platform.lower() == "win64":
            platform = "windows"
        elif platform.lower() == "android":
            if not flavor:
                flavor = "etc2"

        if flavor:
            platform = platform + "_" + flavor

        platform = platform.lower()

        if runtime.lower() != "game":
            platform += runtime.lower()

        if platform.lower() == "windowsserver":
            platform = "linuxserver"

        return f"{project}.oplog.{branch}.{platform}".lower()

    def query_builds(self, project, platform, branch, runtime, flavor, buildtype = "", changelist = 0, try_alternate = True):
        namespace = self.get_namespace(branch)
        bucket = self.get_bucket(project, branch, platform, runtime, flavor)

        url = f"{self.host}/api/v2/builds/{namespace}/{bucket}/search"
        headers = {
            "Authorization" : f"Bearer {self.access_token}",
            "Content-Type" : "application/json"
        }

        query = {}
        if changelist != 0:
            query["changelist"] = {"$lte" : changelist}

        query["runtime"] = {"$eq" : runtime}
        query["branch"] = {"$eq" : branch.replace("+", "/")}

        data = json.dumps({"query": query}).encode()
        request = urllib.request.Request(url, data=data, headers=headers)
        response = urllib.request.urlopen(request)
        body = response.read().decode()

        results = json.loads(body)["results"]
        results.sort(key=lambda r: r["metadata"]["changelist"])

        if not results and try_alternate:
            alternate_runtime = ""
            if runtime == "client" or runtime == "server":
                alternate_runtime = "game"
            elif runtime == "game":
                alternate_runtime = "client"

            results = self.query_builds(project, platform, branch, alternate_runtime, flavor, buildtype, changelist, False)
            if results:
                error = f"No builds found for project '{project}' and runtime '{runtime}'.\n"
                error += f"Builds are available for the '{alternate_runtime}' runtime. Did you mean to use the '{alternate_runtime}' runtime instead of the '{runtime}' runtime?"
                raise BuildIndexException(error)

        if buildtype:
            results = [r for r in results if r["metadata"]["hordeTemplateId"].startswith(buildtype)]

        return results

    def get_available_changelists(self, project, platform, branch, runtime, flavor):
        builds = self.query_builds(project, platform, branch, runtime, flavor)
        return list(map(lambda build : int(build["metadata"]["changelist"]), builds))

    def find_build(self, project, platform, branch, runtime, flavor, buildtype, changelist):
        builds = self.query_builds(project, platform, branch, runtime, flavor, buildtype, changelist)

        if len(builds) == 0:
            return changelist, []

        changelist = int(builds[-1]["metadata"]["changelist"])
        return changelist, builds

    def get_snapshot_descriptor(self, project, platform, branch, runtime, flavor, buildtype, changelist):
        builds = self.query_builds(project, platform, branch, runtime, flavor, buildtype, changelist)
        if len(builds) == 0:
            return None

        build = builds[-1]
        builds_id = build["buildId"]

        return int(build["metadata"]["changelist"]), {
            "type" : "builds",
            "host" : self.host,
            "namespace" : self.get_namespace(branch),
            "bucket" : self.get_bucket(project, branch, platform, runtime, flavor),
            "builds-id" : builds_id,
            "targetplatform" : build["metadata"]["cookPlatform"],
            "name" : build["metadata"]["name"]
        }

#-------------------------------------------------------------------------------
class FileshareBuildIndex(BuildIndex):
    def __init__(self, buildroot, context, host_platform):
        self.buildroot = buildroot
        self.context = context
        self.host_platform = host_platform

    @staticmethod
    def get_platform_subdir(platform, runtime):
        runtime_suffix = ""
        if runtime.lower() != "game":
            runtime_suffix = runtime.title()

        platform_subdir = None

        if platform.lower() == "win64":
            platform_subdir = "Windows" + runtime_suffix
        elif platform.lower() == "android":
            platform_subdir = "Android"
        else:
            platform_subdir = platform.title() + runtime_suffix

        return platform_subdir

    def get_build_root(self):
        if self.buildroot == "":
            engine = self.context.get_engine()
            version = engine.get_info()
            if not version:
                raise BuildIndexException("Unable to determine engine version information")

            host_platform_name = self.host_platform
            root = self.context.get_config().get("Editor", "FileshareBuildIndex", "RootDirectory")
            path = self.context.get_config().get("Editor", "FileshareBuildIndex", "Path")

            if not root or not path:
                error = "Unable to determine fileshare build root from editor configuration.\n"
                error += "To use a fileshare build index, you must have a [FileshareBuildIndex] section in an editor .ini file with the following elements:\n"
                error += "  RootDirectory(Windows=<fileshare root directory>, Mac=<fileshare root directory>, Linux=<fileshare root directory>)\n"
                error += "  Path=<shared path>\n"
                error += "The placeholders {Branch} and {Project} can be used to fill in snapshot information for a given Perforce branch and/or project.\n"
                error += "For example:\n"
                error += "  RootDirectory=(Windows=\\\\my.unc.path\\, Mac=/Volumes, Linux=/mnt)\n"
                error += "  Path=Snapshots/{Branch}/{Project}"
                raise BuildIndexException(error)

            if host_platform_name == "Win64":
                self.buildroot = os.path.join(str(root.Windows), str(path))
            elif host_platform_name == "Mac":
                self.buildroot = os.path.join(str(root.Mac), str(path))
            else:
                self.buildroot = os.path.join(str(root.Linux), str(path))

        return self.buildroot

    def get_builds_dir(self, project, platform, branch, runtime):
        project_name = self.context.get_project().get_name()
        project_branch_dir = self.get_build_root().replace("{Branch}", branch).replace("{Project}", project)

        platform_subdir = FileshareBuildIndex.get_platform_subdir(platform, runtime)
        builds_dir = os.path.join(project_branch_dir, platform_subdir)

        if not os.path.exists(builds_dir):
            error = f"Unable to reach build index. Please ensure that the build index location is correct and reachable (may require mounting and/or VPN access): {builds_dir}"
            alternate_runtime = ""
            if runtime == "client" or runtime == "server":
                alternate_runtime = "game"
            elif runtime == "game":
                alternate_runtime = "client"

            alternate_builds_dir = os.path.join(project_branch_dir, FileshareBuildIndex.get_platform_subdir(platform, alternate_runtime))
            if os.path.exists(alternate_builds_dir):
                error += f"\nA build index was available for the '{alternate_runtime}' runtime. Did you mean to use the '{alternate_runtime}' runtime instead of the '{runtime}' runtime?"

            raise BuildIndexException(error)

        return builds_dir

    def get_available_changelists(self, project, platform, branch, runtime, flavor):
        builds_dir = self.get_builds_dir(project, platform, branch, runtime)
        if builds_dir == None:
            return []

        return [int(dir.name) for dir in os.scandir(builds_dir) if dir.name.isdecimal()]

    def get_filtered_builds(self, project, platform, branch, runtime, buildtype, changelist):
        builds_dir_for_changelist = os.path.join(self.get_builds_dir(project, platform, branch, runtime), str(changelist))
        builds = []

        try:
            builds_unfiltered = os.scandir(builds_dir_for_changelist)
            if buildtype:
                for build in builds_unfiltered:
                    build_name_lower = build.name.lower()
                    if build_name_lower.startswith(buildtype.lower()):
                        builds.append(build.name)
            else:
                for build in builds_unfiltered:
                    builds.append(build.name)
        except:
            pass

        return builds

    def find_build(self, project, platform, branch, runtime, flavor, buildtype, changelist):
        builds_dir_for_changelist = ""
        builds = []

        current_iteration = 0
        max_iterations = 20

        buildchangelists = None

        while not builds and (current_iteration < max_iterations):
            builds = self.get_filtered_builds(project, platform, branch, runtime, buildtype, changelist)

            if not builds:
                if buildchangelists is None:
                    try:
                        buildchangelistdirs = os.scandir(self.get_builds_dir(project, platform, branch, runtime))
                    except:
                        buildchangelistdirs = []

                    buildchangelists = []
                    for buildchangelistdir in buildchangelistdirs:
                        try:
                            buildchangelists.append(int(buildchangelistdir.name))
                        except:
                            pass
                    buildchangelists.sort()
                i = bisect.bisect_right(buildchangelists, changelist-1)
                if i != 0:
                    changelist = buildchangelists[i-1]
            current_iteration = current_iteration + 1

        return int(changelist), builds

    def get_snapshot_descriptor(self, project, platform, branch, runtime, flavor, buildtype, changelist):
        builds_dir = self.get_builds_dir(project, platform, branch, runtime)
        if builds_dir is None:
            return None

        found_changelist, builds = self.find_build(project, platform, branch, runtime, flavor, buildtype, changelist)
        if not builds:
            raise BuildIndexException("No build found")

        descriptorfile = os.path.join(builds_dir, str(found_changelist), builds[0])
        try:
            with (open(descriptorfile, "rt")) as file:
                descriptors = json.load(file)
                return int(found_changelist), descriptors["snapshots"][0]
        except:
            raise BuildIndexException(f"Error loading snapshot descriptor from file {descriptorfile}")

#-------------------------------------------------------------------------------
class _Base(zen.cmd.ZenUtilityBaseCmd):
    runtime     = flow.cmd.Arg(str, "Type of runtime to use (client, server, game)")
    platform    = flow.cmd.Arg(str, "Platform to use")
    changelist  = flow.cmd.Arg(-1, "Changelist to query snapshot for (defaults to current changelist)")
    buildroot   = unrealcmd.Opt("", "The root directory to use for builds (defaults to fileshare roots specified in [FileshareBuildIndex] editor configuration section")
    nofileshare = flow.cmd.Opt(False, "Find snapshots stored in Unreal Cloud DDC rather than using the network fileshare")
    fileshare   = flow.cmd.Opt(False, "Find snapshots stored on the network fileshare instead of searching Unreal Cloud")
    cloudhost   = unrealcmd.Opt("", "The cloud host to use for builds (defaults to cloud host specified in [StorageServers] engine configuration section)")
    flavor      = unrealcmd.Opt("", "Flavor of build to retrieve (e.g. 'ASTC' or 'ETC2' for Android builds).")
    buildtype   = flow.cmd.Opt("", "Specific build type to get like nightly, standard, etc... (defaults to any type available)")
    build_index = None

    def get_build_index(self):
        if not self.build_index:
            if self.args.nofileshare:
                self.print_info("--nofileshare is now the default for .snapshot commands. If you would like to use the fileshare instead of the Unreal Cloud build index, use --fileshare.")
            if not self.args.fileshare:
                if not self.refresh_zen_token():
                    self.print_error("Unable to get cloud access token")
                    return None

                if not self.args.cloudhost:
                    config = self.get_unreal_context().get_config().get("Engine", "StorageServers", "Cloud")
                    if config.Host:
                        self.args.cloudhost = str(config.Host).split(";")[0]
                    else:
                        error = "Unable to determine cloud host from [StorageServers] config section\n"
                        error += "To use an Unreal Cloud build index, you must have a [StorageServers] config section in an engine .ini file with a value Cloud=(Host=https://my.cloud.host)"
                        raise BuildIndexException(error)

                self.build_index = UnrealCloudDDCBuildIndex(self.args.cloudhost, self._AccessToken, self.get_unreal_context())
                self.print_info(f"Finding snapshots from builds URL {self.build_index.host}")
            else:
                self.build_index = FileshareBuildIndex(self.args.buildroot, self.get_unreal_context(), self.get_host_platform().get_name())
                self.print_info(f"Finding snapshots from fileshare build root {self.build_index.get_build_root()}")

        return self.build_index

    def complete_changelist(self, prefix):
        context = self.get_unreal_context()
        engine = context.get_engine()
        version = engine.get_info()
        if version:
            branch_name = version["BranchName"]
            project_name = context.get_project().get_name()

            changelists = self.get_build_index().get_available_changelists(project_name, self.args.platform, branch_name, self.args.runtime, self.args.flavor)
            for changelist in changelists:
                yield changelist

    def complete_runtime(self, prefix):
        yield "client"
        yield "server"
        yield "game"

    def complete_platform(self, prefix):
        context = self.get_unreal_context()
        engine = context.get_engine()
        version = engine.get_info()
        if version:
            branch_name = version["BranchName"]
            for platform in unrealcmd.Cmd.complete_platform(self, prefix):
                if platform == "android":
                    if self.args.runtime == "server":
                        continue

                yield platform

    def get_changelist(self):
        changelist = self.args.changelist
        if changelist < 1:
            changelist = self.get_unreal_context().get_engine().get_info()["Changelist"]
        return changelist

    def get_project_name(self):
        return self.get_unreal_context().get_project().get_name()

    def get_branch_name(self):
        return self.get_unreal_context().get_engine().get_info()["BranchName"]

#-------------------------------------------------------------------------------
class Find(_Base):
    """ Finds the best available snapshot by runtime and platform while filling
    in remaining details like project and branch using ushell context information."""

    buildtype = flow.cmd.Opt("", "Specific build type to get like nightly, standard, etc... (defaults to any type available)")

    def complete_projectid(self, prefix):
        return self.perform_project_completion(prefix)

    def complete_oplog(self, prefix):
        return self.perform_oplog_completion(prefix, self.get_project_or_default(self.args.projectid))

    def main(self):
        found_changelist = self.get_closest_changelist()
        if found_changelist < 1:
            return False

        print(found_changelist)
        changelist = self.get_changelist()
        if (found_changelist == changelist):
            print(f"Exact match for target changelist")
        else:
            print(f"Closest candidate for target changelist '{changelist}'")

    def get_closest_changelist(self):
        changelist = self.get_changelist()
        project = self.get_project_name()
        branch = self.get_branch_name()

        try:
            found_changelist, builds = self.get_build_index().find_build(project, self.args.platform, branch, self.args.runtime, self.args.flavor, self.args.buildtype, changelist)

            if not builds:
                self.print_error(f"No build found")
                return 0

        except BuildIndexException as e:
            self.print_error(e)
            return 0

        return found_changelist

#-------------------------------------------------------------------------------
class Get(_Base):
    """ Gets a cooked data snapshot by runtime and platform while filling in
    remaining details like project, branch, and even changelist using ushell
    context information.  If a snapshot cannot be found for the precise changelist,
    offers to pick nearest preceding changelist that matches the other criteria."""

    buildtype   = flow.cmd.Opt("", "Specific build type to get like nightly, standard, etc... (defaults to any type available)")
    projectid   = unrealcmd.Opt("", "The zen project ID to import into (defaults to an ID based on the current ushell project)")
    oplog       = unrealcmd.Opt("", "The zen oplog to import into (defaults to the oplog name in the snapshot)")
    sourcehost  = unrealcmd.Opt("", "The source host to import from (defaults to the host specified in the snapshot)")
    asyncimport = unrealcmd.Opt(False, "Trigger import but don't wait for completion")
    forceimport = unrealcmd.Opt(False, "Force import of all attachments")

    def complete_projectid(self, prefix):
        return self.perform_project_completion(prefix)

    def complete_oplog(self, prefix):
        return self.perform_oplog_completion(prefix, self.get_project_or_default(self.args.projectid))

    def main(self):
        self.launch_zenserver_if_not_running()
        changelist = self.get_changelist()
        project_name = self.get_project_name()
        branch = self.get_branch_name()

        try:
            found_changelist, snapshotdescriptor = self.get_build_index().get_snapshot_descriptor(project_name, self.args.platform, branch, self.args.runtime, self.args.flavor, self.args.buildtype, changelist)

            while found_changelist != changelist:
                self.print_error(f"No snapshot found for the specified project({project_name}), branch({branch}), platform({self.args.platform}), runtime({self.args.runtime}), and changelist({changelist})")
                print("Now you have a few courses of action available to you;")
                print("  use closest [p]receding snapshot changelist")
                print("  retry [Enter]")
                print("  abort [Ctrl-C]")

                choice = input("Which one do you fancy? [p/Enter/Ctrl-C] ")
                if choice == "p":
                    break

                found_changelist, snapshotdescriptor = self.get_build_index().get_snapshot_descriptor(project_name, self.args.platform, version["BranchName"], self.args.runtime, self.args.flavor, self.args.buildtype, changelist)

            if not snapshotdescriptor:
                self.print_error(f"No build found")
                return False

            print(f"Importing snapshot for specified project({project_name}), branch({branch}), platform({self.args.platform}), runtime({self.args.runtime}), and changelist({changelist}) from {snapshotdescriptor['type']} snapshot:")
            print(snapshotdescriptor)

            self.import_snapshot(snapshotdescriptor, self.args.sourcehost, self.args.projectid, self.args.oplog, self.args.asyncimport, self.args.forceimport)
            self.post_import_snapshot(found_changelist)
        except BuildIndexException as e:
            self.print_error(e)

    def post_import_snapshot(self, snapshot_changelist):
        return

#-------------------------------------------------------------------------------
class List(_Base):
    """ Lists available snapshots by runtime and platform while filling in
    remaining details like project and branch using ushell context information."""

    def complete_projectid(self, prefix):
        return self.perform_project_completion(prefix)

    def complete_oplog(self, prefix):
        return self.perform_oplog_completion(prefix, self.get_project_or_default(self.args.projectid))

    def main(self):
        changelists = self.get_snapshot_changelists()
        if not changelists:
            return False

        current_changelist = self.get_changelist()
        for changelist in changelists:
            if int(changelist) == current_changelist:
                print(f"{changelist}*")
            else:
                print(f"{changelist}")

    def get_snapshot_changelists(self):
        project_name = self.get_project_name()
        branch_name = self.get_branch_name()

        try:
            changelists = self.get_build_index().get_available_changelists(project_name, self.args.platform, branch_name, self.args.runtime, self.args.flavor)
        except BuildIndexException as e:
            self.print_error(e)
            return []

        if not changelists:
            self.print_error("No build found")
            return []

        return changelists
