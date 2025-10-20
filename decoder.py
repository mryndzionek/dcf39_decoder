import math

import numpy as np
import scipy.signal as sig
import scipy.io.wavfile as wv

import matplotlib.pyplot as plt


SAMPLES_PER_SYMBOL = 10
DCF39_F_SHIFT_HZ = 370


class Telegram:
    def __init__(self, data):
        self.num = (data[4] >> 4) & 0b1111
        self.a1 = data[5]
        self.a2 = data[6]
        self.user_data = data[7:-2]
        self.crc = data[-2]

    def __str__(self):
        data = " ".join([f"0x{s:02X}" for s in self.user_data])
        return (
            f"telegram number: {self.num}, A1: 0x{self.a1:02X}, A2: 0x{self.a2:02X}, "
            f"user data: {data}, CRC: 0x{self.crc:02X}"
        )


class DateTimeTelegram:
    def __init__(self, data):
        if data[0] != 0:
            raise ValueError("First byte not zero")
        if len(data) < 7:
            raise ValueError("Data too short")
        self.second = data[1] >> 2
        self.minute = data[2]
        self.hour = data[3] & ~((1 << 7))
        self.dst = data[3] & (1 << 7) != 0
        self.dayOfWeek = data[4] >> 5
        self.dayOfMonth = data[4] & ~(0b111 << 5)
        self.month = data[5]
        self.year = 2000 + data[6]

    def __str__(self):
        dst = "DST" if self.dst else ""
        dow = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"][self.dayOfWeek]
        return (
            f"{self.year}-{self.month}-{self.dayOfMonth} ({dow}) - "
            f"{self.hour}:{self.minute}:{self.second} {dst}"
        )


class DCF39Decoder:
    def __init__(self):
        self.state = 0
        self.idx = 0
        self.byte_count = 0
        self.bits = 0
        self.tele_num = 0
        self.data = []
        self.data_len = 0
        self.telegrams = []

        self.bc = 0
        self.bit_idx = 0

    def reset(self):
        self.state = 0
        self.data = []
        self.byte_count = 0

    def decode_byte(self, bit):
        self.bits |= int(bit) << self.bit_idx
        self.bit_idx += 1
        if self.idx == 10 * SAMPLES_PER_SYMBOL:
            self.bit_idx = 0
            parity = self.bits >> 8
            if parity & 0b10 == 0:
                print("Wrong stop bit")
                self.reset()
                return
            byte = self.bits & ~(0b11 << 8)
            bc = byte.bit_count()
            if parity & 0b1:
                bc += 1
            if bc % 2 != 0:
                print("Wrong parity")
                self.reset()
                return
            self.data.append(byte)
            # print(self.data)
            self.state = 0
            self.byte_count += 1
            return byte

    def decode_telegram(self, byte):
        if self.byte_count == 1:
            if byte != 0x68:
                print(f"Wrong start character (expected 0x68, got 0x{byte:02X})")
                self.reset()
                return
        elif self.byte_count == 3:
            if self.data[1] != self.data[2]:
                print(
                    f"Wrong repeated length field (0x{self.data[1]:02X} != 0x{self.data[2]:02X})"
                )
                self.reset()
                return
            self.data_len = self.data[1]
        elif self.byte_count == 4:
            if byte != 0x68:
                print(
                    f"Wrong repeated start character (expected 0x68, got 0x{byte:02X})"
                )
                self.reset()
                return
        elif self.byte_count == 5 + self.data_len:
            crc_exp = byte
            crc = sum(self.data[4:-1]) & 0xFF
            if crc_exp != crc:
                print(f"Wrong CRC (expected 0x{crc_exp:02X}, got 0x{crc: 02X})")
                self.reset()
                return
        elif self.byte_count == 6 + self.data_len:
            if byte != 0x16:
                print(f"Wrong stop character (expected 0x16, got 0x{byte:02X})")
                self.reset()
                return

            return Telegram(self.data)

    def update(self, bit):
        self.idx += 1
        if self.state == 0 and bit == 0:
            self.state = 1
            self.idx = 1
            self.bits = 0
            self.bc = 0

        if self.state == 1:
            self.bc += bit
            if self.idx == SAMPLES_PER_SYMBOL:
                if self.bc < (SAMPLES_PER_SYMBOL / 2):
                    self.state = 2
                    self.idx = 0
                    self.bc = 0
                else:
                    self.state = 0
        elif self.state == 2:
            self.bc += bit
            if self.idx % SAMPLES_PER_SYMBOL == 0:
                byte = self.decode_byte(self.bc > (SAMPLES_PER_SYMBOL / 2))
                self.bc = 0
                if byte:
                    telegram = self.decode_telegram(byte)
                    if telegram:
                        if telegram.a1 == 0 and telegram.a2 == 0:
                            telegram = DateTimeTelegram(telegram.user_data)
                        self.byte_count = 0
                        self.data_len = 0
                        self.telegrams.append(telegram)
                        self.data = []


def demod(asig, sr, ff=250):

    ref = 1.0 / (2 * math.pi * 0.99)
    r_prime = np.concatenate((np.zeros(1), asig[:-1]))
    fm_demod = (np.angle(np.conj(r_prime) * asig)) * ref * sr

    if ff > 0:
        demod = sig.filtfilt(0.154, [1.0, -(1 - 0.154)], fm_demod)
    else:
        demod = fm_demod

    demod = demod[round(sr * 0.1) : -round(sr * 0.1)]
    h, e = np.histogram(demod, 1000)
    f = e[np.argmax(h)]
    return demod - f, f


def plot_spectrum(ax, c, f_shift, sr):
    f, t, Zxx = sig.stft(c, sr)
    ax.pcolormesh(t, f, np.abs(Zxx), cmap="inferno")
    ax.set_title("Spectrum (amplitude)")
    ax.set_ylim(f_shift - 100, f_shift + DCF39_F_SHIFT_HZ + 100)
    # ax.set_xlabel("time [s]")


assert SAMPLES_PER_SYMBOL <= 255
plots = True

if plots:
    fig, axs = plt.subplots(4)
    fig.set_figwidth(20)
    fig.set_figheight(10)
    fig.tight_layout()
    fig.subplots_adjust(hspace=0.3)
    for ax in axs:
        ax.grid(True)

sr, samples = wv.read("resources/c_sample.wav")
print("Samplerate: {}Hz".format(sr))
samples = samples.astype(np.float32) / np.iinfo(type(samples[0])).max

# Add simulated noise
if False:
    noise_amp = np.average(np.abs(samples)) * 0.8
    noise = 2 * noise_amp * np.random.rand(len(samples)) - noise_amp
    samples = samples + noise

if plots:
    t_end = len(samples) / sr
    axs[0].plot(
        np.arange(0, t_end, 1 / sr),
        samples,
    )
    axs[0].set_xlim(0, t_end)
    axs[0].set_title("Input samples (WAV file)")
    # axs[0].set_xlabel("time [s]")

a = sig.hilbert(samples)

fm, f = demod(a, sr, DCF39_F_SHIFT_HZ + 50)
print("Freq offset {}Hz".format(f))
fm = sig.medfilt(
    fm, SAMPLES_PER_SYMBOL if SAMPLES_PER_SYMBOL % 2 == 1 else SAMPLES_PER_SYMBOL + 1
)

if plots:
    plot_spectrum(axs[1], samples, f, sr)
    axs[2].plot(fm)
    axs[2].set_xlim(0, len(fm))
    axs[2].set_ylim(-100, DCF39_F_SHIFT_HZ + 100)
    axs[2].set_title("FM demodulated signal")
    # axs[2].set_xlabel("sample number")


print(f"Resample freq {200 * SAMPLES_PER_SYMBOL}")  # 200bps
rfac = sr / (200 * SAMPLES_PER_SYMBOL)
fm = sig.resample(fm, round(len(fm) / rfac))
bstream = fm > DCF39_F_SHIFT_HZ / 2

if plots:
    axs[3].plot(
        bstream,
        color="0.75",
        marker="o",
        markersize=7,
        markeredgecolor="darkblue",
        markerfacecolor="#00000000",
        markeredgewidth=0.5,
        markevery=SAMPLES_PER_SYMBOL,
    )
    axs[3].set_xlim(0, len(bstream))
    axs[3].set_title(f"Bitstream ({SAMPLES_PER_SYMBOL} samples per symbol)")
    # axs[3].set_xlabel("sample number")


bstream = (bstream == False).astype(np.uint8)

decoder = DCF39Decoder()
for bit in bstream:
    decoder.update(bit)

for t in decoder.telegrams:
    print(t)

if plots:
    plt.show()
    fig.savefig("resources/plots.png")
