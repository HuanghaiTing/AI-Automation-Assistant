using System;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Input;

namespace ModernUI.Uwp
{
    public sealed partial class MainWindow : Window
    {
        private Process? _backend;
        private StreamWriter? _backendInput;

        public MainWindow()
        {
            InitializeComponent();
            StartBackend();
            this.Closed += MainWindow_Closed;
        }

        private void StartBackend()
        {
            try
            {
                // 查找控制台后端 Project4.exe（相对 UI 输出目录向上找）
                string? exe = FindBackendExe();
                if (exe == null)
                {
                    AppendLog("[错误] 未找到 Project4.exe，请先构建 C++ 项目。\r\n");
                    return;
                }

                var psi = new ProcessStartInfo
                {
                    FileName = exe,
                    RedirectStandardInput = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    WorkingDirectory = Path.GetDirectoryName(exe)!
                };
                // 控制台用 UTF-8 输出
                psi.StandardOutputEncoding = System.Text.Encoding.UTF8;
                psi.StandardErrorEncoding = System.Text.Encoding.UTF8;

                _backend = new Process { StartInfo = psi, EnableRaisingEvents = true };
                _backend.Start();
                _backendInput = _backend.StandardInput;

                _ = Task.Run(() => PumpStream(_backend.StandardOutput));
                _ = Task.Run(() => PumpStream(_backend.StandardError));

                AppendLog($"[系统] 已启动后端: {exe}\r\n");
            }
            catch (Exception ex)
            {
                AppendLog($"[错误] 启动后端失败: {ex.Message}\r\n");
            }
        }

        private static string? FindBackendExe()
        {
            // 从 UI 的 bin 目录向上搜索解决方案下的 x64/Debug/Project4.exe 等
            string baseDir = AppContext.BaseDirectory;
            var dir = new DirectoryInfo(baseDir);
            for (int i = 0; i < 8 && dir != null; i++, dir = dir.Parent)
            {
                string[] candidates =
                {
                    Path.Combine(dir.FullName, "x64", "Debug", "Project4.exe"),
                    Path.Combine(dir.FullName, "x64", "Release", "Project4.exe"),
                    Path.Combine(dir.FullName, "Debug", "Project4.exe"),
                    Path.Combine(dir.FullName, "Release", "Project4.exe"),
                    Path.Combine(dir.FullName, "Project4.exe"),
                };
                foreach (var c in candidates)
                    if (File.Exists(c)) return c;
            }
            return null;
        }

        private async Task PumpStream(StreamReader reader)
        {
            char[] buffer = new char[1024];
            int n;
            while ((n = await reader.ReadAsync(buffer, 0, buffer.Length)) > 0)
            {
                string text = new string(buffer, 0, n);
                DispatcherQueue.TryEnqueue(() => AppendLog(text));
            }
        }

        private void AppendLog(string text)
        {
            LogText.Text += text;
            LogScroll.ChangeView(null, double.MaxValue, null);
        }

        private void SendCurrentInput()
        {
            string input = InputBox.Text.Trim();
            if (input.Length == 0) return;
            AppendLog($">>> {input}\r\n");
            try
            {
                _backendInput?.WriteLine(input);
                _backendInput?.Flush();
            }
            catch (Exception ex)
            {
                AppendLog($"[错误] 发送失败: {ex.Message}\r\n");
            }
            InputBox.Text = string.Empty;
        }

        private void SendButton_Click(object sender, RoutedEventArgs e) => SendCurrentInput();

        private void InputBox_KeyDown(object sender, KeyRoutedEventArgs e)
        {
            if (e.Key == Windows.System.VirtualKey.Enter)
            {
                SendCurrentInput();
                e.Handled = true;
            }
        }

        private void MainWindow_Closed(object sender, WindowEventArgs args)
        {
            try
            {
                if (_backend is { HasExited: false })
                    _backend.Kill();
            }
            catch { }
        }
    }
}
