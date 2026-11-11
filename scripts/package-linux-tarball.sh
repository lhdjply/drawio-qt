#!/usr/bin/env bash
# Build a portable Linux tar.gz from the freshly built .deb.
# Layout:
#   drawio-qt              launcher (sets LD_LIBRARY_PATH/QT_PLUGIN_PATH)
#   bin/drawio-qt          real binary
#   bin/qt.conf            points Qt at bundled plugins/translations
#   lib/ plugins/ translations/ share/
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_NAME="drawio-qt"
VERSION="$(sed 's/^[vV]//' "$REPO_ROOT/VERSION" | tr -d '[:space:]')"
ARCH=""
FROM_DEB=""
OUT_DIR="$REPO_ROOT/dist"

while [ $# -gt 0 ]; do
    case "$1" in
        --arch) ARCH="${2:?}"; shift 2 ;;
        --from-deb) FROM_DEB="${2:?}"; shift 2 ;;
        --out) OUT_DIR="${2:?}"; shift 2 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done
[ -n "$ARCH" ] || { echo "--arch is required" >&2; exit 2; }
[ -n "$FROM_DEB" ] || { echo "--from-deb is required" >&2; exit 2; }
[ -f "$FROM_DEB" ] || { echo "missing deb: $FROM_DEB" >&2; exit 1; }

STAGE="$OUT_DIR/${APP_NAME}-${VERSION}-linux-${ARCH}"
TARBALL="$OUT_DIR/${APP_NAME}-${VERSION}-linux-${ARCH}.tar.gz"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

rm -rf "$STAGE" "$TARBALL"
mkdir -p "$STAGE/bin" "$STAGE/lib" "$STAGE/plugins" "$STAGE/translations" \
         "$STAGE/share/applications" "$STAGE/share/icons"

echo "==> extracting $FROM_DEB"
dpkg-deb -x "$FROM_DEB" "$TMP/deb"
cp "$TMP/deb/usr/bin/$APP_NAME" "$STAGE/bin/$APP_NAME"
chmod 0755 "$STAGE/bin/$APP_NAME"

for desktop in "$TMP"/deb/usr/share/applications/*.desktop; do
    [ -f "$desktop" ] && cp "$desktop" "$STAGE/share/applications/"
done
if [ -d "$TMP/deb/usr/share/icons" ]; then
    cp -a "$TMP/deb/usr/share/icons/." "$STAGE/share/icons/"
fi
for doc in README.md LICENSE CHANGELOG.md; do
    [ -f "$REPO_ROOT/$doc" ] && cp "$REPO_ROOT/$doc" "$STAGE/"
done

# Collect shared libraries except glibc and graphics drivers, which must match
# the host.  Copies are keyed by soname because that is what the loader looks up.
declare -A SEEN=()
excluded() {
    case "$1" in
        ld-linux*|libc.so.*|libm.so.*|libmvec.so.*|libdl.so.*|libpthread.so.*) return 0 ;;
        librt.so.*|libresolv.so.*|libutil.so.*|libnsl.so.*|libanl.so.*) return 0 ;;
        libGL.so.*|libGLX*.so.*|libOpenGL.so.*|libGLdispatch.so.*|libEGL.so.*) return 0 ;;
        libdrm*.so.*|libvulkan.so.*|libnvidia-*.so.*|libcuda.so.*) return 0 ;;
    esac
    return 1
}
collect() {
    local file="$1" name path
    while IFS=$'\t' read -r name path; do
        [ -n "$name" ] || continue
        [ "$name" = "MISSING" ] && { echo "missing dependency $path for $file" >&2; exit 1; }
        [ -n "${SEEN[$name]:-}" ] && continue
        SEEN[$name]=1
        excluded "$name" && continue
        [ -f "$path" ] && cp -L "$path" "$STAGE/lib/$name"
    done < <(ldd "$file" 2>/dev/null | awk '/=>/ { if ($3=="not found") print "MISSING\t"$1; else print $1"\t"$3; next } NF==1 && $1 ~ /\.so/ { print $1"\t"$1 }')
}

echo "==> collecting shared libraries"
collect "$STAGE/bin/$APP_NAME"

QT_PLUGIN_DIR=""
for probe in qtpaths6 qmake6; do
    if command -v "$probe" >/dev/null 2>&1; then
        if [ "$probe" = qtpaths6 ]; then QT_PLUGIN_DIR="$($probe --plugin-dir 2>/dev/null || true)";
        else QT_PLUGIN_DIR="$($probe -query QT_INSTALL_PLUGINS 2>/dev/null || true)"; fi
        [ -n "$QT_PLUGIN_DIR" ] && [ -d "$QT_PLUGIN_DIR" ] && break
        QT_PLUGIN_DIR=""
    fi
done
if [ -z "$QT_PLUGIN_DIR" ]; then
    for guess in /usr/lib/*/qt6/plugins /usr/lib/qt6/plugins /usr/lib64/qt6/plugins; do
        [ -d "$guess" ] && { QT_PLUGIN_DIR="$guess"; break; }
    done
fi
if [ -n "$QT_PLUGIN_DIR" ]; then
    echo "==> collecting Qt plugins from $QT_PLUGIN_DIR"
    for dir in platforms imageformats iconengines tls; do
        [ -d "$QT_PLUGIN_DIR/$dir" ] || continue
        mkdir -p "$STAGE/plugins/$dir"
        for file in "$QT_PLUGIN_DIR/$dir"/*.so; do
            [ -f "$file" ] || continue
            base="$(basename "$file")"
            case "$base" in
                *gtk3*|*deepin*|*dxcb*|*dci*|*xraw*) continue ;;
            esac
            cp "$file" "$STAGE/plugins/$dir/"
            collect "$file"
        done
    done
fi

QT_TRANSLATIONS=""
for probe in qtpaths6 qmake6; do
    if command -v "$probe" >/dev/null 2>&1; then
        if [ "$probe" = qtpaths6 ]; then QT_TRANSLATIONS="$($probe --translations-dir 2>/dev/null || true)";
        else QT_TRANSLATIONS="$($probe -query QT_INSTALL_TRANSLATIONS 2>/dev/null || true)"; fi
        [ -n "$QT_TRANSLATIONS" ] && [ -d "$QT_TRANSLATIONS" ] && break
        QT_TRANSLATIONS=""
    fi
done
if [ -n "$QT_TRANSLATIONS" ]; then
    for qm in qtbase_zh_CN.qm qt_zh_CN.qm; do
        [ -f "$QT_TRANSLATIONS/$qm" ] && cp "$QT_TRANSLATIONS/$qm" "$STAGE/translations/"
    done
fi

cat > "$STAGE/bin/qt.conf" <<'QTCONF'
[Paths]
Prefix=..
Plugins=plugins
Translations=translations
QTCONF

cat > "$STAGE/$APP_NAME" <<'LAUNCHER'
#!/bin/sh
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$HERE/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/plugins/platforms"
exec "$HERE/bin/drawio-qt" "$@"
LAUNCHER
chmod 0755 "$STAGE/$APP_NAME"

mkdir -p "$OUT_DIR"
tar -C "$OUT_DIR" -czf "$TARBALL" "$(basename "$STAGE")"
echo "==> wrote $TARBALL"
