using System.Windows.Forms;

namespace UnrealGameSync.Controls
{
	partial class SyncFilterControl
	{
		/// <summary> 
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary> 
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Component Designer generated code

		/// <summary> 
		/// Required method for Designer support - do not modify 
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			ViewGroupBox = new GroupBox();
			tableLayoutPanel2 = new TableLayoutPanel();
			ViewDataGrid = new DataGridView();
			SyntaxButton = new LinkLabel();
			CategoriesGroupBox = new GroupBox();
			tableLayoutPanel4 = new TableLayoutPanel();
			CategoriesCheckList = new CheckedListBox();
			SplitContainer = new SplitContainer();
			tableLayoutPanel1 = new TableLayoutPanel();
			groupBox1 = new GroupBox();
			tableLayoutPanel3 = new TableLayoutPanel();
			SyncAllProjects = new CheckBox();
			IncludeAllProjectsInSolution = new CheckBox();
			GenerateUprojectSpecificSolution = new CheckBox();
			ViewGroupBox.SuspendLayout();
			tableLayoutPanel2.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)ViewDataGrid).BeginInit();
			CategoriesGroupBox.SuspendLayout();
			tableLayoutPanel4.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)SplitContainer).BeginInit();
			SplitContainer.Panel1.SuspendLayout();
			SplitContainer.Panel2.SuspendLayout();
			SplitContainer.SuspendLayout();
			tableLayoutPanel1.SuspendLayout();
			groupBox1.SuspendLayout();
			tableLayoutPanel3.SuspendLayout();
			SuspendLayout();
			// 
			// ViewGroupBox
			// 
			ViewGroupBox.Controls.Add(tableLayoutPanel2);
			ViewGroupBox.Controls.Add(SyntaxButton);
			ViewGroupBox.Dock = DockStyle.Fill;
			ViewGroupBox.Location = new System.Drawing.Point(0, 0);
			ViewGroupBox.Name = "ViewGroupBox";
			ViewGroupBox.Padding = new Padding(16, 8, 16, 8);
			ViewGroupBox.Size = new System.Drawing.Size(1008, 301);
			ViewGroupBox.TabIndex = 5;
			ViewGroupBox.TabStop = false;
			ViewGroupBox.Text = "Custom View";
			// 
			// tableLayoutPanel2
			// 
			tableLayoutPanel2.ColumnCount = 1;
			tableLayoutPanel2.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50F));
			tableLayoutPanel2.Controls.Add(ViewDataGrid, 0, 0);
			tableLayoutPanel2.Dock = DockStyle.Fill;
			tableLayoutPanel2.Location = new System.Drawing.Point(16, 23);
			tableLayoutPanel2.Name = "tableLayoutPanel2";
			tableLayoutPanel2.RowCount = 1;
			tableLayoutPanel2.RowStyles.Add(new RowStyle(SizeType.Percent, 50F));
			tableLayoutPanel2.Size = new System.Drawing.Size(976, 270);
			tableLayoutPanel2.TabIndex = 8;
			// 
			// ViewDataGrid
			// 
			ViewDataGrid.AllowUserToResizeColumns = false;
			ViewDataGrid.BackgroundColor = System.Drawing.SystemColors.Window;
			ViewDataGrid.BorderStyle = BorderStyle.None;
			ViewDataGrid.CellBorderStyle = DataGridViewCellBorderStyle.None;
			ViewDataGrid.Dock = DockStyle.Fill;
			ViewDataGrid.Font = new System.Drawing.Font("Courier New", 8F);
			ViewDataGrid.ImeMode = ImeMode.Off;
			ViewDataGrid.Location = new System.Drawing.Point(0, 0);
			ViewDataGrid.Margin = new Padding(0);
			ViewDataGrid.MultiSelect = false;
			ViewDataGrid.Name = "ViewDataGrid";
			ViewDataGrid.RowHeadersWidthSizeMode = DataGridViewRowHeadersWidthSizeMode.DisableResizing;
			ViewDataGrid.ScrollBars = ScrollBars.Vertical;
			ViewDataGrid.SelectionMode = DataGridViewSelectionMode.FullRowSelect;
			ViewDataGrid.Size = new System.Drawing.Size(976, 270);
			ViewDataGrid.TabIndex = 6;
			// 
			// SyntaxButton
			// 
			SyntaxButton.Anchor = AnchorStyles.Top | AnchorStyles.Right;
			SyntaxButton.AutoSize = true;
			SyntaxButton.Location = new System.Drawing.Point(933, 0);
			SyntaxButton.Name = "SyntaxButton";
			SyntaxButton.Size = new System.Drawing.Size(40, 13);
			SyntaxButton.TabIndex = 7;
			SyntaxButton.TabStop = true;
			SyntaxButton.Text = "Syntax";
			SyntaxButton.LinkClicked += SyntaxButton_LinkClicked;
			// 
			// CategoriesGroupBox
			// 
			CategoriesGroupBox.AutoSizeMode = AutoSizeMode.GrowAndShrink;
			CategoriesGroupBox.Controls.Add(tableLayoutPanel4);
			CategoriesGroupBox.Dock = DockStyle.Fill;
			CategoriesGroupBox.Location = new System.Drawing.Point(0, 86);
			CategoriesGroupBox.Margin = new Padding(0, 3, 0, 0);
			CategoriesGroupBox.Name = "CategoriesGroupBox";
			CategoriesGroupBox.Padding = new Padding(16, 8, 16, 8);
			CategoriesGroupBox.Size = new System.Drawing.Size(1008, 320);
			CategoriesGroupBox.TabIndex = 4;
			CategoriesGroupBox.TabStop = false;
			CategoriesGroupBox.Text = "Categories";
			// 
			// tableLayoutPanel4
			// 
			tableLayoutPanel4.ColumnCount = 1;
			tableLayoutPanel4.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50F));
			tableLayoutPanel4.Controls.Add(CategoriesCheckList, 0, 0);
			tableLayoutPanel4.Dock = DockStyle.Fill;
			tableLayoutPanel4.Location = new System.Drawing.Point(16, 23);
			tableLayoutPanel4.Name = "tableLayoutPanel4";
			tableLayoutPanel4.RowCount = 1;
			tableLayoutPanel4.RowStyles.Add(new RowStyle(SizeType.Percent, 50F));
			tableLayoutPanel4.Size = new System.Drawing.Size(976, 289);
			tableLayoutPanel4.TabIndex = 8;
			// 
			// CategoriesCheckList
			// 
			CategoriesCheckList.BorderStyle = BorderStyle.None;
			CategoriesCheckList.CheckOnClick = true;
			CategoriesCheckList.Dock = DockStyle.Fill;
			CategoriesCheckList.FormattingEnabled = true;
			CategoriesCheckList.IntegralHeight = false;
			CategoriesCheckList.Location = new System.Drawing.Point(3, 3);
			CategoriesCheckList.Name = "CategoriesCheckList";
			CategoriesCheckList.Size = new System.Drawing.Size(970, 283);
			CategoriesCheckList.Sorted = true;
			CategoriesCheckList.TabIndex = 7;
			// 
			// SplitContainer
			// 
			SplitContainer.Dock = DockStyle.Fill;
			SplitContainer.Location = new System.Drawing.Point(7, 7);
			SplitContainer.Name = "SplitContainer";
			SplitContainer.Orientation = Orientation.Horizontal;
			// 
			// SplitContainer.Panel1
			// 
			SplitContainer.Panel1.Controls.Add(tableLayoutPanel1);
			// 
			// SplitContainer.Panel2
			// 
			SplitContainer.Panel2.Controls.Add(ViewGroupBox);
			SplitContainer.Size = new System.Drawing.Size(1008, 719);
			SplitContainer.SplitterDistance = 406;
			SplitContainer.SplitterWidth = 12;
			SplitContainer.TabIndex = 8;
			// 
			// tableLayoutPanel1
			// 
			tableLayoutPanel1.AutoSize = true;
			tableLayoutPanel1.ColumnCount = 1;
			tableLayoutPanel1.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
			tableLayoutPanel1.Controls.Add(groupBox1, 0, 0);
			tableLayoutPanel1.Controls.Add(CategoriesGroupBox, 0, 1);
			tableLayoutPanel1.Dock = DockStyle.Fill;
			tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
			tableLayoutPanel1.Margin = new Padding(0);
			tableLayoutPanel1.Name = "tableLayoutPanel1";
			tableLayoutPanel1.RowCount = 2;
			tableLayoutPanel1.RowStyles.Add(new RowStyle());
			tableLayoutPanel1.RowStyles.Add(new RowStyle(SizeType.Percent, 100F));
			tableLayoutPanel1.Size = new System.Drawing.Size(1008, 406);
			tableLayoutPanel1.TabIndex = 8;
			// 
			// groupBox1
			// 
			groupBox1.AutoSize = true;
			groupBox1.AutoSizeMode = AutoSizeMode.GrowAndShrink;
			groupBox1.Controls.Add(tableLayoutPanel3);
			groupBox1.Dock = DockStyle.Fill;
			groupBox1.Location = new System.Drawing.Point(0, 3);
			groupBox1.Margin = new Padding(0, 3, 0, 3);
			groupBox1.Name = "groupBox1";
			groupBox1.Padding = new Padding(16, 8, 16, 8);
			groupBox1.Size = new System.Drawing.Size(1008, 77);
			groupBox1.TabIndex = 8;
			groupBox1.TabStop = false;
			groupBox1.Text = "General";
			// 
			// tableLayoutPanel3
			// 
			tableLayoutPanel3.AutoSize = true;
			tableLayoutPanel3.AutoSizeMode = AutoSizeMode.GrowAndShrink;
			tableLayoutPanel3.ColumnCount = 2;
			tableLayoutPanel3.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50F));
			tableLayoutPanel3.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50F));
			tableLayoutPanel3.Controls.Add(SyncAllProjects, 0, 0);
			tableLayoutPanel3.Controls.Add(IncludeAllProjectsInSolution, 0, 1);
			tableLayoutPanel3.Controls.Add(GenerateUprojectSpecificSolution, 1, 1);
			tableLayoutPanel3.Dock = DockStyle.Fill;
			tableLayoutPanel3.Location = new System.Drawing.Point(16, 23);
			tableLayoutPanel3.Name = "tableLayoutPanel3";
			tableLayoutPanel3.RowCount = 2;
			tableLayoutPanel3.RowStyles.Add(new RowStyle(SizeType.Percent, 50F));
			tableLayoutPanel3.RowStyles.Add(new RowStyle(SizeType.Percent, 50F));
			tableLayoutPanel3.Size = new System.Drawing.Size(976, 46);
			tableLayoutPanel3.TabIndex = 8;
			// 
			// SyncAllProjects
			// 
			SyncAllProjects.Anchor = AnchorStyles.Left;
			SyncAllProjects.AutoSize = true;
			SyncAllProjects.Location = new System.Drawing.Point(3, 3);
			SyncAllProjects.Name = "SyncAllProjects";
			SyncAllProjects.Size = new System.Drawing.Size(158, 17);
			SyncAllProjects.TabIndex = 6;
			SyncAllProjects.Text = "Sync all projects in stream";
			SyncAllProjects.UseVisualStyleBackColor = true;
			// 
			// IncludeAllProjectsInSolution
			// 
			IncludeAllProjectsInSolution.Anchor = AnchorStyles.Left;
			IncludeAllProjectsInSolution.AutoSize = true;
			IncludeAllProjectsInSolution.Location = new System.Drawing.Point(3, 26);
			IncludeAllProjectsInSolution.Name = "IncludeAllProjectsInSolution";
			IncludeAllProjectsInSolution.Size = new System.Drawing.Size(220, 17);
			IncludeAllProjectsInSolution.TabIndex = 7;
			IncludeAllProjectsInSolution.Text = "Include all synced projects in solution";
			IncludeAllProjectsInSolution.UseVisualStyleBackColor = true;
			// 
			// GenerateUprojectSpecificSolution
			// 
			GenerateUprojectSpecificSolution.Anchor = AnchorStyles.Left;
			GenerateUprojectSpecificSolution.AutoSize = true;
			GenerateUprojectSpecificSolution.Location = new System.Drawing.Point(491, 26);
			GenerateUprojectSpecificSolution.Name = "GenerateUprojectSpecificSolution";
			GenerateUprojectSpecificSolution.Size = new System.Drawing.Size(252, 17);
			GenerateUprojectSpecificSolution.TabIndex = 8;
			GenerateUprojectSpecificSolution.Text = "Generate minimal .uproject specific solution";
			GenerateUprojectSpecificSolution.UseVisualStyleBackColor = true;
			// 
			// SyncFilterControl
			// 
			AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
			AutoScaleMode = AutoScaleMode.Dpi;
			BackColor = System.Drawing.SystemColors.Window;
			Controls.Add(SplitContainer);
			Font = new System.Drawing.Font("Segoe UI", 8.25F);
			Name = "SyncFilterControl";
			Padding = new Padding(7);
			Size = new System.Drawing.Size(1022, 733);
			ViewGroupBox.ResumeLayout(false);
			ViewGroupBox.PerformLayout();
			tableLayoutPanel2.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)ViewDataGrid).EndInit();
			CategoriesGroupBox.ResumeLayout(false);
			tableLayoutPanel4.ResumeLayout(false);
			SplitContainer.Panel1.ResumeLayout(false);
			SplitContainer.Panel1.PerformLayout();
			SplitContainer.Panel2.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)SplitContainer).EndInit();
			SplitContainer.ResumeLayout(false);
			tableLayoutPanel1.ResumeLayout(false);
			tableLayoutPanel1.PerformLayout();
			groupBox1.ResumeLayout(false);
			groupBox1.PerformLayout();
			tableLayoutPanel3.ResumeLayout(false);
			tableLayoutPanel3.PerformLayout();
			ResumeLayout(false);
		}

		#endregion

		private System.Windows.Forms.GroupBox ViewGroupBox;
		private System.Windows.Forms.GroupBox CategoriesGroupBox;
		public System.Windows.Forms.CheckedListBox CategoriesCheckList;
		private System.Windows.Forms.SplitContainer SplitContainer;
		private System.Windows.Forms.LinkLabel SyntaxButton;
		private System.Windows.Forms.DataGridView ViewDataGrid;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel1;
		private System.Windows.Forms.GroupBox groupBox1;
		public System.Windows.Forms.CheckBox SyncAllProjects;
		public System.Windows.Forms.CheckBox IncludeAllProjectsInSolution;
		public System.Windows.Forms.CheckBox GenerateUprojectSpecificSolution;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel3;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel2;
		private System.Windows.Forms.TableLayoutPanel tableLayoutPanel4;
	}
}
