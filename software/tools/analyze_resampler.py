#!/usr/bin/env python3
"""
Analyze resampler test output using scipy/numpy.

Run the C++ generator first:
    ./resampler-test --all --wav

Then analyze:
    python analyze_resampler.py alias_test.wav
    python analyze_resampler.py scratch_test.wav
    python analyze_resampler.py --all  # analyze all test files
    python analyze_resampler.py --all --plot  # with spectrograms
"""

import sys
import numpy as np
from scipy import signal
from scipy.io import wavfile
import argparse
from pathlib import Path

SAMPLE_RATE = 48000
PLOT_ENABLED = False

try:
    import matplotlib.pyplot as plt
    PLOT_ENABLED = True
except ImportError:
    pass


def load_wav(path):
    """Load WAV file, return (sample_rate, channels as list of arrays)."""
    sr, data = wavfile.read(path)
    if data.ndim == 1:
        return sr, [data.astype(np.float64)]
    else:
        return sr, [data[:, i].astype(np.float64) for i in range(data.shape[1])]


def compute_spectrum(data, sr=SAMPLE_RATE, nperseg=4096):
    """Compute power spectrum using Welch's method."""
    freqs, psd = signal.welch(data, sr, nperseg=nperseg, scaling='density')
    return freqs, psd


def band_energy(data, low_hz, high_hz, sr=SAMPLE_RATE):
    """Compute energy in a frequency band."""
    freqs, psd = compute_spectrum(data, sr)
    mask = (freqs >= low_hz) & (freqs <= high_hz)
    return np.sum(psd[mask]) * (freqs[1] - freqs[0])  # Integrate PSD


def total_energy(data, sr=SAMPLE_RATE):
    """Compute total signal energy."""
    freqs, psd = compute_spectrum(data, sr)
    return np.sum(psd) * (freqs[1] - freqs[0])


def rms(data):
    """Root mean square."""
    return np.sqrt(np.mean(data**2))


def snr_db(signal_data, noise_data):
    """Signal-to-noise ratio in dB."""
    sig_power = np.mean(signal_data**2)
    noise_power = np.mean(noise_data**2)
    if noise_power < 1e-20:
        return 200.0
    return 10 * np.log10(sig_power / noise_power)


def plot_stft_comparison(signals, labels, sr=SAMPLE_RATE, title="STFT Comparison",
                         output_file=None, nperseg=1024, vmin=-80, vmax=0):
    """
    Plot STFT spectrograms for multiple signals side by side.

    signals: list of 1D arrays
    labels: list of strings for subplot titles
    """
    if not PLOT_ENABLED:
        print("matplotlib not available, skipping plot")
        return

    n = len(signals)
    fig, axes = plt.subplots(1, n, figsize=(5*n, 4), sharey=True)
    if n == 1:
        axes = [axes]

    for ax, sig, label in zip(axes, signals, labels):
        f, t, Zxx = signal.stft(sig, sr, nperseg=nperseg)

        # Convert to dB, normalized to max
        magnitude = np.abs(Zxx)
        magnitude_db = 20 * np.log10(magnitude + 1e-10)
        magnitude_db -= np.max(magnitude_db)  # Normalize to 0 dB max

        im = ax.pcolormesh(t, f/1000, magnitude_db, shading='gouraud',
                          cmap='viridis', vmin=vmin, vmax=vmax)
        ax.set_xlabel('Time [s]')
        ax.set_title(label)
        ax.set_ylim(0, sr/2000)  # 0 to Nyquist in kHz

    axes[0].set_ylabel('Frequency [kHz]')

    # Colorbar
    fig.colorbar(im, ax=axes, label='Magnitude [dB]', shrink=0.8)
    fig.suptitle(title)
    plt.tight_layout()

    if output_file:
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        print(f"  Saved: {output_file}")
    else:
        plt.show()


def plot_spectrum_comparison(signals, labels, sr=SAMPLE_RATE, title="Spectrum Comparison",
                            output_file=None, nperseg=4096):
    """
    Plot power spectra for multiple signals overlaid.
    """
    if not PLOT_ENABLED:
        print("matplotlib not available, skipping plot")
        return

    fig, ax = plt.subplots(figsize=(10, 5))

    colors = ['blue', 'red', 'green', 'orange', 'purple']

    for sig, label, color in zip(signals, labels, colors):
        f, psd = signal.welch(sig, sr, nperseg=nperseg)
        psd_db = 10 * np.log10(psd + 1e-20)
        ax.plot(f/1000, psd_db, label=label, color=color, alpha=0.8)

    ax.set_xlabel('Frequency [kHz]')
    ax.set_ylabel('Power [dB]')
    ax.set_title(title)
    ax.legend()
    ax.grid(True, alpha=0.3)
    ax.set_xlim(0, sr/2000)

    plt.tight_layout()

    if output_file:
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        print(f"  Saved: {output_file}")
    else:
        plt.show()


def generate_alias_input(samples, sr=SAMPLE_RATE):
    """Regenerate the input signal for aliasing test (18/20/22 kHz tones)."""
    freqs = [18000, 20000, 22000]
    amp = 0.3
    t = np.arange(samples) / sr
    sig = np.zeros(samples)
    for f in freqs:
        sig += (amp / len(freqs)) * np.sin(2 * np.pi * f * t)
    return sig


def analyze_alias_test(wav_path, do_plot=False):
    """
    Analyze aliasing test WAV.
    Input: tones at 18/20/22 kHz, resampled at 2x pitch
    Expect: Cubic passes through (aliases), Sinc filters
    """
    sr, channels = load_wav(wav_path)
    if len(channels) < 2:
        print(f"Error: Expected 2 channels, got {len(channels)}")
        return

    cubic = channels[0]
    sinc = channels[1]

    # Regenerate input signal (same length as output * 2 for 2x pitch)
    input_sig = generate_alias_input(len(cubic) * 2, sr)

    print(f"\n{'='*60}")
    print(f"ALIASING TEST: {wav_path}")
    print(f"{'='*60}")
    print(f"Samples: {len(cubic)}, Sample rate: {sr} Hz")
    print()

    # RMS comparison
    cubic_rms = rms(cubic)
    sinc_rms = rms(sinc)
    print(f"RMS Levels:")
    print(f"  Cubic: {cubic_rms:.6f}")
    print(f"  Sinc:  {sinc_rms:.6f}")
    print(f"  Attenuation: {20*np.log10(sinc_rms/cubic_rms):.1f} dB")
    print()

    # Frequency band analysis
    print(f"Energy by Band (% of total):")
    bands = [(0, 4000), (4000, 8000), (8000, 12000), (12000, 16000), (16000, 20000), (20000, 24000)]
    cubic_total = total_energy(cubic, sr)
    sinc_total = total_energy(sinc, sr)

    for low, high in bands:
        cubic_pct = band_energy(cubic, low, high, sr) / cubic_total * 100 if cubic_total > 0 else 0
        sinc_pct = band_energy(sinc, low, high, sr) / sinc_total * 100 if sinc_total > 0 else 0
        print(f"  {low/1000:4.0f}-{high/1000:4.0f} kHz: Cubic {cubic_pct:5.1f}%, Sinc {sinc_pct:5.1f}%")
    print()

    # The key test: input was 18-22kHz. At 2x pitch, these become 36-44kHz,
    # which fold back to 4-12kHz. With anti-aliasing, Sinc should have
    # minimal energy, while Cubic should have significant aliased energy.
    print(f"Anti-Aliasing Effectiveness:")
    cubic_low = band_energy(cubic, 0, 12000, sr)
    sinc_low = band_energy(sinc, 0, 12000, sr)
    print(f"  Energy below 12kHz (aliased region):")
    print(f"    Cubic: {cubic_low:.6f}")
    print(f"    Sinc:  {sinc_low:.6f}")
    if sinc_low > 0:
        print(f"    Improvement: {cubic_low/sinc_low:.1f}x")

    # Plotting
    if do_plot:
        print()
        # STFT comparison
        plot_stft_comparison(
            [input_sig, cubic, sinc],
            ['Input (18-22kHz)', 'Cubic (aliased)', 'Sinc (filtered)'],
            sr=sr,
            title='Aliasing Test: STFT Comparison (2x pitch)',
            output_file='alias_stft.png'
        )
        # Spectrum comparison
        plot_spectrum_comparison(
            [input_sig, cubic, sinc],
            ['Input', 'Cubic', 'Sinc'],
            sr=sr,
            title='Aliasing Test: Power Spectrum (2x pitch)',
            output_file='alias_spectrum.png'
        )


def analyze_scratch_test(wav_path):
    """
    Analyze scratch simulation WAV.
    Input: 8-23kHz tones, scratch trajectory varies pitch 1.5x-2.5x
    Expect: Sinc filters high frequencies, less aliasing into low bands
    """
    sr, channels = load_wav(wav_path)
    if len(channels) < 2:
        print(f"Error: Expected at least 2 channels, got {len(channels)}")
        return

    cubic = channels[0]
    sinc = channels[1]
    pitch = channels[2] * 4.0 if len(channels) > 2 else None  # Undo /4 scaling

    print(f"\n{'='*60}")
    print(f"SCRATCH SIMULATION TEST: {wav_path}")
    print(f"{'='*60}")
    print(f"Samples: {len(cubic)}, Sample rate: {sr} Hz")
    if pitch is not None:
        print(f"Pitch range: {np.min(pitch):.2f}x to {np.max(pitch):.2f}x")
    print()

    # RMS
    print(f"RMS Levels:")
    print(f"  Cubic: {rms(cubic):.6f}")
    print(f"  Sinc:  {rms(sinc):.6f}")
    print()

    # Aliasing detection: input was 8-23kHz
    # At pitch 1.5x-2.5x, frequencies shift to 12-57.5kHz
    # Aliasing folds high content back into audible range
    # Detection: energy below 8kHz indicates aliasing (input had nothing there)
    print(f"Aliasing Detection (input had no energy below 8kHz):")
    cutoffs = [2000, 4000, 6000, 8000]
    for cutoff in cutoffs:
        cubic_low = band_energy(cubic, 0, cutoff, sr)
        sinc_low = band_energy(sinc, 0, cutoff, sr)
        cubic_total = total_energy(cubic, sr)
        sinc_total = total_energy(sinc, sr)

        cubic_pct = (cubic_low / cubic_total * 100) if cubic_total > 0 else 0
        sinc_pct = (sinc_low / sinc_total * 100) if sinc_total > 0 else 0
        improvement = cubic_pct / sinc_pct if sinc_pct > 0.01 else float('inf')

        print(f"  Below {cutoff/1000:.0f}kHz: Cubic {cubic_pct:5.1f}%, Sinc {sinc_pct:5.1f}%, Improvement {improvement:.1f}x")
    print()

    # Frequency distribution
    print(f"Energy Distribution (% of total):")
    bands = [(0, 4000), (4000, 8000), (8000, 12000), (12000, 16000), (16000, 24000)]
    cubic_total = total_energy(cubic, sr)
    sinc_total = total_energy(sinc, sr)

    for low, high in bands:
        cubic_pct = band_energy(cubic, low, high, sr) / cubic_total * 100 if cubic_total > 0 else 0
        sinc_pct = band_energy(sinc, low, high, sr) / sinc_total * 100 if sinc_total > 0 else 0
        print(f"  {low/1000:4.0f}-{high/1000:4.0f} kHz: Cubic {cubic_pct:5.1f}%, Sinc {sinc_pct:5.1f}%")


def analyze_sine_test(wav_path):
    """Analyze basic sine wave resampling test."""
    sr, channels = load_wav(wav_path)
    if len(channels) < 2:
        print(f"Error: Expected 2 channels, got {len(channels)}")
        return

    cubic = channels[0]
    sinc = channels[1]

    print(f"\n{'='*60}")
    print(f"SINE WAVE TEST: {wav_path}")
    print(f"{'='*60}")
    print(f"Samples: {len(cubic)}, Sample rate: {sr} Hz")
    print()

    print(f"RMS Levels (should be similar for basic sine):")
    print(f"  Cubic: {rms(cubic):.6f}")
    print(f"  Sinc:  {rms(sinc):.6f}")

    # Difference
    diff = cubic - sinc
    print(f"  Difference RMS: {rms(diff):.6f}")


def analyze_sweep_test(wav_path):
    """Analyze frequency sweep resampling test."""
    sr, channels = load_wav(wav_path)
    if len(channels) < 2:
        print(f"Error: Expected 2 channels, got {len(channels)}")
        return

    cubic = channels[0]
    sinc = channels[1]

    print(f"\n{'='*60}")
    print(f"FREQUENCY SWEEP TEST: {wav_path}")
    print(f"{'='*60}")
    print(f"Samples: {len(cubic)}, Sample rate: {sr} Hz")
    print()

    print(f"RMS Levels:")
    print(f"  Cubic: {rms(cubic):.6f}")
    print(f"  Sinc:  {rms(sinc):.6f}")


def main():
    parser = argparse.ArgumentParser(description='Analyze resampler test output')
    parser.add_argument('files', nargs='*', help='WAV files to analyze')
    parser.add_argument('--all', action='store_true', help='Analyze all standard test files')
    parser.add_argument('--plot', action='store_true', help='Generate STFT and spectrum plots')
    args = parser.parse_args()

    if args.plot and not PLOT_ENABLED:
        print("Warning: matplotlib not available, plots will be skipped")
        print("Install with: pip install matplotlib")

    if args.all:
        test_files = ['alias_test.wav', 'scratch_test.wav', 'sine_test.wav', 'sweep_test.wav']
        for f in test_files:
            if Path(f).exists():
                args.files.append(f)

    if not args.files:
        print("No files to analyze. Run --all or specify WAV files.")
        print("\nGenerate test files first:")
        print("  ./resampler-test --all --wav")
        return

    for path in args.files:
        if not Path(path).exists():
            print(f"File not found: {path}")
            continue

        name = Path(path).stem.lower()
        if 'alias' in name:
            analyze_alias_test(path, do_plot=args.plot)
        elif 'scratch' in name:
            analyze_scratch_test(path)
        elif 'sine' in name:
            analyze_sine_test(path)
        elif 'sweep' in name:
            analyze_sweep_test(path)
        else:
            print(f"Unknown test type: {path}")


if __name__ == '__main__':
    main()
