@echo off
setlocal
where bash >nul 2>nul
if errorlevel 1 (
  echo ERROR: Bash was not found. Install MSYS2 and run this from its shell, or add usr\bin\bash.exe to PATH.
  exit /b 1
)
bash build-msys2.sh
exit /b %errorlevel%
