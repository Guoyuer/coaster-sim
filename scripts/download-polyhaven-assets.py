import argparse
import hashlib
import json
from pathlib import Path
from urllib.request import Request, urlopen


ASSETS = [
    {
        "id": "rock_face_01",
        "kind": "model",
        "source": "https://dl.polyhaven.org/file/ph-assets/Models/fbx/1k/rock_face_01/rock_face_01_1k.fbx",
        "target": "SourceAssets/PolyHaven/rock_face_01/rock_face_01_1k.fbx",
        "md5": "13fcd96a88130e584ce883463b231891",
        "license": "CC0",
    },
    {
        "id": "rock_face_02",
        "kind": "model",
        "source": "https://dl.polyhaven.org/file/ph-assets/Models/fbx/1k/rock_face_02/rock_face_02_1k.fbx",
        "target": "SourceAssets/PolyHaven/rock_face_02/rock_face_02_1k.fbx",
        "md5": "25985c3fa9313824ad498b678546b789",
        "license": "CC0",
    },
    {
        "id": "shrub_03",
        "kind": "model",
        "source": "https://dl.polyhaven.org/file/ph-assets/Models/fbx/1k/shrub_03/shrub_03_1k.fbx",
        "target": "SourceAssets/PolyHaven/shrub_03/shrub_03_1k.fbx",
        "md5": "321b133f596ba7fa7f78c8ab8b914c90",
        "license": "CC0",
    },
    {
        "id": "shrub_04",
        "kind": "model",
        "source": "https://dl.polyhaven.org/file/ph-assets/Models/fbx/1k/shrub_04/shrub_04_1k.fbx",
        "target": "SourceAssets/PolyHaven/shrub_04/shrub_04_1k.fbx",
        "md5": "3b313098938fa2c6bbef69761685da05",
        "license": "CC0",
    },
]


def file_md5(path: Path) -> str:
    digest = hashlib.md5()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download(url: str, target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    request = Request(url, headers={"User-Agent": "coaster-sim-codex-dev/0.1"})
    with urlopen(request, timeout=120) as response, target.open("wb") as handle:
        while True:
            chunk = response.read(1024 * 1024)
            if not chunk:
                break
            handle.write(chunk)


def main() -> None:
    parser = argparse.ArgumentParser(description="Download locked Poly Haven CC0 assets.")
    parser.add_argument("--manifest", action="store_true", help="Print the locked asset manifest and exit.")
    args = parser.parse_args()

    if args.manifest:
        print(json.dumps(ASSETS, indent=2))
        return

    repo_root = Path(__file__).resolve().parents[1]
    for asset in ASSETS:
        target = repo_root / asset["target"]
        if target.exists() and file_md5(target) == asset["md5"]:
            print(f"[POLYHAVEN-DOWNLOAD] fresh id={asset['id']} path={target}")
            continue

        print(f"[POLYHAVEN-DOWNLOAD] download id={asset['id']} url={asset['source']}")
        download(asset["source"], target)
        actual_md5 = file_md5(target)
        if actual_md5 != asset["md5"]:
            raise RuntimeError(f"MD5 mismatch for {target}: got {actual_md5}, expected {asset['md5']}")
        print(f"[POLYHAVEN-DOWNLOAD] ok id={asset['id']} bytes={target.stat().st_size}")


if __name__ == "__main__":
    main()
