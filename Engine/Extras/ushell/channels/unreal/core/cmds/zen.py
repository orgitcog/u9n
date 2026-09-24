# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys
import shutil
import unreal
import unrealcmd
import unreal.cmdline
import zen.cmd
import http.client
import json
import hashlib

#-------------------------------------------------------------------------------
class Dashboard(zen.cmd.ZenUETargetBaseCmd):
    """ Starts the zen dashboard GUI."""

    def main(self):
        return self.run_ue_program_target("ZenDashboard")

#-------------------------------------------------------------------------------
class Start(zen.cmd.ZenUETargetBaseCmd):
    """ Starts an instance of zenserver."""
    SponsorProcessID = unrealcmd.Opt("", "Process ID to be added as a sponsor process for the zenserver process")

    def main(self):
        args = []
        if self.args.SponsorProcessID:
            args.append("-SponsorProcessID=" + str(self.args.SponsorProcessID))
        elif sys.platform != 'win32':
            grandparent_pid = os.popen("ps -p %d -oppid=" % os.getppid()).read().strip()
            args.append("-SponsorProcessID=" + grandparent_pid)
        else:
            args.append("-SponsorProcessID=" + str(os.getppid()))

        return self.run_ue_program_target("ZenLaunch", args)

#-------------------------------------------------------------------------------
class Stop(zen.cmd.ZenUtilityBaseCmd):
    """ Stops any running instance of zenserver."""

    def main(self):
        return self.run_zen_utility(["down"])

#-------------------------------------------------------------------------------
class Status(zen.cmd.ZenUtilityBaseCmd):
    """ Get the status of running zenserver instances."""

    def main(self):
        return self.run_zen_utility(["status"])

#-------------------------------------------------------------------------------
class Version(zen.cmd.ZenUtilityBaseCmd):
    """ Get the version of the in-tree zenserver executable."""

    def main(self):
        return self.run_zen_utility(["version"])

#-------------------------------------------------------------------------------
class ImportSnapshot(zen.cmd.ZenUtilityBaseCmd):
    """ Imports an oplog snapshot into the running zenserver process."""
    snapshotdescriptor = unrealcmd.Arg(str, "Snapshot descriptor file path to import from")
    snapshotindex = unrealcmd.Arg(0, "0-based index of the snapshot within the snapshot descriptor to import from")
    projectid = unrealcmd.Opt("", "The zen project ID to import into (defaults to an ID based on the current ushell project)")
    oplog = unrealcmd.Opt("", "The zen oplog to import into (defaults to the oplog name in the snapshot)")
    sourcehost = unrealcmd.Opt("", "The source host to import from (defaults to the host specified in the snapshot)")
    asyncimport = unrealcmd.Opt(False, "Trigger import but don't wait for completion")
    forceimport = unrealcmd.Opt(False, "Force import of all attachments")

    def complete_projectid(self, prefix):
        return self.perform_project_completion(prefix)

    def complete_oplog(self, prefix):
        return self.perform_oplog_completion(prefix, self.get_project_or_default(self.args.projectid))

    def complete_snapshotdescriptor(self, prefix):
        prefix = prefix or "."
        for item in os.scandir(prefix):
            if item.name.endswith(".json"):
                yield item.name
                return

        for item in os.scandir(prefix):
            if item.is_dir():
                yield item.name + "/"

    @unrealcmd.Cmd.summarise
    def main(self):
        self.launch_zenserver_if_not_running()
        try:
            with open(self.args.snapshotdescriptor, "rt") as file:
                descriptor = json.load(file)
        except FileNotFoundError:
            self.print_error(f"Error accessing snapshot descriptor file {self.args.snapshotdescriptor}")
            return False

        snapshot = descriptor["snapshots"][self.args.snapshotindex]

        return self.import_snapshot(snapshot, self.args.sourcehost, self.args.projectid, self.args.oplog, self.args.asyncimport, self.args.forceimport)

#-------------------------------------------------------------------------------
class CreateWorkspace(zen.cmd.ZenUtilityBaseCmd):
    """ Create a Zen workspace"""
    base_dir = unrealcmd.Arg(str, "Base path for the Zen workspace")
    dynamic = unrealcmd.Opt(False, "Should the zen server allow for dynamic creation of shares by clients in this workspace")

    @unrealcmd.Cmd.summarise
    def main(self):
        self.launch_zenserver_if_not_running()
        workspaces = self.get_workspaces()
        workspace = self.get_workspace(workspaces, self.args.base_dir)
        if workspace is None:
            self.create_workspace(self.args.base_dir, self.args.dynamic)
        else:
            self.print_info("Workspace already exists at this location")

#-------------------------------------------------------------------------------
class CreateShare(zen.cmd.ZenUtilityBaseCmd):
    """ Create a Zen workspace share"""
    share_dir = unrealcmd.Arg(str, "Base path for the Zen share. Must be inside an already existing workspace")

    @unrealcmd.Cmd.summarise
    def main(self):
        self.launch_zenserver_if_not_running()
        workspaces = self.get_workspaces()
        share_path = os.path.abspath(self.args.share_dir)
        found_workspace = None
        for workspace in workspaces:
            workspace_abs_path = os.path.abspath(workspace['root_path'])
            if (os.path.commonpath([workspace_abs_path]) == os.path.commonpath([workspace_abs_path, share_path])):
                found_workspace = workspace
                share_path = os.path.relpath(share_path, workspace_abs_path)
                break

        if found_workspace is None:
            self.print_error("No workspace found at " + share_path)
            return False

        self.create_workspaceshare(workspace['root_path'], share_path, False)
