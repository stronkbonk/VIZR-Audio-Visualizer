@echo off
:: VIZR — Windows Compile Script (MinGW-w64)
:: Run this from inside your vizr/ project folder.

echo Compiling VIZR...

g++ -std=c++17 -O2 ^
    vizr.cpp ^
    imgui/imgui.cpp ^
    imgui/imgui_draw.cpp ^
    imgui/imgui_tables.cpp ^
    imgui/imgui_widgets.cpp ^
    imgui-sfml/imgui-SFML.cpp ^
    -I./imgui ^
    -I./imgui-sfml ^
    -I./SFML/include ^
    -L./SFML/lib ^
    -lsfml-graphics -lsfml-audio -lsfml-window -lsfml-system ^
    -lopengl32 -lwinmm -lgdi32 ^
    -mwindows ^
    -o vizr.exe

if %ERRORLEVEL% == 0 (
    echo.
    echo  Build successful! vizr.exe is ready.
    echo  Make sure these DLLs are in the same folder as vizr.exe:
    echo    sfml-graphics-2.dll
    echo    sfml-audio-2.dll
    echo    sfml-window-2.dll
    echo    sfml-system-2.dll
    echo    openal32.dll
    echo  They are in SFML\bin\
) else (
    echo.
    echo  Build FAILED. Check the errors above.
)
pause
