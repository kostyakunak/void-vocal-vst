#!/usr/bin/env python3
"""
Проверка правильности работы HarmonicGlide (PsychoCore #2)
Проверяет корреляцию между RMS-огибающей и микро-сдвигом гармоник
"""

import sys
import numpy as np
from scipy.io import wavfile
from scipy import signal

def calculate_rms_envelope(audio_data, window_ms=10, sample_rate=44100):
    """
    Вычисляет RMS-огибающую аудио сигнала.
    """
    window_samples = int(window_ms * sample_rate / 1000)
    num_windows = len(audio_data) // window_samples
    
    rms_envelope = []
    for i in range(num_windows):
        start = i * window_samples
        end = start + window_samples
        window = audio_data[start:end]
        rms = np.sqrt(np.mean(window ** 2))
        rms_envelope.append(rms)
    
    return np.array(rms_envelope)

def estimate_pitch_shift(audio_data, sample_rate=44100, window_ms=50):
    """
    Оценивает изменения питча через autocorrelation.
    Возвращает массив изменений питча в центах.
    """
    window_samples = int(window_ms * sample_rate / 1000)
    num_windows = len(audio_data) // window_samples
    
    pitch_shifts = []
    previous_pitch = None
    
    for i in range(num_windows):
        start = i * window_samples
        end = start + window_samples
        window = audio_data[start:end]
        
        # Нормализуем
        window = window - np.mean(window)
        if np.std(window) == 0:
            pitch_shifts.append(0.0)
            continue
        
        # Autocorrelation для оценки основного тона
        autocorr = np.correlate(window, window, mode='full')
        autocorr = autocorr[len(autocorr)//2:]
        
        # Ищем пик (избегаем DC и низкие частоты)
        search_start = int(sample_rate / 2000)  # Минимум 2 кГц
        search_end = int(sample_rate / 80)      # Максимум 80 Гц
        
        if search_end > len(autocorr):
            search_end = len(autocorr)
        
        if search_start < search_end:
            peak_idx = np.argmax(autocorr[search_start:search_end]) + search_start
            period = peak_idx
            if period > 0:
                freq = sample_rate / period
            else:
                freq = 0
        else:
            freq = 0
        
        # Конвертируем частоту в центы (относительно предыдущего значения)
        if freq > 0 and previous_pitch is not None and previous_pitch > 0:
            # cents = 1200 * log2(f_new / f_old)
            cents = 1200 * np.log2(freq / previous_pitch)
            pitch_shifts.append(cents)
        else:
            pitch_shifts.append(0.0)
        
        if freq > 0:
            previous_pitch = freq
    
    return np.array(pitch_shifts)

def calculate_smoothness(values, threshold=5.0):
    """
    Проверяет плавность изменений (отсутствие резких скачков).
    Возвращает True, если максимальное изменение между соседними значениями < threshold.
    """
    if len(values) < 2:
        return True
    
    diffs = np.abs(np.diff(values))
    max_diff = np.max(diffs)
    
    return max_diff < threshold

def check_harmonic_glide(wav_file, sample_rate=44100):
    """
    Проверяет правильность работы HarmonicGlide.
    """
    try:
        # Читаем WAV файл
        sr, audio_data = wavfile.read(wav_file)
        sample_rate = sr
        
        # Проверяем, что это моно или стерео
        if len(audio_data.shape) == 2:
            # Стерео - берём среднее
            audio_data = np.mean(audio_data.astype(np.float32), axis=1)
        
        # Нормализуем в диапазон [-1, 1]
        if audio_data.dtype == np.int16:
            audio_data = audio_data.astype(np.float32) / 32768.0
        elif audio_data.dtype == np.int32:
            audio_data = audio_data.astype(np.float32) / 2147483648.0
        
        print(f"\n📊 Анализ HarmonicGlide: {wav_file}")
        print("=" * 60)
        
        # 1. Вычисляем RMS-огибающую
        print("1️⃣  Вычисление RMS-огибающей...")
        rms_envelope = calculate_rms_envelope(audio_data, window_ms=10, sample_rate=sample_rate)
        rms_delta = np.diff(rms_envelope)  # Изменения RMS
        
        print(f"   RMS-огибающая: {len(rms_envelope)} точек")
        print(f"   Диапазон RMS: {np.min(rms_envelope):.4f} - {np.max(rms_envelope):.4f}")
        
        # 2. Оцениваем изменения питча
        print("\n2️⃣  Оценка изменений питча...")
        pitch_shifts = estimate_pitch_shift(audio_data, sample_rate=sample_rate, window_ms=50)
        
        # Убираем первые значения (могут быть неточными)
        if len(pitch_shifts) > 10:
            pitch_shifts = pitch_shifts[5:]  # Пропускаем первые 5 окон
            rms_delta = rms_delta[5:] if len(rms_delta) > 5 else rms_delta
        
        # Обрезаем до одинаковой длины
        min_len = min(len(rms_delta), len(pitch_shifts))
        rms_delta = rms_delta[:min_len]
        pitch_shifts = pitch_shifts[:min_len]
        
        print(f"   Изменения питча: {len(pitch_shifts)} точек")
        print(f"   Диапазон сдвига: {np.min(pitch_shifts):.2f} - {np.max(pitch_shifts):.2f} центов")
        
        # 3. Проверяем корреляцию RMS → питч-шифт
        print("\n3️⃣  Проверка корреляции RMS → питч-шифт...")
        if len(rms_delta) > 1 and len(pitch_shifts) > 1:
            # Нормализуем для корректного вычисления корреляции
            rms_delta_norm = rms_delta - np.mean(rms_delta)
            pitch_shifts_norm = pitch_shifts - np.mean(pitch_shifts)
            
            if np.std(rms_delta_norm) > 0 and np.std(pitch_shifts_norm) > 0:
                correlation = np.corrcoef(rms_delta_norm, pitch_shifts_norm)[0, 1]
                
                # Ожидаем положительную корреляцию: при росте RMS → питч вверх
                expected_positive = correlation > 0.1  # Порог для положительной корреляции
                
                print(f"   Корреляция: {correlation:.3f}")
                print(f"   Ожидается: положительная (> 0.1)")
                print(f"   Статус: {'✅ PASS' if expected_positive else '⚠️  WEAK'}")
            else:
                correlation = 0.0
                expected_positive = False
                print("   ⚠️  Недостаточно вариаций для корреляции")
        else:
            correlation = 0.0
            expected_positive = False
            print("   ⚠️  Недостаточно данных для анализа")
        
        # 4. Проверяем плавность изменений
        print("\n4️⃣  Проверка плавности изменений...")
        smooth = calculate_smoothness(pitch_shifts, threshold=5.0)  # Макс 5 центов между окнами
        
        print(f"   Максимальное изменение между окнами: {np.max(np.abs(np.diff(pitch_shifts))):.2f} центов")
        print(f"   Порог: < 5.0 центов")
        print(f"   Статус: {'✅ PASS' if smooth else '❌ FAIL'}")
        
        # 5. Проверяем диапазон сдвига
        print("\n5️⃣  Проверка диапазона сдвига...")
        max_shift = np.max(np.abs(pitch_shifts))
        expected_range = (2.0, 3.0)  # Ожидаемый диапазон: ±2-3 цента
        
        in_range = max_shift >= expected_range[0] and max_shift <= expected_range[1] * 2  # Учитываем возможные выбросы
        
        print(f"   Максимальный сдвиг: {max_shift:.2f} центов")
        print(f"   Ожидаемый диапазон: ±{expected_range[0]}-{expected_range[1]} центов")
        print(f"   Статус: {'✅ PASS' if in_range else '⚠️  OUT OF RANGE'}")
        
        # Итоговый результат
        print("\n" + "=" * 60)
        print("📋 Итоговый результат:")
        
        all_passed = expected_positive and smooth and in_range
        
        print(f"   Корреляция RMS→питч: {'✅' if expected_positive else '⚠️'}")
        print(f"   Плавность изменений: {'✅' if smooth else '❌'}")
        print(f"   Диапазон сдвига: {'✅' if in_range else '⚠️'}")
        
        if all_passed:
            print("\n✅ HarmonicGlide работает корректно!")
        else:
            print("\n⚠️  HarmonicGlide может работать некорректно или эффект слишком слабый.")
            print("   Проверь настройки Energy и Flow в плагине.")
        
        return all_passed
        
    except Exception as e:
        print(f"❌ Ошибка при обработке {wav_file}: {e}")
        import traceback
        traceback.print_exc()
        return False

def main():
    if len(sys.argv) < 2:
        print("Использование: python3 check_harmonic_glide.py <wav_file>")
        print("\nПример:")
        print("  python3 check_harmonic_glide.py processed.wav")
        print("\nПримечание:")
        print("  - Файл должен быть обработан плагином с включённым HarmonicGlide")
        print("  - Рекомендуется: Energy > 50%, Flow > 50%, Mix = 50-100%")
        print("  - Вокал должен иметь заметные изменения громкости")
        sys.exit(1)
    
    wav_file = sys.argv[1]
    
    print("🔍 Проверка HarmonicGlide (PsychoCore #2)")
    print("=" * 60)
    
    result = check_harmonic_glide(wav_file)
    
    if result:
        sys.exit(0)
    else:
        sys.exit(1)

if __name__ == "__main__":
    main()

