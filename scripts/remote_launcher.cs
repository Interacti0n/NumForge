using System;
using System.Diagnostics;
using System.Text;

// Built as a Windows GUI executable so no console flashes before PowerShell starts.
internal static class RemoteLauncher
{
    private static string Quote(string value)
    {
        var text = new StringBuilder("\"");
        int slashes = 0;
        foreach (char c in value)
        {
            if (c == '\\') { ++slashes; continue; }
            text.Append('\\', c == '"' ? 2 * slashes + 1 : slashes);
            text.Append(c);
            slashes = 0;
        }
        return text.Append('\\', 2 * slashes).Append('"').ToString();
    }

    private static int Main(string[] args)
    {
        try
        {
            var command = new StringBuilder();
            foreach (string arg in args) command.Append(Quote(arg)).Append(' ');
            var settings = new ProcessStartInfo
            {
                FileName = Environment.GetEnvironmentVariable("SystemRoot") +
                    (Environment.Is64BitOperatingSystem && !Environment.Is64BitProcess ? "\\Sysnative" : "\\System32") +
                    "\\WindowsPowerShell\\v1.0\\powershell.exe",
                Arguments = command.ToString(),
                UseShellExecute = false,
                CreateNoWindow = true
            };
            using (var process = Process.Start(settings))
            {
                process.WaitForExit();
                return process.ExitCode;
            }
        }
        catch { return 1; }
    }
}
