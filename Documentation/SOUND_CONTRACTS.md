# 🎵 Звуковые контракты и метрики качества VØID Engine

Документ описывает систему формализации звуковых требований через измеримые метрики для автоматической проверки качества ИИ-разработки.

---

## 📋 Оглавление

- [Концепция звуковых контрактов](#концепция-звуковых-контрактов)
- [Автоматические метрики](#автоматические-метрики)
- [Канонический офлайн-пайплайн](#канонический-офлайн-пайплайн)
- [Шаблоны промптов для ИИ](#шаблоны-промптов-для-ии)
- [Чеклист качества](#чеклист-качества)
- [Интеграция с этапами разработки](#интеграция-с-этапами-разработки)
- [Типичные проблемы и решения](#типичные-проблемы-и-решения)

---

## 🎯 Концепция звуковых контрактов

### Что такое звуковой контракт

**Звуковой контракт** — это формализация вайба в измеримые условия. Вместо "звучит хорошо" мы определяем конкретные метрики, которые можно проверить автоматически.

### Пример: Glass Scene — "свет переливается внутри"

**Звуковой контракт:**

| Метрика | Целевое значение | Проверка |
|---------|------------------|----------|
| **Область воздействия** | Только 5–12 кГц (±0.5 кГц по краям) | Спектр до/после: изменения < 0.5 дБ вне полосы |
| **Бинауральная задержка** | 0.05–0.15 мс RMS (не больше 0.2 мс) | Статистика L/R задержек в полосе 5–12 кГц |
| **Частота "дыхания"** | 0.03–0.1 Гц (медленная модуляция) | FFT модуляции: пик в диапазоне 0.03–0.1 Гц |
| **Моно-совместимость** | Корреляция L/R ≥ 0.6 | Пирсоновская корреляция (L+R)/2 |
| **Уровень "воздуха"** | Спектральный центроид +5% (макс) | Изменение центроида 5–12 кГц |
| **Сибилянты** | 6–9 кГц: рост ≤ +2 дБ к dry | Энергия в полосе 6–9 кГц |
| **Громкость** | LUFS отличие < 1 дБ от dry | Integrated LUFS (EBU R128) |

**Как использовать:**
1. Отдаёшь ИИ контракт вместе с задачей
2. ИИ генерирует код модуля + код проверки
3. Автоматический тест проверяет все метрики
4. Отчёт с PASS/FAIL по каждой метрике

---

## 📊 Автоматические метрики

### Базовые метрики (без лицензий, Python: numpy/scipy/librosa)

#### 1. Диапазон частот воздействия

**Что проверяем:** Изменения спектра только в целевой полосе

```python
import numpy as np
from scipy import signal

def check_frequency_band(spec_dry, spec_wet, target_band=(5000, 12000), 
                         tolerance_db=0.5, sample_rate=44100):
    """
    Проверяет, что изменения > tolerance_db только в target_band.
    
    Returns:
        (pass, max_deviation_outside_band, max_deviation_inside_band)
    """
    freqs = np.fft.rfftfreq(len(spec_dry), 1/sample_rate)
    
    # Вне полосы
    mask_outside = (freqs < target_band[0]) | (freqs > target_band[1])
    deviation_outside = np.abs(spec_wet[mask_outside] - spec_dry[mask_outside])
    max_outside = np.max(deviation_outside)
    
    # Внутри полосы
    mask_inside = (freqs >= target_band[0]) & (freqs <= target_band[1])
    deviation_inside = np.abs(spec_wet[mask_inside] - spec_dry[mask_inside])
    max_inside = np.max(deviation_inside)
    
    pass_check = max_outside < tolerance_db
    return pass_check, max_outside, max_inside
```

#### 2. IACC (Inter-Aural Cross-Correlation) — бинауральная "движуха"

**Что проверяем:** Межушная корреляция в целевой полосе

```python
def calculate_iacc(left_ch, right_ch, target_band=(5000, 12000), 
                   sample_rate=44100, window_ms=50):
    """
    Считает IACC в полосе для каждого окна.
    
    Returns:
        iacc_array: массив IACC по времени
        mean_iacc: средний IACC
    """
    # Фильтр в полосе
    sos = signal.butter(4, target_band, btype='band', 
                       fs=sample_rate, output='sos')
    left_filtered = signal.sosfilt(sos, left_ch)
    right_filtered = signal.sosfilt(sos, right_ch)
    
    # Корреляция по окнам
    window_samples = int(window_ms * sample_rate / 1000)
    iacc_values = []
    
    for i in range(0, len(left_filtered) - window_samples, window_samples):
        l_window = left_filtered[i:i+window_samples]
        r_window = right_filtered[i:i+window_samples]
        
        # Нормализация
        l_norm = (l_window - np.mean(l_window)) / (np.std(l_window) + 1e-10)
        r_norm = (r_window - np.mean(r_window)) / (np.std(r_window) + 1e-10)
        
        # Корреляция
        corr = np.corrcoef(l_norm, r_norm)[0, 1]
        iacc_values.append(corr)
    
    return np.array(iacc_values), np.mean(iacc_values)
```

**Целевые значения:**
- **Iceberg/Glass:** 0.1–0.4 (ниже — рассыпается, выше — нет эффекта движения)
- **Platina:** 0.3–0.6 (более стабильно, но с движением)

#### 3. IPD/ITD модуляция (фаза и задержка)

**Что проверяем:** Статистика сдвига фазы/задержки L↔R с нужной частотой модуляции

```python
def analyze_binaural_modulation(left_ch, right_ch, target_band=(5000, 12000),
                                lfo_range=(0.03, 0.1), sample_rate=44100):
    """
    Анализирует бинауральную модуляцию в полосе.
    
    Returns:
        lfo_detected: обнаружена ли модуляция в диапазоне lfo_range
        lfo_frequency: частота модуляции (Гц)
        delay_rms: RMS задержка (мс)
        delay_max: максимальная задержка (мс)
    """
    # Фильтр в полосе
    sos = signal.butter(4, target_band, btype='band', 
                       fs=sample_rate, output='sos')
    left_f = signal.sosfilt(sos, left_ch)
    right_f = signal.sosfilt(sos, right_ch)
    
    # Кросс-корреляция для определения задержки
    window_ms = 50
    window_samples = int(window_ms * sample_rate / 1000)
    delays = []
    
    for i in range(0, len(left_f) - window_samples, window_samples):
        l_win = left_f[i:i+window_samples]
        r_win = right_f[i:i+window_samples]
        
        # Корреляция со сдвигом
        corr = signal.correlate(l_win, r_win, mode='full')
        delay_idx = np.argmax(corr) - len(l_win)
        delay_ms = delay_idx * 1000 / sample_rate
        delays.append(delay_ms)
    
    delays = np.array(delays)
    
    # Анализ модуляции (FFT огибающей задержек)
    delay_envelope = np.abs(signal.hilbert(delays))
    fft = np.fft.rfft(delay_envelope)
    freqs = np.fft.rfftfreq(len(delay_envelope), window_ms/1000)
    
    # Поиск пика в диапазоне LFO
    mask = (freqs >= lfo_range[0]) & (freqs <= lfo_range[1])
    if np.any(mask):
        peak_idx = np.argmax(np.abs(fft[mask]))
        lfo_freq = freqs[mask][peak_idx]
        lfo_detected = np.abs(fft[mask][peak_idx]) > threshold  # threshold настроить
    else:
        lfo_freq = 0
        lfo_detected = False
    
    delay_rms = np.sqrt(np.mean(delays**2)) * 1000  # в мс
    delay_max = np.max(np.abs(delays)) * 1000
    
    return lfo_detected, lfo_freq, delay_rms, delay_max
```

**Целевые значения:**
- **Iceberg:** задержка 0.05–0.15 мс RMS, LFO 0.03–0.1 Гц
- **Glass:** задержка 0.05–0.15 мс RMS, LFO 0.03–0.1 Гц

#### 4. Моно-совместимость

**Что проверяем:** Корреляция L/R всего сигнала

```python
def check_mono_compatibility(left_ch, right_ch, threshold=0.6):
    """
    Проверяет моно-совместимость.
    
    Returns:
        (pass, correlation_value)
    """
    # Моно-сумма
    mono = (left_ch + right_ch) / 2
    
    # Корреляция L и R
    corr = np.corrcoef(left_ch, right_ch)[0, 1]
    
    return corr >= threshold, corr
```

**Целевое значение:** ≥ 0.6 для всех сцен

#### 5. LUFS (EBU R128) — громкость

**Что проверяем:** Изменение громкости не должно маскировать эффект

```python
import pyloudnorm as pyln

def check_lufs_difference(dry_audio, wet_audio, sample_rate=44100, 
                          max_diff_db=1.0):
    """
    Проверяет, что LUFS отличается не более чем на max_diff_db.
    
    Returns:
        (pass, dry_lufs, wet_lufs, difference_db)
    """
    meter = pyln.Meter(sample_rate)
    
    dry_lufs = meter.integrated_loudness(dry_audio)
    wet_lufs = meter.integrated_loudness(wet_audio)
    
    diff_db = abs(wet_lufs - dry_lufs)
    
    return diff_db < max_diff_db, dry_lufs, wet_lufs, diff_db
```

**Целевое значение:** < 1 дБ для всех сцен

#### 6. Сибилянты (6–9 кГц)

**Что проверяем:** Энергия сибилянтов не должна расти слишком сильно

```python
def check_sibilants(dry_audio, wet_audio, sample_rate=44100, 
                    band=(6000, 9000), max_increase_db=2.0):
    """
    Проверяет рост энергии в полосе сибилянтов.
    
    Returns:
        (pass, dry_energy_db, wet_energy_db, increase_db)
    """
    # Полосовой фильтр
    sos = signal.butter(4, band, btype='band', fs=sample_rate, output='sos')
    
    dry_filtered = signal.sosfilt(sos, dry_audio)
    wet_filtered = signal.sosfilt(sos, wet_audio)
    
    # Энергия (RMS)
    dry_energy = np.sqrt(np.mean(dry_filtered**2))
    wet_energy = np.sqrt(np.mean(wet_filtered**2))
    
    dry_energy_db = 20 * np.log10(dry_energy + 1e-10)
    wet_energy_db = 20 * np.log10(wet_energy + 1e-10)
    
    increase_db = wet_energy_db - dry_energy_db
    
    return increase_db <= max_increase_db, dry_energy_db, wet_energy_db, increase_db
```

**Целевое значение:** ≤ +2 дБ для Glass/Iceberg

#### 7. Харш-фактор (резкость)

**Что проверяем:** Наклон спектра 3–10 кГц (резкость по Цвикеру)

```python
def calculate_sharpness(audio, sample_rate=44100):
    """
    Упрощённая оценка резкости (sharpness) по наклону спектра 3–10 кГц.
    
    Returns:
        sharpness_value: относительная резкость (0–1)
    """
    # FFT
    fft = np.fft.rfft(audio)
    freqs = np.fft.rfftfreq(len(audio), 1/sample_rate)
    magnitude = np.abs(fft)
    
    # Полоса 3–10 кГц
    mask = (freqs >= 3000) & (freqs <= 10000)
    band_magnitude = magnitude[mask]
    band_freqs = freqs[mask]
    
    # Наклон (линейная регрессия)
    if len(band_magnitude) > 1:
        slope = np.polyfit(band_freqs, 20*np.log10(band_magnitude + 1e-10), 1)[0]
        # Нормализация к 0–1 (примерно)
        sharpness = np.clip((slope + 10) / 20, 0, 1)
    else:
        sharpness = 0
    
    return sharpness
```

**Целевое значение:** Не должно расти более чем на 0.2 для Glass/Crystals

#### 8. Клипы/клики

**Что проверяем:** Отсутствие перегрузок и артефактов

```python
def check_clips_and_clicks(audio, max_level_db=0.0, crest_threshold=20.0):
    """
    Проверяет клипы и клики.
    
    Returns:
        (has_clips, has_clicks, crest_factor)
    """
    # Клипы (> 0 dBFS)
    max_sample = np.max(np.abs(audio))
    max_db = 20 * np.log10(max_sample + 1e-10)
    has_clips = max_db > max_level_db
    
    # Crest factor (отношение пика к RMS)
    rms = np.sqrt(np.mean(audio**2))
    peak = np.max(np.abs(audio))
    crest_factor = peak / (rms + 1e-10)
    crest_factor_db = 20 * np.log10(crest_factor)
    
    # Детект кликов (резкие изменения)
    diff = np.diff(audio)
    diff_db = 20 * np.log10(np.abs(diff) + 1e-10)
    has_clicks = np.any(diff_db > crest_threshold)
    
    return has_clips, has_clicks, crest_factor_db
```

**Целевое значение:** Нет клипов, crest factor < 20 дБ

### Специфичные метрики для психоакустических кирпичей

#### Harmonic Glide (Platina)

**Что проверяем:** Корреляция между RMS-энвелопой и микродетюном гармоник

```python
def analyze_harmonic_glide(audio, sample_rate=44100):
    """
    Анализирует Harmonic Glide: корреляция RMS-энвелопы и микродетюна.
    
    Returns:
        (correlation, glide_smoothness, max_cent_shift)
    """
    # RMS-энвелопа
    window_ms = 10
    window_samples = int(window_ms * sample_rate / 1000)
    rms_envelope = []
    
    for i in range(0, len(audio) - window_samples, window_samples):
        window = audio[i:i+window_samples]
        rms = np.sqrt(np.mean(window**2))
        rms_envelope.append(rms)
    
    rms_envelope = np.array(rms_envelope)
    
    # Анализ гармоник (упрощённо: pitch tracking)
    # Используем autocorrelation для определения основного тона
    autocorr = np.correlate(audio, audio, mode='full')
    autocorr = autocorr[len(autocorr)//2:]
    
    # Поиск пика (основной тон)
    peak_idx = np.argmax(autocorr[100:]) + 100  # избегаем DC
    fundamental_period = peak_idx
    fundamental_freq = sample_rate / fundamental_period
    
    # Микродетюн (изменение фазы гармоник)
    # Упрощённо: смотрим изменение фазы в окнах
    phase_shifts = []
    for i in range(0, len(audio) - window_samples, window_samples):
        window = audio[i:i+window_samples]
        fft = np.fft.fft(window)
        phase = np.angle(fft)
        
        # Фаза основной гармоники
        harmonic_idx = int(fundamental_freq * len(window) / sample_rate)
        if harmonic_idx < len(phase):
            phase_shifts.append(phase[harmonic_idx])
    
    phase_shifts = np.array(phase_shifts)
    phase_diff = np.diff(phase_shifts)  # изменение фазы
    
    # Конвертация в центы
    cent_shifts = phase_diff * 1200 / (2 * np.pi)
    
    # Корреляция RMS-энвелопы и детюна
    if len(rms_envelope) == len(cent_shifts) + 1:
        rms_trimmed = rms_envelope[:-1]
        correlation = np.corrcoef(rms_trimmed, cent_shifts)[0, 1]
    else:
        correlation = 0
    
    # Плавность (без резких рывков)
    glide_smoothness = 1.0 - np.std(np.diff(cent_shifts)) / (np.abs(np.mean(cent_shifts)) + 1e-10)
    max_cent_shift = np.max(np.abs(cent_shifts))
    
    return correlation, glide_smoothness, max_cent_shift
```

**Целевые значения:**
- Корреляция > 0.3 (положительная связь)
- Плавность > 0.7 (без рывков)
- Максимальный сдвиг ≤ 3 цента

#### Temporal Ease (Opiate)

**Что проверяем:** Увеличение времени затухания огибающей на ~150 мс

```python
def analyze_temporal_ease(dry_audio, wet_audio, sample_rate=44100, 
                          expected_increase_ms=150, tolerance_ms=50):
    """
    Анализирует Temporal Ease: увеличение времени затухания.
    
    Returns:
        (pass, dry_decay_ms, wet_decay_ms, increase_ms)
    """
    def find_decay_time(audio, threshold_db=-60):
        """
        Находит время затухания до threshold_db от пика.
        """
        # Энвелопа (RMS)
        window_ms = 10
        window_samples = int(window_ms * sample_rate / 1000)
        envelope = []
        
        for i in range(0, len(audio) - window_samples, window_samples):
            window = audio[i:i+window_samples]
            rms = np.sqrt(np.mean(window**2))
            envelope.append(rms)
        
        envelope = np.array(envelope)
        envelope_db = 20 * np.log10(envelope + 1e-10)
        
        # Пик
        peak_idx = np.argmax(envelope_db)
        peak_db = envelope_db[peak_idx]
        
        # Затухание до threshold
        target_db = peak_db + threshold_db
        
        # Ищем от пика к концу
        decay_samples = None
        for i in range(peak_idx, len(envelope_db)):
            if envelope_db[i] <= target_db:
                decay_samples = i - peak_idx
                break
        
        if decay_samples is None:
            decay_samples = len(envelope_db) - peak_idx
        
        decay_ms = decay_samples * window_ms
        
        return decay_ms, peak_db
    
    dry_decay, dry_peak = find_decay_time(dry_audio)
    wet_decay, wet_peak = find_decay_time(wet_audio)
    
    increase_ms = wet_decay - dry_decay
    
    # Проверка: увеличение в пределах expected ± tolerance
    target_min = expected_increase_ms - tolerance_ms
    target_max = expected_increase_ms + tolerance_ms
    
    pass_check = target_min <= increase_ms <= target_max
    
    return pass_check, dry_decay, wet_decay, increase_ms
```

**Целевые значения:**
- Увеличение 100–200 мс (150 мс ± 50 мс)
- Начало фразы неизменно

---

## 🔄 Канонический офлайн-пайплайн

### Структура тестового раннера

```
tests/
├── test_runner.py          # Главный скрипт
├── metrics.py              # Все метрики
├── renderer.py             # Офлайн-рендеринг через JUCE
├── contracts/              # Контракты для каждой сцены
│   ├── iceberg.json
│   ├── glass.json
│   └── ...
├── test_samples/           # Эталонные фразы
│   ├── male_sustained.wav
│   ├── female_whisper.wav
│   ├── fast_phrase.wav
│   ├── low_note.wav
│   └── sibilant_test.wav
└── reports/                # Генерируемые отчёты
    ├── report_20250115.html
    ├── ab_comparison.wav
    └── plots/
```

### Шаблон test_runner.py

```python
#!/usr/bin/env python3
"""
Тестовый раннер для проверки звуковых контрактов VØID Engine.
"""

import json
import numpy as np
import soundfile as sf
from pathlib import Path
from metrics import *
from renderer import render_vst3_offline
from contracts import load_contract

def run_contract_test(scene_name, contract_file, test_samples, output_dir):
    """
    Запускает тесты для одной сцены по контракту.
    
    Args:
        scene_name: название сцены (Iceberg, Glass, etc.)
        contract_file: путь к JSON контракту
        test_samples: список путей к тестовым файлам
        output_dir: директория для отчётов
    """
    # Загружаем контракт
    contract = load_contract(contract_file)
    
    results = {
        'scene': scene_name,
        'contract': contract,
        'tests': []
    }
    
    # Рендерим каждый тестовый файл
    for sample_path in test_samples:
        print(f"Processing {sample_path}...")
        
        # Загружаем dry
        dry_audio, sr = sf.read(sample_path)
        if len(dry_audio.shape) == 1:
            dry_audio = np.column_stack([dry_audio, dry_audio])
        
        # Рендерим wet через VST3
        wet_audio = render_vst3_offline(
            sample_path,
            preset=scene_name,
            parameters=contract.get('default_parameters', {})
        )
        
        # Проверяем все метрики из контракта
        test_result = {
            'sample': Path(sample_path).name,
            'metrics': {}
        }
        
        for metric_name, metric_config in contract['metrics'].items():
            metric_func = globals()[f"check_{metric_name}"]
            pass_result, *values = metric_func(
                dry_audio, wet_audio, sr, **metric_config
            )
            
            test_result['metrics'][metric_name] = {
                'pass': pass_result,
                'values': values,
                'target': metric_config.get('target', None)
            }
        
        results['tests'].append(test_result)
    
    # Генерируем отчёт
    generate_report(results, output_dir)
    
    return results

def generate_report(results, output_dir):
    """
    Генерирует HTML отчёт с таблицами, графиками и A/B WAV.
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(exist_ok=True)
    
    # HTML шаблон
    html = f"""
    <!DOCTYPE html>
    <html>
    <head>
        <title>VØID Engine Test Report - {results['scene']}</title>
        <style>
            body {{ font-family: monospace; margin: 20px; }}
            table {{ border-collapse: collapse; width: 100%; }}
            th, td {{ border: 1px solid #ddd; padding: 8px; text-align: left; }}
            th {{ background-color: #f2f2f2; }}
            .pass {{ color: green; }}
            .fail {{ color: red; }}
        </style>
    </head>
    <body>
        <h1>VØID Engine Test Report</h1>
        <h2>Scene: {results['scene']}</h2>
        
        <h3>Contract Summary</h3>
        <pre>{json.dumps(results['contract'], indent=2)}</pre>
        
        <h3>Test Results</h3>
        <table>
            <tr>
                <th>Sample</th>
                <th>Metric</th>
                <th>Status</th>
                <th>Value</th>
                <th>Target</th>
            </tr>
    """
    
    for test in results['tests']:
        for metric_name, metric_result in test['metrics'].items():
            status_class = 'pass' if metric_result['pass'] else 'fail'
            status_text = '✅ PASS' if metric_result['pass'] else '❌ FAIL'
            
            html += f"""
            <tr>
                <td>{test['sample']}</td>
                <td>{metric_name}</td>
                <td class="{status_class}">{status_text}</td>
                <td>{metric_result['values']}</td>
                <td>{metric_result.get('target', 'N/A')}</td>
            </tr>
            """
    
    html += """
        </table>
    </body>
    </html>
    """
    
    # Сохраняем
    report_path = output_dir / f"report_{results['scene']}.html"
    report_path.write_text(html)
    
    print(f"Report saved to {report_path}")

if __name__ == "__main__":
    # Конфигурация
    scenes = {
        'Iceberg': 'contracts/iceberg.json',
        'Glass': 'contracts/glass.json',
        'Platina': 'contracts/platina.json',
    }
    
    test_samples = [
        'test_samples/male_sustained.wav',
        'test_samples/female_whisper.wav',
        'test_samples/fast_phrase.wav',
        'test_samples/low_note.wav',
        'test_samples/sibilant_test.wav',
    ]
    
    output_dir = Path('reports')
    
    # Запускаем тесты для каждой сцены
    for scene_name, contract_file in scenes.items():
        print(f"\n{'='*60}")
        print(f"Testing scene: {scene_name}")
        print(f"{'='*60}\n")
        
        results = run_contract_test(
            scene_name, contract_file, test_samples, output_dir
        )
        
        # Сводка
        total_tests = sum(len(t['metrics']) for t in results['tests'])
        passed = sum(
            sum(1 for m in t['metrics'].values() if m['pass'])
            for t in results['tests']
        )
        
        print(f"\n{scene_name}: {passed}/{total_tests} tests passed")
```

### Пример контракта (JSON)

```json
{
  "scene": "Glass",
  "description": "Свет переливается внутри",
  "default_parameters": {
    "flow": 0.7,
    "depth": 0.5,
    "mix": 0.5
  },
  "metrics": {
    "frequency_band": {
      "target_band": [5000, 12000],
      "tolerance_db": 0.5,
      "description": "Область воздействия: только 5–12 кГц"
    },
    "binaural_delay": {
      "target_range_ms": [0.05, 0.15],
      "max_ms": 0.2,
      "target_band": [5000, 12000],
      "description": "Бинауральная задержка: 0.05–0.15 мс RMS"
    },
    "lfo_modulation": {
      "target_range_hz": [0.03, 0.1],
      "target_band": [5000, 12000],
      "description": "Частота 'дыхания': 0.03–0.1 Гц"
    },
    "mono_compatibility": {
      "threshold": 0.6,
      "description": "Моно-совместимость: корреляция ≥ 0.6"
    },
    "spectral_centroid": {
      "max_increase_percent": 5,
      "target_band": [5000, 12000],
      "description": "Уровень 'воздуха': центроид +5% макс"
    },
    "sibilants": {
      "band": [6000, 9000],
      "max_increase_db": 2.0,
      "description": "Сибилянты: 6–9 кГц ≤ +2 дБ"
    },
    "lufs": {
      "max_diff_db": 1.0,
      "description": "Громкость: LUFS отличие < 1 дБ"
    }
  }
}
```

---

## 📝 Шаблоны промптов для ИИ

### Шаблон полного промпта

```
Ты — аудио-инженер. Задача: реализовать модуль [НАЗВАНИЕ] для VST3 (JUCE, C++) 
и автотесты (Python: numpy/scipy/librosa).

Сначала напиши спецификацию и тест-сценарии, потом код.

Звуковой контракт:

- Полоса воздействия: [ДИАПАЗОН] кГц (±0.5 кГц), вне полосы изменения < 0.5 дБ.
- Бинауральная задержка: [ДИАПАЗОН] мс RMS, LFO [ДИАПАЗОН] Гц.
- IACC в полосе: [ДИАПАЗОН]. Моно-корреляция ≥ 0.6.
- LUFS: изменение < 1 дБ.
- Сибилянты: [ДИАПАЗОН] кГц ≤ +2 дБ к dry.
- [ДРУГИЕ МЕТРИКИ ИЗ КОНТРАКТА]

Что нужно отдать:

1) C++ класс JUCE: prepare(sampleRate), processBlock, без аллокаций в аудио-потоке, 
   параметр-сглаживание (5–30 мс).

2) Тестовый офлайн-рендер 5 файлов (данный список путей), dry и wet.

3) Метрики: полосной спектр до/после, IACC(время), LUFS, моно-корреляция. 
   PASS/FAIL чеклист.

4) Отчёт (HTML/PDF) + A/B WAV (равная громкость, LUFS-нормализация).

Если FAIL — предложи конкретные правки параметров и автоматически перерендерь.

Контекст проекта:
- См. Documentation/main.md для концепции
- См. Documentation/AI_WORKING_RULES.md для стандартов кода
- Текущий этап: [ЭТАП]
- Существующие файлы: [СПИСОК]
```

### Пример для Glass Scene

```
Контекст: Этап 2 (Сцены MVP), реализуем Glass scene
Документ: main.md:790 "СЦЕНА 5: GLASS"
Существующий код: Source/PluginProcessor.cpp (уже есть Iceberg и Platina)

Задача: Реализовать модуль BinauralUpperBand для Glass scene

Звуковой контракт (Glass: "свет переливается внутри"):

- Полоса воздействия: 5–12 кГц (±0.5 кГц), вне полосы изменения < 0.5 дБ.
- Бинауральная задержка: 0.05–0.15 мс RMS (не больше 0.2 мс).
- Частота "дыхания": 0.03–0.1 Гц (медленная модуляция).
- Моно-совместимость: корреляция L/R ≥ 0.6.
- Уровень "воздуха": спектральный центроид не более +5%.
- Сибилянты: энергия 6–9 кГц не должна расти > +2 дБ относительно dry.
- Громкость: Integrated LUFS отличается < 1 дБ от dry.

Технические требования:
- C++ класс BinauralUpperBand в Source/BinauralUpperBand.h/cpp
- Интеграция в PluginProcessor для сцены Glass
- Параметр Flow управляет LFO частотой (0.03–0.1 Гц)
- Параметр Depth управляет амплитудой задержки (0.05–0.15 мс)
- Сглаживание параметров: 10 мс

Автотесты:
- tests/test_glass.py с офлайн-рендером 5 голосовых сэмплов
- Метрики: frequency_band, binaural_delay, lfo_modulation, mono_compatibility, 
  spectral_centroid, sibilants, lufs
- HTML отчёт с таблицами и графиками
- A/B WAV файл (равная громкость)

Если FAIL — предложи конкретные правки параметров и перерендерь автоматически.
```

---

## ✅ Чеклист качества

### Быстрый контроль "приятно и стильно"

**Перед каждым коммитом проверяй:**

- [ ] **Нет резких "S"** — де-эссер триггерится < 20% времени
- [ ] **LUFS ±1 дБ** от dry — переключение dry/wet не "обманывает" ухо
- [ ] **Моно-совместимость OK** — корреляция > 0.6
- [ ] **Нет клипов/кликов** при крутилках — сглаживание есть
- [ ] **Верх не звенит** — прирост 6–10 кГц ≤ +2 дБ (кроме специальных сцен)
- [ ] **Саб не "бубнит"** — 60–120 Гц контролируются (особенно Pluto)
- [ ] **ABX-прослушка** — ты уверенно отличаешь эффект и он лучше dry

### Детальный чеклист по сценам

#### Iceberg

- [ ] LFO реверба: 0.03–0.1 Гц (Flow параметр)
- [ ] Decay реверба: до 20 сек
- [ ] Предзадержка: 80–120 мс
- [ ] Бинауральная задержка (Binaural Flow): 0.4 мс ±0.3 мс
- [ ] Моно-корреляция ≥ 0.6
- [ ] LUFS ±1 дБ

#### Glass

- [ ] Полоса воздействия: только 5–12 кГц
- [ ] Бинауральная задержка: 0.05–0.15 мс RMS
- [ ] LFO "дыхания": 0.03–0.1 Гц
- [ ] Сибилянты: 6–9 кГц ≤ +2 дБ
- [ ] Спектральный центроид: +5% макс
- [ ] Моно-корреляция ≥ 0.6

#### Platina

- [ ] Harmonic Glide: корреляция RMS→детюн > 0.3
- [ ] Максимальный сдвиг: ≤ 3 цента
- [ ] Плавность: > 0.7 (без рывков)
- [ ] Spectral Vibe: IACC 0.3–0.6
- [ ] Моно-корреляция ≥ 0.6

#### Opiate

- [ ] Temporal Ease: увеличение decay 100–200 мс
- [ ] Начало фразы неизменно
- [ ] Мягкость атак (спектральное маскирование)
- [ ] Моно-корреляция ≥ 0.6

---

## 🔗 Интеграция с этапами разработки

### Этап 0: Инфра-старт

**Контракт:**
- Видится в Ableton/FL
- Байт-код подписан (macOS), без крашей
- Латентность = 0
- Звук идентичен dry при Mix=0%

**Метрики:**
- Нет клипов
- LUFS идентичен dry (±0.1 дБ)
- Спектр идентичен dry (разница < 0.1 дБ везде)

### Этап 1: Ядро DSP (черновое)

**Контракты по модулям:**

#### Space (реверб)
- Decay: согласно сцене (Iceberg: до 20 сек)
- Предзадержка: согласно сцене (Iceberg: 80–120 мс)
- Моно-корреляция ≥ 0.6
- LUFS ±1 дБ

#### Motion (LFO)
- Частота: согласно параметру (Iceberg: 0.03–0.1 Гц)
- Сглаживание: 5–30 мс
- Нет кликов при изменении параметров

#### Binaural Flow (Iceberg)
- Задержка: 0.05–0.15 мс RMS (макс 0.2 мс)
- LFO: 0.03–0.1 Гц
- IACC: 0.1–0.4
- Моно-корреляция ≥ 0.6

#### Harmonic Glide (Platina)
- Корреляция RMS→детюн > 0.3
- Максимальный сдвиг ≤ 3 цента
- Плавность > 0.7

### Этап 2: Сцены MVP (Iceberg и Platina)

**Контракт Iceberg:**
- Все метрики из Binaural Flow
- Decay реверба: до 20 сек
- LFO реверба: 0.03–0.1 Гц (Flow параметр)

**Контракт Platina:**
- Все метрики из Harmonic Glide
- Spectral Vibe: IACC 0.3–0.6
- Energy параметр работает корректно

**ABX тест:**
- 5 эталонных фраз (м/ж, протяжные/быстрые)
- Ты уверенно отличаешь и выбираешь Wet

### Этап 3: Тестовый раннер

**Контракт:**
- Офлайн-рендер 5 фраз × 3 сцены
- Все метрики считаются автоматически
- HTML/PDF отчёт генерируется
- A/B WAV создаётся

**Проверка:**
- Все контракты PASS
- Если FAIL — автоматическая рекомендация правок

### Этап 4: Расширение сцен

**Контракты для каждой новой сцены:**

#### Crystals
- Короткие гранулы: 50–120 мс
- Шимер: малая громкость
- Моно-корреляция ≥ 0.6

#### Pluto
- Perceptual EQ: стабилизация по Fletcher–Munson
- Саб-гид: 80–120 Гц контролируется
- Моно-корреляция ≥ 0.6

#### Opiate
- Temporal Ease: decay +100–200 мс
- Мягкость атак
- Моно-корреляция ≥ 0.6

#### Glass
- Полный контракт (см. выше)

**Проверка:**
- Каждая сцена проходит свой контракт
- Нет пересечения характеров (слепой тест)

---

## 🚨 Типичные проблемы и решения

### Проблема 1: ИИ делает эффект на весь спектр

**Решение:**
- Вводи мультибанд-каркас сразу
- Чётко указывай полосу воздействия в контракте
- Проверяй метрику `frequency_band`

### Проблема 2: Большие глубины эффекта

**Решение:**
- Жёстко фиксируй диапазоны параметров в контракте
- Используй сглаживание параметров (5–30 мс)
- Проверяй метрики на максимальных значениях

### Проблема 3: Эффект привносит громкость

**Решение:**
- Всегда автогейн / LUFS-нормализация в тестах
- Проверяй метрику `lufs` (< 1 дБ)
- Используй компенсацию громкости в processBlock

### Проблема 4: Нет офлайн-рендера

**Решение:**
- Специально проси CLI-рендер из JUCE (Standalone/Headless)
- Или через небольшой host-скрипт (Python + JUCE Python bindings)
- Всегда включай офлайн-рендер в требования

### Проблема 5: "На слух не то"

**Решение:**
- Дай ИИ не общую эмоцию, а разницу в метриках
- Пример: "энергия 6–8 кГц выросла на +4.2 дБ (над лимитом +2). 
  Уменьши усиление эксайтера на 2 дБ, ограничь Q, удерживай LUFS в пределах ±0.5 дБ"
- Добавляй constraint-ы в контракт

### Проблема 6: Клики при крутилках

**Решение:**
- Всегда требуй параметр-сглаживание (5–30 мс)
- Проверяй метрику `clips_and_clicks` при изменении параметров
- Используй `SmoothedValue` в JUCE

---

## 📊 Примеры контрактов для всех сцен

### Iceberg — "холодное дыхание, глубина"

```json
{
  "scene": "Iceberg",
  "metrics": {
    "frequency_band": {
      "target_band": [0, 20000],
      "tolerance_db": 0.5,
      "description": "Полный спектр, но с акцентом на реверб"
    },
    "reverb_decay": {
      "min_seconds": 15,
      "max_seconds": 25,
      "description": "Decay реверба: до 20 сек"
    },
    "reverb_predelay": {
      "target_range_ms": [80, 120],
      "description": "Предзадержка: 80–120 мс"
    },
    "binaural_flow": {
      "delay_range_ms": [0.05, 0.15],
      "max_delay_ms": 0.2,
      "lfo_range_hz": [0.03, 0.1],
      "target_band": [0, 20000],
      "description": "Binaural Flow: задержка 0.05–0.15 мс, LFO 0.03–0.1 Гц"
    },
    "mono_compatibility": {
      "threshold": 0.6
    },
    "lufs": {
      "max_diff_db": 1.0
    }
  }
}
```

### Glass — "свет переливается внутри"

(См. пример выше в разделе "Пример контракта")

### Platina — "уверенность, естественный драйв"

```json
{
  "scene": "Platina",
  "metrics": {
    "harmonic_glide": {
      "correlation_threshold": 0.3,
      "smoothness_threshold": 0.7,
      "max_cent_shift": 3,
      "description": "Harmonic Glide: корреляция > 0.3, плавность > 0.7, сдвиг ≤ 3 цента"
    },
    "spectral_vibe": {
      "iacc_range": [0.3, 0.6],
      "description": "Spectral Vibe: IACC 0.3–0.6"
    },
    "mono_compatibility": {
      "threshold": 0.6
    },
    "lufs": {
      "max_diff_db": 1.0
    }
  }
}
```

---

## 🎯 Как использовать в работе

### Шаг 1: Определи контракт для фичи

1. Открой `Documentation/main.md` и найди описание сцены/кирпича
2. Определи 5–8 измеримых метрик
3. Запиши контракт в JSON (см. примеры выше)

### Шаг 2: Отдай ИИ задачу с контрактом

Используй шаблон промпта (см. выше) и подставь:
- Название модуля
- Контракт (JSON или текстом)
- Контекст проекта

### Шаг 3: ИИ генерирует код + тесты

ИИ должен отдать:
1. C++ код модуля (JUCE)
2. Python тесты с метриками
3. Офлайн-рендер скрипт
4. HTML отчёт генератор

### Шаг 4: Запусти тесты

```bash
cd tests
python test_runner.py --scene Glass --contract contracts/glass.json
```

### Шаг 5: Проверь отчёт

- Открой HTML отчёт
- Проверь все метрики PASS/FAIL
- Прослушай A/B WAV
- Если FAIL — используй рекомендации ИИ

### Шаг 6: Итерация

Если FAIL:
- ИИ предлагает правки параметров
- Автоматически перерендеривает
- Проверяет снова

---

## 📚 Связанные документы

- `Documentation/main.md` — полная концепция проекта
- `Documentation/AI_WORKING_RULES.md` — правила работы с ИИ
- `Documentation/CURRENT_STATUS.md` — текущий статус

---

**Последнее обновление:** 2025-01-XX  
**Версия:** 1.0  
**Проект:** VØID Engine






