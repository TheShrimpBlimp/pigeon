#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Make the site (GitHub Pages):

  index.html                  redirect to the installer (the old URL keeps working)
  firmware/felucca-VER.fwsc   the package (+ LICENSE, LICENSING.md, LICENSES/: the package holds
                              JieLi SDK files under Apache-2.0, see LICENSING.md)
  webapp/installer/index.html index_pkg.html, self-contained (fm1pkg.js, fm1ota.js, metadata inlined)
  webapp/editor/index.html    editor.html (+ fukiai.ttf, FUKIAI-LICENSE.txt)
  webapp/pigeon/index.html   offline local Pigeon sample GUI (+ bundled encoder/source)
  src/                        not touched

  web/make_site.py build/felucca-X.Y.fwsc X.Y OUT_DIR

The package must be one made by tools/fm1pkg_make.py (Felucca's own loader, no vendor files).
Its identity (FM-1_9xx) is read from the package; the device must report it after
the install.
"""
import hashlib
import html as html_escape
import subprocess
import tarfile
from datetime import datetime
from zoneinfo import ZoneInfo
import json
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    worker_root=None
    hosting=out.parent / '.openai' / 'hosting.json'
    if hosting.exists() and json.loads(hosting.read_text()).get('d1'):
        worker_root=out
        if out.exists():shutil.rmtree(out)
        out=out / 'client'
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Felucca package (FM-1_9xx)")
    if b"FELUCCA-LOADER-1" not in raw:              # Felucca loader marker: never publish a package with vendor files
        raise SystemExit(f"{pkg}: no Felucca loader in it; the site ships only fm1pkg_make.py packages "
                         "(a package patched from an official one carries vendor files)")
    html = (HERE / "pigeon-installer.html").read_text(encoding="utf-8")
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1backup.js").read_text(encoding="utf-8"))
    name = f"pigeon-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    meta = json.dumps({"version": version, "product": product, "pkg": "../../firmware/" + name, "sha256": hashlib.sha256(raw).hexdigest()})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"index_pkg.html must contain {mark} once; update make_site.py")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)
    inst, ed, fw = out / "webapp" / "installer", out / "webapp" / "editor", out / "firmware"
    for d in (inst, ed, fw):
        d.mkdir(parents=True, exist_ok=True)
    for old in fw.glob("felucca-*.fwsc"):          # one package: the current one
        old.unlink()
    (inst / "index.html").write_text(html, encoding="utf-8")
    shutil.copy(pkg, fw / name)
    lic = HERE.parent / "LICENSES"                  # the package holds JieLi SDK files (Apache-2.0): their
    (fw / "LICENSES").mkdir(exist_ok=True)          # licence travels next to it, with Felucca's own
    names = sorted(f.name for f in lic.glob("*.txt"))
    for n in names:
        shutil.copy(lic / n, fw / "LICENSES" / n)
    (fw / "LICENSES" / "index.html").write_text(    # the installer links this folder: Pages lists no folders
        '<!doctype html><meta charset="utf-8"><title>Felucca licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(HERE.parent / doc, fw / doc)
    shutil.copy(HERE / "editor.html", ed / "index.html")
    for f in ("fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js", "ko_bank.js"):
        if (HERE / f).exists():
            shutil.copy(HERE / f, ed / f)
    # Standalone sample GUI; these parent modules keep the same relative URLs as web/.
    shutil.copytree(HERE / "pigeon", out / "webapp" / "pigeon", dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns("test*.mjs"))
    for dependency in ("fm1backup.js", "ko_bank.js", "fm1pkg.js", "fm1ota.js"):
        shutil.copy(HERE / dependency, out / "webapp" / dependency)
    safe_version=re.sub(r'[^A-Za-z0-9.-]', '-', version)
    landing=(HERE / "site" / "index.html").read_text()
    tokens={"VERSION":safe_version,"BUILD_DATE":datetime.now(ZoneInfo("America/Toronto")).strftime("%B %d, %Y"),"PACKAGE_BYTES":f"{len(raw):,}","SHA256":hashlib.sha256(raw).hexdigest()}
    for key,value in tokens.items():landing=landing.replace("{{"+key+"}}",html_escape.escape(value))
    (out / "index.html").write_text(landing,encoding="utf-8")
    shutil.copy(HERE / "site" / "site.css",out / "site.css")
    shutil.copytree(HERE / "site" / "ui",out / "ui",dirs_exist_ok=True,ignore=shutil.ignore_patterns("*.ppm"))
    guide=out / "guide";guide.mkdir(exist_ok=True)
    for doc in ("KO_MODE.md","KO_STORAGE.md"):shutil.copy(HERE.parent / "docs" / doc,guide / doc)
    # Matching working source, excluding user audio, build artifacts, and hosting state.
    root=HERE.parent
    tracked=subprocess.check_output(["git","ls-files","-z"],cwd=root).decode().split("\0")
    extra=["web/pigeon/install-count.js","web/pigeon-installer.html","tools/pigeon_site_ui.c","tests/pigeon_expansion_test.c","tests/pigeon_performance_test.c","docs/NEXT_WORK.md","web/pigeon/editing.js","web/pigeon/editors.js","web/pigeon/wave-editor.js","web/pigeon/test-editing.mjs","web/site/index.html","web/site/site.css"]
    with tarfile.open(fw / f"pigeon-source-{safe_version}.tar.gz","w:gz") as archive:
        for relative in sorted(set(tracked+extra)):
            path=root / relative
            if relative and path.is_file() and not relative.startswith(("build/","web/site/.openai/","web/site/dist/")):
                archive.add(path,arcname="pigeon/"+relative,recursive=False)
    if worker_root:
        server=worker_root / 'server';server.mkdir(parents=True,exist_ok=True)
        shutil.copy(HERE / 'site' / 'worker' / 'index.js',server / 'index.js')
        metadata=worker_root / '.openai';metadata.mkdir(exist_ok=True)
        shutil.copy(hosting,metadata / 'hosting.json')
        config={"name":"pigeon-fm1","main":"index.js","compatibility_date":"2026-10-12","assets":{"directory":"../client","binding":"ASSETS","run_worker_first":["/api/*"]},"d1_databases":[{"binding":"DB","database_name":"pigeon-installs","database_id":"00000000-0000-4000-8000-000000000000"}]}
        (server / 'wrangler.json').write_text(json.dumps(config,indent=2)+'\n')
    print(f"site: {out}: webapp/installer ({len(html)} B), webapp/editor, webapp/pigeon, firmware/{name} ({len(raw)} B, {product})")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
