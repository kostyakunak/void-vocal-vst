# VoidEngine - VØID Engine Audio Plugin

Проект для разработки аудио-плагина VoidEngine на основе JUCE.

## 📁 Структура проекта

```
VoidEngine/
├── Source/              # Исходные файлы плагина
│   ├── PluginProcessor.h/cpp
│   └── PluginEditor.h/cpp
├── Build/               # Результаты сборки (build/)
├── Documentation/       # Документация и заметки
├── CMakeLists.txt       # Конфигурация CMake
└── README.md           # Этот файл
```

## 🚀 Быстрый старт

### Сборка проекта:

```bash
cd ~/Desktop/VoidEngine

# Конфигурация
cmake -B Build -G "Unix Makefiles" \
  -DJUCE_DIR=/Users/kostakunak/Downloads/JUCE-master

# Компиляция
cmake --build Build --config Release --target AudioPluginDemo_AU
```

### Установка плагина:

```bash
# AU версия
cp -R Build/AudioPluginDemo_artefacts/AU/AudioPluginDemo.component \
  ~/Library/Audio/Plug-Ins/Components/

# VST3 версия
cmake --build Build --config Release --target AudioPluginDemo_VST3
cp -R Build/AudioPluginDemo_artefacts/VST3/AudioPluginDemo.vst3 \
  ~/Library/Audio/Plug-Ins/VST3/
```

## 📝 Текущий этап

**Этап 1:** ✅ Добавлен параметр Mix
- При Mix=0% звук проходит без изменений (bit-perfect dry)
- При Mix>0% реализована базовая логика dry/wet mix

## 🎯 Следующие этапы

1. Добавить остальные параметры (Flow, Melt, Ghost, Depth, Clarity, Gravity, Energy, Output)
2. Добавить заглушки модулей (Granular, Spectral, Space, Dynamics, Motion)
3. Постепенно реализовывать обработку

## 📚 Документация

Смотрите файлы в папке `Documentation/`:
- `main.md` - полная концепция проекта с оглавлением (1559 строк)
- `AI_WORKING_RULES.md` - правила работы с ИИ для эффективной разработки
- `SOUND_CONTRACTS.md` - звуковые контракты и метрики качества (автоматическая проверка)
- `STAGE_1.md` - описание этапа 1
- `CURRENT_STATUS.md` - текущий статус проекта
- `PHASE_0_CHECKLIST.md` - чеклист этапа 0
- `QUICK_START.md` - быстрый старт
- `PROJECT_STRUCTURE.md` - структура проекта

## 🔧 Требования

- JUCE framework (путь: `/Users/kostakunak/Downloads/JUCE-master`)
- CMake 3.22+
- Xcode Command Line Tools или полный Xcode

