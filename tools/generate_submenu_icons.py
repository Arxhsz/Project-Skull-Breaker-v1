from pathlib import Path

FG = 0xFFFF
BG = 0x0000


class IconCanvas:
    def __init__(self, size: int):
        self.size = size
        self.pixels = [[BG for _ in range(size)] for _ in range(size)]

    def set(self, x: int, y: int, color: int = FG):
        if 0 <= x < self.size and 0 <= y < self.size:
            self.pixels[y][x] = color

    def dot(self, x: int, y: int, thickness: int = 1, color: int = FG):
        radius = max(0, thickness - 1)
        for yy in range(y - radius, y + radius + 1):
            for xx in range(x - radius, x + radius + 1):
                self.set(xx, yy, color)

    def hline(self, x0: int, x1: int, y: int, thickness: int = 1, color: int = FG):
        if x0 > x1:
            x0, x1 = x1, x0
        for yy in range(y, y + thickness):
            for xx in range(x0, x1 + 1):
                self.set(xx, yy, color)

    def vline(self, x: int, y0: int, y1: int, thickness: int = 1, color: int = FG):
        if y0 > y1:
            y0, y1 = y1, y0
        for xx in range(x, x + thickness):
            for yy in range(y0, y1 + 1):
                self.set(xx, yy, color)

    def fill_rect(self, x: int, y: int, w: int, h: int, color: int = FG):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.set(xx, yy, color)

    def rect(self, x: int, y: int, w: int, h: int, thickness: int = 1, color: int = FG):
        self.hline(x, x + w - 1, y, thickness, color)
        self.hline(x, x + w - 1, y + h - thickness, thickness, color)
        self.vline(x, y, y + h - 1, thickness, color)
        self.vline(x + w - thickness, y, y + h - 1, thickness, color)

    def line(self, x0: int, y0: int, x1: int, y1: int, thickness: int = 1, color: int = FG):
        dx = abs(x1 - x0)
        dy = -abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx + dy
        while True:
            self.dot(x0, y0, thickness, color)
            if x0 == x1 and y0 == y1:
                break
            e2 = err * 2
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def circle(self, cx: int, cy: int, r: int, thickness: int = 1, color: int = FG):
        x = r
        y = 0
        err = 1 - x
        while x >= y:
            points = (
                (cx + x, cy + y), (cx + y, cy + x),
                (cx - y, cy + x), (cx - x, cy + y),
                (cx - x, cy - y), (cx - y, cy - x),
                (cx + y, cy - x), (cx + x, cy - y),
            )
            for px, py in points:
                self.dot(px, py, thickness, color)
            y += 1
            if err < 0:
                err += 2 * y + 1
            else:
                x -= 1
                err += 2 * (y - x + 1)

    def fill_circle(self, cx: int, cy: int, r: int, color: int = FG):
        for yy in range(cy - r, cy + r + 1):
            for xx in range(cx - r, cx + r + 1):
                if ((xx - cx) * (xx - cx)) + ((yy - cy) * (yy - cy)) <= r * r:
                    self.set(xx, yy, color)

    def arc_top(self, cx: int, cy: int, r: int, thickness: int = 1, color: int = FG):
        for xx in range(-r, r + 1):
            yy_sq = r * r - xx * xx
            if yy_sq < 0:
                continue
            yy = int(round(yy_sq ** 0.5))
            self.dot(cx + xx, cy - yy, thickness, color)

    def scale_to(self, target_size: int):
        if target_size == self.size:
            return self.pixels

        scaled = [[BG for _ in range(target_size)] for _ in range(target_size)]
        for yy in range(target_size):
            source_y = min(self.size - 1, (yy * self.size) // target_size)
            for xx in range(target_size):
                source_x = min(self.size - 1, (xx * self.size) // target_size)
                scaled[yy][xx] = self.pixels[source_y][source_x]
        return scaled

    def save(self, path: Path, target_size: int):
        pixels = self.scale_to(target_size)
        data = bytearray()
        for row in pixels:
            for pixel in row:
                data.extend(int(pixel).to_bytes(2, "little"))
        path.write_bytes(data)


def draw_magnifier(c: IconCanvas, cx: int, cy: int, r: int):
    c.circle(cx, cy, r, 2)
    c.line(cx + r - 1, cy + r - 1, cx + r + 3, cy + r + 3, 2)


def draw_bt_rune(c: IconCanvas, x: int, y: int, thick: int = 2):
    c.vline(x + 2, y, y + 11, thick)
    c.line(x + 2, y, x + 6, y + 4, thick)
    c.line(x + 2, y + 5, x + 6, y + 2, thick)
    c.line(x + 2, y + 6, x + 6, y + 9, thick)
    c.line(x + 2, y + 11, x + 6, y + 7, thick)


def draw_antenna(c: IconCanvas, x: int, y: int):
    c.vline(x, y, y + 5, 2)
    c.line(x - 1, y + 6, x, y + 9, 2)
    c.line(x + 1, y + 6, x, y + 9, 2)
    c.arc_top(x, y, 4, 2)
    c.arc_top(x, y, 7, 2)


def draw_chip(c: IconCanvas, x: int, y: int, w: int = 8, h: int = 8):
    c.rect(x, y, w, h, 2)
    for yy in (y + 1, y + 4, y + 7):
        c.hline(x - 2, x - 1, yy, 1)
        c.hline(x + w, x + w + 1, yy, 1)
    c.fill_rect(x + 2, y + 2, 4, 4)


def icon_packet_monitor():
    c = IconCanvas(18)
    c.rect(1, 2, 10, 7, 2)
    c.fill_rect(3, 5, 2, 2)
    c.line(4, 11, 6, 9, 2)
    c.line(6, 9, 8, 11, 2)
    c.line(8, 11, 11, 7, 2)
    c.fill_rect(13, 4, 2, 8)
    c.fill_rect(15, 6, 2, 4)
    return c


def icon_beacon_spammer():
    c = IconCanvas(18)
    draw_antenna(c, 7, 8)
    c.fill_circle(14, 4, 2)
    c.line(12, 6, 16, 2, 1)
    c.line(12, 2, 16, 6, 1)
    return c


def icon_ble_scanner():
    c = IconCanvas(18)
    draw_bt_rune(c, 2, 3, 2)
    draw_magnifier(c, 12, 11, 3)
    return c


def icon_ble_device():
    c = IconCanvas(18)
    c.rect(2, 1, 8, 14, 2)
    c.fill_rect(4, 3, 4, 1)
    c.fill_rect(5, 12, 2, 1)
    draw_bt_rune(c, 11, 4, 1)
    return c


def icon_ble_radar():
    c = IconCanvas(18)
    c.circle(8, 9, 3, 1)
    c.circle(8, 9, 6, 1)
    c.circle(8, 9, 8, 1)
    c.line(8, 9, 14, 5, 2)
    c.fill_circle(13, 4, 1)
    return c


def icon_signal_logger():
    c = IconCanvas(18)
    c.rect(2, 2, 12, 12, 2)
    c.line(4, 11, 7, 8, 2)
    c.line(7, 8, 9, 9, 2)
    c.line(9, 9, 12, 5, 2)
    c.fill_rect(14, 4, 2, 8)
    return c


def icon_channel_scanner():
    c = IconCanvas(18)
    c.fill_rect(2, 9, 2, 6)
    c.fill_rect(5, 7, 2, 8)
    c.fill_rect(8, 4, 2, 11)
    c.fill_rect(11, 6, 2, 9)
    c.fill_rect(14, 2, 2, 13)
    c.hline(2, 16, 15, 1)
    return c


def icon_noise_analyzer():
    c = IconCanvas(18)
    c.hline(2, 15, 14, 1)
    c.line(2, 9, 4, 7, 2)
    c.line(4, 7, 6, 11, 2)
    c.line(6, 11, 8, 5, 2)
    c.line(8, 5, 10, 10, 2)
    c.line(10, 10, 12, 6, 2)
    c.line(12, 6, 15, 8, 2)
    return c


def icon_rf24_test():
    c = IconCanvas(18)
    draw_chip(c, 4, 4, 9, 9)
    c.line(6, 10, 8, 8, 2)
    c.line(8, 8, 10, 10, 2)
    c.line(10, 10, 13, 6, 2)
    return c


def icon_rf_scanner():
    c = IconCanvas(18)
    draw_antenna(c, 5, 9)
    draw_magnifier(c, 12, 11, 3)
    return c


def icon_rf_monitor():
    c = IconCanvas(18)
    c.arc_top(8, 12, 6, 2)
    c.line(8, 12, 12, 8, 2)
    c.fill_circle(8, 12, 1)
    c.fill_rect(3, 8, 2, 2)
    c.fill_rect(7, 5, 2, 2)
    c.fill_rect(12, 8, 2, 2)
    return c


def icon_signal_capture():
    c = IconCanvas(18)
    c.rect(2, 9, 12, 5, 2)
    c.line(1, 8, 4, 8, 1)
    c.line(4, 8, 6, 6, 2)
    c.line(6, 6, 8, 10, 2)
    c.line(8, 10, 10, 7, 2)
    c.line(10, 7, 13, 7, 2)
    c.fill_circle(15, 4, 2)
    return c


def icon_frequency_sweep():
    c = IconCanvas(18)
    c.fill_rect(2, 12, 2, 3)
    c.fill_rect(5, 10, 2, 5)
    c.fill_rect(8, 6, 2, 9)
    c.fill_rect(11, 9, 2, 6)
    c.fill_rect(14, 8, 2, 7)
    c.vline(9, 2, 15, 1)
    return c


def icon_rf_transmit():
    c = IconCanvas(18)
    draw_antenna(c, 4, 9)
    c.line(8, 9, 15, 9, 2)
    c.line(12, 6, 15, 9, 2)
    c.line(12, 12, 15, 9, 2)
    return c


def icon_radio_nrf24():
    c = IconCanvas(16)
    draw_chip(c, 4, 4, 7, 7)
    c.line(9, 8, 12, 6, 1)
    c.line(9, 8, 12, 10, 1)
    return c


def icon_radio_cc1101():
    c = IconCanvas(16)
    draw_antenna(c, 5, 8)
    c.fill_rect(9, 6, 3, 1)
    c.fill_rect(10, 4, 2, 1)
    c.fill_rect(10, 8, 2, 1)
    return c


ICON_SPECS = {
    "packet_monitor.bin": (icon_packet_monitor, 36),
    "beacon_spammer.bin": (icon_beacon_spammer, 36),
    "ble_scanner.bin": (icon_ble_scanner, 36),
    "ble_device.bin": (icon_ble_device, 36),
    "ble_radar.bin": (icon_ble_radar, 36),
    "signal_logger.bin": (icon_signal_logger, 36),
    "channel_scanner.bin": (icon_channel_scanner, 36),
    "noise_analyzer.bin": (icon_noise_analyzer, 36),
    "rf24_test.bin": (icon_rf24_test, 36),
    "rf_scanner.bin": (icon_rf_scanner, 36),
    "rf_monitor.bin": (icon_rf_monitor, 36),
    "signal_capture.bin": (icon_signal_capture, 36),
    "frequency_sweep.bin": (icon_frequency_sweep, 36),
    "rf_transmit.bin": (icon_rf_transmit, 36),
    "radio_nrf24.bin": (icon_radio_nrf24, 16),
    "radio_cc1101.bin": (icon_radio_cc1101, 16),
}


def main():
    repo_root = Path(__file__).resolve().parents[1]
    icons_dir = repo_root / "data" / "icons"
    icons_dir.mkdir(parents=True, exist_ok=True)

    for filename, (builder, target_size) in ICON_SPECS.items():
        builder().save(icons_dir / filename, target_size)
        print(f"wrote {filename}")


if __name__ == "__main__":
    main()
