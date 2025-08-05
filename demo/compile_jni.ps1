# PowerShell script to compile JNI library
Write-Host "Compiling LLaMA JNI wrapper..."

# Set paths
$JAVA_HOME = "C:\Program Files\Java\jdk-17"
$LLAMA_PATH = "C:\Users\Volodymyr_Prudnikov\source\repos\LLAama\llama.cpp"

# Initialize Visual Studio environment in this session
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35211\bin\Hostx64\x64"
$env:INCLUDE += ";C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35211\include"
$env:LIB += ";C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35211\lib\x64"

# Set up Windows SDK paths
$env:INCLUDE += ";C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt"
$env:INCLUDE += ";C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\shared"
$env:INCLUDE += ";C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um"
$env:LIB += ";C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64"
$env:LIB += ";C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"

# Compile the JNI library
$compilerArgs = @(
    "/LD",
    "/I`"$JAVA_HOME\include`"",
    "/I`"$JAVA_HOME\include\win32`"",
    "/I`"$LLAMA_PATH\include`"",
    "/I`"$LLAMA_PATH`"",
    "/I`"$LLAMA_PATH\ggml\include`"",
    "llama_jni.c",
    "/link",
    "`"$LLAMA_PATH\build\src\Release\llama.lib`"",
    "`"$LLAMA_PATH\build\ggml\src\Release\ggml.lib`"",
    "/OUT:llama_jni.dll"
)

try {
    & "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35211\bin\Hostx64\x64\cl.exe" @compilerArgs
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Compilation successful! llama_jni.dll created."
        
        # Copy DLL to resources folder
        if (Test-Path "llama_jni.dll") {
            Copy-Item "llama_jni.dll" "src\main\resources\" -Force
            Write-Host "DLL copied to resources folder."
        }
    } else {
        Write-Host "Compilation failed!"
        exit 1
    }
} catch {
    Write-Host "Error running compiler: $_"
    exit 1
}

Write-Host "Done!"
