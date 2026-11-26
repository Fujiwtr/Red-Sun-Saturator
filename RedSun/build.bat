@echo off
REM Builds Red Sun.vst3 with Visual Studio 2022. Run from "x64 Native Tools Command Prompt" or any prompt with CMake + Git on PATH.
cmake -B build -G "Visual Studio 17 2022" -A x64 || goto :fail
cmake --build build --config Release --parallel || goto :fail
echo.
echo Done: build\RedSun_artefacts\Release\VST3\Red Sun.vst3
echo Copy that folder to C:\Program Files\Common Files\VST3\
goto :eof
:fail
echo Build failed. Copy the error text and send it back for a fix.
exit /b 1
