# quick-system-info-gui
Qt wrapper for quick-system-info script, 

[![latest packaged version(s)](https://repology.org/badge/latest-versions/quick-system-info-gui.svg)](https://repology.org/project/quick-system-info-gui/versions)
[![build result](https://build.opensuse.org/projects/home:mx-packaging/packages/quick-system-info-gui/badge.svg?type=default)](https://software.opensuse.org//download.html?project=home%3Amx-packaging&package=quick-system-info-gui)

## AppImage

Download the x86-64 AppImage from the **Assets** section of a GitHub release and make it executable with `chmod +x`. Run it as your desktop user. It includes the MX report script, `inxi`, the log helpers, Qt, and offline help. The report uses the host’s Bash, Perl and standard system utilities. The available hardware details depend on which probes the host provides.

The application uses the host’s `pkexec` and polkit authentication agent to list protected logs and read them on request. These operations reopen the AppImage and require FUSE. System journal reports also require `journalctl`.

To build locally on Debian 13 (trixie), install the build dependencies and run the script.

```sh
sudo apt-get install ca-certificates git build-essential cmake ninja-build dpkg-dev curl file \
    qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-svg-plugins qt6-translations-l10n \
    qt6-wayland libarchive-dev inxi gettext-base
./make-appimage.sh
```

The script checks the signed MX 25 package index for `mx-goodies` and verifies its checksum before extracting the report script. After an online build, `./make-appimage.sh --offline` reuses the verified downloads. The output is `build-appimage/quick-system-info-gui-<version>-x86_64.AppImage`, with the version from `debian/changelog`. GitHub Actions attaches the image to each non-Arch tagged release.
