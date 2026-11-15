# ASCII Art Renderer PowerShell Launcher
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
& "$PSScriptRoot\build\ascii-art.exe" @args
