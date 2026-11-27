#!/usr/bin/env bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
repo=$PWD
app=quick-system-info-gui
output=$repo/build-appimage
tools=$output/tools

offline=false
if [[ $# == 1 && $1 == --offline ]]; then
    offline=true
elif [[ $# != 0 ]]; then
    echo "Usage: $0 [--offline]" >&2
    exit 2
fi

if [[ $(uname -sm) != 'Linux x86_64' ]]; then
    echo "This script builds an x86-64 AppImage on x86-64 Linux." >&2
    exit 1
fi

mkdir -p "$tools"
work=$(mktemp -d "$output/staging.XXXXXXXX")
trap 'rm -rf -- "$work"' EXIT

# Keep verified tools for subsequent offline builds.
fetch() {
    local name=$1 url=$2 digest=$3
    if [[ ! -f $tools/$name ]]; then
        if "$offline"; then
            echo "Missing cached file for offline build: $name" >&2
            exit 1
        fi
        curl --fail --location --retry 3 --output "$work/$name" "$url"
        (cd "$work" && echo "$digest  $name" | sha256sum --check)
        mv -f -- "$work/$name" "$tools/$name"
    else
        (cd "$tools" && echo "$digest  $name" | sha256sum --check)
    fi
}

deploy=linuxdeploy-x86_64.AppImage
fetch "$deploy" "https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/$deploy" \
    c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
fetch runtime-x86_64 https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64 \
    2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d
chmod 755 "$tools/$deploy" "$tools/runtime-x86_64"

mxrepo=https://mxrepo.com/mx/repo
keyring=mx25-archive-keyring_2025.03_all.deb
fetch "$keyring" "$mxrepo/pool/main/m/mx25-archive-keyring/$keyring" \
    c490bc3fa24b8fe1c3850c0f5175b1e7f595c379369c08890db5044e26385bee
dpkg-deb -x "$tools/$keyring" "$work/keyring"

# Give APT its own sources, keys and package lists, including on plain Debian.
aptEtc=$work/keyring/etc/apt
# APT's _apt user needs to reach the public key when CI runs as root.
chmod 711 "$work"
mkdir -p "$aptEtc/apt.conf.d" "$aptEtc/preferences.d" "$aptEtc/sources.list.d" "$tools/mx25-lists/partial"
echo "deb [arch=amd64] $mxrepo trixie main" > "$aptEtc/sources.list"
cat > "$work/apt.conf" <<EOF
Dir::Etc "$aptEtc";
Dir::State::lists "$tools/mx25-lists";
Dir::State::status "/dev/null";
Dir::Cache "$work";
Acquire::Languages "none";
EOF
if ! "$offline"; then
    APT_CONFIG="$work/apt.conf" apt-get --error-on=any update
fi

record=$(APT_CONFIG="$work/apt.conf" apt-cache --no-all-versions show mx-goodies)
path=$(sed -n 's/^Filename: //p' <<< "$record")
digest=$(sed -n 's/^SHA256: //p' <<< "$record")
fetch "${path##*/}" "$mxrepo/$path" "$digest"
dpkg-deb -x "$tools/${path##*/}" "$work/mx-goodies"

build=$work/build
appdir=$work/AppDir
cmake -S "$repo" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DAPPIMAGE_BUILD=ON
cmake --build "$build" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
DESTDIR="$appdir" cmake --install "$build"

prefix=$appdir/usr
install -Dm755 -t "$prefix/bin" /usr/bin/inxi /usr/bin/gettext "$work/mx-goodies/usr/bin/quick-system-info-mx"
# Keep the distro metadata optional and resolve the catalog directory when the image runs.
# shellcheck disable=SC2016
sed -i -e 's|^    \. /etc/lsb-release$|    if [ -r /etc/lsb-release ]; then . /etc/lsb-release; fi|' \
    -e 's|^TEXTDOMAINDIR=/usr/share/locale/$|export TEXTDOMAINDIR="${APPDIR}/usr/share/locale"|' \
    "$prefix/bin/quick-system-info-mx"
cp -R "$work/mx-goodies/usr/share/locale" "$prefix/share/"
install -Dm644 "$work/mx-goodies/usr/share/doc/mx-goodies/copyright" "$prefix/share/doc/mx-goodies/copyright"
for package in inxi gettext-base; do
    install -Dm644 "/usr/share/doc/$package/copyright" "$prefix/share/doc/$package/copyright"
done
install -Dm644 -t "$prefix/share/doc/$app" LICENSE

qt_plugins=$(qmake6 -query QT_INSTALL_PLUGINS)
plugins=$prefix/plugins
install -Dm644 -t "$plugins/platforms" "$qt_plugins/platforms/libqxcb.so" "$qt_plugins"/platforms/libqwayland*.so \
    "$qt_plugins/platforms/libqoffscreen.so"
deploy_deps=(--deploy-deps-only "$plugins/platforms")
install -Dm644 -t "$plugins/imageformats" "$qt_plugins"/imageformats/libq{gif,ico,jpeg,svg}.so
deploy_deps+=(--deploy-deps-only "$plugins/imageformats")
for group in iconengines xcbglintegrations platforminputcontexts \
             wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration; do
    install -Dm644 -t "$plugins/$group" "$qt_plugins/$group/"*.so
    deploy_deps+=(--deploy-deps-only "$plugins/$group")
done
translations=$(qmake6 -query QT_INSTALL_TRANSLATIONS)
install -Dm644 -t "$prefix/translations" "$translations"/qt{,base}_*.qm
cat > "$prefix/bin/qt.conf" <<'CONF'
[Paths]
Prefix=..
Plugins=plugins
Translations=translations
CONF

cat > "$work/AppRun" <<'RUN'
#!/bin/sh
set -eu
prefix=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)/usr
if [ "${1:-}" = --log-helper ]; then
    shift
    case "${1:-}" in
        list) exec "$prefix/lib/quick-system-info-gui/qsig-lib-list" list ;;
        readadminfile|journalctl_command) exec "$prefix/lib/quick-system-info-gui/qsig-lib" "$@" ;;
        *) exit 2 ;;
    esac
fi
export PATH="$prefix/bin:$PATH"
export XDG_DATA_DIRS="$prefix/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
exec "$prefix/bin/quick-system-info-gui" "$@"
RUN
chmod 755 "$work/AppRun"

version=$(dpkg-parsechangelog -SVersion)
filename=$app-$version-x86_64.AppImage
export APPIMAGE_EXTRACT_AND_RUN=1
export LDAI_RUNTIME_FILE="$tools/runtime-x86_64"
export LDAI_OUTPUT="$work/$filename"
"$tools/$deploy" --appdir "$appdir" "${deploy_deps[@]}" --custom-apprun "$work/AppRun" \
    --desktop-file Quick_System_Info.desktop --icon-file icons/mx-qsi.png --output appimage

if [[ $(env -u DISPLAY -u WAYLAND_DISPLAY "$work/$filename" --version) != "$app $version" ]]; then
    echo "The packaged application failed its version check." >&2
    exit 1
fi
mv -f -- "$work/$filename" "$output/$filename"
echo "Built $output/$filename"
