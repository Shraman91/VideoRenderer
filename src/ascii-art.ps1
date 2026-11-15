# ASCII Art Renderer Launcher from src/
$root = (Get-Item "$PSScriptRoot\..").FullName
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
& "$root\build\ascii-art.exe" @args
