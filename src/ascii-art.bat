@echo off
setlocal
set "PATH=C:\msys64\mingw64\bin;%PATH%"
"%~dp0..\build\ascii-art.exe" %*
