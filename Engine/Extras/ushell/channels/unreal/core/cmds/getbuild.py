# Copyright Epic Games, Inc. All Rights Reserved.

import sys
import unrealcmd
import unreal
import tempfile
import json
import os
from peafour import P4
import urllib.request
import string
import fzf

#-------------------------------------------------------------------------------
class GetBuild(unrealcmd.Cmd):
    """ Download a build/artifact from UE Cloud Storage

Unless otherwise specified it is downloaded to <projectdir>/Staged/<platform>.

If your search only matches one build that build is downloaded for you, to add filtering you can use --match as follows:
    * --match 8v93f39cvn3           - If you know the build id:
    * --match 1337                  - To search for the build cloest to a changelist
    * --match job=v99f3;step=39f983 - For advanced searching you can specify key values

If you find multiple builds that match you will be given the option to pick one, were the * indicates the build closest to your current commit."""

    buildtype   = unrealcmd.Arg(str, "The type of build to download. Usually packaged or staged builds.")
    platforms   = unrealcmd.Arg([str], "The platform(s) you want to download from a build")
    runargs     = unrealcmd.Arg([str], "Additional arguments to pass to the process being launched")

    project     = unrealcmd.Opt("", "The project you want to download a build for. Defaults to the current project in ushell.")
    branch      = unrealcmd.Opt("", "Branch/Stream to fetch data for, defaults to current p4 stream")
    namespace   = unrealcmd.Opt("", "The namespace to use overriding what is defined in engine ini")
    host        = unrealcmd.Opt("", "The host name of the server to connect to, defaults to your cloud storage server in engine ini")
    clean       = unrealcmd.Opt(False, "Ignore any previously downloaded data and fetch it all again")
    verbose     = unrealcmd.Opt(False, "Enable verbose logging output")

    dest        = unrealcmd.Opt("", "The directory to download the build to. Local contents will be overwritten. Defaults to `<projectdir>/Staged/<platform>`")
    match       = unrealcmd.Opt("", "Either a build id (hexadecimal string), a changelist (number) or a key value seperate list of key=values you want to find a build for. e.g. changelist=1337;job=foo")

    def lookup_service_name(self):
        config = self.get_unreal_context().get_config()
        default = config.get("Engine", "StorageServers", "Default")
        if value := default.OAuthProviderIdentifier:
            return value
        else:
            cloud = config.get("Engine", "StorageServers", "Cloud")
            if value := cloud.OAuthProviderIdentifier:
                return value

    def lookup_namespace(self):
        config = self.get_unreal_context().get_config()
        default = config.get("Engine", "StorageServers", "Default")
        if value := default.BuildsNamespace:
            return value
        else:
            cloud = config.get("Engine", "StorageServers", "Cloud")
            if value := cloud.BuildsNamespace:
                return value

    def lookup_host(self):
        config = self.get_unreal_context().get_config()
        default = config.get("Engine", "StorageServers", "Default")
        host = ""
        if value := default.Host:
            host = str(value)
        else:
            cloud = config.get("Engine", "StorageServers", "Cloud")
            if value := cloud.Host:
                host=  str(value)

        # if host is a semicolon seperated list we parse out the first entry as zen cli does not yet support a list of hosts
        if host!= None and host.find(";") != -1:
            host = host.split(";")[0]

        return host

    def get_oidc_token_path(self):
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

        return str(bin_path)

    def complete_buildtype(self, prefix):
        return ["packaged", "staged"]

    def complete_platforms(self, prefix):
        server_platforms = ("win64", "linux", "mac")
        for platform in unrealcmd.Cmd.complete_platform(self, prefix):
            yield platform
            yield platform + "-client"
            if platform in server_platforms:
                yield platform + "-server"

    def complete_dest(self, prefix):
        prefix = prefix or "."
        for item in os.scandir(prefix):
            if item.is_dir():
                yield item.name + "/"

    def convert_platform(self, platform):
        wasClient = False
        wasServer = False
        if platform.endswith("-client"):
            platform = platform[0:-7]
            wasClient = True
        if platform.endswith("-server"):
            platform = platform[0:-7]
            wasServer = True

        try:
            convertedPlatform = self.get_platform(platform).get_config_name()
            if wasClient:
                return convertedPlatform + "-client"
            if wasServer:
                return convertedPlatform + "-server"
            return convertedPlatform
        except ValueError:
            # unknown platform, just assume the name it valid
            return platform

    def convert_buildtype(self, build_type):
        if build_type == "packaged":
            return "packaged-build"
        elif build_type == "staged":
            return "staged-build"
        else:
            return build_type

    def _get_p4_stream(self):
        self.print_info("Checking Perforce stream")
        if branch := self.args.branch:
            print("User provided:", branch)
            return branch

        branch_dir = self.get_unreal_context().get_branch().get_dir()
        info = P4(d=str(branch_dir)).info().run()

        branch = getattr(info, "clientStream", None)
        if not branch:
            raise EnvironmentError("Unable to get Perforce client name")
        print("Using stream", branch)
        # massage the perforce stream name to match expectations, no initial / and - seperation between depot and stream
        return branch.lstrip("/").replace("/", "-")

    def get_build_id(self, build_id_arg):
        if len(build_id_arg) == 24 and all(char in string.hexdigits for char in build_id_arg):
            return build_id_arg
        return None

    def build_query(self, search_request):
        search_changelist = None
        if search_request.isnumeric():
            search_changelist = search_request
            print("Searching for build closest to changelist: " + str(search_changelist))
            search_request = "" # we special case handle the search for changelists

        query = {}
        for statement in search_request.split(";"):
            if statement.find("=") != -1:
                k, v = statement.split("=")
                query[k] = {"$eq": v}

        if search_changelist:
            query["commit"] = {"$lte": search_changelist}

        return query

    def search_build(self, host, override_host, namespace, bucket, query):
        # Run a search against the api to find builds that matches the request
        temp_dir = tempfile.TemporaryDirectory()
        query_file_path = f"{temp_dir.name}/query.json"
        with open(query_file_path, "w") as query_file:
            json.dump(query, query_file)

        result_file_path = f"{temp_dir.name}/result.json"

        args = (
            "builds",
            "list",
            "--host=" + str(host) if host != None else "--override-host=" + str(override_host),
            "--namespace=" + str(namespace),
            "--bucket=" + bucket,
            "--oidctoken-exe-path=" + self.get_oidc_token_path(),
            "--query-path=" + query_file_path,
            "--result-path=" + result_file_path
        )

        result = self.run_zen(args, include_passthrough_args = False)
        if result != 0:
            raise Exception("Failed to search")

        with open(result_file_path, "r") as result_fd:
            results = json.load(result_fd)["results"]

        return [{"name" : r["metadata"]["name"], "platforms": {r["metadata"]["platform"]: r["buildId"]}, "commit": r["metadata"].get("commit", "")} for r in results]

    def _show_select_build_prompt(self, branch_cl, builds):
        closest_cl = 1 << 30
        p4_change_found = False
        active_platforms = set()
        available_builds = builds.values()
        for build in available_builds:
            cl = build["commit"]
            active_platforms.update(build["platforms"].keys())

            if type(cl) is int or (type(cl) is str and cl.isnumeric()):
                p4_change_found = True
                # is a changelist number
                if abs(int(cl) - branch_cl) < abs(closest_cl - branch_cl):
                    closest_cl = int(cl)

        def abbreviate(x):
            vowels_plus = ("a", "e", "i", "o", "u", "y")
            x = "".join(x for x in x if x not in vowels_plus)
            return x[:7]
        columns = {x:abbreviate(x) for x in active_platforms}

        def list_available(closest_cl, available_builds):
            for build in available_builds:
                cl = build["commit"]
                name = build["name"]
                platforms = build["platforms"].keys()
                middle = ""
                for column_platform,short_name in columns.items():
                    middle += " "
                    middle += short_name if column_platform in platforms else (" " * len(short_name))
                if cl != None:
                    if not p4_change_found:
                        closest_cl = branch_cl
                    cl = str(cl) + ("*" if cl == closest_cl else " ")
                yield "%10s%s %s" % (cl, middle, name)

        for line in fzf.run(list_available(closest_cl, available_builds), sort=False):
            segments = line.split()
            build_name = segments[-1]
            commit = segments[0].rstrip("*")
            build_key = build_name + commit

            if build_key in builds:
                return build_key
            elif build_name in builds:
                return build_name
            else:
                raise Exception(f"Unable to map choice {line} back to selected build")

    def run_zen(self, args, include_passthrough_args = True):
        ue_context = self.get_unreal_context()
        engine = ue_context.get_engine()

        bin_ext = ".exe" if os.name == "nt" else ""
        bin_type = unreal.Platform.get_host()

        bin_path = engine.get_dir()
        bin_path /= f"Binaries/{bin_type}/zen{bin_ext}"

        if not bin_path.is_file():
            raise FileNotFoundError(f"Unable to find '{bin_path}'")

        if include_passthrough_args:
            args = (*args, *self.args.runargs)
        cmd = self.get_exec_context().create_runnable(bin_path, *args)
        result = cmd.run()

        return result

    def main(self):
        ue_context = self.get_unreal_context()
        project = self.args.project
        project_path = None
        if not project:
            project = ue_context.get_project().get_name()
            project_path = os.path.dirname(ue_context.get_project().get_path())

        if len(self.args.platforms) == 0:
            self.print_error("You need to select at least one platform")
            return False

        build_type = self.convert_buildtype(self.args.buildtype)
        dest = self.args.dest
        if dest == "" and project_path:
            dest = os.path.join(project_path, self.args.buildtype.capitalize())

        build_id = self.get_build_id(self.args.match)
        branch = self._get_p4_stream()

        branch_cl = ue_context.get_engine().get_info()["Changelist"]
        print("Branch synced to:", branch_cl)

        if self.args.namespace:
            namespace = self.args.namespace
        else:
            namespace = self.lookup_namespace()

        host = None
        override_host = None
        if self.args.host:
            override_host = self.args.host
        else:
            host = self.lookup_host()

        if not host and not override_host:
            self.print_error("No host specified")
            return False
        if not namespace:
            self.print_error("Namespace was not resolved, unable to determine what to download.")
            return False

        platforms_to_download = [self.convert_platform(p) for p in self.args.platforms]

        builds_to_download = []
        if build_id:
            # a single build id was specified, this can only be used wit ha single platform as well
            if len(platforms_to_download) != 1:
                self.print_error("Only a single platform can be specified when specifying a build id")
                return False

            platform = platforms_to_download[0]
            bucket = f"{project}.{build_type}.{branch}.{platform}".lower()
            builds_to_download = [(bucket, build_id, platforms_to_download[0])]
        else:
            # we need to search
            found_builds = {}

            query = self.build_query(self.args.match)
            # Scan all platforms to see which ones have a build that matches
            for platform in platforms_to_download:
                self.print_info(f"Enumerating builds for platform {platform}")
                bucket = f"{project}.{build_type}.{branch}.{platform}".lower()

                builds = self.search_build(host, override_host, namespace, bucket, query)
                print(f"Found {len(builds)} builds")
                for build in builds:
                    build_name = build["name"]
                    commit_str = str(build.get("commit", ""))
                    build_key = build_name + commit_str
                    if build_name in found_builds:
                        # this build already existed but a new platform was found add it to the list if it didn't already exist
                        for platform in build["platforms"].keys():
                            if platform not in found_builds[build_key]["platforms"]:
                                found_builds[build_key]["platforms"][platform] = build["platforms"][platform]
                            else:
                                self.print_info(f"Multiple builds for platform {platform} found for build {build_name}. Using newer build.")

                    else:
                        found_builds[build_key] = build

            # remove builds that only have some of the platforms we want
            found_builds = {k:v for k, v in found_builds.items() if len(v["platforms"]) == len(platforms_to_download)}

            if found_builds == None or len(found_builds) == 0:
                self.print_error(f"Failed to find any build that matches query {query} in namespace {namespace} and bucket {bucket}")
                return False

            selected_build = None
            if len(found_builds) == 1:
                selected_build = next(iter(found_builds.values()))
                selected_build_name = selected_build["name"]
                print(f"Single build found, using build {selected_build_name}")
            else:
                # Ask the user to select a build
                print("Select build to fetch:", end="")
                selected_build_id = self._show_select_build_prompt(branch_cl, found_builds)
                if not selected_build_id:
                    self.print_error("No build was selected")
                    return False

                selected_build = found_builds[selected_build_id]
                selected_build_name = selected_build["name"]
                print(f"Using build {selected_build_name}")

            # build the selection of build ids to download from which buckets
            for platform, build_id in selected_build["platforms"].items():
                bucket = f"{project}.{build_type}.{branch}.{platform}".lower()
                builds_to_download.extend([(bucket, build_id, platform)])

        result = 0
        for bucket, build_id, build_platform in builds_to_download:
            if result:
                self.print_error(f"Returned error exit code {result}, aborting download")
                return False

            build_dest = os.path.join(dest, build_platform)
            args = (
                "builds",
                "download",
                "--host=" + str(host) if host != None else "--override-host=" + str(override_host),
                "--namespace=" + str(namespace),
                "--bucket=" + bucket,
                "--oidctoken-exe-path=" + self.get_oidc_token_path(),
                "--clean" if self.args.clean else "",
                "--verbose" if self.args.verbose else "",
                build_dest,
                build_id
            )

            result = self.run_zen(args)

        return result
