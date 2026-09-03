using System.Drawing;
using System.Windows.Forms;

namespace UnrealGameSync
{
	partial class WorkspaceControl
	{
		/// <summary> 
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		#region Component Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			components = new System.ComponentModel.Container();
			OptionsContextMenu = new ContextMenuStrip(components);
			OptionsContextMenu_ApplicationSettings = new ToolStripMenuItem();
			toolStripSeparator2 = new ToolStripSeparator();
			OptionsContextMenu_ScheduledSync = new ToolStripMenuItem();
			OptionsContextMenu_SyncPrecompiledBinaries = new ToolStripMenuItem();
			disabledToolStripMenuItem = new ToolStripMenuItem();
			toolStripSeparator11 = new ToolStripSeparator();
			editorToolStripMenuItem = new ToolStripMenuItem();
			editorPhysXToolStripMenuItem = new ToolStripMenuItem();
			OptionsContextMenu_ImportSnapshots = new ToolStripMenuItem();
			OptionsContextMenu_AutoResolveConflicts = new ToolStripMenuItem();
			OptionsContextMenu_AlwaysClobberFiles = new ToolStripMenuItem();
			OptionsContextMenu_AlwaysDeleteFiles = new ToolStripMenuItem();
			OptionsContextMenu_SyncFilter = new ToolStripMenuItem();
			OptionsContextMenu_Presets = new ToolStripMenuItem();
			toolStripSeparator3 = new ToolStripSeparator();
			OptionsContextMenu_EditorBuildConfiguration = new ToolStripMenuItem();
			OptionsContextMenu_BuildConfig_Debug = new ToolStripMenuItem();
			OptionsContextMenu_BuildConfig_DebugGame = new ToolStripMenuItem();
			OptionsContextMenu_BuildConfig_Development = new ToolStripMenuItem();
			OptionsContextMenu_CustomizeBuildSteps = new ToolStripMenuItem();
			OptionsContextMenu_EditorArguments = new ToolStripMenuItem();
			toolStripSeparator5 = new ToolStripSeparator();
			tabLabelsToolStripMenuItem = new ToolStripMenuItem();
			OptionsContextMenu_TabNames_Stream = new ToolStripMenuItem();
			OptionsContextMenu_TabNames_WorkspaceName = new ToolStripMenuItem();
			OptionsContextMenu_TabNames_WorkspaceRoot = new ToolStripMenuItem();
			OptionsContextMenu_TabNames_ProjectFile = new ToolStripMenuItem();
			showChangesToolStripMenuItem = new ToolStripMenuItem();
			OptionsContextMenu_ShowChanges_ShowUnreviewed = new ToolStripMenuItem();
			OptionsContextMenu_ShowChanges_ShowAutomated = new ToolStripMenuItem();
			OptionsContextMenu_TimeZone = new ToolStripMenuItem();
			OptionsContextMenu_TimeZone_Local = new ToolStripMenuItem();
			OptionsContextMenu_TimeZone_PerforceServer = new ToolStripMenuItem();
			toolStripSeparator6 = new ToolStripSeparator();
			OptionsContextMenu_Diagnostics = new ToolStripMenuItem();
			RunAfterSyncCheckBox = new Controls.ReadOnlyCheckbox();
			BuildAfterSyncCheckBox = new Controls.ReadOnlyCheckbox();
			AfterSyncingLabel = new Label();
			BuildListContextMenu = new ContextMenuStrip(components);
			BuildListContextMenu_LaunchEditor = new ToolStripMenuItem();
			BuildListContextMenu_Sync = new ToolStripMenuItem();
			BuildListContextMenu_SyncContentOnly = new ToolStripMenuItem();
			BuildListContextMenu_SyncOnlyThisChange = new ToolStripMenuItem();
			BuildListContextMenu_Build = new ToolStripMenuItem();
			BuildListContextMenu_Rebuild = new ToolStripMenuItem();
			BuildListContextMenu_GenerateProjectFiles = new ToolStripMenuItem();
			BuildListContextMenu_Cancel = new ToolStripMenuItem();
			BuildListContextMenu_OpenVisualStudio = new ToolStripMenuItem();
			BuildListContextMenu_Bisect_Separator = new ToolStripSeparator();
			BuildListContextMenu_Bisect_Pass = new ToolStripMenuItem();
			BuildListContextMenu_Bisect_Fail = new ToolStripMenuItem();
			BuildListContextMenu_Bisect_Include = new ToolStripMenuItem();
			BuildListContextMenu_Bisect_Exclude = new ToolStripMenuItem();
			toolStripSeparator4 = new ToolStripSeparator();
			BuildListContextMenu_MarkGood = new ToolStripMenuItem();
			BuildListContextMenu_MarkBad = new ToolStripMenuItem();
			BuildListContextMenu_WithdrawReview = new ToolStripMenuItem();
			BuildListContextMenu_LeaveComment = new ToolStripMenuItem();
			BuildListContextMenu_EditComment = new ToolStripMenuItem();
			BuildListContextMenu_StartInvestigating = new ToolStripMenuItem();
			BuildListContextMenu_FinishInvestigating = new ToolStripMenuItem();
			toolStripSeparator1 = new ToolStripSeparator();
			BuildListContextMenu_AddStar = new ToolStripMenuItem();
			BuildListContextMenu_RemoveStar = new ToolStripMenuItem();
			BuildListContextMenu_TimeZoneSeparator = new ToolStripSeparator();
			BuildListContextMenu_ShowServerTimes = new ToolStripMenuItem();
			BuildListContextMenu_ShowLocalTimes = new ToolStripMenuItem();
			BuildListContextMenu_CustomTool_Start = new ToolStripSeparator();
			BuildListContextMenu_CustomTool_End = new ToolStripSeparator();
			BuildListContextMenu_ViewInSwarm = new ToolStripMenuItem();
			BuildListContextMenu_CopyChangelistNumber = new ToolStripMenuItem();
			BuildListContextMenu_OpenPreflight = new ToolStripMenuItem();
			BuildListContextMenu_MoreInfo = new ToolStripMenuItem();
			NotifyIcon = new NotifyIcon(components);
			toolStripSeparator7 = new ToolStripSeparator();
			BuildListToolTip = new ToolTip(components);
			flowLayoutPanel1 = new FlowLayoutPanel();
			GenerateAfterSyncCheckBox = new Controls.ReadOnlyCheckbox();
			OpenSolutionAfterSyncCheckBox = new Controls.ReadOnlyCheckbox();
			tableLayoutPanel3 = new TableLayoutPanel();
			OptionsButton = new Button();
			FilterButton = new Button();
			tableLayoutPanel2 = new TableLayoutPanel();
			Splitter = new LogSplitContainer();
			StatusLayoutPanel = new TableLayoutPanel();
			StatusPanel = new StatusPanel();
			BuildList = new BuildListControl();
			IconColumn = new ColumnHeader();
			TypeColumn = new ColumnHeader();
			ChangeColumn = new ColumnHeader();
			TimeColumn = new ColumnHeader();
			AuthorColumn = new ColumnHeader();
			DescriptionColumn = new ColumnHeader();
			CISColumn = new ColumnHeader();
			ReviewerColumn = new ColumnHeader();
			PreflightColumn = new ColumnHeader();
			StatusColumn = new ColumnHeader();
			panel1 = new Panel();
			SyncLog = new LogControl();
			MoreToolsContextMenu = new ContextMenuStrip(components);
			MoreActionsContextMenu_CustomToolSeparator = new ToolStripSeparator();
			MoreToolsContextMenu_UpdateTools = new ToolStripMenuItem();
			MoreToolsContextMenu_CleanWorkspace = new ToolStripMenuItem();
			SyncContextMenu = new ContextMenuStrip(components);
			SyncContexMenu_EnterChangelist = new ToolStripMenuItem();
			toolStripSeparator8 = new ToolStripSeparator();
			StreamContextMenu = new ContextMenuStrip(components);
			RecentMenu = new ContextMenuStrip(components);
			RecentMenu_Browse = new ToolStripMenuItem();
			toolStripSeparator9 = new ToolStripSeparator();
			RecentMenu_Separator = new ToolStripSeparator();
			RecentMenu_ClearList = new ToolStripMenuItem();
			BuildListMultiContextMenu = new ContextMenuStrip(components);
			BuildListMultiContextMenu_Bisect = new ToolStripMenuItem();
			BuildListMultiContextMenu_TimeZoneSeparator = new ToolStripSeparator();
			BuildListMultiContextMenu_ShowServerTimes = new ToolStripMenuItem();
			BuildListMultiContextMenu_ShowLocalTimes = new ToolStripMenuItem();
			FilterContextMenu = new ContextMenuStrip(components);
			FilterContextMenu_Default = new ToolStripMenuItem();
			FilterContextMenu_BeforeBadgeSeparator = new ToolStripSeparator();
			FilterContextMenu_Type = new ToolStripMenuItem();
			FilterContextMenu_Type_ShowAll = new ToolStripMenuItem();
			toolStripSeparator10 = new ToolStripSeparator();
			FilterContextMenu_Type_Code = new ToolStripMenuItem();
			FilterContextMenu_Type_Content = new ToolStripMenuItem();
			FilterContextMenu_Badges = new ToolStripMenuItem();
			FilterContextMenu_Robomerge = new ToolStripMenuItem();
			FilterContextMenu_Robomerge_ShowAll = new ToolStripMenuItem();
			FilterContextMenu_Robomerge_ShowBadged = new ToolStripMenuItem();
			FilterContextMenu_Robomerge_ShowNone = new ToolStripMenuItem();
			FilterContextMenu_AfterRobomergeShowSeparator = new ToolStripSeparator();
			FilterContextMenu_Robomerge_Annotate = new ToolStripMenuItem();
			FilterContextMenu_Author = new ToolStripMenuItem();
			FilterContextMenu_Author_Name = new ToolStripTextBox();
			FilterContextMenu_AfterBadgeSeparator = new ToolStripSeparator();
			FilterContextMenu_ShowBuildMachineChanges = new ToolStripMenuItem();
			BadgeContextMenu = new ContextMenuStrip(components);
			BuildHealthContextMenu = new ContextMenuStrip(components);
			BuildHealthContextMenu_Browse = new ToolStripMenuItem();
			BuildHealthContextMenu_MinSeparator = new ToolStripSeparator();
			BuildHealthContextMenu_MaxSeparator = new ToolStripSeparator();
			BuildHealthContextMenu_Settings = new ToolStripMenuItem();
			EditorConfigWatcher = new System.IO.FileSystemWatcher();
			OptionsContextMenu.SuspendLayout();
			BuildListContextMenu.SuspendLayout();
			flowLayoutPanel1.SuspendLayout();
			tableLayoutPanel3.SuspendLayout();
			tableLayoutPanel2.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)Splitter).BeginInit();
			Splitter.Panel1.SuspendLayout();
			Splitter.Panel2.SuspendLayout();
			Splitter.SuspendLayout();
			StatusLayoutPanel.SuspendLayout();
			panel1.SuspendLayout();
			MoreToolsContextMenu.SuspendLayout();
			SyncContextMenu.SuspendLayout();
			RecentMenu.SuspendLayout();
			BuildListMultiContextMenu.SuspendLayout();
			FilterContextMenu.SuspendLayout();
			BuildHealthContextMenu.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)EditorConfigWatcher).BeginInit();
			SuspendLayout();
			// 
			// OptionsContextMenu
			// 
			OptionsContextMenu.Items.AddRange(new ToolStripItem[] { OptionsContextMenu_ApplicationSettings, toolStripSeparator2, OptionsContextMenu_ScheduledSync, OptionsContextMenu_SyncPrecompiledBinaries, OptionsContextMenu_ImportSnapshots, OptionsContextMenu_AutoResolveConflicts, OptionsContextMenu_AlwaysClobberFiles, OptionsContextMenu_AlwaysDeleteFiles, OptionsContextMenu_SyncFilter, OptionsContextMenu_Presets, toolStripSeparator3, OptionsContextMenu_EditorBuildConfiguration, OptionsContextMenu_CustomizeBuildSteps, OptionsContextMenu_EditorArguments, toolStripSeparator5, tabLabelsToolStripMenuItem, showChangesToolStripMenuItem, OptionsContextMenu_TimeZone, toolStripSeparator6, OptionsContextMenu_Diagnostics });
			OptionsContextMenu.Name = "ToolsMenuStrip";
			OptionsContextMenu.Size = new Size(262, 380);
			// 
			// OptionsContextMenu_ApplicationSettings
			// 
			OptionsContextMenu_ApplicationSettings.Name = "OptionsContextMenu_ApplicationSettings";
			OptionsContextMenu_ApplicationSettings.Size = new Size(261, 22);
			OptionsContextMenu_ApplicationSettings.Text = "Application Settings...";
			OptionsContextMenu_ApplicationSettings.Click += OptionsContextMenu_ApplicationSettings_Click;
			// 
			// toolStripSeparator2
			// 
			toolStripSeparator2.Name = "toolStripSeparator2";
			toolStripSeparator2.Size = new Size(258, 6);
			// 
			// OptionsContextMenu_ScheduledSync
			// 
			OptionsContextMenu_ScheduledSync.Name = "OptionsContextMenu_ScheduledSync";
			OptionsContextMenu_ScheduledSync.Size = new Size(261, 22);
			OptionsContextMenu_ScheduledSync.Text = "Scheduled Sync...";
			OptionsContextMenu_ScheduledSync.Click += OptionsContextMenu_ScheduleSync_Click;
			// 
			// OptionsContextMenu_SyncPrecompiledBinaries
			// 
			OptionsContextMenu_SyncPrecompiledBinaries.DropDownItems.AddRange(new ToolStripItem[] { disabledToolStripMenuItem, toolStripSeparator11, editorToolStripMenuItem, editorPhysXToolStripMenuItem });
			OptionsContextMenu_SyncPrecompiledBinaries.Name = "OptionsContextMenu_SyncPrecompiledBinaries";
			OptionsContextMenu_SyncPrecompiledBinaries.Size = new Size(261, 22);
			OptionsContextMenu_SyncPrecompiledBinaries.Text = "Sync Precompiled Binaries";
			OptionsContextMenu_SyncPrecompiledBinaries.Click += OptionsContextMenu_SyncPrecompiledBinaries_Click;
			// 
			// disabledToolStripMenuItem
			// 
			disabledToolStripMenuItem.Name = "disabledToolStripMenuItem";
			disabledToolStripMenuItem.Size = new Size(210, 22);
			disabledToolStripMenuItem.Text = "Disabled (compile locally)";
			// 
			// toolStripSeparator11
			// 
			toolStripSeparator11.Name = "toolStripSeparator11";
			toolStripSeparator11.Size = new Size(207, 6);
			// 
			// editorToolStripMenuItem
			// 
			editorToolStripMenuItem.Name = "editorToolStripMenuItem";
			editorToolStripMenuItem.Size = new Size(210, 22);
			editorToolStripMenuItem.Text = "Editor";
			// 
			// editorPhysXToolStripMenuItem
			// 
			editorPhysXToolStripMenuItem.Name = "editorPhysXToolStripMenuItem";
			editorPhysXToolStripMenuItem.Size = new Size(210, 22);
			editorPhysXToolStripMenuItem.Text = "Editor (PhysX)";
			// 
			// OptionsContextMenu_ImportSnapshots
			// 
			OptionsContextMenu_ImportSnapshots.Name = "OptionsContextMenu_ImportSnapshots";
			OptionsContextMenu_ImportSnapshots.Size = new Size(261, 22);
			OptionsContextMenu_ImportSnapshots.Text = "Import Snapshots";
			// 
			// OptionsContextMenu_AutoResolveConflicts
			// 
			OptionsContextMenu_AutoResolveConflicts.Name = "OptionsContextMenu_AutoResolveConflicts";
			OptionsContextMenu_AutoResolveConflicts.Size = new Size(261, 22);
			OptionsContextMenu_AutoResolveConflicts.Text = "Auto-Resolve Conflicts";
			OptionsContextMenu_AutoResolveConflicts.Click += OptionsContextMenu_AutoResolveConflicts_Click;
			// 
			// OptionsContextMenu_AlwaysClobberFiles
			// 
			OptionsContextMenu_AlwaysClobberFiles.Name = "OptionsContextMenu_AlwaysClobberFiles";
			OptionsContextMenu_AlwaysClobberFiles.Size = new Size(261, 22);
			OptionsContextMenu_AlwaysClobberFiles.Text = "Always Clobber Files";
			OptionsContextMenu_AlwaysClobberFiles.Click += OptionsContextMenu_AlwaysClobberFiles_Click;
			// 
			// OptionsContextMenu_AlwaysDeleteFiles
			// 
			OptionsContextMenu_AlwaysDeleteFiles.Name = "OptionsContextMenu_AlwaysDeleteFiles";
			OptionsContextMenu_AlwaysDeleteFiles.Size = new Size(261, 22);
			OptionsContextMenu_AlwaysDeleteFiles.Text = "Always Delete Files";
			OptionsContextMenu_AlwaysDeleteFiles.Click += OptionsContextMenu_AlwaysDeleteFiles_Click;
			// 
			// OptionsContextMenu_SyncFilter
			// 
			OptionsContextMenu_SyncFilter.Name = "OptionsContextMenu_SyncFilter";
			OptionsContextMenu_SyncFilter.Size = new Size(261, 22);
			OptionsContextMenu_SyncFilter.Text = "Sync Filter...";
			OptionsContextMenu_SyncFilter.Click += OptionsContextMenu_SyncFilter_Click;
			// 
			// OptionsContextMenu_Presets
			// 
			OptionsContextMenu_Presets.Name = "OptionsContextMenu_Presets";
			OptionsContextMenu_Presets.Size = new Size(261, 22);
			OptionsContextMenu_Presets.Text = "Presets...";
			OptionsContextMenu_Presets.Click += OptionsContextMenu_Presets_Click;
			// 
			// toolStripSeparator3
			// 
			toolStripSeparator3.Name = "toolStripSeparator3";
			toolStripSeparator3.Size = new Size(258, 6);
			// 
			// OptionsContextMenu_EditorBuildConfiguration
			// 
			OptionsContextMenu_EditorBuildConfiguration.DropDownItems.AddRange(new ToolStripItem[] { OptionsContextMenu_BuildConfig_Debug, OptionsContextMenu_BuildConfig_DebugGame, OptionsContextMenu_BuildConfig_Development });
			OptionsContextMenu_EditorBuildConfiguration.Name = "OptionsContextMenu_EditorBuildConfiguration";
			OptionsContextMenu_EditorBuildConfiguration.Size = new Size(261, 22);
			OptionsContextMenu_EditorBuildConfiguration.Text = "Editor Build Configuration";
			// 
			// OptionsContextMenu_BuildConfig_Debug
			// 
			OptionsContextMenu_BuildConfig_Debug.Name = "OptionsContextMenu_BuildConfig_Debug";
			OptionsContextMenu_BuildConfig_Debug.Size = new Size(145, 22);
			OptionsContextMenu_BuildConfig_Debug.Text = "Debug";
			OptionsContextMenu_BuildConfig_Debug.Click += OptionsContextMenu_BuildConfig_Debug_Click;
			// 
			// OptionsContextMenu_BuildConfig_DebugGame
			// 
			OptionsContextMenu_BuildConfig_DebugGame.Name = "OptionsContextMenu_BuildConfig_DebugGame";
			OptionsContextMenu_BuildConfig_DebugGame.Size = new Size(145, 22);
			OptionsContextMenu_BuildConfig_DebugGame.Text = "DebugGame";
			OptionsContextMenu_BuildConfig_DebugGame.Click += OptionsContextMenu_BuildConfig_DebugGame_Click;
			// 
			// OptionsContextMenu_BuildConfig_Development
			// 
			OptionsContextMenu_BuildConfig_Development.Checked = true;
			OptionsContextMenu_BuildConfig_Development.CheckState = CheckState.Checked;
			OptionsContextMenu_BuildConfig_Development.Name = "OptionsContextMenu_BuildConfig_Development";
			OptionsContextMenu_BuildConfig_Development.Size = new Size(145, 22);
			OptionsContextMenu_BuildConfig_Development.Text = "Development";
			OptionsContextMenu_BuildConfig_Development.Click += OptionsContextMenu_BuildConfig_Development_Click;
			// 
			// OptionsContextMenu_CustomizeBuildSteps
			// 
			OptionsContextMenu_CustomizeBuildSteps.Name = "OptionsContextMenu_CustomizeBuildSteps";
			OptionsContextMenu_CustomizeBuildSteps.Size = new Size(261, 22);
			OptionsContextMenu_CustomizeBuildSteps.Text = "Customize Commands...";
			OptionsContextMenu_CustomizeBuildSteps.Click += OptionsContextMenu_EditBuildSteps_Click;
			// 
			// OptionsContextMenu_EditorArguments
			// 
			OptionsContextMenu_EditorArguments.Name = "OptionsContextMenu_EditorArguments";
			OptionsContextMenu_EditorArguments.Size = new Size(261, 22);
			OptionsContextMenu_EditorArguments.Text = "Editor Command Line Arguments...";
			OptionsContextMenu_EditorArguments.Click += OptionsContextMenu_EditorArguments_Click;
			// 
			// toolStripSeparator5
			// 
			toolStripSeparator5.Name = "toolStripSeparator5";
			toolStripSeparator5.Size = new Size(258, 6);
			// 
			// tabLabelsToolStripMenuItem
			// 
			tabLabelsToolStripMenuItem.DropDownItems.AddRange(new ToolStripItem[] { OptionsContextMenu_TabNames_Stream, OptionsContextMenu_TabNames_WorkspaceName, OptionsContextMenu_TabNames_WorkspaceRoot, OptionsContextMenu_TabNames_ProjectFile });
			tabLabelsToolStripMenuItem.Name = "tabLabelsToolStripMenuItem";
			tabLabelsToolStripMenuItem.Size = new Size(261, 22);
			tabLabelsToolStripMenuItem.Text = "Tab Names";
			// 
			// OptionsContextMenu_TabNames_Stream
			// 
			OptionsContextMenu_TabNames_Stream.Name = "OptionsContextMenu_TabNames_Stream";
			OptionsContextMenu_TabNames_Stream.Size = new Size(167, 22);
			OptionsContextMenu_TabNames_Stream.Text = "Stream";
			OptionsContextMenu_TabNames_Stream.Click += OptionsContextMenu_TabNames_Stream_Click;
			// 
			// OptionsContextMenu_TabNames_WorkspaceName
			// 
			OptionsContextMenu_TabNames_WorkspaceName.Name = "OptionsContextMenu_TabNames_WorkspaceName";
			OptionsContextMenu_TabNames_WorkspaceName.Size = new Size(167, 22);
			OptionsContextMenu_TabNames_WorkspaceName.Text = "Workspace Name";
			OptionsContextMenu_TabNames_WorkspaceName.Click += OptionsContextMenu_TabNames_WorkspaceName_Click;
			// 
			// OptionsContextMenu_TabNames_WorkspaceRoot
			// 
			OptionsContextMenu_TabNames_WorkspaceRoot.Name = "OptionsContextMenu_TabNames_WorkspaceRoot";
			OptionsContextMenu_TabNames_WorkspaceRoot.Size = new Size(167, 22);
			OptionsContextMenu_TabNames_WorkspaceRoot.Text = "Workspace Root";
			OptionsContextMenu_TabNames_WorkspaceRoot.Click += OptionsContextMenu_TabNames_WorkspaceRoot_Click;
			// 
			// OptionsContextMenu_TabNames_ProjectFile
			// 
			OptionsContextMenu_TabNames_ProjectFile.Name = "OptionsContextMenu_TabNames_ProjectFile";
			OptionsContextMenu_TabNames_ProjectFile.Size = new Size(167, 22);
			OptionsContextMenu_TabNames_ProjectFile.Text = "Project File";
			OptionsContextMenu_TabNames_ProjectFile.Click += OptionsContextMenu_TabNames_ProjectFile_Click;
			// 
			// showChangesToolStripMenuItem
			// 
			showChangesToolStripMenuItem.DropDownItems.AddRange(new ToolStripItem[] { OptionsContextMenu_ShowChanges_ShowUnreviewed, OptionsContextMenu_ShowChanges_ShowAutomated });
			showChangesToolStripMenuItem.Name = "showChangesToolStripMenuItem";
			showChangesToolStripMenuItem.Size = new Size(261, 22);
			showChangesToolStripMenuItem.Text = "Show Changes";
			// 
			// OptionsContextMenu_ShowChanges_ShowUnreviewed
			// 
			OptionsContextMenu_ShowChanges_ShowUnreviewed.Name = "OptionsContextMenu_ShowChanges_ShowUnreviewed";
			OptionsContextMenu_ShowChanges_ShowUnreviewed.Size = new Size(281, 22);
			OptionsContextMenu_ShowChanges_ShowUnreviewed.Text = "Show changes without reviews";
			OptionsContextMenu_ShowChanges_ShowUnreviewed.Click += OptionsContextMenu_ShowChanges_ShowUnreviewed_Click;
			// 
			// OptionsContextMenu_ShowChanges_ShowAutomated
			// 
			OptionsContextMenu_ShowChanges_ShowAutomated.Name = "OptionsContextMenu_ShowChanges_ShowAutomated";
			OptionsContextMenu_ShowChanges_ShowAutomated.Size = new Size(281, 22);
			OptionsContextMenu_ShowChanges_ShowAutomated.Text = "Show changes by automated processes";
			OptionsContextMenu_ShowChanges_ShowAutomated.Click += OptionsContextMenu_ShowChanges_ShowAutomated_Click;
			// 
			// OptionsContextMenu_TimeZone
			// 
			OptionsContextMenu_TimeZone.DropDownItems.AddRange(new ToolStripItem[] { OptionsContextMenu_TimeZone_Local, OptionsContextMenu_TimeZone_PerforceServer });
			OptionsContextMenu_TimeZone.Name = "OptionsContextMenu_TimeZone";
			OptionsContextMenu_TimeZone.Size = new Size(261, 22);
			OptionsContextMenu_TimeZone.Text = "Time Zone";
			// 
			// OptionsContextMenu_TimeZone_Local
			// 
			OptionsContextMenu_TimeZone_Local.Name = "OptionsContextMenu_TimeZone_Local";
			OptionsContextMenu_TimeZone_Local.Size = new Size(153, 22);
			OptionsContextMenu_TimeZone_Local.Text = "Local";
			OptionsContextMenu_TimeZone_Local.Click += BuildListContextMenu_ShowLocalTimes_Click;
			// 
			// OptionsContextMenu_TimeZone_PerforceServer
			// 
			OptionsContextMenu_TimeZone_PerforceServer.Name = "OptionsContextMenu_TimeZone_PerforceServer";
			OptionsContextMenu_TimeZone_PerforceServer.Size = new Size(153, 22);
			OptionsContextMenu_TimeZone_PerforceServer.Text = "Perforce Server";
			OptionsContextMenu_TimeZone_PerforceServer.Click += BuildListContextMenu_ShowServerTimes_Click;
			// 
			// toolStripSeparator6
			// 
			toolStripSeparator6.Name = "toolStripSeparator6";
			toolStripSeparator6.Size = new Size(258, 6);
			// 
			// OptionsContextMenu_Diagnostics
			// 
			OptionsContextMenu_Diagnostics.Name = "OptionsContextMenu_Diagnostics";
			OptionsContextMenu_Diagnostics.Size = new Size(261, 22);
			OptionsContextMenu_Diagnostics.Text = "Diagnostics...";
			OptionsContextMenu_Diagnostics.Click += OptionsContextMenu_Diagnostics_Click;
			// 
			// RunAfterSyncCheckBox
			// 
			RunAfterSyncCheckBox.AutoSize = true;
			RunAfterSyncCheckBox.Location = new Point(277, 0);
			RunAfterSyncCheckBox.Margin = new Padding(3, 0, 3, 0);
			RunAfterSyncCheckBox.Name = "RunAfterSyncCheckBox";
			RunAfterSyncCheckBox.Size = new Size(47, 19);
			RunAfterSyncCheckBox.TabIndex = 6;
			RunAfterSyncCheckBox.Text = "Run";
			RunAfterSyncCheckBox.UseVisualStyleBackColor = true;
			RunAfterSyncCheckBox.CheckedChanged += RunAfterSyncCheckBox_CheckedChanged;
			// 
			// BuildAfterSyncCheckBox
			// 
			BuildAfterSyncCheckBox.AutoSize = true;
			BuildAfterSyncCheckBox.Location = new Point(218, 0);
			BuildAfterSyncCheckBox.Margin = new Padding(3, 0, 3, 0);
			BuildAfterSyncCheckBox.Name = "BuildAfterSyncCheckBox";
			BuildAfterSyncCheckBox.Size = new Size(53, 19);
			BuildAfterSyncCheckBox.TabIndex = 5;
			BuildAfterSyncCheckBox.Text = "Build";
			BuildAfterSyncCheckBox.UseVisualStyleBackColor = true;
			BuildAfterSyncCheckBox.CheckedChanged += BuildAfterSyncCheckBox_CheckedChanged;
			// 
			// AfterSyncingLabel
			// 
			AfterSyncingLabel.AutoSize = true;
			AfterSyncingLabel.Location = new Point(3, 0);
			AfterSyncingLabel.Name = "AfterSyncingLabel";
			AfterSyncingLabel.Padding = new Padding(0, 1, 5, 0);
			AfterSyncingLabel.Size = new Size(85, 16);
			AfterSyncingLabel.TabIndex = 4;
			AfterSyncingLabel.Text = "After syncing:";
			// 
			// BuildListContextMenu
			// 
			BuildListContextMenu.Items.AddRange(new ToolStripItem[] { BuildListContextMenu_LaunchEditor, BuildListContextMenu_Sync, BuildListContextMenu_SyncContentOnly, BuildListContextMenu_SyncOnlyThisChange, BuildListContextMenu_Build, BuildListContextMenu_Rebuild, BuildListContextMenu_GenerateProjectFiles, BuildListContextMenu_Cancel, BuildListContextMenu_OpenVisualStudio, BuildListContextMenu_Bisect_Separator, BuildListContextMenu_Bisect_Pass, BuildListContextMenu_Bisect_Fail, BuildListContextMenu_Bisect_Include, BuildListContextMenu_Bisect_Exclude, toolStripSeparator4, BuildListContextMenu_MarkGood, BuildListContextMenu_MarkBad, BuildListContextMenu_WithdrawReview, BuildListContextMenu_LeaveComment, BuildListContextMenu_EditComment, BuildListContextMenu_StartInvestigating, BuildListContextMenu_FinishInvestigating, toolStripSeparator1, BuildListContextMenu_AddStar, BuildListContextMenu_RemoveStar, BuildListContextMenu_TimeZoneSeparator, BuildListContextMenu_ShowServerTimes, BuildListContextMenu_ShowLocalTimes, BuildListContextMenu_CustomTool_Start, BuildListContextMenu_CustomTool_End, BuildListContextMenu_ViewInSwarm, BuildListContextMenu_CopyChangelistNumber, BuildListContextMenu_OpenPreflight, BuildListContextMenu_MoreInfo });
			BuildListContextMenu.Name = "BuildListContextMenu";
			BuildListContextMenu.Size = new Size(199, 656);
			// 
			// BuildListContextMenu_LaunchEditor
			// 
			BuildListContextMenu_LaunchEditor.Name = "BuildListContextMenu_LaunchEditor";
			BuildListContextMenu_LaunchEditor.Size = new Size(198, 22);
			BuildListContextMenu_LaunchEditor.Text = "Launch editor";
			BuildListContextMenu_LaunchEditor.Click += BuildListContextMenu_LaunchEditor_Click;
			// 
			// BuildListContextMenu_Sync
			// 
			BuildListContextMenu_Sync.Name = "BuildListContextMenu_Sync";
			BuildListContextMenu_Sync.Size = new Size(198, 22);
			BuildListContextMenu_Sync.Text = "Sync";
			BuildListContextMenu_Sync.Click += BuildListContextMenu_Sync_Click;
			// 
			// BuildListContextMenu_SyncContentOnly
			// 
			BuildListContextMenu_SyncContentOnly.Name = "BuildListContextMenu_SyncContentOnly";
			BuildListContextMenu_SyncContentOnly.Size = new Size(198, 22);
			BuildListContextMenu_SyncContentOnly.Text = "Sync (Just Content)";
			BuildListContextMenu_SyncContentOnly.Click += BuildListContextMenu_SyncContentOnly_Click;
			// 
			// BuildListContextMenu_SyncOnlyThisChange
			// 
			BuildListContextMenu_SyncOnlyThisChange.Name = "BuildListContextMenu_SyncOnlyThisChange";
			BuildListContextMenu_SyncOnlyThisChange.Size = new Size(198, 22);
			BuildListContextMenu_SyncOnlyThisChange.Text = "Sync (Just This Change)";
			BuildListContextMenu_SyncOnlyThisChange.Click += BuildListContextMenu_SyncOnlyThisChange_Click;
			// 
			// BuildListContextMenu_Build
			// 
			BuildListContextMenu_Build.Name = "BuildListContextMenu_Build";
			BuildListContextMenu_Build.Size = new Size(198, 22);
			BuildListContextMenu_Build.Text = "Build";
			BuildListContextMenu_Build.Click += BuildListContextMenu_Build_Click;
			// 
			// BuildListContextMenu_Rebuild
			// 
			BuildListContextMenu_Rebuild.Name = "BuildListContextMenu_Rebuild";
			BuildListContextMenu_Rebuild.Size = new Size(198, 22);
			BuildListContextMenu_Rebuild.Text = "Rebuild";
			BuildListContextMenu_Rebuild.Click += BuildListContextMenu_Rebuild_Click;
			// 
			// BuildListContextMenu_GenerateProjectFiles
			// 
			BuildListContextMenu_GenerateProjectFiles.Name = "BuildListContextMenu_GenerateProjectFiles";
			BuildListContextMenu_GenerateProjectFiles.Size = new Size(198, 22);
			BuildListContextMenu_GenerateProjectFiles.Text = "Generate project files";
			BuildListContextMenu_GenerateProjectFiles.Click += BuildListContextMenu_GenerateProjectFiles_Click;
			// 
			// BuildListContextMenu_Cancel
			// 
			BuildListContextMenu_Cancel.Name = "BuildListContextMenu_Cancel";
			BuildListContextMenu_Cancel.Size = new Size(198, 22);
			BuildListContextMenu_Cancel.Text = "Cancel";
			BuildListContextMenu_Cancel.Click += BuildListContextMenu_CancelSync_Click;
			// 
			// BuildListContextMenu_OpenVisualStudio
			// 
			BuildListContextMenu_OpenVisualStudio.Name = "BuildListContextMenu_OpenVisualStudio";
			BuildListContextMenu_OpenVisualStudio.Size = new Size(198, 22);
			BuildListContextMenu_OpenVisualStudio.Text = "Open in Visual Studio...";
			BuildListContextMenu_OpenVisualStudio.Click += BuildListContextMenu_OpenVisualStudio_Click;
			// 
			// BuildListContextMenu_Bisect_Separator
			// 
			BuildListContextMenu_Bisect_Separator.Name = "BuildListContextMenu_Bisect_Separator";
			BuildListContextMenu_Bisect_Separator.Size = new Size(195, 6);
			// 
			// BuildListContextMenu_Bisect_Pass
			// 
			BuildListContextMenu_Bisect_Pass.Name = "BuildListContextMenu_Bisect_Pass";
			BuildListContextMenu_Bisect_Pass.Size = new Size(198, 22);
			BuildListContextMenu_Bisect_Pass.Text = "Bisect: Pass";
			BuildListContextMenu_Bisect_Pass.Click += BuildListContextMenu_Bisect_Pass_Click;
			// 
			// BuildListContextMenu_Bisect_Fail
			// 
			BuildListContextMenu_Bisect_Fail.Name = "BuildListContextMenu_Bisect_Fail";
			BuildListContextMenu_Bisect_Fail.Size = new Size(198, 22);
			BuildListContextMenu_Bisect_Fail.Text = "Bisect: Fail";
			BuildListContextMenu_Bisect_Fail.Click += BuildListContextMenu_Bisect_Fail_Click;
			// 
			// BuildListContextMenu_Bisect_Include
			// 
			BuildListContextMenu_Bisect_Include.Name = "BuildListContextMenu_Bisect_Include";
			BuildListContextMenu_Bisect_Include.Size = new Size(198, 22);
			BuildListContextMenu_Bisect_Include.Text = "Bisect: Include";
			BuildListContextMenu_Bisect_Include.Click += BuildListContextMenu_Bisect_Include_Click;
			// 
			// BuildListContextMenu_Bisect_Exclude
			// 
			BuildListContextMenu_Bisect_Exclude.Name = "BuildListContextMenu_Bisect_Exclude";
			BuildListContextMenu_Bisect_Exclude.Size = new Size(198, 22);
			BuildListContextMenu_Bisect_Exclude.Text = "Bisect: Exclude";
			BuildListContextMenu_Bisect_Exclude.Click += BuildListContextMenu_Bisect_Exclude_Click;
			// 
			// toolStripSeparator4
			// 
			toolStripSeparator4.Name = "toolStripSeparator4";
			toolStripSeparator4.Size = new Size(195, 6);
			// 
			// BuildListContextMenu_MarkGood
			// 
			BuildListContextMenu_MarkGood.Name = "BuildListContextMenu_MarkGood";
			BuildListContextMenu_MarkGood.Size = new Size(198, 22);
			BuildListContextMenu_MarkGood.Text = "Mark as good";
			BuildListContextMenu_MarkGood.Click += BuildListContextMenu_MarkGood_Click;
			// 
			// BuildListContextMenu_MarkBad
			// 
			BuildListContextMenu_MarkBad.Name = "BuildListContextMenu_MarkBad";
			BuildListContextMenu_MarkBad.Size = new Size(198, 22);
			BuildListContextMenu_MarkBad.Text = "Mark as bad";
			BuildListContextMenu_MarkBad.Click += BuildListContextMenu_MarkBad_Click;
			// 
			// BuildListContextMenu_WithdrawReview
			// 
			BuildListContextMenu_WithdrawReview.Name = "BuildListContextMenu_WithdrawReview";
			BuildListContextMenu_WithdrawReview.Size = new Size(198, 22);
			BuildListContextMenu_WithdrawReview.Text = "Withdraw review";
			BuildListContextMenu_WithdrawReview.Click += BuildListContextMenu_WithdrawReview_Click;
			// 
			// BuildListContextMenu_LeaveComment
			// 
			BuildListContextMenu_LeaveComment.Name = "BuildListContextMenu_LeaveComment";
			BuildListContextMenu_LeaveComment.Size = new Size(198, 22);
			BuildListContextMenu_LeaveComment.Text = "Leave comment...";
			BuildListContextMenu_LeaveComment.Click += BuildListContextMenu_LeaveOrEditComment_Click;
			// 
			// BuildListContextMenu_EditComment
			// 
			BuildListContextMenu_EditComment.Name = "BuildListContextMenu_EditComment";
			BuildListContextMenu_EditComment.Size = new Size(198, 22);
			BuildListContextMenu_EditComment.Text = "Edit comment...";
			BuildListContextMenu_EditComment.Click += BuildListContextMenu_LeaveOrEditComment_Click;
			// 
			// BuildListContextMenu_StartInvestigating
			// 
			BuildListContextMenu_StartInvestigating.Name = "BuildListContextMenu_StartInvestigating";
			BuildListContextMenu_StartInvestigating.Size = new Size(198, 22);
			BuildListContextMenu_StartInvestigating.Text = "Start investigating";
			BuildListContextMenu_StartInvestigating.Click += BuildListContextMenu_StartInvestigating_Click;
			// 
			// BuildListContextMenu_FinishInvestigating
			// 
			BuildListContextMenu_FinishInvestigating.Name = "BuildListContextMenu_FinishInvestigating";
			BuildListContextMenu_FinishInvestigating.Size = new Size(198, 22);
			BuildListContextMenu_FinishInvestigating.Text = "Finish investigating";
			BuildListContextMenu_FinishInvestigating.Click += BuildListContextMenu_FinishInvestigating_Click;
			// 
			// toolStripSeparator1
			// 
			toolStripSeparator1.Name = "toolStripSeparator1";
			toolStripSeparator1.Size = new Size(195, 6);
			// 
			// BuildListContextMenu_AddStar
			// 
			BuildListContextMenu_AddStar.Name = "BuildListContextMenu_AddStar";
			BuildListContextMenu_AddStar.Size = new Size(198, 22);
			BuildListContextMenu_AddStar.Text = "Add Star";
			BuildListContextMenu_AddStar.Click += BuildListContextMenu_AddStar_Click;
			// 
			// BuildListContextMenu_RemoveStar
			// 
			BuildListContextMenu_RemoveStar.Name = "BuildListContextMenu_RemoveStar";
			BuildListContextMenu_RemoveStar.Size = new Size(198, 22);
			BuildListContextMenu_RemoveStar.Text = "Remove Star";
			BuildListContextMenu_RemoveStar.Click += BuildListContextMenu_RemoveStar_Click;
			// 
			// BuildListContextMenu_TimeZoneSeparator
			// 
			BuildListContextMenu_TimeZoneSeparator.Name = "BuildListContextMenu_TimeZoneSeparator";
			BuildListContextMenu_TimeZoneSeparator.Size = new Size(195, 6);
			// 
			// BuildListContextMenu_ShowServerTimes
			// 
			BuildListContextMenu_ShowServerTimes.Name = "BuildListContextMenu_ShowServerTimes";
			BuildListContextMenu_ShowServerTimes.Size = new Size(198, 22);
			BuildListContextMenu_ShowServerTimes.Text = "Show server times";
			BuildListContextMenu_ShowServerTimes.Click += BuildListContextMenu_ShowServerTimes_Click;
			// 
			// BuildListContextMenu_ShowLocalTimes
			// 
			BuildListContextMenu_ShowLocalTimes.Name = "BuildListContextMenu_ShowLocalTimes";
			BuildListContextMenu_ShowLocalTimes.Size = new Size(198, 22);
			BuildListContextMenu_ShowLocalTimes.Text = "Show local times";
			BuildListContextMenu_ShowLocalTimes.Click += BuildListContextMenu_ShowLocalTimes_Click;
			// 
			// BuildListContextMenu_CustomTool_Start
			// 
			BuildListContextMenu_CustomTool_Start.Name = "BuildListContextMenu_CustomTool_Start";
			BuildListContextMenu_CustomTool_Start.Size = new Size(195, 6);
			// 
			// BuildListContextMenu_CustomTool_End
			// 
			BuildListContextMenu_CustomTool_End.Name = "BuildListContextMenu_CustomTool_End";
			BuildListContextMenu_CustomTool_End.Size = new Size(195, 6);
			// 
			// BuildListContextMenu_ViewInSwarm
			// 
			BuildListContextMenu_ViewInSwarm.Name = "BuildListContextMenu_ViewInSwarm";
			BuildListContextMenu_ViewInSwarm.Size = new Size(198, 22);
			BuildListContextMenu_ViewInSwarm.Text = "View in Swarm...";
			BuildListContextMenu_ViewInSwarm.Click += BuildListContextMenu_ViewInSwarm_Click;
			// 
			// BuildListContextMenu_CopyChangelistNumber
			// 
			BuildListContextMenu_CopyChangelistNumber.Name = "BuildListContextMenu_CopyChangelistNumber";
			BuildListContextMenu_CopyChangelistNumber.Size = new Size(198, 22);
			BuildListContextMenu_CopyChangelistNumber.Text = "Copy Changelist";
			BuildListContextMenu_CopyChangelistNumber.Click += BuildListContextMenu_CopyChangelistNumber_Click;
			// 
			// BuildListContextMenu_OpenPreflight
			// 
			BuildListContextMenu_OpenPreflight.Name = "BuildListContextMenu_OpenPreflight";
			BuildListContextMenu_OpenPreflight.Size = new Size(198, 22);
			BuildListContextMenu_OpenPreflight.Text = "Open Preflight";
			BuildListContextMenu_OpenPreflight.Click += BuildListContextMenu_OpenPreflight_Click;
			// 
			// BuildListContextMenu_MoreInfo
			// 
			BuildListContextMenu_MoreInfo.Name = "BuildListContextMenu_MoreInfo";
			BuildListContextMenu_MoreInfo.Size = new Size(198, 22);
			BuildListContextMenu_MoreInfo.Text = "More Info...";
			BuildListContextMenu_MoreInfo.Click += BuildListContextMenu_MoreInfo_Click;
			// 
			// toolStripSeparator7
			// 
			toolStripSeparator7.Name = "toolStripSeparator7";
			toolStripSeparator7.Size = new Size(193, 6);
			// 
			// BuildListToolTip
			// 
			BuildListToolTip.OwnerDraw = true;
			BuildListToolTip.Draw += BuildListToolTip_Draw;
			// 
			// flowLayoutPanel1
			// 
			flowLayoutPanel1.Anchor = AnchorStyles.None;
			flowLayoutPanel1.AutoSize = true;
			flowLayoutPanel1.AutoSizeMode = AutoSizeMode.GrowAndShrink;
			flowLayoutPanel1.Controls.Add(AfterSyncingLabel);
			flowLayoutPanel1.Controls.Add(GenerateAfterSyncCheckBox);
			flowLayoutPanel1.Controls.Add(BuildAfterSyncCheckBox);
			flowLayoutPanel1.Controls.Add(RunAfterSyncCheckBox);
			flowLayoutPanel1.Controls.Add(OpenSolutionAfterSyncCheckBox);
			flowLayoutPanel1.Location = new Point(464, 17);
			flowLayoutPanel1.Margin = new Padding(0, 4, 0, 3);
			flowLayoutPanel1.Name = "flowLayoutPanel1";
			flowLayoutPanel1.Size = new Size(435, 19);
			flowLayoutPanel1.TabIndex = 8;
			flowLayoutPanel1.WrapContents = false;
			// 
			// GenerateAfterSyncCheckBox
			// 
			GenerateAfterSyncCheckBox.AutoSize = true;
			GenerateAfterSyncCheckBox.Location = new Point(94, 0);
			GenerateAfterSyncCheckBox.Margin = new Padding(3, 0, 3, 0);
			GenerateAfterSyncCheckBox.Name = "GenerateAfterSyncCheckBox";
			GenerateAfterSyncCheckBox.Size = new Size(118, 19);
			GenerateAfterSyncCheckBox.TabIndex = 8;
			GenerateAfterSyncCheckBox.Text = "Generate Projects";
			GenerateAfterSyncCheckBox.UseVisualStyleBackColor = true;
			GenerateAfterSyncCheckBox.CheckedChanged += GenerateAfterSyncCheckBox_CheckedChanged;
			// 
			// OpenSolutionAfterSyncCheckBox
			// 
			OpenSolutionAfterSyncCheckBox.AutoSize = true;
			OpenSolutionAfterSyncCheckBox.Location = new Point(330, 0);
			OpenSolutionAfterSyncCheckBox.Margin = new Padding(3, 0, 3, 0);
			OpenSolutionAfterSyncCheckBox.Name = "OpenSolutionAfterSyncCheckBox";
			OpenSolutionAfterSyncCheckBox.Size = new Size(102, 19);
			OpenSolutionAfterSyncCheckBox.TabIndex = 7;
			OpenSolutionAfterSyncCheckBox.Text = "Open Solution";
			OpenSolutionAfterSyncCheckBox.UseVisualStyleBackColor = true;
			OpenSolutionAfterSyncCheckBox.CheckedChanged += OpenSolutionAfterSyncCheckBox_CheckedChanged;
			// 
			// tableLayoutPanel3
			// 
			tableLayoutPanel3.AutoSize = true;
			tableLayoutPanel3.AutoSizeMode = AutoSizeMode.GrowAndShrink;
			tableLayoutPanel3.BackColor = SystemColors.Control;
			tableLayoutPanel3.ColumnCount = 3;
			tableLayoutPanel3.ColumnStyles.Add(new ColumnStyle());
			tableLayoutPanel3.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
			tableLayoutPanel3.ColumnStyles.Add(new ColumnStyle());
			tableLayoutPanel3.Controls.Add(flowLayoutPanel1, 1, 0);
			tableLayoutPanel3.Controls.Add(OptionsButton, 2, 0);
			tableLayoutPanel3.Controls.Add(FilterButton, 0, 0);
			tableLayoutPanel3.Dock = DockStyle.Fill;
			tableLayoutPanel3.Location = new Point(0, 713);
			tableLayoutPanel3.Margin = new Padding(0);
			tableLayoutPanel3.Name = "tableLayoutPanel3";
			tableLayoutPanel3.Padding = new Padding(0, 13, 0, 0);
			tableLayoutPanel3.RowCount = 1;
			tableLayoutPanel3.RowStyles.Add(new RowStyle());
			tableLayoutPanel3.Size = new Size(1363, 39);
			tableLayoutPanel3.TabIndex = 11;
			// 
			// OptionsButton
			// 
			OptionsButton.Anchor = AnchorStyles.Right;
			OptionsButton.Image = Properties.Resources.DropList;
			OptionsButton.ImageAlign = ContentAlignment.MiddleRight;
			OptionsButton.Location = new Point(1243, 13);
			OptionsButton.Margin = new Padding(0);
			OptionsButton.Name = "OptionsButton";
			OptionsButton.Size = new Size(120, 26);
			OptionsButton.TabIndex = 7;
			OptionsButton.Tag = "dropdown";
			OptionsButton.Text = "Options  ";
			OptionsButton.UseVisualStyleBackColor = true;
			OptionsButton.Click += OptionsButton_Click;
			// 
			// FilterButton
			// 
			FilterButton.Anchor = AnchorStyles.Right;
			FilterButton.Image = Properties.Resources.DropList;
			FilterButton.ImageAlign = ContentAlignment.MiddleRight;
			FilterButton.Location = new Point(0, 13);
			FilterButton.Margin = new Padding(0);
			FilterButton.Name = "FilterButton";
			FilterButton.Size = new Size(120, 26);
			FilterButton.TabIndex = 9;
			FilterButton.Tag = "dropdown";
			FilterButton.Text = "Filter  ";
			FilterButton.UseVisualStyleBackColor = true;
			FilterButton.Click += FilterButton_Click;
			// 
			// tableLayoutPanel2
			// 
			tableLayoutPanel2.AutoSizeMode = AutoSizeMode.GrowAndShrink;
			tableLayoutPanel2.ColumnCount = 1;
			tableLayoutPanel2.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
			tableLayoutPanel2.Controls.Add(Splitter, 0, 0);
			tableLayoutPanel2.Controls.Add(tableLayoutPanel3, 0, 1);
			tableLayoutPanel2.Dock = DockStyle.Fill;
			tableLayoutPanel2.GrowStyle = TableLayoutPanelGrowStyle.FixedSize;
			tableLayoutPanel2.Location = new Point(0, 0);
			tableLayoutPanel2.Margin = new Padding(0);
			tableLayoutPanel2.Name = "tableLayoutPanel2";
			tableLayoutPanel2.RowCount = 2;
			tableLayoutPanel2.RowStyles.Add(new RowStyle(SizeType.Percent, 100F));
			tableLayoutPanel2.RowStyles.Add(new RowStyle());
			tableLayoutPanel2.Size = new Size(1363, 752);
			tableLayoutPanel2.TabIndex = 12;
			// 
			// Splitter
			// 
			Splitter.Caption = "Log";
			Splitter.Dock = DockStyle.Fill;
			Splitter.Location = new Point(0, 0);
			Splitter.Margin = new Padding(0);
			Splitter.Name = "Splitter";
			Splitter.Orientation = Orientation.Horizontal;
			// 
			// Splitter.Panel1
			// 
			Splitter.Panel1.Controls.Add(StatusLayoutPanel);
			// 
			// Splitter.Panel2
			// 
			Splitter.Panel2.Controls.Add(panel1);
			Splitter.Panel2MinSize = 50;
			Splitter.Size = new Size(1363, 713);
			Splitter.SplitterDistance = 444;
			Splitter.SplitterWidth = 28;
			Splitter.TabIndex = 0;
			// 
			// StatusLayoutPanel
			// 
			StatusLayoutPanel.ColumnCount = 1;
			StatusLayoutPanel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
			StatusLayoutPanel.Controls.Add(StatusPanel, 0, 0);
			StatusLayoutPanel.Controls.Add(BuildList, 0, 1);
			StatusLayoutPanel.Dock = DockStyle.Fill;
			StatusLayoutPanel.Location = new Point(0, 0);
			StatusLayoutPanel.Name = "StatusLayoutPanel";
			StatusLayoutPanel.RowCount = 2;
			StatusLayoutPanel.RowStyles.Add(new RowStyle(SizeType.Absolute, 148F));
			StatusLayoutPanel.RowStyles.Add(new RowStyle(SizeType.Percent, 100F));
			StatusLayoutPanel.Size = new Size(1363, 444);
			StatusLayoutPanel.TabIndex = 1;
			// 
			// StatusPanel
			// 
			StatusPanel.BackColor = Color.FromArgb(250, 250, 250);
			StatusPanel.BorderStyle = BorderStyle.FixedSingle;
			StatusPanel.Dock = DockStyle.Fill;
			StatusPanel.Location = new Point(0, 0);
			StatusPanel.Margin = new Padding(0, 0, 0, 3);
			StatusPanel.Name = "StatusPanel";
			StatusPanel.Size = new Size(1363, 145);
			StatusPanel.TabIndex = 1;
			// 
			// BuildList
			// 
			BuildList.Columns.AddRange(new ColumnHeader[] { IconColumn, TypeColumn, ChangeColumn, TimeColumn, AuthorColumn, DescriptionColumn, CISColumn, ReviewerColumn, PreflightColumn, StatusColumn });
			BuildList.Dock = DockStyle.Fill;
			BuildList.FullRowSelect = true;
			BuildList.HeaderStyle = ColumnHeaderStyle.Nonclickable;
			BuildList.Location = new Point(0, 148);
			BuildList.Margin = new Padding(0);
			BuildList.Name = "BuildList";
			BuildList.OwnerDraw = true;
			BuildList.Size = new Size(1363, 296);
			BuildList.TabIndex = 0;
			BuildList.UseCompatibleStateImageBehavior = false;
			BuildList.View = View.Details;
			BuildList.ColumnWidthChanged += BuildList_ColumnWidthChanged;
			BuildList.ColumnWidthChanging += BuildList_ColumnWidthChanging;
			BuildList.DrawItem += BuildList_DrawItem;
			BuildList.DrawSubItem += BuildList_DrawSubItem;
			BuildList.ItemMouseHover += BuildList_ItemMouseHover;
			BuildList.SelectedIndexChanged += BuildList_SelectedIndexChanged;
			BuildList.FontChanged += BuildList_FontChanged;
			BuildList.KeyDown += BuildList_KeyDown;
			BuildList.MouseClick += BuildList_MouseClick;
			BuildList.MouseDoubleClick += BuildList_MouseDoubleClick;
			BuildList.MouseLeave += BuildList_MouseLeave;
			BuildList.MouseMove += BuildList_MouseMove;
			BuildList.Resize += BuildList_Resize;
			// 
			// IconColumn
			// 
			IconColumn.Text = "";
			IconColumn.Width = 45;
			// 
			// TypeColumn
			// 
			TypeColumn.Text = "Type";
			TypeColumn.Width = 100;
			// 
			// ChangeColumn
			// 
			ChangeColumn.Text = "Change";
			ChangeColumn.Width = 74;
			// 
			// TimeColumn
			// 
			TimeColumn.Text = "Time";
			// 
			// AuthorColumn
			// 
			AuthorColumn.Text = "Author";
			AuthorColumn.Width = 120;
			// 
			// DescriptionColumn
			// 
			DescriptionColumn.Text = "Description";
			DescriptionColumn.Width = 245;
			// 
			// CISColumn
			// 
			CISColumn.Text = "CIS";
			CISColumn.Width = 184;
			// 
			// ReviewerColumn
			// 
			ReviewerColumn.Text = "Reviewer";
			ReviewerColumn.Width = 120;
			// 
			// PreflightColumn
			// 
			PreflightColumn.Text = "Preflight";
			PreflightColumn.Width = 120;
			// 
			// StatusColumn
			// 
			StatusColumn.Text = "Status";
			StatusColumn.Width = 375;
			// 
			// panel1
			// 
			panel1.BorderStyle = BorderStyle.FixedSingle;
			panel1.Controls.Add(SyncLog);
			panel1.Dock = DockStyle.Fill;
			panel1.Location = new Point(0, 0);
			panel1.Name = "panel1";
			panel1.Size = new Size(1363, 241);
			panel1.TabIndex = 0;
			// 
			// SyncLog
			// 
			SyncLog.BackColor = Color.White;
			SyncLog.Cursor = Cursors.IBeam;
			SyncLog.Dock = DockStyle.Fill;
			SyncLog.Font = new Font("Courier New", 8.25F);
			SyncLog.ForeColor = Color.FromArgb(32, 32, 32);
			SyncLog.Location = new Point(0, 0);
			SyncLog.Name = "SyncLog";
			SyncLog.Size = new Size(1361, 239);
			SyncLog.TabIndex = 0;
			// 
			// MoreToolsContextMenu
			// 
			MoreToolsContextMenu.Items.AddRange(new ToolStripItem[] { MoreActionsContextMenu_CustomToolSeparator, MoreToolsContextMenu_UpdateTools, MoreToolsContextMenu_CleanWorkspace });
			MoreToolsContextMenu.Name = "MoreActionsContextMenu";
			MoreToolsContextMenu.Size = new Size(202, 54);
			// 
			// MoreActionsContextMenu_CustomToolSeparator
			// 
			MoreActionsContextMenu_CustomToolSeparator.Name = "MoreActionsContextMenu_CustomToolSeparator";
			MoreActionsContextMenu_CustomToolSeparator.Size = new Size(198, 6);
			// 
			// MoreToolsContextMenu_UpdateTools
			// 
			MoreToolsContextMenu_UpdateTools.Name = "MoreToolsContextMenu_UpdateTools";
			MoreToolsContextMenu_UpdateTools.Size = new Size(201, 22);
			MoreToolsContextMenu_UpdateTools.Text = "Check for Tools Updates";
			MoreToolsContextMenu_UpdateTools.Click += MoreToolsContextMenu_UpdateTools_Click;
			// 
			// MoreToolsContextMenu_CleanWorkspace
			// 
			MoreToolsContextMenu_CleanWorkspace.Name = "MoreToolsContextMenu_CleanWorkspace";
			MoreToolsContextMenu_CleanWorkspace.Size = new Size(201, 22);
			MoreToolsContextMenu_CleanWorkspace.Text = "Clean Workspace...";
			MoreToolsContextMenu_CleanWorkspace.Click += MoreToolsContextMenu_CleanWorkspace_Click;
			// 
			// SyncContextMenu
			// 
			SyncContextMenu.Items.AddRange(new ToolStripItem[] { SyncContexMenu_EnterChangelist, toolStripSeparator8 });
			SyncContextMenu.Name = "SyncContextMenu";
			SyncContextMenu.Size = new Size(184, 32);
			// 
			// SyncContexMenu_EnterChangelist
			// 
			SyncContexMenu_EnterChangelist.Name = "SyncContexMenu_EnterChangelist";
			SyncContexMenu_EnterChangelist.Size = new Size(183, 22);
			SyncContexMenu_EnterChangelist.Text = "Specific Changelist...";
			SyncContexMenu_EnterChangelist.Click += SyncContextMenu_EnterChangelist_Click;
			// 
			// toolStripSeparator8
			// 
			toolStripSeparator8.Name = "toolStripSeparator8";
			toolStripSeparator8.Size = new Size(180, 6);
			// 
			// StreamContextMenu
			// 
			StreamContextMenu.Name = "StreamContextMenu";
			StreamContextMenu.Size = new Size(61, 4);
			// 
			// RecentMenu
			// 
			RecentMenu.Items.AddRange(new ToolStripItem[] { RecentMenu_Browse, toolStripSeparator9, RecentMenu_Separator, RecentMenu_ClearList });
			RecentMenu.Name = "RecentMenu";
			RecentMenu.Size = new Size(123, 60);
			// 
			// RecentMenu_Browse
			// 
			RecentMenu_Browse.Name = "RecentMenu_Browse";
			RecentMenu_Browse.Size = new Size(122, 22);
			RecentMenu_Browse.Text = "Browse...";
			RecentMenu_Browse.Click += RecentMenu_Browse_Click;
			// 
			// toolStripSeparator9
			// 
			toolStripSeparator9.Name = "toolStripSeparator9";
			toolStripSeparator9.Size = new Size(119, 6);
			// 
			// RecentMenu_Separator
			// 
			RecentMenu_Separator.Name = "RecentMenu_Separator";
			RecentMenu_Separator.Size = new Size(119, 6);
			// 
			// RecentMenu_ClearList
			// 
			RecentMenu_ClearList.Name = "RecentMenu_ClearList";
			RecentMenu_ClearList.Size = new Size(122, 22);
			RecentMenu_ClearList.Text = "Clear List";
			RecentMenu_ClearList.Click += RecentMenu_ClearList_Click;
			// 
			// BuildListMultiContextMenu
			// 
			BuildListMultiContextMenu.Items.AddRange(new ToolStripItem[] { BuildListMultiContextMenu_Bisect, BuildListMultiContextMenu_TimeZoneSeparator, BuildListMultiContextMenu_ShowServerTimes, BuildListMultiContextMenu_ShowLocalTimes });
			BuildListMultiContextMenu.Name = "BuildListContextMenu";
			BuildListMultiContextMenu.Size = new Size(184, 76);
			// 
			// BuildListMultiContextMenu_Bisect
			// 
			BuildListMultiContextMenu_Bisect.Name = "BuildListMultiContextMenu_Bisect";
			BuildListMultiContextMenu_Bisect.Size = new Size(183, 22);
			BuildListMultiContextMenu_Bisect.Text = "Bisect these changes";
			BuildListMultiContextMenu_Bisect.Click += BuidlListMultiContextMenu_Bisect_Click;
			// 
			// BuildListMultiContextMenu_TimeZoneSeparator
			// 
			BuildListMultiContextMenu_TimeZoneSeparator.Name = "BuildListMultiContextMenu_TimeZoneSeparator";
			BuildListMultiContextMenu_TimeZoneSeparator.Size = new Size(180, 6);
			// 
			// BuildListMultiContextMenu_ShowServerTimes
			// 
			BuildListMultiContextMenu_ShowServerTimes.Name = "BuildListMultiContextMenu_ShowServerTimes";
			BuildListMultiContextMenu_ShowServerTimes.Size = new Size(183, 22);
			BuildListMultiContextMenu_ShowServerTimes.Text = "Show server times";
			BuildListMultiContextMenu_ShowServerTimes.Click += BuildListContextMenu_ShowServerTimes_Click;
			// 
			// BuildListMultiContextMenu_ShowLocalTimes
			// 
			BuildListMultiContextMenu_ShowLocalTimes.Name = "BuildListMultiContextMenu_ShowLocalTimes";
			BuildListMultiContextMenu_ShowLocalTimes.Size = new Size(183, 22);
			BuildListMultiContextMenu_ShowLocalTimes.Text = "Show local times";
			BuildListMultiContextMenu_ShowLocalTimes.Click += BuildListContextMenu_ShowServerTimes_Click;
			// 
			// FilterContextMenu
			// 
			FilterContextMenu.Items.AddRange(new ToolStripItem[] { FilterContextMenu_Default, FilterContextMenu_BeforeBadgeSeparator, FilterContextMenu_Type, FilterContextMenu_Badges, FilterContextMenu_Robomerge, FilterContextMenu_Author, FilterContextMenu_AfterBadgeSeparator, FilterContextMenu_ShowBuildMachineChanges });
			FilterContextMenu.Name = "FilterContextMenu";
			FilterContextMenu.Size = new Size(232, 170);
			// 
			// FilterContextMenu_Default
			// 
			FilterContextMenu_Default.Name = "FilterContextMenu_Default";
			FilterContextMenu_Default.Size = new Size(231, 22);
			FilterContextMenu_Default.Text = "Default";
			FilterContextMenu_Default.Click += FilterContextMenu_Default_Click;
			// 
			// FilterContextMenu_BeforeBadgeSeparator
			// 
			FilterContextMenu_BeforeBadgeSeparator.Name = "FilterContextMenu_BeforeBadgeSeparator";
			FilterContextMenu_BeforeBadgeSeparator.Size = new Size(228, 6);
			// 
			// FilterContextMenu_Type
			// 
			FilterContextMenu_Type.DropDownItems.AddRange(new ToolStripItem[] { FilterContextMenu_Type_ShowAll, toolStripSeparator10, FilterContextMenu_Type_Code, FilterContextMenu_Type_Content });
			FilterContextMenu_Type.Name = "FilterContextMenu_Type";
			FilterContextMenu_Type.Size = new Size(231, 22);
			FilterContextMenu_Type.Text = "Type";
			// 
			// FilterContextMenu_Type_ShowAll
			// 
			FilterContextMenu_Type_ShowAll.Name = "FilterContextMenu_Type_ShowAll";
			FilterContextMenu_Type_ShowAll.Size = new Size(120, 22);
			FilterContextMenu_Type_ShowAll.Text = "Show All";
			FilterContextMenu_Type_ShowAll.Click += FilterContextMenu_Type_ShowAll_Click;
			// 
			// toolStripSeparator10
			// 
			toolStripSeparator10.Name = "toolStripSeparator10";
			toolStripSeparator10.Size = new Size(117, 6);
			// 
			// FilterContextMenu_Type_Code
			// 
			FilterContextMenu_Type_Code.Name = "FilterContextMenu_Type_Code";
			FilterContextMenu_Type_Code.Size = new Size(120, 22);
			FilterContextMenu_Type_Code.Text = "Code";
			FilterContextMenu_Type_Code.Click += FilterContextMenu_Type_Code_Click;
			// 
			// FilterContextMenu_Type_Content
			// 
			FilterContextMenu_Type_Content.Name = "FilterContextMenu_Type_Content";
			FilterContextMenu_Type_Content.Size = new Size(120, 22);
			FilterContextMenu_Type_Content.Text = "Content";
			FilterContextMenu_Type_Content.Click += FilterContextMenu_Type_Content_Click;
			// 
			// FilterContextMenu_Badges
			// 
			FilterContextMenu_Badges.Name = "FilterContextMenu_Badges";
			FilterContextMenu_Badges.Size = new Size(231, 22);
			FilterContextMenu_Badges.Text = "Badges";
			// 
			// FilterContextMenu_Robomerge
			// 
			FilterContextMenu_Robomerge.DropDownItems.AddRange(new ToolStripItem[] { FilterContextMenu_Robomerge_ShowAll, FilterContextMenu_Robomerge_ShowBadged, FilterContextMenu_Robomerge_ShowNone, FilterContextMenu_AfterRobomergeShowSeparator, FilterContextMenu_Robomerge_Annotate });
			FilterContextMenu_Robomerge.Name = "FilterContextMenu_Robomerge";
			FilterContextMenu_Robomerge.Size = new Size(231, 22);
			FilterContextMenu_Robomerge.Text = "Robomerge";
			// 
			// FilterContextMenu_Robomerge_ShowAll
			// 
			FilterContextMenu_Robomerge_ShowAll.Name = "FilterContextMenu_Robomerge_ShowAll";
			FilterContextMenu_Robomerge_ShowAll.Size = new Size(237, 22);
			FilterContextMenu_Robomerge_ShowAll.Text = "Show All";
			FilterContextMenu_Robomerge_ShowAll.Click += FilterContextMenu_Robomerge_ShowAll_Click;
			// 
			// FilterContextMenu_Robomerge_ShowBadged
			// 
			FilterContextMenu_Robomerge_ShowBadged.Name = "FilterContextMenu_Robomerge_ShowBadged";
			FilterContextMenu_Robomerge_ShowBadged.Size = new Size(237, 22);
			FilterContextMenu_Robomerge_ShowBadged.Text = "Show Changes with Badges";
			FilterContextMenu_Robomerge_ShowBadged.Click += FilterContextMenu_Robomerge_ShowBadged_Click;
			// 
			// FilterContextMenu_Robomerge_ShowNone
			// 
			FilterContextMenu_Robomerge_ShowNone.Name = "FilterContextMenu_Robomerge_ShowNone";
			FilterContextMenu_Robomerge_ShowNone.Size = new Size(237, 22);
			FilterContextMenu_Robomerge_ShowNone.Text = "Show None";
			FilterContextMenu_Robomerge_ShowNone.Click += FilterContextMenu_Robomerge_ShowNone_Click;
			// 
			// FilterContextMenu_AfterRobomergeShowSeparator
			// 
			FilterContextMenu_AfterRobomergeShowSeparator.Name = "FilterContextMenu_AfterRobomergeShowSeparator";
			FilterContextMenu_AfterRobomergeShowSeparator.Size = new Size(234, 6);
			// 
			// FilterContextMenu_Robomerge_Annotate
			// 
			FilterContextMenu_Robomerge_Annotate.Name = "FilterContextMenu_Robomerge_Annotate";
			FilterContextMenu_Robomerge_Annotate.Size = new Size(237, 22);
			FilterContextMenu_Robomerge_Annotate.Text = "Annotate Robomerge Changes";
			FilterContextMenu_Robomerge_Annotate.Click += FilterContextMenu_Robomerge_Annotate_Click;
			// 
			// FilterContextMenu_Author
			// 
			FilterContextMenu_Author.DropDownItems.AddRange(new ToolStripItem[] { FilterContextMenu_Author_Name });
			FilterContextMenu_Author.Name = "FilterContextMenu_Author";
			FilterContextMenu_Author.Size = new Size(231, 22);
			FilterContextMenu_Author.Text = "Author";
			// 
			// FilterContextMenu_Author_Name
			// 
			FilterContextMenu_Author_Name.AutoToolTip = true;
			FilterContextMenu_Author_Name.Name = "FilterContextMenu_Author_Name";
			FilterContextMenu_Author_Name.Size = new Size(100, 23);
			FilterContextMenu_Author_Name.ToolTipText = "P4 username search. use '+' or ';' to separate multiple usernames to search for";
			// 
			// FilterContextMenu_AfterBadgeSeparator
			// 
			FilterContextMenu_AfterBadgeSeparator.Name = "FilterContextMenu_AfterBadgeSeparator";
			FilterContextMenu_AfterBadgeSeparator.Size = new Size(228, 6);
			// 
			// FilterContextMenu_ShowBuildMachineChanges
			// 
			FilterContextMenu_ShowBuildMachineChanges.Name = "FilterContextMenu_ShowBuildMachineChanges";
			FilterContextMenu_ShowBuildMachineChanges.Size = new Size(231, 22);
			FilterContextMenu_ShowBuildMachineChanges.Text = "Show Build Machine Changes";
			FilterContextMenu_ShowBuildMachineChanges.Click += FilterContextMenu_ShowBuildMachineChanges_Click;
			// 
			// BadgeContextMenu
			// 
			BadgeContextMenu.Name = "BadgeContextMenu";
			BadgeContextMenu.Size = new Size(61, 4);
			// 
			// BuildHealthContextMenu
			// 
			BuildHealthContextMenu.Items.AddRange(new ToolStripItem[] { BuildHealthContextMenu_Browse, BuildHealthContextMenu_MinSeparator, BuildHealthContextMenu_MaxSeparator, BuildHealthContextMenu_Settings });
			BuildHealthContextMenu.Name = "BuildHealthContextMenu";
			BuildHealthContextMenu.Size = new Size(130, 60);
			// 
			// BuildHealthContextMenu_Browse
			// 
			BuildHealthContextMenu_Browse.Name = "BuildHealthContextMenu_Browse";
			BuildHealthContextMenu_Browse.Size = new Size(129, 22);
			BuildHealthContextMenu_Browse.Text = "Show All...";
			BuildHealthContextMenu_Browse.Click += BuildHealthContextMenu_Browse_Click;
			// 
			// BuildHealthContextMenu_MinSeparator
			// 
			BuildHealthContextMenu_MinSeparator.Name = "BuildHealthContextMenu_MinSeparator";
			BuildHealthContextMenu_MinSeparator.Size = new Size(126, 6);
			// 
			// BuildHealthContextMenu_MaxSeparator
			// 
			BuildHealthContextMenu_MaxSeparator.Name = "BuildHealthContextMenu_MaxSeparator";
			BuildHealthContextMenu_MaxSeparator.Size = new Size(126, 6);
			// 
			// BuildHealthContextMenu_Settings
			// 
			BuildHealthContextMenu_Settings.Name = "BuildHealthContextMenu_Settings";
			BuildHealthContextMenu_Settings.Size = new Size(129, 22);
			BuildHealthContextMenu_Settings.Text = "Settings...";
			BuildHealthContextMenu_Settings.Click += BuildHealthContextMenu_Settings_Click;
			// 
			// EditorConfigWatcher
			// 
			EditorConfigWatcher.EnableRaisingEvents = true;
			EditorConfigWatcher.Filter = "EditorPerProjectUserSettings.ini";
			EditorConfigWatcher.NotifyFilter = System.IO.NotifyFilters.LastWrite;
			EditorConfigWatcher.SynchronizingObject = this;
			EditorConfigWatcher.Changed += EditorConfigWatcher_Changed;
			EditorConfigWatcher.Created += EditorConfigWatcher_Changed;
			EditorConfigWatcher.Deleted += EditorConfigWatcher_Changed;
			EditorConfigWatcher.Renamed += EditorConfigWatcher_Renamed;
			// 
			// WorkspaceControl
			// 
			AutoScaleDimensions = new SizeF(96F, 96F);
			AutoScaleMode = AutoScaleMode.Dpi;
			BackColor = SystemColors.Control;
			Controls.Add(tableLayoutPanel2);
			Margin = new Padding(0);
			Name = "WorkspaceControl";
			Size = new Size(1363, 752);
			Load += MainWindow_Load;
			VisibleChanged += WorkspaceControl_VisibleChanged;
			DpiChangedAfterParent += WorkspaceControl_DpiChangedAfterParent;
			OptionsContextMenu.ResumeLayout(false);
			BuildListContextMenu.ResumeLayout(false);
			flowLayoutPanel1.ResumeLayout(false);
			flowLayoutPanel1.PerformLayout();
			tableLayoutPanel3.ResumeLayout(false);
			tableLayoutPanel3.PerformLayout();
			tableLayoutPanel2.ResumeLayout(false);
			tableLayoutPanel2.PerformLayout();
			Splitter.Panel1.ResumeLayout(false);
			Splitter.Panel2.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)Splitter).EndInit();
			Splitter.ResumeLayout(false);
			StatusLayoutPanel.ResumeLayout(false);
			panel1.ResumeLayout(false);
			MoreToolsContextMenu.ResumeLayout(false);
			SyncContextMenu.ResumeLayout(false);
			RecentMenu.ResumeLayout(false);
			BuildListMultiContextMenu.ResumeLayout(false);
			FilterContextMenu.ResumeLayout(false);
			BuildHealthContextMenu.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)EditorConfigWatcher).EndInit();
			ResumeLayout(false);
		}

		#endregion

		private BuildListControl BuildList;
		private LogControl SyncLog;
		private System.Windows.Forms.ColumnHeader IconColumn;
		private System.Windows.Forms.ColumnHeader TimeColumn;
		private System.Windows.Forms.ColumnHeader DescriptionColumn;
		private System.Windows.Forms.ColumnHeader StatusColumn;
		private System.Windows.Forms.ColumnHeader ChangeColumn;
		private System.Windows.Forms.ColumnHeader AuthorColumn;
		private System.Windows.Forms.Button OptionsButton;
		private System.Windows.Forms.ContextMenuStrip OptionsContextMenu;
		private UnrealGameSync.Controls.ReadOnlyCheckbox RunAfterSyncCheckBox;
		private UnrealGameSync.Controls.ReadOnlyCheckbox BuildAfterSyncCheckBox;
		private System.Windows.Forms.Label AfterSyncingLabel;
		private System.Windows.Forms.ContextMenuStrip BuildListContextMenu;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Cancel;
		private LogSplitContainer Splitter;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_MoreInfo;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_AddStar;
		private System.Windows.Forms.ToolStripSeparator BuildListContextMenu_CustomTool_End;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_MarkGood;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_MarkBad;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_StartInvestigating;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Sync;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator4;
		private System.Windows.Forms.NotifyIcon NotifyIcon;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_RemoveStar;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_FinishInvestigating;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_LaunchEditor;
		private System.Windows.Forms.ToolTip BuildListToolTip;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_AutoResolveConflicts;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_AlwaysClobberFiles;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_AlwaysDeleteFiles;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_EditorArguments;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_OpenVisualStudio;
		private System.Windows.Forms.Panel panel1;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Build;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_GenerateProjectFiles;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_EditorBuildConfiguration;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_BuildConfig_Debug;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_BuildConfig_DebugGame;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_BuildConfig_Development;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Rebuild;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_WithdrawReview;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_ScheduledSync;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_LeaveComment;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator7;
		private System.Windows.Forms.ColumnHeader CISColumn;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_EditComment;
		private System.Windows.Forms.FlowLayoutPanel flowLayoutPanel1;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel3;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
		private System.Windows.Forms.ToolStripSeparator BuildListContextMenu_TimeZoneSeparator;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_ShowServerTimes;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_ShowLocalTimes;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_CustomizeBuildSteps;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_TimeZone;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_TimeZone_Local;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_TimeZone_PerforceServer;
		private UnrealGameSync.Controls.ReadOnlyCheckbox OpenSolutionAfterSyncCheckBox;
		private System.Windows.Forms.TableLayoutPanel StatusLayoutPanel;
		private StatusPanel StatusPanel;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_SyncContentOnly;
		private System.Windows.Forms.ContextMenuStrip MoreToolsContextMenu;
		private System.Windows.Forms.ToolStripMenuItem MoreToolsContextMenu_CleanWorkspace;
		private System.Windows.Forms.ToolStripMenuItem MoreToolsContextMenu_UpdateTools;
		private System.Windows.Forms.ToolStripSeparator MoreActionsContextMenu_CustomToolSeparator;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator3;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator5;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator6;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_Diagnostics;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_SyncFilter;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_Presets;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_SyncOnlyThisChange;
		private System.Windows.Forms.ContextMenuStrip SyncContextMenu;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator8;
		private System.Windows.Forms.ToolStripMenuItem SyncContexMenu_EnterChangelist;
		private System.Windows.Forms.ContextMenuStrip StreamContextMenu;
		private System.Windows.Forms.ToolStripMenuItem tabLabelsToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_TabNames_Stream;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_TabNames_WorkspaceName;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_TabNames_WorkspaceRoot;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_TabNames_ProjectFile;
		private System.Windows.Forms.ContextMenuStrip RecentMenu;
		private System.Windows.Forms.ToolStripSeparator RecentMenu_Separator;
		private System.Windows.Forms.ToolStripMenuItem RecentMenu_ClearList;
		private System.Windows.Forms.ToolStripMenuItem RecentMenu_Browse;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator9;
		private System.Windows.Forms.ToolStripSeparator BuildListContextMenu_CustomTool_Start;
		private System.Windows.Forms.ToolStripMenuItem showChangesToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_ShowChanges_ShowUnreviewed;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_ShowChanges_ShowAutomated;
		private System.Windows.Forms.ColumnHeader TypeColumn;
		private System.Windows.Forms.ContextMenuStrip BuildListMultiContextMenu;
		private System.Windows.Forms.ToolStripMenuItem BuildListMultiContextMenu_Bisect;
		private System.Windows.Forms.ToolStripSeparator BuildListMultiContextMenu_TimeZoneSeparator;
		private System.Windows.Forms.ToolStripMenuItem BuildListMultiContextMenu_ShowServerTimes;
		private System.Windows.Forms.ToolStripMenuItem BuildListMultiContextMenu_ShowLocalTimes;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Bisect_Pass;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Bisect_Fail;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Bisect_Exclude;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_Bisect_Include;
		private System.Windows.Forms.ToolStripSeparator BuildListContextMenu_Bisect_Separator;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_ApplicationSettings;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator2;
		private System.Windows.Forms.Button FilterButton;
		private System.Windows.Forms.ContextMenuStrip FilterContextMenu;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Default;
		private System.Windows.Forms.ToolStripSeparator FilterContextMenu_BeforeBadgeSeparator;
		private System.Windows.Forms.ToolStripSeparator FilterContextMenu_AfterBadgeSeparator;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_ShowBuildMachineChanges;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Badges;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Type;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Type_Code;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Type_Content;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Type_ShowAll;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Robomerge;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Robomerge_ShowAll;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Robomerge_ShowBadged;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Robomerge_ShowNone;
		private System.Windows.Forms.ToolStripSeparator FilterContextMenu_AfterRobomergeShowSeparator;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Robomerge_Annotate;
		private System.Windows.Forms.ToolStripMenuItem FilterContextMenu_Author;
		private System.Windows.Forms.ToolStripTextBox FilterContextMenu_Author_Name;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator10;
		private System.Windows.Forms.ContextMenuStrip BuildHealthContextMenu;
		private System.Windows.Forms.ContextMenuStrip BadgeContextMenu;
		private System.Windows.Forms.ToolStripSeparator BuildHealthContextMenu_MaxSeparator;
		private System.Windows.Forms.ToolStripMenuItem BuildHealthContextMenu_Settings;
		private System.Windows.Forms.ToolStripMenuItem BuildHealthContextMenu_Browse;
		private System.Windows.Forms.ToolStripSeparator BuildHealthContextMenu_MinSeparator;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_SyncPrecompiledBinaries;
		private System.Windows.Forms.ToolStripMenuItem disabledToolStripMenuItem;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator11;
		private System.Windows.Forms.ToolStripMenuItem editorToolStripMenuItem;
		private System.Windows.Forms.ToolStripMenuItem editorPhysXToolStripMenuItem;
		private System.IO.FileSystemWatcher EditorConfigWatcher;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_ViewInSwarm;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_CopyChangelistNumber;
		private System.Windows.Forms.ColumnHeader ReviewerColumn;
		private System.Windows.Forms.ColumnHeader PreflightColumn;
		private System.Windows.Forms.ToolStripMenuItem BuildListContextMenu_OpenPreflight;
		private UnrealGameSync.Controls.ReadOnlyCheckbox GenerateAfterSyncCheckBox;
		private System.Windows.Forms.ToolStripMenuItem OptionsContextMenu_ImportSnapshots;
	}
}
