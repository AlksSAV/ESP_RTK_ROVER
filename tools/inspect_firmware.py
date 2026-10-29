"""Compare the committed installer image with the current build.

The image in install/firmware/ is what the browser installer flashes, and it is
built with the dashboard embedded into the app partition by tools/embed_html.py.
A change in image size is therefore expected whenever the dashboard changes, and
suspicious when nothing did - so check what is actually inside before committing.

Run from the project root: python tools/inspect_firmware.py
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGE = os.path.join(ROOT, "install", "firmware", "xiao-c6-ble.bin")
HTML = os.path.join(ROOT, "web", "rtk-monitor.html")


def committed_image():
    out = subprocess.run(
        ["git", "show", "HEAD:install/firmware/xiao-c6-ble.bin"],
        cwd=ROOT, capture_output=True, check=True,
    )
    return out.stdout


def escaped(data):
    """The dashboard is stored as "\\xNN" escapes inside a PROGMEM string."""
    return "".join("\\x%02x" % c for c in data).encode("ascii")


def report(name, image, html):
    print(f"{name}: {len(image)} bytes, ESP32 image signature: "
          f"{'ok' if image[:1] == b'\\xe9' else 'MISSING'}")
    head = escaped(html[:32])
    print(f"   dashboard embedded: {head in image}")
    for marker in (b"Web Bluetooth", b"navigator.bluetooth", b"WebSocket"):
        print(f"   {marker.decode():22s} {marker in image}")


def main():
    if not os.path.exists(IMAGE):
        print("no image at", IMAGE)
        return 1
    html = open(HTML, "rb").read()
    current = open(IMAGE, "rb").read()
    old = committed_image()

    print(f"dashboard source: {len(html)} bytes\n")
    report("committed", old, html)
    print()
    report("current  ", current, html)

    print()
    if old == current:
        print("identical - nothing to commit for the image")
    else:
        size_delta = len(current) - len(old)
        print(f"differs, {size_delta:+d} bytes")
        n = min(len(old), len(current))
        for i in range(n):
            if old[i] != current[i]:
                print(f"first difference at offset {i} (0x{i:x})")
                break
    return 0


if __name__ == "__main__":
    sys.exit(main())
