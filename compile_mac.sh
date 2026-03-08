#!/bin/bash
# VIZR — macOS Compile Script
# Run from inside your vizr/ project folder.
# Install deps first: brew install sfml

echo "Compiling VIZR..."

clang++ -std=c++17 -O2 \
    vizr.cpp \
    imgui/imgui.cpp \
    imgui/imgui_draw.cpp \
    imgui/imgui_tables.cpp \
    imgui/imgui_widgets.cpp \
    imgui-sfml/imgui-SFML.cpp \
    -I./imgui \
    -I./imgui-sfml \
    $(pkg-config --cflags sfml-all 2>/dev/null || echo "-I/usr/local/include") \
    $(pkg-config --libs   sfml-all 2>/dev/null || echo "-L/usr/local/lib -lsfml-graphics -lsfml-audio -lsfml-window -lsfml-system") \
    -framework OpenGL \
    -o vizr

if [ $? -eq 0 ]; then
    echo ""
    echo " Build successful! Run with: ./vizr"
else
    echo ""
    echo " Build FAILED. Check errors above."
    echo " Make sure you have: brew install sfml"
fi
