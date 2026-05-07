@echo off
set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "VITASDK=%ROOT%\tools\vitasdk\vitasdk"
set "PATH=%VITASDK%\bin;%ROOT%\tools\ninja;%PATH%"
echo VITASDK=%VITASDK%
arm-vita-eabi-gcc --version | more +0
ninja --version
