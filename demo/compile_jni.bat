@echo off
setlocal

echo Compiling LLaMA JNI wrapper...

REM Set paths (adjust these to match your system)
set JAVA_HOME=C:\Program Files\Java\jdk-17
set LLAMA_PATH=C:\Users\Volodymyr_Prudnikov\source\repos\LLAama\llama.cpp
set MSVC_PATH="C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\14.40.33807\bin\Hostx64\x64"

REM Initialize Visual Studio environment
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"

REM Compile the JNI library
cl.exe /LD ^
    /I"%JAVA_HOME%\include" ^
    /I"%JAVA_HOME%\include\win32" ^
    /I"%LLAMA_PATH%\include" ^
    /I"%LLAMA_PATH%" ^
    /I"%LLAMA_PATH%\ggml\include" ^
    llama_jni.c ^
    /link ^
    "%LLAMA_PATH%\build\src\Release\llama.lib" ^
    "%LLAMA_PATH%\build\ggml\src\Release\ggml.lib" ^
    /OUT:llama_jni.dll

if %ERRORLEVEL% EQU 0 (
    echo Compilation successful! llama_jni.dll created.
    
    REM Copy DLL to resources folder
    copy llama_jni.dll src\main\resources\
    if %ERRORLEVEL% EQU 0 (
        echo DLL copied to resources folder.
    ) else (
        echo Failed to copy DLL to resources folder.
    )
) else (
    echo Compilation failed!
    exit /b 1
)

echo Done!
pause
