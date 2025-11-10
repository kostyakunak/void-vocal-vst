#!/bin/bash
# Безопасный скрипт для добавления файлов в Git
# Предотвращает случайное добавление больших или ненужных файлов

set -e

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "🔍 Безопасное добавление файлов в Git..."

# Проверяем, что мы в корне репозитория
if [ ! -d .git ]; then
    echo -e "${RED}❌ Ошибка: не найден .git директорий${NC}"
    exit 1
fi

# Если указаны аргументы, добавляем только их
if [ $# -gt 0 ]; then
    echo -e "${GREEN}✅ Добавляем указанные файлы/папки: $@${NC}"
    git add "$@"
else
    # Безопасное добавление только файлов проекта
    echo -e "${YELLOW}⚠️  Добавляем только файлы проекта (Source/, Documentation/, CMakeLists.txt, etc.)${NC}"
    
    # Добавляем только нужные директории и файлы
    git add Source/ Documentation/ CMakeLists.txt .gitignore build.sh install.sh test.sh 2>/dev/null || true
    
    # Добавляем тесты, если есть
    [ -d tests ] && git add tests/ 2>/dev/null || true
    
    # Добавляем README, если есть
    [ -f README.md ] && git add README.md 2>/dev/null || true
fi

# Показываем статус
echo ""
echo -e "${GREEN}📊 Статус изменений:${NC}"
git status --short

# Проверяем размер файлов
echo ""
echo -e "${YELLOW}🔍 Проверка размера файлов...${NC}"
staged_files=$(git diff --cached --name-only)
total_size=0
large_files=()

for file in $staged_files; do
    if [ -f "$file" ]; then
        file_size=$(stat -f%z "$file" 2>/dev/null || stat -c%s "$file" 2>/dev/null)
        total_size=$((total_size + file_size))
        
        # Файлы больше 5MB
        if [ "$file_size" -gt 5242880 ]; then
            file_size_mb=$((file_size / 1048576))
            large_files+=("$file (${file_size_mb}MB)")
        fi
    fi
done

if [ ${#large_files[@]} -gt 0 ]; then
    echo -e "${YELLOW}⚠️  Обнаружены большие файлы:${NC}"
    for file in "${large_files[@]}"; do
        echo "   - $file"
    done
    echo -e "${YELLOW}   Убедитесь, что эти файлы нужны в репозитории${NC}"
fi

total_size_mb=$((total_size / 1048576))
echo -e "${GREEN}✅ Общий размер: ${total_size_mb}MB${NC}"

if [ "$total_size_mb" -gt 50 ]; then
    echo -e "${RED}⚠️  ВНИМАНИЕ: Размер изменений превышает 50MB!${NC}"
    read -p "Продолжить? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Отменено. Используйте 'git reset' для отмены."
        exit 1
    fi
fi

echo ""
echo -e "${GREEN}✅ Готово! Файлы добавлены. Используйте 'git commit' для коммита.${NC}"

