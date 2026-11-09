"""Compare the committed installer image with the current build.

The image in install/firmware/ is what the browser installer flashes, and it is
built with the dashboard embedded into the app partition by tools/embed_html.py.
A change in image size is therefore expected whenever the dashboard changes, and
suspicious when nothing did - so check what is actually inside before committing.

Two things are checked, both of which have failed silently before:
  * the image starts with the ESP32 magic byte 0xE9 (an esptool-generated
    factory image, not a merged or truncated file);
  * web/rtk-monitor.html appears inside the image byte for byte, which proves
    the image was rebuilt after the last dashboard edit.

Run from the project root: python tools/inspect_firmware.py
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMAGE = os.path.join(ROOT, "install", "firmware", "xiao-c6-ble.bin")
HTML = os.path.join(ROOT, "web", "rtk-monitor.html")
FACTORY = os.path.join(ROOT, ".pio", "build", "xiao_esp32c6_ble",
                       "firmware.factory.bin")

ESP_IMAGE_MAGIC = 0xE9


def committed_image():
    out = subprocess.run(
        ["git", "show", "HEAD:install/firmware/xiao-c6-ble.bin"],
        cwd=ROOT, capture_output=True, check=True,
    )
    return out.stdout


def report(name, image, html):
    """Say whether this image is a valid ESP32 image with this dashboard in it.

    The dashboard is compiled from "\\xNN" escapes (see embed_html.py) into
    plain bytes, so the page sits inside the app image verbatim. Searching for
    its source spelling instead finds nothing and reports a healthy image as
    broken - which is what this script did until it was fixed.
    """
    magic_ok = len(image) > 0 and image[0] == ESP_IMAGE_MAGIC
    embedded = html in image
    head = html[:64]

    print(f"{name}: {len(image)} bytes, ESP32 image signature: "
          f"{'ok' if magic_ok else 'MISSING'}")
    if embedded:
        print("   dashboard embedded: yes")
    elif head in image:
        print("   dashboard embedded: STALE - image holds an older page")
    else:
        print("   dashboard embedded: no - page not in this image at all")
    return magic_ok and embedded


def main():
    if not os.path.exists(IMAGE):
        print("no image at", IMAGE)
        return 1
    html = open(HTML, "rb").read()
    current = open(IMAGE, "rb").read()
    old = committed_image()

    print(f"dashboard source: {len(html)} bytes\n")
    old_ok = report("committed", old, html)
    print()
    new_ok = report("current  ", current, html)

    print()
    if old == current:
        print("identical to the committed image - nothing to commit")
    else:
        size_delta = len(current) - len(old)
        print(f"differs from the committed image, {size_delta:+d} bytes")
        n = min(len(old), len(current))
        for i in range(n):
            if old[i] != current[i]:
                print(f"first difference at offset {i} (0x{i:x})")
                break

    # The build directory is the source of install/firmware/; a mismatch means
    # the committed copy was not refreshed from the last build.
    if os.path.exists(FACTORY):
        factory = open(FACTORY, "rb").read()
        print()
        print(f"build dir: {os.path.relpath(FACTORY, ROOT)} "
              f"({len(factory)} bytes)")
        print("   matches install/firmware/:",
              "yes" if factory == current else "NO - rebuild and copy")

    return 0 if (old_ok and new_ok) else 2


if __name__ == "__main__":
    sys.exit(main())
