#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_root"

architecture="$(uname -m)"
if [[ "$architecture" != "x86_64" ]]; then
  echo "Only x86_64 builds are currently supported." >&2
  exit 1
fi

version="$(tr -d '[:space:]' < VERSION)"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "VERSION must contain a semantic version such as 1.0.0." >&2
  exit 1
fi

linuxdeploy_url="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
tools_dir="$project_root/.tools"
linuxdeploy="$tools_dir/linuxdeploy-x86_64.AppImage"

mkdir -p "$tools_dir"
if [[ ! -x "$linuxdeploy" ]]; then
  curl --fail --location --retry 3 --output "$linuxdeploy" "$linuxdeploy_url"
  chmod +x "$linuxdeploy"
fi

rm -rf AppDir dist
mkdir -p AppDir dist

make clean
make all
make DESTDIR="$project_root/AppDir" install

export VERSION="$version"
export OUTPUT="window-clickthrough-${version}-x86_64.AppImage"

"$linuxdeploy" \
  --appdir AppDir \
  --executable build/window-clickthrough \
  --desktop-file packaging/window-clickthrough.desktop \
  --icon-file packaging/window-clickthrough.svg \
  --output appimage

mv "$OUTPUT" "dist/$OUTPUT"
chmod +x "dist/$OUTPUT"
file "dist/$OUTPUT"
"dist/$OUTPUT" --appimage-extract-and-run --version | grep -qx "window-clickthrough $version"
