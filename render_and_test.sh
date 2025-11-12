#!/bin/bash
# Скрипт для офлайн-рендеринга и автоматического тестирования

set -e

if [ $# -lt 1 ]; then
    echo "Использование: ./render_and_test.sh <input.wav> [параметры]"
    echo ""
    echo "Примеры:"
    echo "  ./render_and_test.sh input.wav"
    echo "  ./render_and_test.sh input.wav flow=0.5,energy=0.7,mix=0.5"
    echo ""
    echo "Параметры (опционально, через запятую):"
    echo "  flow=0.5      - Скорость движения (0.0-1.0)"
    echo "  energy=0.7   - Сила движения (0.0-1.0)"
    echo "  mix=0.5      - Mix (0.0-1.0)"
    echo "  depth=0.5    - Depth (0.0-1.0)"
    echo "  ghost=0.3    - Ghost (0.0-1.0)"
    echo "  clarity=0.0  - Clarity (-0.5-0.5)"
    exit 1
fi

INPUT_FILE="$1"
PARAMS="${2:-}"

# Проверяем наличие входного файла
if [ ! -f "$INPUT_FILE" ]; then
    echo "❌ Ошибка: Файл $INPUT_FILE не найден"
    exit 1
fi

# Создаём имена выходных файлов
BASENAME=$(basename "$INPUT_FILE" .wav)
OUTPUT_DIR="test_output"
mkdir -p "$OUTPUT_DIR"

OUTPUT_FILE="$OUTPUT_DIR/${BASENAME}_processed.wav"
DRY_FILE="$OUTPUT_DIR/${BASENAME}_dry.wav"
WET_FILE="$OUTPUT_DIR/${BASENAME}_wet.wav"

echo "🎵 Офлайн-рендеринг и тестирование VØID Engine"
echo "============================================================"

# 1. Рендерим dry версию (Mix=0%)
echo ""
echo "1️⃣  Рендеринг dry версии (Mix=0%)..."
./Build/offline_render "$INPUT_FILE" "$DRY_FILE" "mix=0.0"

# 2. Рендерим wet версию
echo ""
echo "2️⃣  Рендеринг wet версии..."
if [ -n "$PARAMS" ]; then
    ./Build/offline_render "$INPUT_FILE" "$WET_FILE" "$PARAMS"
else
    # По умолчанию: Mix=50%, Energy=70%, Flow=50%
    ./Build/offline_render "$INPUT_FILE" "$WET_FILE" "mix=0.5,energy=0.7,flow=0.5"
fi

# 3. Рендерим полную версию для HarmonicGlide проверки
echo ""
echo "3️⃣  Рендеринг полной версии для HarmonicGlide..."
if [ -n "$PARAMS" ]; then
    ./Build/offline_render "$INPUT_FILE" "$OUTPUT_FILE" "$PARAMS"
else
    ./Build/offline_render "$INPUT_FILE" "$OUTPUT_FILE" "mix=1.0,energy=0.8,flow=0.6"
fi

# 4. Запускаем проверки
echo ""
echo "4️⃣  Запуск проверок качества..."
echo ""

# Активируем виртуальное окружение если есть
if [ -d "venv" ]; then
    source venv/bin/activate
fi

# Проверка моно-совместимости
echo "📊 Проверка моно-совместимости..."
python3 check_mono_compatibility.py "$WET_FILE"
MONO_RESULT=$?

echo ""

# Проверка LUFS
echo "📊 Проверка LUFS..."
python3 check_lufs.py "$DRY_FILE" "$WET_FILE"
LUFS_RESULT=$?

echo ""

# Проверка HarmonicGlide
echo "📊 Проверка HarmonicGlide..."
python3 check_harmonic_glide.py "$OUTPUT_FILE"
GLIDE_RESULT=$?

# Итоговый результат
echo ""
echo "============================================================"
echo "📋 Итоговые результаты:"
echo ""

if [ $MONO_RESULT -eq 0 ]; then
    echo "   ✅ Моно-совместимость: PASS"
else
    echo "   ❌ Моно-совместимость: FAIL"
fi

if [ $LUFS_RESULT -eq 0 ]; then
    echo "   ✅ LUFS: PASS"
else
    echo "   ❌ LUFS: FAIL"
fi

if [ $GLIDE_RESULT -eq 0 ]; then
    echo "   ✅ HarmonicGlide: PASS"
else
    echo "   ⚠️  HarmonicGlide: WEAK или FAIL"
fi

echo ""
echo "📁 Обработанные файлы сохранены в: $OUTPUT_DIR/"

if [ $MONO_RESULT -eq 0 ] && [ $LUFS_RESULT -eq 0 ]; then
    echo ""
    echo "✅ Все основные проверки пройдены!"
    exit 0
else
    echo ""
    echo "⚠️  Некоторые проверки не пройдены. Проверь настройки плагина."
    exit 1
fi

