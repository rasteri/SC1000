#!/usr/bin/env python3
"""
SC1000 Test Output Analyzer

Analyzes WAV files produced by sc1000-test --dump
Generates spectrograms and frequency analysis plots.

Usage:
    python analyze_test_output.py [wav_files...]
    python analyze_test_output.py --all     # Analyze all WAV files in current dir

Requirements:
    pip install numpy scipy matplotlib librosa
"""

import sys
import os
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

try:
    import librosa
    import librosa.display
    HAS_LIBROSA = True
except ImportError:
    HAS_LIBROSA = False
    print("Warning: librosa not installed. Using scipy for basic analysis.")
    print("Install with: pip install librosa")

from scipy.io import wavfile
from scipy import signal
from scipy.fft import fft, fftfreq


def load_audio(path):
    """Load audio file, return (samples, sample_rate)"""
    if HAS_LIBROSA:
        y, sr = librosa.load(path, sr=None, mono=True)
        return y, sr
    else:
        sr, data = wavfile.read(path)
        # Convert to float
        if data.dtype == np.int16:
            data = data.astype(np.float32) / 32768.0
        elif data.dtype == np.int32:
            data = data.astype(np.float32) / 2147483648.0
        # Convert stereo to mono
        if len(data.shape) > 1:
            data = data.mean(axis=1)
        return data, sr


def analyze_spectrum(y, sr, title="Spectrum"):
    """Compute and plot FFT spectrum"""
    n = len(y)
    yf = fft(y)
    xf = fftfreq(n, 1/sr)

    # Only positive frequencies
    pos_mask = xf > 0
    freqs = xf[pos_mask]
    magnitudes = np.abs(yf[pos_mask]) / n

    # Convert to dB
    magnitudes_db = 20 * np.log10(magnitudes + 1e-10)

    # Find peak
    peak_idx = np.argmax(magnitudes)
    peak_freq = freqs[peak_idx]
    peak_db = magnitudes_db[peak_idx]

    return freqs, magnitudes_db, peak_freq, peak_db


def analyze_stft(y, sr, title="STFT"):
    """Compute Short-Time Fourier Transform"""
    if HAS_LIBROSA:
        D = librosa.stft(y)
        S_db = librosa.amplitude_to_db(np.abs(D), ref=np.max)
        return S_db
    else:
        f, t, Zxx = signal.stft(y, sr, nperseg=1024)
        S_db = 20 * np.log10(np.abs(Zxx) + 1e-10)
        return S_db


def plot_analysis(path, output_dir=None):
    """Generate analysis plots for a WAV file"""
    y, sr = load_audio(path)

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle(f"Analysis: {Path(path).name}", fontsize=14)

    # 1. Waveform
    ax = axes[0, 0]
    time = np.arange(len(y)) / sr
    ax.plot(time, y, linewidth=0.5)
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Amplitude")
    ax.set_title("Waveform")
    ax.grid(True, alpha=0.3)

    # 2. FFT Spectrum
    ax = axes[0, 1]
    freqs, magnitudes_db, peak_freq, peak_db = analyze_spectrum(y, sr)
    ax.semilogx(freqs, magnitudes_db, linewidth=0.5)
    ax.axvline(peak_freq, color='r', linestyle='--', alpha=0.7,
               label=f'Peak: {peak_freq:.1f} Hz')
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Magnitude (dB)")
    ax.set_title(f"Spectrum (peak: {peak_freq:.1f} Hz)")
    ax.set_xlim(20, sr/2)
    ax.set_ylim(-80, 0)
    ax.grid(True, alpha=0.3)
    ax.legend()

    # 3. Spectrogram (STFT)
    ax = axes[1, 0]
    if HAS_LIBROSA:
        D = librosa.stft(y)
        S_db = librosa.amplitude_to_db(np.abs(D), ref=np.max)
        img = librosa.display.specshow(S_db, sr=sr, x_axis='time', y_axis='log',
                                        ax=ax, cmap='magma')
        fig.colorbar(img, ax=ax, format='%+2.0f dB')
    else:
        f, t, Zxx = signal.stft(y, sr, nperseg=1024)
        S_db = 20 * np.log10(np.abs(Zxx) + 1e-10)
        ax.pcolormesh(t, f, S_db, shading='gouraud', cmap='magma', vmin=-80, vmax=0)
        ax.set_ylabel("Frequency (Hz)")
        ax.set_yscale('log')
        ax.set_ylim(20, sr/2)
    ax.set_xlabel("Time (s)")
    ax.set_title("Spectrogram")

    # 4. Frequency histogram (where is the energy?)
    ax = axes[1, 1]
    # Bin frequencies logarithmically
    freq_bins = np.logspace(np.log10(20), np.log10(sr/2), 50)
    energy_per_bin = []
    for i in range(len(freq_bins) - 1):
        mask = (freqs >= freq_bins[i]) & (freqs < freq_bins[i+1])
        if mask.any():
            # Convert from dB back to linear for summing
            linear = 10 ** (magnitudes_db[mask] / 20)
            energy_per_bin.append(np.sum(linear**2))
        else:
            energy_per_bin.append(0)

    bin_centers = np.sqrt(freq_bins[:-1] * freq_bins[1:])  # Geometric mean
    ax.bar(range(len(energy_per_bin)), energy_per_bin, width=0.8)
    ax.set_xticks(range(0, len(bin_centers), 5))
    ax.set_xticklabels([f'{int(f)}' for f in bin_centers[::5]], rotation=45)
    ax.set_xlabel("Frequency (Hz)")
    ax.set_ylabel("Energy")
    ax.set_title("Energy Distribution")

    plt.tight_layout()

    if output_dir:
        out_path = Path(output_dir) / (Path(path).stem + "_analysis.png")
        plt.savefig(out_path, dpi=150)
        print(f"Saved: {out_path}")

    return fig, peak_freq


def print_summary(path, peak_freq):
    """Print analysis summary"""
    name = Path(path).stem
    print(f"  {name}: peak = {peak_freq:.1f} Hz")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    files = []
    output_dir = None
    show_plot = True

    i = 1
    while i < len(sys.argv):
        arg = sys.argv[i]
        if arg == "--all":
            files.extend(Path(".").glob("*.wav"))
        elif arg == "--output" or arg == "-o":
            i += 1
            output_dir = sys.argv[i]
        elif arg == "--no-show":
            show_plot = False
        elif arg.endswith(".wav"):
            files.append(arg)
        else:
            print(f"Unknown argument: {arg}")
        i += 1

    if not files:
        print("No WAV files found")
        return 1

    print(f"\nAnalyzing {len(files)} file(s)...\n")
    print("Peak frequencies:")

    for path in files:
        try:
            fig, peak_freq = plot_analysis(str(path), output_dir)
            print_summary(path, peak_freq)
        except Exception as e:
            print(f"  {path}: ERROR - {e}")

    print()

    if show_plot and not output_dir:
        plt.show()

    return 0


if __name__ == "__main__":
    sys.exit(main())
