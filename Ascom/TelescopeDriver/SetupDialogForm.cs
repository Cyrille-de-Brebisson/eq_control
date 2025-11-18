using ASCOM.LocalServer;
using ASCOM.Utilities;
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace ASCOM.BrebissonV1.Telescope
{
    [ComVisible(false)] // Form not registered for COM!
    public partial class SetupDialogForm : Form
    {
        public SetupDialogForm()
        {
            InitializeComponent();
        }
        private void CmdOK_Click(object sender, EventArgs e) // OK button event handler
        {
            Close();
        }

        private void BrowseToAscom(object sender, EventArgs e) // Click on ASCOM logo event handler
        {
            try { System.Diagnostics.Process.Start("https://ascom-standards.org/"); }
            catch (Win32Exception noBrowser) { if (noBrowser.ErrorCode == -2147467259) MessageBox.Show(noBrowser.Message); }
            catch (Exception other) { MessageBox.Show(other.Message); }
        }

        private void SetupDialogForm_Load(object sender, EventArgs e)
        {
            // Bring the setup dialogue to the front of the screen
            if (WindowState == FormWindowState.Minimized) WindowState = FormWindowState.Normal;
            else {
                TopMost = true;
                Focus();
                BringToFront();
                TopMost = false;
            }
        }
    }
}