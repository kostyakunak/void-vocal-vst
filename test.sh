#!/bin/bash
# Скрипт для запуска тестов VØID Engine

set -e

echo "🧪 Запуск тестов VØID Engine..."
echo ""

# Путь к JUCE
JUCE_DIR="/Users/kostakunak/Downloads/JUCE-master"

# Проверка наличия JUCE
if [ ! -d "$JUCE_DIR" ]; then
    echo "❌ Ошибка: JUCE не найден в $JUCE_DIR"
    exit 1
fi

# Конфигурация CMake
echo "📝 Конфигурация CMake..."
cmake -B Build -G "Unix Makefiles" \
  -DJUCE_DIR="$JUCE_DIR"

# Компиляция теста
echo "🔨 Компиляция теста..."
cmake --build Build --config Release --target test_basic

# Запуск теста
echo ""
echo "▶️  Запуск тестов..."
echo ""
./Build/test_basic

echo ""
echo "✅ Тесты завершены!"

