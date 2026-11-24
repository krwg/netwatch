@echo off
set "PATH=%USERPROFILE%\AppData\Roaming\Python\Python312\Scripts;%PATH%"
cd /d "%~dp0"
pio run -t upload %*
