#!/bin/bash
# Скрипт для сборки VoidEngine

set -e

echo "🔨 Сборка VoidEngine..."

# Путь к JUCE
JUCE_DIR="/Users/kostakunak/Downloads/JUCE-master"

# Проверка наличия JUCE
if [ ! -d "$JUCE_DIR" ]; then
    echo "❌ Ошибка: JUCE не найден в $JUCE_DIR"
    exit 1
fi

# Конфигурация
echo "📝 Конфигурация CMake..."
cmake -B Build -G "Unix Makefiles" \
  -DJUCE_DIR="$JUCE_DIR"

# Компиляция AU версии
echo "🔨 Компиляция AU версии..."
cmake --build Build --config Release --target AudioPluginDemo_AU

# Компиляция VST3 версии
echo "🔨 Компиляция VST3 версии..."
cmake --build Build --config Release --target AudioPluginDemo_VST3

echo ""
echo "✅ Сборка завершена!"
echo ""
echo "📦 Установка плагинов:"
echo "   AU:   Build/AudioPluginDemo_artefacts/AU/AudioPluginDemo.component"
echo "   VST3: Build/AudioPluginDemo_artefacts/VST3/AudioPluginDemo.vst3"

