@echo off
setlocal

:: 현재 bat 파일이 위치한 경로로 이동
cd /d %~dp0

:: 설정 (상대경로 기준)
set PROTOC=C:\vcpkg\installed\x64-windows\tools\protobuf\protoc.exe
set PROTO_FILE=NetworkData.proto
set OUTPUT_DIR=.\Generated

set COPY_TARGET1=..\logic-server\logic-server
set COPY_TARGET2=..\ai-client\Assets\Scripts\Network

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
del /Q Generated\*

:: proto 파일 컴파일
echo [Compiling %PROTO_FILE% for c++...]
"%PROTOC%" --cpp_out="%OUTPUT_DIR%" "%PROTO_FILE%"
echo [C++ Compile Success]
echo [Compiling %PROTO_FILE% for c#...]
"%PROTOC%" --csharp_out="%OUTPUT_DIR%" "%PROTO_FILE%
echo [C# Compile Success]

:: 생성된 파일 이름 추출
for %%F in ("%PROTO_FILE%") do set FILE_BASE=%%~nF
set GENERATED_H=%OUTPUT_DIR%\%FILE_BASE%.pb.h
set GENERATED_CC=%OUTPUT_DIR%\%FILE_BASE%.pb.cc
set GENERATED_CS=%OUTPUT_DIR%\%FILE_BASE%.cs

:: 세 폴더로 복사
echo [C++ COPY]
echo [COPY to %COPY_TARGET1%]
copy /Y "%GENERATED_H%" "%COPY_TARGET1%"
copy /Y "%GENERATED_CC%" "%COPY_TARGET1%"

echo [C# COPY]
if not exist %COPY_TARGET2% mkdir echo [Not EXIST DIR: MKDIR...] "%COPY_TARGET%"
echo [COPY %GENERATED_CS% to %COPY_TARGET2%]
copy /Y "%GENERATED_CS%" "%COPY_TARGET2%"

echo [ALL Done!]
endlocal
pause
