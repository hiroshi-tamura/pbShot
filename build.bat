@echo off
setlocal
set PATH=C:\Qt\6.9.1\mingw_64\bin;C:\gcc\mingw64\bin;%PATH%
if not exist build mkdir build
cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release .. || goto :err
cmake --build . --config Release -- -j4 || goto :err
echo [OK] build succeeded
goto :eof
:err
echo [ERR] build failed
exit /b 1
