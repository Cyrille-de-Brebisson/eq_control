    partial class SateliteTrack
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
            this.pictureBox2 = new System.Windows.Forms.PictureBox();
            this.checkBox17 = new System.Windows.Forms.CheckBox();
            this.label51 = new System.Windows.Forms.Label();
            this.textBox23 = new System.Windows.Forms.TextBox();
            this.checkBox14 = new System.Windows.Forms.CheckBox();
            this.button38 = new System.Windows.Forms.Button();
            this.labelVisible = new System.Windows.Forms.Label();
            this.labelErr = new System.Windows.Forms.Label();
            this.labelPos = new System.Windows.Forms.Label();
            this.labelCorrection = new System.Windows.Forms.Label();
            this.label1 = new System.Windows.Forms.Label();
            this.button1 = new System.Windows.Forms.Button();
            this.label2 = new System.Windows.Forms.Label();
            this.button2 = new System.Windows.Forms.Button();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox2)).BeginInit();
            this.SuspendLayout();
            // 
            // pictureBox2
            // 
            this.pictureBox2.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this.pictureBox2.Location = new System.Drawing.Point(0, 216);
            this.pictureBox2.Margin = new System.Windows.Forms.Padding(4, 4, 4, 4);
            this.pictureBox2.Name = "pictureBox2";
            this.pictureBox2.Size = new System.Drawing.Size(693, 424);
            this.pictureBox2.TabIndex = 59;
            this.pictureBox2.TabStop = false;
            // 
            // checkBox17
            // 
            this.checkBox17.AutoSize = true;
            this.checkBox17.BackColor = System.Drawing.SystemColors.WindowText;
            this.checkBox17.ForeColor = System.Drawing.SystemColors.Window;
            this.checkBox17.Location = new System.Drawing.Point(352, 53);
            this.checkBox17.Margin = new System.Windows.Forms.Padding(4, 2, 4, 2);
            this.checkBox17.Name = "checkBox17";
            this.checkBox17.Size = new System.Drawing.Size(129, 29);
            this.checkBox17.TabIndex = 61;
            this.checkBox17.Text = "5mn alarm";
            this.checkBox17.UseVisualStyleBackColor = false;
            this.checkBox17.KeyDown += new System.Windows.Forms.KeyEventHandler(this.FrmMain_KeyDown);
            this.checkBox17.KeyUp += new System.Windows.Forms.KeyEventHandler(this.textBox22_KeyUp);
            // 
            // label51
            // 
            this.label51.AutoSize = true;
            this.label51.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Underline, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label51.ForeColor = System.Drawing.Color.White;
            this.label51.Location = new System.Drawing.Point(258, 17);
            this.label51.Margin = new System.Windows.Forms.Padding(5, 0, 5, 0);
            this.label51.Name = "label51";
            this.label51.Size = new System.Drawing.Size(105, 25);
            this.label51.TabIndex = 59;
            this.label51.Text = "NY2O Site";
            this.label51.Click += new System.EventHandler(this.label51_Click);
            // 
            // textBox23
            // 
            this.textBox23.Location = new System.Drawing.Point(117, 17);
            this.textBox23.Margin = new System.Windows.Forms.Padding(4, 2, 4, 2);
            this.textBox23.Name = "textBox23";
            this.textBox23.Size = new System.Drawing.Size(131, 29);
            this.textBox23.TabIndex = 58;
            this.textBox23.KeyDown += new System.Windows.Forms.KeyEventHandler(this.FrmMain_KeyDown);
            this.textBox23.KeyUp += new System.Windows.Forms.KeyEventHandler(this.textBox22_KeyUp);
            // 
            // checkBox14
            // 
            this.checkBox14.AutoSize = true;
            this.checkBox14.BackColor = System.Drawing.SystemColors.WindowText;
            this.checkBox14.ForeColor = System.Drawing.SystemColors.Window;
            this.checkBox14.Location = new System.Drawing.Point(22, 53);
            this.checkBox14.Margin = new System.Windows.Forms.Padding(4, 2, 4, 2);
            this.checkBox14.Name = "checkBox14";
            this.checkBox14.Size = new System.Drawing.Size(199, 29);
            this.checkBox14.TabIndex = 58;
            this.checkBox14.Text = "Track when visible";
            this.checkBox14.UseVisualStyleBackColor = false;
            this.checkBox14.KeyDown += new System.Windows.Forms.KeyEventHandler(this.FrmMain_KeyDown);
            this.checkBox14.KeyUp += new System.Windows.Forms.KeyEventHandler(this.textBox22_KeyUp);
            // 
            // button38
            // 
            this.button38.BackColor = System.Drawing.SystemColors.ActiveCaptionText;
            this.button38.ForeColor = System.Drawing.Color.White;
            this.button38.Location = new System.Drawing.Point(494, 11);
            this.button38.Margin = new System.Windows.Forms.Padding(4, 2, 4, 2);
            this.button38.Name = "button38";
            this.button38.Size = new System.Drawing.Size(172, 37);
            this.button38.TabIndex = 58;
            this.button38.Text = "Recalc passes";
            this.button38.UseVisualStyleBackColor = false;
            this.button38.Click += new System.EventHandler(this.button38_Click);
            this.button38.MouseDown += new System.Windows.Forms.MouseEventHandler(this.button38_MouseDown);
            // 
            // labelVisible
            // 
            this.labelVisible.AutoSize = true;
            this.labelVisible.ForeColor = System.Drawing.Color.White;
            this.labelVisible.Location = new System.Drawing.Point(15, 86);
            this.labelVisible.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.labelVisible.Name = "labelVisible";
            this.labelVisible.Size = new System.Drawing.Size(78, 25);
            this.labelVisible.TabIndex = 60;
            this.labelVisible.Text = "visibility";
            // 
            // labelErr
            // 
            this.labelErr.AutoSize = true;
            this.labelErr.ForeColor = System.Drawing.Color.White;
            this.labelErr.Location = new System.Drawing.Point(15, 138);
            this.labelErr.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.labelErr.Name = "labelErr";
            this.labelErr.Size = new System.Drawing.Size(35, 25);
            this.labelErr.TabIndex = 59;
            this.labelErr.Text = "err";
            // 
            // labelPos
            // 
            this.labelPos.AutoSize = true;
            this.labelPos.ForeColor = System.Drawing.Color.White;
            this.labelPos.Location = new System.Drawing.Point(15, 110);
            this.labelPos.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.labelPos.Name = "labelPos";
            this.labelPos.Size = new System.Drawing.Size(44, 25);
            this.labelPos.TabIndex = 48;
            this.labelPos.Text = "pos";
            // 
            // labelCorrection
            // 
            this.labelCorrection.AutoSize = true;
            this.labelCorrection.ForeColor = System.Drawing.Color.White;
            this.labelCorrection.Location = new System.Drawing.Point(15, 164);
            this.labelCorrection.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.labelCorrection.Name = "labelCorrection";
            this.labelCorrection.Size = new System.Drawing.Size(112, 25);
            this.labelCorrection.TabIndex = 62;
            this.labelCorrection.Text = "Corrections";
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.ForeColor = System.Drawing.Color.White;
            this.label1.Location = new System.Drawing.Point(17, 20);
            this.label1.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(93, 25);
            this.label1.TabIndex = 63;
            this.label1.Text = "Satelite #";
            // 
            // button1
            // 
            this.button1.BackColor = System.Drawing.SystemColors.ActiveCaptionText;
            this.button1.ForeColor = System.Drawing.Color.White;
            this.button1.Location = new System.Drawing.Point(376, 11);
            this.button1.Margin = new System.Windows.Forms.Padding(4, 2, 4, 2);
            this.button1.Name = "button1";
            this.button1.Size = new System.Drawing.Size(95, 37);
            this.button1.TabIndex = 64;
            this.button1.Text = "GetTLE";
            this.button1.UseVisualStyleBackColor = false;
            this.button1.Click += new System.EventHandler(this.button1_Click);
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.ForeColor = System.Drawing.Color.White;
            this.label2.Location = new System.Drawing.Point(15, 188);
            this.label2.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(306, 25);
            this.label2.TabIndex = 65;
            this.label2.Text = "Press up/down/right/left to correct.";
            // 
            // button2
            // 
            this.button2.BackColor = System.Drawing.SystemColors.ActiveCaptionText;
            this.button2.ForeColor = System.Drawing.Color.White;
            this.button2.Location = new System.Drawing.Point(494, 53);
            this.button2.Margin = new System.Windows.Forms.Padding(4, 2, 4, 2);
            this.button2.Name = "button2";
            this.button2.Size = new System.Drawing.Size(172, 37);
            this.button2.TabIndex = 66;
            this.button2.Text = "GoTo Start";
            this.button2.UseVisualStyleBackColor = false;
            this.button2.Click += new System.EventHandler(this.button2_Click);
            // 
            // SateliteTrack
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(11F, 24F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.Black;
            this.ClientSize = new System.Drawing.Size(691, 630);
            this.Controls.Add(this.button2);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.button1);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.labelCorrection);
            this.Controls.Add(this.labelVisible);
            this.Controls.Add(this.checkBox14);
            this.Controls.Add(this.checkBox17);
            this.Controls.Add(this.button38);
            this.Controls.Add(this.label51);
            this.Controls.Add(this.labelErr);
            this.Controls.Add(this.textBox23);
            this.Controls.Add(this.labelPos);
            this.Controls.Add(this.pictureBox2);
            this.Margin = new System.Windows.Forms.Padding(5, 6, 5, 6);
            this.MaximumSize = new System.Drawing.Size(715, 694);
            this.MinimumSize = new System.Drawing.Size(715, 504);
            this.Name = "SateliteTrack";
            this.Text = "ISS Visual track";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.MyForm_FormClosing);
            this.KeyDown += new System.Windows.Forms.KeyEventHandler(this.FrmMain_KeyDown);
            this.KeyUp += new System.Windows.Forms.KeyEventHandler(this.textBox22_KeyUp);
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox2)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        public System.Windows.Forms.PictureBox pictureBox2;
    private System.Windows.Forms.CheckBox checkBox17;
    private System.Windows.Forms.Label label51;
    private System.Windows.Forms.TextBox textBox23;
    private System.Windows.Forms.CheckBox checkBox14;
    private System.Windows.Forms.Button button38;
    private System.Windows.Forms.Label labelVisible;
    private System.Windows.Forms.Label labelErr;
    private System.Windows.Forms.Label labelPos;
    private System.Windows.Forms.Label labelCorrection;
    private System.Windows.Forms.Label label1;
    private System.Windows.Forms.Button button1;
    private System.Windows.Forms.Label label2;
    private System.Windows.Forms.Button button2;
}
