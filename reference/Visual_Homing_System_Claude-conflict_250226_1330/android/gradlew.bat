@rem Gradle wrapper script for Windows
@echo off
setlocal

set DIRNAME=%~dp0
set JAVA_EXE=java.exe

if defined JAVA_HOME (
    set JAVA_EXE=%JAVA_HOME%\bin\java.exe
)

set WRAPPER_JAR=%DIRNAME%gradle\wrapper\gradle-wrapper.jar

if not exist "%WRAPPER_JAR%" (
    echo Gradle wrapper JAR not found. Please sync project in Android Studio.
    exit /b 1
)

"%JAVA_EXE%" -jar "%WRAPPER_JAR%" %*
