#!/bin/sh
# Build Midnight, install it as an xscreensaver hack, and make it the
# active screensaver for the current user.
set -eu
cd "$(dirname "$0")"

command -v xscreensaver >/dev/null 2>&1 || {
	echo "xscreensaver is not installed. Install it first, e.g.:" >&2
	echo "  sudo apt install xscreensaver   # Debian/Ubuntu" >&2
	echo "  sudo dnf install xscreensaver   # Fedora" >&2
	echo "  sudo pacman -S xscreensaver     # Arch" >&2
	exit 1
}

make midnight

# Install system-wide next to the other hacks if we can, else per-user.
hackdir=""
for d in /usr/libexec/xscreensaver /usr/lib/xscreensaver /usr/local/libexec/xscreensaver; do
	[ -d "$d" ] && hackdir=$d && break
done
if [ -n "$hackdir" ] && { [ -w "$hackdir" ] || command -v sudo >/dev/null 2>&1; }; then
	if [ -w "$hackdir" ]; then install -m 755 midnight "$hackdir/midnight"
	else sudo install -m 755 midnight "$hackdir/midnight"; fi
	cmd="midnight -root"
else
	mkdir -p "$HOME/.local/bin"
	install -m 755 midnight "$HOME/.local/bin/midnight"
	cmd="$HOME/.local/bin/midnight -root"
fi
echo "Installed: $cmd"

# Point ~/.xscreensaver at Midnight: add it as the first program and select it.
conf="$HOME/.xscreensaver"
[ -f "$conf" ] && cp "$conf" "$conf.bak.$(date +%Y%m%d%H%M%S)"
python3 - "$conf" "$cmd" <<'PY'
import os, re, sys
path, cmd = sys.argv[1], sys.argv[2]
entry = '\t"Midnight" ' + cmd + ' \\n\\\n'
text = open(path).read() if os.path.exists(path) else ""

# Drop any previous Midnight entry so re-running is idempotent.
text = re.sub(r'^[ \t-]*"Midnight"[^\n]*\n', '', text, flags=re.M)

if re.search(r'^programs:', text, flags=re.M):
    text = re.sub(r'^programs:[ \t]*\\?\n', lambda m: 'programs:\\\n' + entry, text, count=1, flags=re.M)
else:
    text += '\nprograms:\\\n' + entry + '\n'

for key, val in (("mode", "one"), ("selected", "0")):
    if re.search(r'^%s:' % key, text, flags=re.M):
        text = re.sub(r'^%s:.*$' % key, lambda m: '%s:\t\t%s' % (key, val), text, count=1, flags=re.M)
    else:
        text = '%s:\t\t%s\n' % (key, val) + text
open(path, "w").write(text)
PY
echo "Set Midnight as the active screensaver in $conf"

if xscreensaver-command -version >/dev/null 2>&1; then
	xscreensaver-command -restart >/dev/null 2>&1 || true
	echo "Restarted xscreensaver. Preview now with: xscreensaver-command -activate"
else
	echo "Start the daemon with: xscreensaver --no-splash &"
fi
