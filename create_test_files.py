#!/usr/bin/env python3
"""
Создание тестовых WAV файлов для проверки скриптов
"""

import numpy as np
from scipy.io import wavfile
import os

def create_test_mono_file(filename, duration_sec=2.0, sample_rate=44100):
    """Создаёт тестовый стерео файл для проверки моно-совместимости"""
    num_samples = int(duration_sec * sample_rate)
    t = np.linspace(0, duration_sec, num_samples)
    
    # Создаём сигнал с небольшой разницей между каналами (для проверки корреляции)
    freq = 440.0  # A4
    left = np.sin(2 * np.pi * freq * t) * 0.5
    right = np.sin(2 * np.pi * freq * t + 0.1) * 0.5  # Небольшой фазовый сдвиг
    
    # Объединяем в стерео
    stereo = np.column_stack([left, right])
    
    # Конвертируем в int16
    audio_int16 = (stereo * 32767).astype(np.int16)
    
    wavfile.write(filename, sample_rate, audio_int16)
    print(f"✅ Создан: {filename}")

def create_test_dry_wet_files(dry_filename, wet_filename, duration_sec=2.0, sample_rate=44100):
    """Создаёт тестовые dry и wet файлы для проверки LUFS"""
    num_samples = int(duration_sec * sample_rate)
    t = np.linspace(0, duration_sec, num_samples)
    
    # Dry файл (чистый сигнал)
    freq = 440.0
    dry_signal = np.sin(2 * np.pi * freq * t) * 0.5
    dry_stereo = np.column_stack([dry_signal, dry_signal])
    dry_int16 = (dry_stereo * 32767).astype(np.int16)
    wavfile.write(dry_filename, sample_rate, dry_int16)
    print(f"✅ Создан: {dry_filename}")
    
    # Wet файл (немного тише для имитации обработки)
    wet_signal = dry_signal * 0.95  # На 0.5 дБ тише
    wet_stereo = np.column_stack([wet_signal, wet_signal])
    wet_int16 = (wet_stereo * 32767).astype(np.int16)
    wavfile.write(wet_filename, sample_rate, wet_int16)
    print(f"✅ Создан: {wet_filename}")

def create_test_harmonic_glide_file(filename, duration_sec=3.0, sample_rate=44100):
    """Создаёт тестовый файл с изменяющейся громкостью для проверки HarmonicGlide"""
    num_samples = int(duration_sec * sample_rate)
    t = np.linspace(0, duration_sec, num_samples)
    
    # Создаём сигнал с изменяющейся громкостью (имитация RMS изменений)
    freq = 440.0
    envelope = 0.3 + 0.3 * np.sin(2 * np.pi * 0.5 * t)  # Медленная модуляция громкости
    signal = np.sin(2 * np.pi * freq * t) * envelope
    
    # Добавляем небольшой питч-шифт, коррелированный с громкостью (имитация HarmonicGlide)
    # При росте громкости → питч чуть выше
    pitch_modulation = (envelope - 0.3) * 0.01  # ±0.01 от изменения envelope
    signal_shifted = np.sin(2 * np.pi * freq * (1 + pitch_modulation) * t) * envelope
    
    # Стерео
    stereo = np.column_stack([signal_shifted, signal_shifted])
    audio_int16 = (stereo * 32767).astype(np.int16)
    
    wavfile.write(filename, sample_rate, audio_int16)
    print(f"✅ Создан: {filename}")

if __name__ == "__main__":
    print("🔨 Создание тестовых WAV файлов...")
    print("=" * 60)
    
    # Создаём тестовые файлы
    create_test_mono_file("test_mono.wav")
    create_test_dry_wet_files("test_dry.wav", "test_wet.wav")
    create_test_harmonic_glide_file("test_harmonic_glide.wav")
    
    print("\n✅ Все тестовые файлы созданы!")
    print("\nПримечание: Это упрощённые тестовые файлы.")
    print("Для реальной проверки нужны файлы, обработанные плагином в DAW.")

