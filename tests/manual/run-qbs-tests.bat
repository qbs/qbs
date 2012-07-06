@echo off

:: Usage
:: run-qbs-tests.bat qbs debug release profile:x64 platform:clang

for /D %%i in (%CD%\*) do (
    cmd /C "cd %%i && cmd /C %*"
)
