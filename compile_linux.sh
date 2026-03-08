#!/bin/bash
# VIZR — Linux Compile Script
# Run from inside your vizr/ project folder.
# Install deps first: sudo apt install g++ libsfml-dev

echo "Compiling VIZR..."

g++ -std=c++17 -O2 \
    vizr.cpp \
    imgui/imgui.cpp \
    imgui/imgui_draw.cpp \
    imgui/imgui_tables.cpp \
    imgui/imgui_widgets.cpp \
    imgui-sfml/imgui-SFML.cpp \
    -I./imgui \
    -I./imgui-sfml \
    $(pkg-config --cflags sfml-all 2>/dev/null || echo "-I./SFML/include") \
    $(pkg-config --libs   sfml-all 2>/dev/null || echo "-L./SFML/lib -lsfml-graphics -lsfml-audio -lsfml-window -lsfml-system") \
    -lGL \
    -o vizr

if [ $? -eq 0 ]; then
    echo ""
    echo " Build successful! Run with: ./vizr"
    echo " Or: ./vizr /path/to/song.wav"
else
    echo ""
    echo " Build FAILED. Check errors above."
    echo " Make sure you have: sudo apt install g++ libsfml-dev"
fi
