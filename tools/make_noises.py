"""Synthesises Granulum's built-in OSC 3 noises (loopable, stereo).

Usage: python3 tools/make_noises.py <out_dir>
Writes <NAME>.wav for each noise. All sounds are generated from scratch here (no samples).
"""
import sys, os
import numpy as np
from scipy import signal
from scipy.io import wavfile

SR = 44100
DUR = 4.0          # loop length in seconds
XF = 0.5           # crossfade used to make the loop seamless
rng = np.random.default_rng(2026)
N = int((DUR + XF) * SR)
t = np.arange(N) / SR


def white(n=N):
    return rng.standard_normal(n)


def pink(n=N):
    # Voss-McCartney-ish via filtering white noise (Paul Kellet's filter)
    b = [0.049922035, -0.095993537, 0.050612699, -0.004408786]
    a = [1, -2.494956002, 2.017265875, -0.522189400]
    return signal.lfilter(b, a, white(n))


def brown(n=N):
    x = np.cumsum(white(n))
    return signal.lfilter([1, -1], [1, -0.995], x)


def bp(x, lo, hi, order=2):
    sos = signal.butter(order, [lo, hi], btype="band", fs=SR, output="sos")
    return signal.sosfilt(sos, x)


def lp(x, f, order=2):
    return signal.sosfilt(signal.butter(order, f, fs=SR, output="sos"), x)


def hp(x, f, order=2):
    return signal.sosfilt(signal.butter(order, f, btype="high", fs=SR, output="sos"), x)


def smooth_random(rate_hz, n=N, seed_scale=1.0):
    """Slow random curve 0..1 (for gusts, wobble)."""
    k = max(4, int(n / SR * rate_hz) + 4)
    pts = rng.random(k)
    xs = np.linspace(0, n, k)
    curve = np.interp(np.arange(n), xs, pts)
    return lp(curve, max(0.5, rate_hz), 1) * seed_scale


def stereo(fn):
    return np.stack([fn(), fn()], axis=1)


def impulses(rate, n=N, amp_pow=3.0):
    x = np.zeros(n)
    count = rng.poisson(rate * n / SR)
    pos = rng.integers(0, n, count)
    x[pos] = (rng.random(count) ** amp_pow) * rng.choice([-1, 1], count)
    return x


# ---------------------------------------------------------------- noises

def air():
    def ch():
        x = pink()
        x = hp(x, 120)
        return x * (0.75 + 0.25 * smooth_random(0.5))
    return stereo(ch)


def deep():
    def ch():
        x = lp(brown(), 380, 2)
        return x * (0.7 + 0.3 * smooth_random(0.3))
    return stereo(ch)


def tape_hiss():
    wow = 0.85 + 0.15 * np.sin(2 * np.pi * 0.5 * t)
    def ch():
        x = bp(white(), 1800, 13000, 2)
        x = x * wow * (0.9 + 0.1 * smooth_random(3))
        hum = 0.015 * np.sin(2 * np.pi * 50 * t) + 0.006 * np.sin(2 * np.pi * 150 * t)
        return x + hum * np.std(x) * 6
    return stereo(ch)


def vinyl():
    rumble = lp(brown(), 60, 2)
    rumble *= 0.15 / (np.std(rumble) + 1e-9)
    def ch():
        crackle = impulses(55, amp_pow=4) + impulses(4, amp_pow=1.2) * 2.5
        crackle = bp(crackle, 700, 9000, 2)
        crackle = signal.lfilter([1], [1, -0.55], crackle)
        hiss = hp(white(), 4000) * 0.02
        return crackle + hiss + rumble
    return stereo(ch)


def static():
    def ch():
        x = white()
        f = 1200 + 2500 * smooth_random(1.5)
        # time-varying bandpass by blockwise filtering
        out = np.zeros(N)
        blk = 512
        zi = None
        for i in range(0, N, blk):
            fc = float(f[i])
            sos = signal.butter(2, [fc * 0.6, min(fc * 1.6, 18000)], btype="band", fs=SR, output="sos")
            if zi is None:
                zi = np.zeros((sos.shape[0], 2))
            out[i:i + blk], zi = signal.sosfilt(sos, x[i:i + blk], zi=zi)
        am = 0.5 + 0.5 * np.sign(np.sin(2 * np.pi * (6 + 4 * smooth_random(0.7)) * t)) * smooth_random(2)
        whistle = 0.25 * np.sin(2 * np.pi * np.cumsum(2500 + 900 * np.sin(2 * np.pi * 0.23 * t)) / SR)
        return out * (0.4 + 0.6 * am) + whistle * np.std(out) * 0.6
    return stereo(ch)


def wind():
    def ch():
        x = pink()
        g = smooth_random(0.6) ** 1.5
        out = np.zeros(N)
        blk = 512
        zi = None
        for i in range(0, N, blk):
            fc = 250 + 1300 * float(g[i])
            sos = signal.butter(2, [fc * 0.7, fc * 1.4], btype="band", fs=SR, output="sos")
            if zi is None:
                zi = np.zeros((sos.shape[0], 2))
            out[i:i + blk], zi = signal.sosfilt(sos, x[i:i + blk], zi=zi)
        return out * (0.25 + 0.75 * g)
    return stereo(ch)


def breath():
    # two slow breaths per loop: inhale (brighter) / exhale (darker)
    cyc = (t % 2.25) / 2.25
    env = np.where(cyc < 0.42, np.sin(np.pi * np.clip(cyc, 0, 0.42) / 0.42) ** 2 * 0.7,
                   np.where(cyc < 0.95, np.clip(np.sin(np.pi * (cyc - 0.42) / 0.53), 0, None) ** 1.5, 0))
    def ch():
        x = white()
        f1 = bp(x, 700, 1300, 2)
        f2 = bp(x, 1800, 3200, 2) * 0.8
        f3 = bp(x, 4500, 8000, 2) * 0.35
        return (f1 + f2 + f3) * (env + 0.02)
    return stereo(ch)


def dust():
    def ch():
        out = np.zeros(N)
        count = rng.poisson(38 * N / SR)
        for p in rng.integers(0, N - 4000, count):
            kind = rng.random()
            L = rng.integers(40, 900)
            seg = np.arange(L)
            if kind < 0.6:   # crushed digital crumb
                burst = np.round(rng.standard_normal(L) * 2) / 2
                burst = np.repeat(burst[::8], 8)[:L]
            else:            # tiny high blip
                fr = rng.uniform(3000, 12000)
                burst = np.sin(2 * np.pi * fr * seg / SR)
            burst *= np.exp(-seg / (L * 0.3)) * rng.uniform(0.2, 1.0)
            out[p:p + L] += burst
        return hp(out, 900) + hp(white(), 9000) * 0.01
    return stereo(ch)


def frost():
    def ch():
        bed = bp(white(), 6000, 16000, 2) * 0.12
        out = np.zeros(N)
        count = rng.poisson(90 * N / SR)
        for p in rng.integers(0, N - 6000, count):
            fr = rng.uniform(3500, 11000)
            L = rng.integers(400, 5000)
            seg = np.arange(L)
            out[p:p + L] += np.sin(2 * np.pi * fr * seg / SR) * np.sin(np.pi * seg / L) ** 2 * rng.uniform(0.05, 0.4)
        return bed * np.std(out) * 8 + out
    return stereo(ch)


def metal_rain():
    ratios = np.array([1.0, 2.756, 5.404, 8.933])
    gains = np.array([1.0, 0.5, 0.3, 0.15])
    def ch():
        out = np.zeros(N)
        count = rng.poisson(45 * N / SR)
        for p in rng.integers(0, N - 12000, count):
            f0 = rng.uniform(900, 3200)
            L = 12000
            seg = np.arange(L) / SR
            dec = rng.uniform(0.04, 0.18)
            drop = np.zeros(L)
            for r, gn in zip(ratios, gains):
                if f0 * r < 18000:
                    drop += gn * np.sin(2 * np.pi * f0 * r * seg + rng.random() * 6) * np.exp(-seg / (dec / r ** 0.5))
            out[p:p + L] += drop * rng.uniform(0.1, 1.0) ** 2
        return out + hp(white(), 5000) * 0.01
    return stereo(ch)


def paper():
    def ch():
        out = np.zeros(N)
        # rustling gestures: clusters of dense micro-crackles
        gestures = rng.poisson(7 * N / SR)
        for start in rng.integers(0, N - SR, gestures):
            L = int(rng.uniform(0.15, 0.9) * SR)
            seg = np.arange(L)
            env = np.sin(np.pi * seg / L) ** rng.uniform(0.6, 2.0)
            dens = rng.uniform(200, 1500)
            cr = impulses(dens, L, amp_pow=2.5)
            cr = bp(cr, rng.uniform(900, 2000), rng.uniform(5000, 11000), 2)
            swish = bp(white(L), 2500, 9000, 1) * 0.04
            out[start:start + L] += (cr + swish) * env * rng.uniform(0.3, 1.0)
        # occasional crumple snaps
        snaps = bp(impulses(3, amp_pow=0.8), 1500, 7000, 2) * 2
        bed = bp(impulses(300, amp_pow=3), 1500, 9000, 2) * 0.25 * (0.5 + 0.5 * smooth_random(1.2))
        return out + snaps + bed
    return stereo(ch)


NOISES = [
    ("AIR", air), ("TAPE HISS", tape_hiss), ("VINYL", vinyl),
    ("STATIC", static), ("WIND", wind), ("BREATH", breath), ("PAPER", paper),
    ("DUST", dust), ("FROST", frost), ("METAL RAIN", metal_rain),
]


# ---------------------------------------------------------------- 0.99.5: organic noises

def _drops(count_rate, n, f_lo, f_hi, dec_lo, dec_hi, amp_pow=2.0, chirp=0.0):
    out = np.zeros(n)
    count = rng.poisson(count_rate * n / SR)
    for p in rng.integers(0, n - 6000, count):
        L = 6000
        seg = np.arange(L) / SR
        f0 = rng.uniform(f_lo, f_hi)
        dec = rng.uniform(dec_lo, dec_hi)
        fr = f0 * (1.0 + chirp * seg / dec)            # chirp > 0: rising pitch (bubbles)
        ph = 2 * np.pi * np.cumsum(fr) / SR
        out[p:p + L] += np.sin(ph) * np.exp(-seg / dec) * rng.random() ** amp_pow
    return out


def rain():
    # soft rain on a window: many tiny drops + a gentle wet bed
    def ch():
        drops = _drops(160, N, 1800, 7000, 0.002, 0.012, amp_pow=2.5)
        taps = bp(impulses(220, amp_pow=3), 1200, 8000, 2) * 0.6
        bed = bp(pink(), 400, 9000, 2) * 0.012 * (0.8 + 0.2 * smooth_random(0.4))
        return lp(drops + taps, 9000, 1) + bed * 12
    return stereo(ch)


def fire():
    # fireplace: warm low roar + wood crackles and pops
    def ch():
        roar = lp(brown(), 500, 2) * (0.6 + 0.4 * smooth_random(1.5))
        roar *= 0.25 / (np.std(roar) + 1e-9)
        crack = bp(impulses(18, amp_pow=1.5), 800, 6000, 2) * 3.0
        fizz = bp(impulses(260, amp_pow=4), 2500, 10000, 2) * 0.5 * (0.4 + 0.6 * smooth_random(2.0))
        pops = _drops(3, N, 300, 900, 0.004, 0.015, amp_pow=0.7) * 0.8
        return roar * np.std(crack + 1e-9) * 4 + crack + fizz + pops
    return stereo(ch)


def leaves():
    # dry leaves being stirred: soft rustling gestures
    def ch():
        out = np.zeros(N)
        for start in rng.integers(0, N - SR, rng.poisson(5 * N / SR)):
            L = int(rng.uniform(0.3, 1.2) * SR)
            seg = np.arange(L)
            env = np.sin(np.pi * seg / L) ** 1.5
            cr = impulses(rng.uniform(400, 1400), L, amp_pow=2.2)
            cr = bp(cr, rng.uniform(1200, 2500), rng.uniform(6000, 10000), 2)
            out[start:start + L] += cr * env * rng.uniform(0.3, 1.0)
        return lp(out, 9000, 1) + bp(pink(), 800, 6000, 1) * 0.004
    return stereo(ch)


def sand():
    # sand pouring: dense fine grains, slowly breathing
    def ch():
        x = impulses(4000, amp_pow=2.0)
        x = lp(bp(x, 1500, 9000, 2), 8000, 1)
        return x * (0.55 + 0.45 * smooth_random(0.5) ** 1.3)
    return stereo(ch)


def bubbles():
    # water bubbles: small rising blips over a soft murmur
    def ch():
        b = _drops(22, N, 350, 1400, 0.008, 0.035, amp_pow=1.2, chirp=0.6)
        murmur = lp(pink(), 700, 2) * 0.02 * (0.6 + 0.4 * smooth_random(0.8))
        return b + murmur * 10
    return stereo(ch)


def fabric():
    # cloth / fabric swishes: soft brushed noise strokes
    def ch():
        out = np.zeros(N)
        for start in rng.integers(0, N - SR, rng.poisson(3.5 * N / SR)):
            L = int(rng.uniform(0.25, 0.8) * SR)
            seg = np.arange(L)
            env = np.sin(np.pi * seg / L) ** 2
            lo = rng.uniform(600, 1400)
            out[start:start + L] += bp(white(L), lo, lo * rng.uniform(3, 6), 1) * env * rng.uniform(0.4, 1.0)
        return lp(out, 7000, 1)
    return stereo(ch)


def pencil():
    # pencil on paper: rhythmic scratchy strokes with a woody resonance
    def ch():
        out = np.zeros(N)
        pos = 0
        while pos < N - SR // 2:
            L = int(rng.uniform(0.08, 0.35) * SR)
            seg = np.arange(L)
            env = np.sin(np.pi * seg / L) ** 0.8
            grit = impulses(rng.uniform(1500, 3500), L, amp_pow=1.8)
            grit = bp(grit, 1800, 7000, 2)
            out[pos:pos + L] += grit * env * rng.uniform(0.5, 1.0)
            pos += L + int(rng.uniform(0.04, 0.25) * SR)
        return signal.lfilter([1], [1, -0.35], out)
    return stereo(ch)


def sea():
    # distant sea: slow waves of soft noise rolling in and out
    wave = (0.5 - 0.5 * np.cos(2 * np.pi * t / 2.25)) ** 1.6
    def ch():
        x = pink()
        out = np.zeros(N)
        blk = 512
        zi = None
        for i in range(0, N, blk):
            fc = 400 + 2600 * float(wave[i])
            sos = signal.butter(2, fc, fs=SR, output="sos")
            if zi is None:
                zi = np.zeros((sos.shape[0], 2))
            out[i:i + blk], zi = signal.sosfilt(sos, x[i:i + blk], zi=zi)
        foam = bp(white(), 3000, 10000, 1) * 0.06 * wave ** 3
        return out * (0.25 + 0.75 * wave) + foam * np.std(out) * 8
    return stereo(ch)


def white_n():
    return stereo(lambda: white())


def pink_n():
    return stereo(lambda: hp(pink(), 20))


def brown_n():
    return stereo(lambda: hp(brown(), 25))


def blue_n():
    # +3 dB/oct: white noise through a first difference, softened at the very top
    return stereo(lambda: lp(np.diff(white(N + 1)) * 0.7 + white() * 0.3, 16000, 1))


CLASSIC_NOISES = [("WHITE", white_n), ("PINK", pink_n), ("BROWN", brown_n), ("BLUE", blue_n)]

NEW_NOISES = [
    ("RAIN", rain), ("FIRE", fire), ("LEAVES", leaves), ("SAND", sand),
    ("BUBBLES", bubbles), ("FABRIC", fabric), ("PENCIL", pencil), ("SEA", sea),
]


# ---------------------------------------------------------------- 0.99.5: tonal "organic" noises (like Serum's Organics/AC hum)

def _hum(f0, tilt, even_boost, buzz, crackle_rate, seed_detune=0.0):
    n = N
    tt = np.arange(n) / SR
    wob = 1.0 + 0.002 * np.sin(2 * np.pi * 0.37 * tt)               # the mains drifts a hair
    ph = 2 * np.pi * f0 * np.cumsum(wob) / SR
    def ch():
        x = np.zeros(n)
        for k in range(1, int(16000 / f0)):
            a = k ** -tilt * (even_boost if k % 2 == 0 else 1.0)
            x += a * np.sin(k * ph + rng.random() * 6.283)
        # rectifier buzz: sharp spikes twice per cycle, band-limited
        spikes = np.maximum(0.0, np.abs(np.sin(ph)) - 0.92) ** 2
        x += bp(spikes, 600, 9000, 2) * buzz * np.std(x) / (np.std(bp(spikes, 600, 9000, 2)) + 1e-9)
        x *= 0.85 + 0.15 * smooth_random(0.8)                          # slow life in the level
        cr = bp(impulses(crackle_rate, amp_pow=2.0), 1500, 8000, 2)
        x += cr * np.std(x) * 1.5
        x += bp(pink(), 100, 6000, 1) * np.std(x) * 0.05              # a little air of the room
        return x
    return stereo(ch)


def ac_hum():
    return _hum(50.0, 1.05, 1.8, 0.35, 6)


def buzz():
    return _hum(60.0, 0.75, 1.2, 0.8, 14)


def transformer():
    return _hum(100.0, 1.4, 2.5, 0.15, 2)


HUM_NOISES = [("AC HUM", ac_hum), ("BUZZ", buzz), ("TRANSFORMER", transformer)]


def finish(x):
    n_loop = int(DUR * SR)
    nx = int(XF * SR)
    body = x[:n_loop].copy()
    tail = x[n_loop:n_loop + nx]
    fade = np.sin(np.linspace(0, np.pi / 2, nx)) ** 2
    body[:nx] = body[:nx] * fade[:, None] + tail * (1 - fade[:, None])
    body -= body.mean(axis=0)
    rms = np.sqrt(np.mean(body ** 2))
    body *= 10 ** (-18 / 20) / (rms + 1e-12)          # -18 dBFS RMS
    peak = np.max(np.abs(body))
    if peak > 0.89 * 4.0:                             # sparse sounds: allow up to 12 dB of soft limiting
        body *= 0.89 * 4.0 / peak
    body = 0.89 * np.tanh(body / 0.89)                # soft limiter, -1 dBFS ceiling
    return body


if __name__ == "__main__":
    out = next((a for a in sys.argv[1:] if not a.startswith("--")), "noises")
    os.makedirs(out, exist_ok=True)
    for name, fn in (HUM_NOISES if "--hum" in sys.argv else CLASSIC_NOISES if "--classic" in sys.argv else NEW_NOISES if "--new" in sys.argv else NOISES):
        x = finish(fn())
        path = os.path.join(out, name.replace(" ", "_") + ".wav")
        wavfile.write(path, SR, (x * 32767).astype(np.int16))
        print(f"{name:12s} rms {20*np.log10(np.sqrt(np.mean(x**2))):6.1f} dB  peak {20*np.log10(np.max(np.abs(x))):5.1f} dB")
