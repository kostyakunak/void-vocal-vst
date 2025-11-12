#!/usr/bin/env python3
"""
Проверка моно-совместимости плагина VØID Engine
Этап 1: Проверка корреляции между L и R каналами (должна быть ≥ 0.6)
"""

import sys
import numpy as np
from scipy.io import wavfile
from scipy import signal

def calculate_correlation(left, right):
    """
    Вычисляет корреляцию между левым и правым каналами.
    Возвращает значение от -1 до 1, где 1 = идеальная корреляция.
    """
    # Убеждаемся, что массивы одинаковой длины
    min_len = min(len(left), len(right))
    left = left[:min_len]
    right = right[:min_len]
    
    # Нормализуем для корректного вычисления корреляции
    left = left - np.mean(left)
    right = right - np.mean(right)
    
    # Вычисляем корреляцию
    if np.std(left) == 0 or np.std(right) == 0:
        return 0.0
    
    correlation = np.corrcoef(left, right)[0, 1]
    return correlation

def check_mono_compatibility(wav_file):
    """
    Проверяет моно-совместимость стерео WAV файла.
    """
    try:
        # Читаем WAV файл
        sample_rate, audio_data = wavfile.read(wav_file)
        
        # Проверяем, что это стерео
        if len(audio_data.shape) != 2 or audio_data.shape[1] != 2:
            print(f"❌ Ошибка: {wav_file} не является стерео файлом")
            return False
        
        # Извлекаем каналы
        left_channel = audio_data[:, 0].astype(np.float32)
        right_channel = audio_data[:, 1].astype(np.float32)
        
        # Нормализуем в диапазон [-1, 1]
        if left_channel.dtype == np.int16:
            left_channel = left_channel / 32768.0
        if right_channel.dtype == np.int16:
            right_channel = right_channel / 32768.0
        
        # Вычисляем корреляцию
        correlation = calculate_correlation(left_channel, right_channel)
        
        # Проверяем порог (≥ 0.6)
        threshold = 0.6
        passed = correlation >= threshold
        
        # Выводим результат
        print(f"\n📊 Результаты проверки: {wav_file}")
        print(f"   Корреляция L/R: {correlation:.3f}")
        print(f"   Порог: ≥ {threshold}")
        print(f"   Статус: {'✅ PASS' if passed else '❌ FAIL'}")
        
        if not passed:
            print(f"   ⚠️  Корреляция ниже порога! Моно-совместимость может быть нарушена.")
        
        return passed
        
    except Exception as e:
        print(f"❌ Ошибка при обработке {wav_file}: {e}")
        return False

def main():
    if len(sys.argv) < 2:
        print("Использование: python3 check_mono_compatibility.py <wav_file1> [wav_file2] ...")
        print("\nПример:")
        print("  python3 check_mono_compatibility.py output.wav")
        print("  python3 check_mono_compatibility.py dry.wav wet.wav")
        sys.exit(1)
    
    files = sys.argv[1:]
    results = []
    
    print("🔍 Проверка моно-совместимости VØID Engine")
    print("=" * 60)
    
    for wav_file in files:
        result = check_mono_compatibility(wav_file)
        results.append((wav_file, result))
    
    # Итоговый результат
    print("\n" + "=" * 60)
    print("📋 Итоговый результат:")
    
    all_passed = all(result for _, result in results)
    
    for wav_file, result in results:
        status = "✅ PASS" if result else "❌ FAIL"
        print(f"   {status}: {wav_file}")
    
    if all_passed:
        print("\n✅ Все проверки пройдены! Моно-совместимость в порядке.")
        sys.exit(0)
    else:
        print("\n❌ Некоторые проверки не пройдены. Проверьте настройки плагина.")
        sys.exit(1)

if __name__ == "__main__":
    main()

