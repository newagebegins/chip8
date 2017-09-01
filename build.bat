@echo off
set compilerFlags=/nologo /Od /Z7 /FC /W4
if not exist build mkdir build
pushd build
cl %compilerFlags% ..\main.c /link /INCREMENTAL:NO /SUBSYSTEM:WINDOWS ole32.lib user32.lib gdi32.lib
popd
