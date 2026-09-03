namespace UnrealGameSync
{
	partial class PerforceSyncSettingsWindow
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

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(PerforceSyncSettingsWindow));
			OkButton = new System.Windows.Forms.Button();
			CancButton = new System.Windows.Forms.Button();
			ResetButton = new System.Windows.Forms.Button();
			labelMaxCommandsPerBatch = new System.Windows.Forms.Label();
			labelMaxSizePerBatch = new System.Windows.Forms.Label();
			numericUpDownMaxSizePerBatch = new System.Windows.Forms.NumericUpDown();
			numericUpDownMaxCommandsPerBatch = new System.Windows.Forms.NumericUpDown();
			groupBoxSyncing = new System.Windows.Forms.GroupBox();
			UseNativeLibraryCheckBox = new System.Windows.Forms.CheckBox();
			labelUseNativeLibrary = new System.Windows.Forms.Label();
			FastSyncCheckBox = new System.Windows.Forms.CheckBox();
			label1 = new System.Windows.Forms.Label();
			numericUpDownRetriesOnSyncError = new System.Windows.Forms.NumericUpDown();
			labelRetriesOnSyncError = new System.Windows.Forms.Label();
			numericUpDownSyncErrorRetryDelay = new System.Windows.Forms.NumericUpDown();
			labelSyncErrorRetryDelay = new System.Windows.Forms.Label();
			((System.ComponentModel.ISupportInitialize)numericUpDownMaxSizePerBatch).BeginInit();
			((System.ComponentModel.ISupportInitialize)numericUpDownMaxCommandsPerBatch).BeginInit();
			groupBoxSyncing.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)numericUpDownRetriesOnSyncError).BeginInit();
			((System.ComponentModel.ISupportInitialize)numericUpDownSyncErrorRetryDelay).BeginInit();
			SuspendLayout();
			// 
			// OkButton
			// 
			OkButton.Anchor = System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right;
			OkButton.Location = new System.Drawing.Point(418, 370);
			OkButton.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			OkButton.Name = "OkButton";
			OkButton.Size = new System.Drawing.Size(130, 38);
			OkButton.TabIndex = 12;
			OkButton.Text = "Ok";
			OkButton.UseVisualStyleBackColor = true;
			OkButton.Click += OkButton_Click;
			// 
			// CancButton
			// 
			CancButton.Anchor = System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right;
			CancButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			CancButton.Location = new System.Drawing.Point(556, 370);
			CancButton.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			CancButton.Name = "CancButton";
			CancButton.Size = new System.Drawing.Size(130, 38);
			CancButton.TabIndex = 13;
			CancButton.Text = "Cancel";
			CancButton.UseVisualStyleBackColor = true;
			CancButton.Click += CancButton_Click;
			// 
			// ResetButton
			// 
			ResetButton.Anchor = System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left;
			ResetButton.Location = new System.Drawing.Point(18, 370);
			ResetButton.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			ResetButton.Name = "ResetButton";
			ResetButton.Size = new System.Drawing.Size(178, 38);
			ResetButton.TabIndex = 11;
			ResetButton.Text = "Reset to Default";
			ResetButton.UseVisualStyleBackColor = true;
			ResetButton.Click += ResetButton_Click;
			// 
			// labelMaxCommandsPerBatch
			// 
			labelMaxCommandsPerBatch.AutoSize = true;
			labelMaxCommandsPerBatch.Location = new System.Drawing.Point(10, 46);
			labelMaxCommandsPerBatch.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			labelMaxCommandsPerBatch.Name = "labelMaxCommandsPerBatch";
			labelMaxCommandsPerBatch.Size = new System.Drawing.Size(223, 25);
			labelMaxCommandsPerBatch.TabIndex = 5;
			labelMaxCommandsPerBatch.Text = "Max Commands Per Batch:";
			// 
			// labelMaxSizePerBatch
			// 
			labelMaxSizePerBatch.AutoSize = true;
			labelMaxSizePerBatch.Location = new System.Drawing.Point(10, 90);
			labelMaxSizePerBatch.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			labelMaxSizePerBatch.Name = "labelMaxSizePerBatch";
			labelMaxSizePerBatch.Size = new System.Drawing.Size(203, 25);
			labelMaxSizePerBatch.TabIndex = 7;
			labelMaxSizePerBatch.Text = "Max Size Per Batch (MB):";
			// 
			// numericUpDownMaxSizePerBatch
			// 
			numericUpDownMaxSizePerBatch.Increment = new decimal(new int[] { 64, 0, 0, 0 });
			numericUpDownMaxSizePerBatch.Location = new System.Drawing.Point(390, 86);
			numericUpDownMaxSizePerBatch.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			numericUpDownMaxSizePerBatch.Maximum = new decimal(new int[] { 8192, 0, 0, 0 });
			numericUpDownMaxSizePerBatch.Name = "numericUpDownMaxSizePerBatch";
			numericUpDownMaxSizePerBatch.Size = new System.Drawing.Size(270, 31);
			numericUpDownMaxSizePerBatch.TabIndex = 8;
			// 
			// numericUpDownMaxCommandsPerBatch
			// 
			numericUpDownMaxCommandsPerBatch.Increment = new decimal(new int[] { 50, 0, 0, 0 });
			numericUpDownMaxCommandsPerBatch.Location = new System.Drawing.Point(390, 44);
			numericUpDownMaxCommandsPerBatch.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			numericUpDownMaxCommandsPerBatch.Maximum = new decimal(new int[] { 1000, 0, 0, 0 });
			numericUpDownMaxCommandsPerBatch.Name = "numericUpDownMaxCommandsPerBatch";
			numericUpDownMaxCommandsPerBatch.Size = new System.Drawing.Size(270, 31);
			numericUpDownMaxCommandsPerBatch.TabIndex = 6;
			// 
			// groupBoxSyncing
			// 
			groupBoxSyncing.Anchor = System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right;
			groupBoxSyncing.Controls.Add(UseNativeLibraryCheckBox);
			groupBoxSyncing.Controls.Add(labelUseNativeLibrary);
			groupBoxSyncing.Controls.Add(FastSyncCheckBox);
			groupBoxSyncing.Controls.Add(label1);
			groupBoxSyncing.Controls.Add(numericUpDownRetriesOnSyncError);
			groupBoxSyncing.Controls.Add(labelRetriesOnSyncError);
			groupBoxSyncing.Controls.Add(numericUpDownSyncErrorRetryDelay);
			groupBoxSyncing.Controls.Add(labelSyncErrorRetryDelay);
			groupBoxSyncing.Controls.Add(numericUpDownMaxCommandsPerBatch);
			groupBoxSyncing.Controls.Add(numericUpDownMaxSizePerBatch);
			groupBoxSyncing.Controls.Add(labelMaxSizePerBatch);
			groupBoxSyncing.Controls.Add(labelMaxCommandsPerBatch);
			groupBoxSyncing.Location = new System.Drawing.Point(18, 18);
			groupBoxSyncing.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			groupBoxSyncing.Name = "groupBoxSyncing";
			groupBoxSyncing.Padding = new System.Windows.Forms.Padding(4, 4, 4, 4);
			groupBoxSyncing.Size = new System.Drawing.Size(670, 346);
			groupBoxSyncing.TabIndex = 0;
			groupBoxSyncing.TabStop = false;
			groupBoxSyncing.Text = "Syncing";
			// 
			// UseNativeLibraryCheckBox
			// 
			UseNativeLibraryCheckBox.AutoSize = true;
			UseNativeLibraryCheckBox.Location = new System.Drawing.Point(390, 250);
			UseNativeLibraryCheckBox.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			UseNativeLibraryCheckBox.Name = "UseNativeLibraryCheckBox";
			UseNativeLibraryCheckBox.Size = new System.Drawing.Size(22, 21);
			UseNativeLibraryCheckBox.TabIndex = 18;
			UseNativeLibraryCheckBox.UseVisualStyleBackColor = true;
			// 
			// labelUseNativeLibrary
			// 
			labelUseNativeLibrary.AutoSize = true;
			labelUseNativeLibrary.Location = new System.Drawing.Point(10, 250);
			labelUseNativeLibrary.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			labelUseNativeLibrary.Name = "labelUseNativeLibrary";
			labelUseNativeLibrary.Size = new System.Drawing.Size(154, 25);
			labelUseNativeLibrary.TabIndex = 17;
			labelUseNativeLibrary.Text = "Use Native Library";
			// 
			// FastSyncCheckBox
			// 
			FastSyncCheckBox.AutoSize = true;
			FastSyncCheckBox.Location = new System.Drawing.Point(390, 213);
			FastSyncCheckBox.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			FastSyncCheckBox.Name = "FastSyncCheckBox";
			FastSyncCheckBox.Size = new System.Drawing.Size(22, 21);
			FastSyncCheckBox.TabIndex = 18;
			FastSyncCheckBox.UseVisualStyleBackColor = true;
			// 
			// label1
			// 
			label1.AutoSize = true;
			label1.Location = new System.Drawing.Point(10, 213);
			label1.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			label1.Name = "label1";
			label1.Size = new System.Drawing.Size(234, 25);
			label1.TabIndex = 17;
			label1.Text = "Use Fast Sync (experimental)";
			// 
			// numericUpDownRetriesOnSyncError
			// 
			numericUpDownRetriesOnSyncError.Increment = new decimal(new int[] { 64, 0, 0, 0 });
			numericUpDownRetriesOnSyncError.Location = new System.Drawing.Point(390, 128);
			numericUpDownRetriesOnSyncError.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			numericUpDownRetriesOnSyncError.Maximum = new decimal(new int[] { 8192, 0, 0, 0 });
			numericUpDownRetriesOnSyncError.Name = "numericUpDownRetriesOnSyncError";
			numericUpDownRetriesOnSyncError.Size = new System.Drawing.Size(270, 31);
			numericUpDownRetriesOnSyncError.TabIndex = 14;
			// 
			// labelRetriesOnSyncError
			// 
			labelRetriesOnSyncError.AutoSize = true;
			labelRetriesOnSyncError.Location = new System.Drawing.Point(10, 132);
			labelRetriesOnSyncError.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			labelRetriesOnSyncError.Name = "labelRetriesOnSyncError";
			labelRetriesOnSyncError.Size = new System.Drawing.Size(177, 25);
			labelRetriesOnSyncError.TabIndex = 13;
			labelRetriesOnSyncError.Text = "Retries on sync error:";
			// 
			// numericUpDownSyncErrorRetryDelay
			// 
			numericUpDownSyncErrorRetryDelay.Increment = new decimal(new int[] { 64, 0, 0, 0 });
			numericUpDownSyncErrorRetryDelay.Location = new System.Drawing.Point(390, 170);
			numericUpDownSyncErrorRetryDelay.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			numericUpDownSyncErrorRetryDelay.Maximum = new decimal(new int[] { 8192, 0, 0, 0 });
			numericUpDownSyncErrorRetryDelay.Name = "numericUpDownSyncErrorRetryDelay";
			numericUpDownSyncErrorRetryDelay.Size = new System.Drawing.Size(270, 31);
			numericUpDownSyncErrorRetryDelay.TabIndex = 16;
			// 
			// labelSyncErrorRetryDelay
			// 
			labelSyncErrorRetryDelay.AutoSize = true;
			labelSyncErrorRetryDelay.Location = new System.Drawing.Point(10, 174);
			labelSyncErrorRetryDelay.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
			labelSyncErrorRetryDelay.Name = "labelSyncErrorRetryDelay";
			labelSyncErrorRetryDelay.Size = new System.Drawing.Size(222, 25);
			labelSyncErrorRetryDelay.TabIndex = 15;
			labelSyncErrorRetryDelay.Text = "Sync error retry delay (ms):";
			// 
			// PerforceSyncSettingsWindow
			// 
			AcceptButton = OkButton;
			AutoScaleDimensions = new System.Drawing.SizeF(144F, 144F);
			AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
			CancelButton = CancButton;
			ClientSize = new System.Drawing.Size(706, 426);
			ControlBox = false;
			Controls.Add(ResetButton);
			Controls.Add(CancButton);
			Controls.Add(OkButton);
			Controls.Add(groupBoxSyncing);
			Icon = (System.Drawing.Icon)resources.GetObject("$this.Icon");
			Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
			Name = "PerforceSyncSettingsWindow";
			ShowInTaskbar = false;
			StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			Text = "Perforce Sync Settings";
			Load += PerforceSettingsWindow_Load;
			((System.ComponentModel.ISupportInitialize)numericUpDownMaxSizePerBatch).EndInit();
			((System.ComponentModel.ISupportInitialize)numericUpDownMaxCommandsPerBatch).EndInit();
			groupBoxSyncing.ResumeLayout(false);
			groupBoxSyncing.PerformLayout();
			((System.ComponentModel.ISupportInitialize)numericUpDownRetriesOnSyncError).EndInit();
			((System.ComponentModel.ISupportInitialize)numericUpDownSyncErrorRetryDelay).EndInit();
			ResumeLayout(false);
		}

		#endregion
		private System.Windows.Forms.Button OkButton;
		private System.Windows.Forms.Button CancButton;
		private System.Windows.Forms.Button ResetButton;
		private System.Windows.Forms.Label labelMaxCommandsPerBatch;
		private System.Windows.Forms.Label labelMaxSizePerBatch;
		private System.Windows.Forms.NumericUpDown numericUpDownMaxSizePerBatch;
		private System.Windows.Forms.NumericUpDown numericUpDownMaxCommandsPerBatch;
		private System.Windows.Forms.GroupBox groupBoxSyncing;
		private System.Windows.Forms.NumericUpDown numericUpDownRetriesOnSyncError;
		private System.Windows.Forms.Label labelRetriesOnSyncError;
		private System.Windows.Forms.NumericUpDown numericUpDownSyncErrorRetryDelay;
		private System.Windows.Forms.Label labelSyncErrorRetryDelay;
		private System.Windows.Forms.CheckBox FastSyncCheckBox;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.CheckBox UseNativeLibraryCheckBox;
		private System.Windows.Forms.Label labelUseNativeLibrary;
	}
}