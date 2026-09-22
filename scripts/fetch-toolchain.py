# -*- coding: utf-8 -*-
"""
下载 w64devkit 便携工具链（自包含的 MinGW-w64，含 gcc/g++）。

为什么用 Python 下载而不是 PowerShell::
    本机的 schannel 在受限沙箱下拿不到证书凭证
    (AcquireCredentialsHandle failed: SEC_E_NO_CREDENTIALS)，
    导致 Invoke-WebRequest / curl 的 HTTPS 全部失败。
    Python 自带 OpenSSL，不受影响。

工具链会被解压到仓库内的 .toolchain/ 目录，不写入任何系统目录，
不需要管理员权限，删掉整个 .toolchain 即可完全卸载。
"""

import os
import sys
import urllib.request

VERSION = "2.10.0"
URL = (
    "https://github.com/skeeto/w64devkit/releases/download/"
    "v{0}/w64devkit-x64-{0}.7z.exe".format(VERSION)
)

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST_DIR = os.path.join(REPO_ROOT, ".toolchain")
DEST = os.path.join(DEST_DIR, os.path.basename(URL))


def human(n):
    return "%.1f MB" % (n / 1048576.0)


def main():
    os.makedirs(DEST_DIR, exist_ok=True)

    if os.path.exists(DEST):
        size = os.path.getsize(DEST)
        if size > 50 * 1048576:
            print("[skip] already present: %s (%s)" % (DEST, human(size)))
            return 0
        print("[warn] incomplete download, re-fetching: %s" % DEST)
        os.remove(DEST)

    print("[get ] %s" % URL)
    req = urllib.request.Request(URL, headers={"User-Agent": "Mozilla/5.0"})
    tmp = DEST + ".part"
    last_pct = -5

    with urllib.request.urlopen(req, timeout=60) as resp:
        total = int(resp.headers.get("Content-Length") or 0)
        if total:
            print("[info] size: %s" % human(total))
        done = 0
        with open(tmp, "wb") as fh:
            while True:
                chunk = resp.read(1 << 16)
                if not chunk:
                    break
                fh.write(chunk)
                done += len(chunk)
                if total:
                    pct = done * 100 // total
                    if pct >= last_pct + 5:
                        last_pct = pct
                        print("[down] %3d%%  %s / %s" % (pct, human(done), human(total)),
                              flush=True)

    os.replace(tmp, DEST)
    print("[ok  ] saved: %s (%s)" % (DEST, human(os.path.getsize(DEST))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
