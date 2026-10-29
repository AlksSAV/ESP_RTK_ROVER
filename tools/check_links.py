"""Check relative links and GitHub-style anchors in the project's Markdown.

Run from the project root: python tools/check_links.py
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP = (".pio", "node_modules", ".git")


def markdown_files():
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in SKIP]
        for name in filenames:
            if name.endswith(".md"):
                yield os.path.join(dirpath, name)


def anchors(path):
    """GitHub-style heading anchors for one file."""
    found = set()
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            m = re.match(r"^(#{1,6})\s+(.*?)\s*$", line)
            if not m:
                continue
            text = m.group(2).lower()
            text = re.sub(r"`([^`]*)`", r"\1", text)
            text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)
            text = re.sub(r"[^\w\s\u0400-\u04ff-]", "", text)
            found.add(text.strip().replace(" ", "-"))
    return found


def main():
    files = sorted(markdown_files())
    cache = {f: anchors(f) for f in files}
    broken = 0

    for path in files:
        with open(path, encoding="utf-8") as fh:
            content = fh.read()
        for m in re.finditer(r"\[([^\]]*)\]\(([^)]+)\)", content):
            target = m.group(2)
            if target.startswith(("http://", "https://", "mailto:")):
                continue
            file_part, _, fragment = target.partition("#")
            if file_part:
                dest = os.path.normpath(os.path.join(os.path.dirname(path), file_part))
                if not os.path.exists(dest):
                    print(f"BROKEN FILE   {os.path.relpath(path, ROOT)} -> {target}")
                    broken += 1
                    continue
            else:
                dest = path
            if fragment:
                if dest not in cache:
                    cache[dest] = anchors(dest)
                if fragment not in cache[dest]:
                    print(f"BROKEN ANCHOR {os.path.relpath(path, ROOT)} -> {target}")
                    broken += 1

    print(f"\n{len(files)} files checked, {broken} broken")
    return 1 if broken else 0


if __name__ == "__main__":
    sys.exit(main())
