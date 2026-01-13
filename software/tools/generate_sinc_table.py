#!/usr/bin/env python3
"""
Generate windowed sinc interpolation tables for SC1000 audio resampler.

The tables contain pre-computed filter coefficients for polyphase sinc interpolation.
Each phase represents a fractional sample position between input samples.

Multiple bandwidth variants are generated for anti-aliasing at different pitch ratios:
- bandwidth 1.0: Use when pitch <= 1.0 (normal/slow playback)
- bandwidth 0.5: Use when 1.0 < pitch <= 2.0 (fast playback)
- bandwidth 0.25: Use when 2.0 < pitch <= 4.0 (very fast)

At runtime:
- Select bandwidth based on abs(pitch): bw = min(1.0, 1.0 / abs(pitch))
- Phase = fractional sample position * NUM_PHASES

Usage:
    python generate_sinc_table.py [options]
    python generate_sinc_table.py --help

Output:
    Generates src/dsp/sinc_table.h with float coefficients.
"""

import argparse
import math
import sys
from datetime import datetime
from typing import List, Tuple


def bessel_i0(x: float) -> float:
    """
    Modified Bessel function of the first kind, order 0.
    Used for Kaiser window calculation.
    """
    # Series expansion: I0(x) = sum_{k=0}^inf ((x/2)^k / k!)^2
    sum_val = 1.0
    term = 1.0
    x_half = x / 2.0

    for k in range(1, 50):  # 50 terms is plenty for convergence
        term *= (x_half / k) ** 2
        sum_val += term
        if term < 1e-20:
            break

    return sum_val


def kaiser_window(n: int, N: int, beta: float) -> float:
    """
    Kaiser window function.

    Args:
        n: Sample index (0 to N-1)
        N: Window length
        beta: Shape parameter (higher = better stopband, wider main lobe)
              Typical values: 4-12

    Returns:
        Window coefficient at position n
    """
    if N <= 1:
        return 1.0

    # Normalized position: -1 to +1
    alpha = (N - 1) / 2.0
    ratio = (n - alpha) / alpha

    # Avoid sqrt of negative due to floating point
    arg = 1.0 - ratio * ratio
    if arg < 0:
        arg = 0

    return bessel_i0(beta * math.sqrt(arg)) / bessel_i0(beta)


def sinc(x: float) -> float:
    """
    Normalized sinc function: sin(pi*x) / (pi*x)
    """
    if abs(x) < 1e-10:
        return 1.0
    return math.sin(math.pi * x) / (math.pi * x)


def generate_sinc_table(
    num_phases: int,
    num_taps: int,
    bandwidth: float,
    kaiser_beta: float
) -> Tuple[List[List[float]], dict]:
    """
    Generate polyphase sinc interpolation table.

    Args:
        num_phases: Number of phase positions (typically 16, 32, 64)
        num_taps: Filter length per phase (typically 8, 16, 32)
        bandwidth: Cutoff relative to Nyquist (0.0-1.0, typically 0.9-0.95)
        kaiser_beta: Kaiser window parameter (typically 6-10)

    Returns:
        Tuple of (table, stats) where:
        - table[phase][tap] contains the filter coefficients
        - stats contains normalization and quality info
    """
    table = []
    center = (num_taps - 1) / 2.0  # Center of the filter

    # For quality analysis
    max_coeff = 0.0
    min_coeff = 0.0

    for phase in range(num_phases):
        # Phase offset: 0 = aligned with sample, 0.5 = halfway between samples
        # We go from 0 to (num_phases-1)/num_phases, not including 1.0
        # (phase 0 of next sample = our sample itself)
        phase_offset = phase / num_phases

        coeffs = []
        for tap in range(num_taps):
            # Distance from center, adjusted for phase
            x = (tap - center) - phase_offset

            # Windowed sinc
            window = kaiser_window(tap, num_taps, kaiser_beta)
            coeff = sinc(x * bandwidth) * bandwidth * window

            coeffs.append(coeff)
            max_coeff = max(max_coeff, coeff)
            min_coeff = min(min_coeff, coeff)

        # Normalize so coefficients sum to 1.0 (unity gain)
        coeff_sum = sum(coeffs)
        if abs(coeff_sum) > 1e-10:
            coeffs = [c / coeff_sum for c in coeffs]

        table.append(coeffs)

    stats = {
        'max_coeff': max_coeff,
        'min_coeff': min_coeff,
        'num_phases': num_phases,
        'num_taps': num_taps,
        'bandwidth': bandwidth,
        'kaiser_beta': kaiser_beta,
    }

    return table, stats


def format_single_table(table: List[List[float]], name: str, indent: str = "") -> List[str]:
    """Format a single table as C++ array initializer."""
    lines = []
    num_phases = len(table)
    num_taps = len(table[0])

    lines.append(f"{indent}{{ // {name}")
    for phase_idx, phase in enumerate(table):
        coeff_strs = [f"{c:13.10f}f" for c in phase]

        # Split into lines of 4 coefficients
        chunks = []
        for i in range(0, len(coeff_strs), 4):
            chunks.append(", ".join(coeff_strs[i:i+4]))

        comma = "," if phase_idx < num_phases - 1 else ""
        lines.append(f"{indent}    {{ // Phase {phase_idx}")
        for i, chunk in enumerate(chunks):
            chunk_comma = "," if i < len(chunks) - 1 else ""
            lines.append(f"{indent}        {chunk}{chunk_comma}")
        lines.append(f"{indent}    }}{comma}")

    lines.append(f"{indent}}}")
    return lines


def format_tables_c(tables: List[Tuple[List[List[float]], dict]], kaiser_beta: float) -> str:
    """Format multiple bandwidth tables as C++ code."""

    num_bandwidths = len(tables)
    num_phases = len(tables[0][0])
    num_taps = len(tables[0][0][0])

    lines = []
    lines.append(f"// Polyphase sinc interpolation tables for SC1000")
    lines.append(f"// Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    lines.append(f"//")
    lines.append(f"// Structure: sinc_tables[bandwidth_idx][phase][tap]")
    lines.append(f"//   - bandwidth_idx: Select based on pitch ratio (see SINC_BANDWIDTHS)")
    lines.append(f"//   - phase: Fractional sample position (0 to NUM_PHASES-1)")
    lines.append(f"//   - tap: Filter coefficient index (0 to NUM_TAPS-1)")
    lines.append(f"//")
    lines.append(f"// Runtime usage:")
    lines.append(f"//   1. Compute bandwidth: bw = min(1.0f, 1.0f / fabsf(pitch))")
    lines.append(f"//   2. Select table index: idx = bw < 0.5f ? 2 : (bw < 1.0f ? 1 : 0)")
    lines.append(f"//   3. Compute phase: phase = (int)(frac * SINC_NUM_PHASES)")
    lines.append(f"//   4. Convolve: sum(sample[i] * sinc_tables[idx][phase][i])")
    lines.append(f"//")
    lines.append(f"#pragma once")
    lines.append(f"")
    lines.append(f"#include <cstddef>")
    lines.append(f"")
    lines.append(f"namespace sc {{")
    lines.append(f"namespace dsp {{")
    lines.append(f"")
    lines.append(f"// Table dimensions")
    lines.append(f"constexpr int SINC_NUM_BANDWIDTHS = {num_bandwidths};")
    lines.append(f"constexpr int SINC_NUM_PHASES = {num_phases};")
    lines.append(f"constexpr int SINC_NUM_TAPS = {num_taps};")
    lines.append(f"")
    lines.append(f"// Kaiser window parameter used for all tables")
    lines.append(f"constexpr float SINC_KAISER_BETA = {kaiser_beta:.6f}f;")
    lines.append(f"")

    # Bandwidth array
    bandwidths = [t[1]['bandwidth'] for t in tables]
    bw_strs = [f"{bw:.6f}f" for bw in bandwidths]
    lines.append(f"// Available bandwidth values (for anti-aliasing at different pitch ratios)")
    lines.append(f"// Use index 0 when pitch <= 1.0, index 1 when pitch <= 2.0, etc.")
    lines.append(f"constexpr float SINC_BANDWIDTHS[{num_bandwidths}] = {{ {', '.join(bw_strs)} }};")
    lines.append(f"")

    # Helper function to select bandwidth index
    lines.append(f"// Select bandwidth table index based on pitch ratio")
    lines.append(f"// Returns: 0 for pitch <= 1.0, 1 for pitch <= 2.0, 2 for pitch <= 4.0")
    lines.append(f"inline int sinc_select_bandwidth(float abs_pitch) {{")
    for i, bw in enumerate(bandwidths):
        max_pitch = 1.0 / bw
        if i == 0:
            lines.append(f"    if (abs_pitch <= {max_pitch:.1f}f) return {i};")
        elif i < len(bandwidths) - 1:
            lines.append(f"    if (abs_pitch <= {max_pitch:.1f}f) return {i};")
        else:
            lines.append(f"    return {i}; // pitch > {1.0/bandwidths[i-1]:.1f}, use lowest bandwidth")
    lines.append(f"}}")
    lines.append(f"")

    # Main table
    lines.append(f"// Sinc interpolation tables")
    lines.append(f"// Indexed as: sinc_tables[bandwidth_idx][phase][tap]")
    lines.append(f"alignas(64) constexpr float sinc_tables[{num_bandwidths}][{num_phases}][{num_taps}] = {{")

    for i, (table, stats) in enumerate(tables):
        name = f"Bandwidth {stats['bandwidth']:.2f} (pitch <= {1.0/stats['bandwidth']:.1f}x)"
        table_lines = format_single_table(table, name, "    ")
        if i < num_bandwidths - 1:
            table_lines[-1] += ","
        lines.extend(table_lines)

    lines.append(f"}};")
    lines.append(f"")
    lines.append(f"}} // namespace dsp")
    lines.append(f"}} // namespace sc")
    lines.append(f"")

    return "\n".join(lines)


def print_quality_analysis(table: List[List[float]], stats: dict):
    """Print quality analysis of the generated table."""

    num_phases = len(table)
    num_taps = len(table[0])

    print(f"\n=== Sinc Table Quality Analysis ===")
    print(f"Phases:      {num_phases}")
    print(f"Taps:        {num_taps}")
    print(f"Bandwidth:   {stats['bandwidth']:.3f} ({stats['bandwidth']*100:.1f}% of Nyquist)")
    print(f"Kaiser beta: {stats['kaiser_beta']:.1f}")
    print(f"")

    # Check DC gain (sum of coefficients per phase should be ~1.0)
    dc_gains = [sum(phase) for phase in table]
    print(f"DC gain range: {min(dc_gains):.6f} to {max(dc_gains):.6f}")

    # Check symmetry (phase 0 should be symmetric)
    phase0 = table[0]
    center = len(phase0) // 2
    symmetry_error = 0.0
    for i in range(center):
        diff = abs(phase0[i] - phase0[num_taps - 1 - i])
        symmetry_error = max(symmetry_error, diff)
    print(f"Phase 0 symmetry error: {symmetry_error:.2e}")

    # Memory usage
    mem_bytes = num_phases * num_taps * 4  # float = 4 bytes
    print(f"Memory usage: {mem_bytes} bytes ({mem_bytes/1024:.1f} KB)")

    # Compute cost per sample
    macs_per_sample = num_taps  # One MAC per tap
    print(f"MACs per output sample: {macs_per_sample}")
    print(f"")


def main():
    parser = argparse.ArgumentParser(
        description="Generate sinc interpolation tables for SC1000",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Default settings (good balance for DJ scratching)
    python generate_sinc_table.py

    # Higher quality (more taps, more phases)
    python generate_sinc_table.py --taps 32 --phases 64

    # Lower CPU (fewer taps)
    python generate_sinc_table.py --taps 8

    # Custom bandwidth variants for different pitch ranges
    python generate_sinc_table.py --bandwidths 1.0,0.5,0.25
        """
    )

    parser.add_argument(
        "--phases", type=int, default=32,
        help="Number of phase positions (default: 32)"
    )
    parser.add_argument(
        "--taps", type=int, default=16,
        help="Filter taps per phase (default: 16)"
    )
    parser.add_argument(
        "--bandwidths", type=str, default="1.0,0.5,0.25",
        help="Comma-separated bandwidths for anti-aliasing at different pitch ratios (default: 1.0,0.5,0.25)"
    )
    parser.add_argument(
        "--kaiser-beta", type=float, default=8.0,
        help="Kaiser window parameter (default: 8.0)"
    )
    parser.add_argument(
        "--output", "-o", type=str, default=None,
        help="Output file (default: src/dsp/sinc_table.h)"
    )
    parser.add_argument(
        "--quiet", "-q", action="store_true",
        help="Suppress analysis output"
    )

    args = parser.parse_args()

    # Parse bandwidths
    try:
        bandwidths = [float(b.strip()) for b in args.bandwidths.split(",")]
    except ValueError:
        print(f"Error: invalid bandwidths format: {args.bandwidths}", file=sys.stderr)
        return 1

    # Sort descending (highest bandwidth = lowest pitch first)
    bandwidths.sort(reverse=True)

    # Validate parameters
    if args.phases < 2 or args.phases > 256:
        print(f"Error: phases must be 2-256, got {args.phases}", file=sys.stderr)
        return 1

    if args.taps < 4 or args.taps > 64:
        print(f"Error: taps must be 4-64, got {args.taps}", file=sys.stderr)
        return 1

    for bw in bandwidths:
        if bw <= 0 or bw > 1.0:
            print(f"Error: bandwidth must be 0.0-1.0, got {bw}", file=sys.stderr)
            return 1

    if args.kaiser_beta < 0 or args.kaiser_beta > 20:
        print(f"Error: kaiser-beta must be 0-20, got {args.kaiser_beta}", file=sys.stderr)
        return 1

    # Generate tables for each bandwidth
    tables = []
    for bw in bandwidths:
        table, stats = generate_sinc_table(
            args.phases, args.taps, bw, args.kaiser_beta
        )
        tables.append((table, stats))

        if not args.quiet:
            print_quality_analysis(table, stats)

    # Format as C++
    c_code = format_tables_c(tables, args.kaiser_beta)

    # Determine output path
    if args.output:
        output_path = args.output
    else:
        # Default: relative to script location
        import os
        script_dir = os.path.dirname(os.path.abspath(__file__))
        output_path = os.path.join(script_dir, "..", "src", "dsp", "sinc_table.h")

    # Ensure directory exists
    import os
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)

    # Write output
    with open(output_path, 'w') as f:
        f.write(c_code)

    if not args.quiet:
        # Summary
        total_bytes = len(bandwidths) * args.phases * args.taps * 4
        print(f"\n=== Summary ===")
        print(f"Tables: {len(bandwidths)} bandwidth variants")
        print(f"Total memory: {total_bytes} bytes ({total_bytes/1024:.1f} KB)")
        print(f"Written to: {output_path}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
