# Быстрый старт - VoidEngine

## 🚀 Работа с проектом

### 1. Сборка плагина:

```bash
cd ~/Desktop/VoidEngine
./build.sh
```

Или вручную:
```bash
cmake -B Build -G "Unix Makefiles" -DJUCE_DIR=/Users/kostakunak/Downloads/JUCE-master
cmake --build Build --config Release --target AudioPluginDemo_AU
```

### 2. Установка плагина:

```bash
./install.sh
```

Или вручную:
```bash
cp -R Build/AudioPluginDemo_artefacts/AU/AudioPluginDemo.component \
  ~/Library/Audio/Plug-Ins/Components/
```

### 3. Использование в Ableton:

1. Перезапустите Ableton Live
2. Найдите "AudioPluginDemo" в разделе Plug-Ins
3. Добавьте на аудио-трек
4. Проверьте параметр Mix (при Mix=0% звук должен быть идентичен dry)

## 📁 Структура проекта

- **Source/** - исходные файлы плагина
- **Build/** - результаты сборки (не коммитить в git)
- **Documentation/** - документация и заметки

## 🔄 Рабочий процесс

1. Редактируйте файлы в `Source/`
2. Запустите `./build.sh` для сборки
3. Запустите `./install.sh` для установки
4. Проверьте в Ableton Live

## 📝 Редактирование кода

Основные файлы:
- `Source/PluginProcessor.h/cpp` - обработка аудио
- `Source/PluginEditor.h/cpp` - GUI плагина

