#!/usr/bin/env python3
"""
Проверка LUFS (Loudness Units relative to Full Scale) для плагина VØID Engine
Этап 1: Проверка, что LUFS dry ≈ wet (±1 дБ) при Mix=50%
"""

import sys
import numpy as np
from scipy.io import wavfile

def calculate_lufs(audio_data, sample_rate):
    """
    Вычисляет LUFS (Loudness Units relative to Full Scale) используя упрощённый алгоритм.
    Это приблизительная оценка, для точных измерений нужна библиотека pyloudnorm.
    """
    # Нормализуем в диапазон [-1, 1]
    if audio_data.dtype == np.int16:
        audio_data = audio_data.astype(np.float32) / 32768.0
    elif audio_data.dtype == np.int32:
        audio_data = audio_data.astype(np.float32) / 2147483648.0
    
    # Упрощённый расчёт LUFS через RMS
    # Реальная формула LUFS сложнее (K-weighting, gating), но для проверки достаточно RMS
    rms = np.sqrt(np.mean(audio_data ** 2))
    
    # Конвертируем RMS в дБ
    if rms > 0:
        db = 20 * np.log10(rms)
    else:
        db = -np.inf
    
    # LUFS ≈ dB - 23 (приблизительная константа для K-weighting)
    lufs = db - 23.0
    
    return lufs

def check_lufs_compatibility(dry_file, wet_file, tolerance_db=1.0):
    """
    Проверяет, что LUFS dry ≈ wet в пределах tolerance_db.
    """
    try:
        # Читаем dry файл
        sample_rate_dry, audio_dry = wavfile.read(dry_file)
        if len(audio_dry.shape) == 2:
            # Стерео - берём среднее
            audio_dry = np.mean(audio_dry.astype(np.float32), axis=1)
        
        # Читаем wet файл
        sample_rate_wet, audio_wet = wavfile.read(wet_file)
        if len(audio_wet.shape) == 2:
            # Стерео - берём среднее
            audio_wet = np.mean(audio_wet.astype(np.float32), axis=1)
        
        # Вычисляем LUFS
        lufs_dry = calculate_lufs(audio_dry, sample_rate_dry)
        lufs_wet = calculate_lufs(audio_wet, sample_rate_wet)
        
        # Разница в дБ
        diff_db = abs(lufs_wet - lufs_dry)
        
        # Проверяем порог
        passed = diff_db <= tolerance_db
        
        # Выводим результат
        print(f"\n📊 Результаты проверки LUFS:")
        print(f"   Dry файл: {dry_file}")
        print(f"   Wet файл: {wet_file}")
        print(f"   LUFS dry:  {lufs_dry:.2f} LUFS")
        print(f"   LUFS wet:  {lufs_wet:.2f} LUFS")
        print(f"   Разница:   {diff_db:.2f} дБ")
        print(f"   Порог:     ±{tolerance_db} дБ")
        print(f"   Статус:    {'✅ PASS' if passed else '❌ FAIL'}")
        
        if not passed:
            print(f"   ⚠️  Разница превышает порог! Громкость изменяется слишком сильно.")
        
        return passed
        
    except Exception as e:
        print(f"❌ Ошибка при обработке файлов: {e}")
        return False

def main():
    if len(sys.argv) < 3:
        print("Использование: python3 check_lufs.py <dry_wav_file> <wet_wav_file> [tolerance_db]")
        print("\nПример:")
        print("  python3 check_lufs.py dry.wav wet.wav")
        print("  python3 check_lufs.py dry.wav wet.wav 1.0")
        print("\nПримечание: Для точных измерений установите pyloudnorm:")
        print("  pip install pyloudnorm")
        sys.exit(1)
    
    dry_file = sys.argv[1]
    wet_file = sys.argv[2]
    tolerance_db = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
    
    print("🔍 Проверка LUFS для VØID Engine")
    print("=" * 60)
    
    result = check_lufs_compatibility(dry_file, wet_file, tolerance_db)
    
    print("\n" + "=" * 60)
    if result:
        print("✅ Проверка пройдена! Громкость сохраняется.")
        sys.exit(0)
    else:
        print("❌ Проверка не пройдена. Проверьте настройки Mix и Output.")
        sys.exit(1)

if __name__ == "__main__":
    main()

