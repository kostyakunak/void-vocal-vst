#!/bin/bash
# Скрипт для установки плагина в системные папки

set -e

echo "📦 Установка AudioPluginDemo..."

# Проверка наличия собранных плагинов
AU_PLUGIN="Build/AudioPluginDemo_artefacts/AU/AudioPluginDemo.component"
VST3_PLUGIN="Build/AudioPluginDemo_artefacts/VST3/AudioPluginDemo.vst3"

if [ ! -d "$AU_PLUGIN" ]; then
    echo "❌ Ошибка: AU плагин не найден. Запустите build.sh сначала."
    exit 1
fi

# Создание папок если нужно
mkdir -p ~/Library/Audio/Plug-Ins/Components
mkdir -p ~/Library/Audio/Plug-Ins/VST3

# Удаление старых версий
echo "🗑️  Удаление старых версий..."
rm -rf ~/Library/Audio/Plug-Ins/Components/AudioPluginDemo.component
rm -rf ~/Library/Audio/Plug-Ins/VST3/AudioPluginDemo.vst3

# Установка AU
echo "📦 Установка AU версии..."
cp -R "$AU_PLUGIN" ~/Library/Audio/Plug-Ins/Components/

# Установка VST3 (если есть)
if [ -d "$VST3_PLUGIN" ]; then
    echo "📦 Установка VST3 версии..."
    cp -R "$VST3_PLUGIN" ~/Library/Audio/Plug-Ins/VST3/
fi

echo ""
echo "✅ Плагин установлен!"
echo ""
echo "📝 Следующие шаги:"
echo "   1. Перезапустите Ableton Live"
echo "   2. Найдите AudioPluginDemo в разделе Plug-Ins"
echo "   3. Проверьте работу плагина"

