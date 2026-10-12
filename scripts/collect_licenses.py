"""Collect upstream license notices, with exact versioned source locations."""
import io
import pathlib
import urllib.request
import zipfile

root = pathlib.Path(__file__).resolve().parents[1]
target = root / 'licenses'
target.mkdir(exist_ok=True)
sources = []
for module in ('qtbase', 'qtmultimedia', 'qtsvg'):
    url = f'https://github.com/qt/{module}/archive/refs/tags/v6.10.2.zip'
    print('Downloading', url, flush=True)
    data = urllib.request.urlopen(url, timeout=120).read()
    archive = root / '.tools' / (module + '-6.10.2-source.zip')
    archive.write_bytes(data)
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        for item in z.infolist():
            path = pathlib.PurePosixPath(item.filename)
            lower = path.name.lower()
            if not item.is_dir() and ('LICENSES' in path.parts or ('3rdparty' in path.parts and any(x in lower for x in ('license', 'copying', 'copyright', 'notice')))):
                relative = pathlib.Path(*path.parts[1:])
                output = target / module / relative
                output.parent.mkdir(parents=True, exist_ok=True)
                output.write_bytes(z.read(item))
    sources.append(f'{module} 6.10.2: {url}')
ffmpeg_url = 'https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n7.1.2.zip'
print('Downloading', ffmpeg_url, flush=True)
data = urllib.request.urlopen(ffmpeg_url, timeout=120).read()
(root / '.tools' / 'ffmpeg-7.1.2-source.zip').write_bytes(data)
with zipfile.ZipFile(io.BytesIO(data)) as z:
    for item in z.infolist():
        path = pathlib.PurePosixPath(item.filename)
        if not item.is_dir() and (path.name.startswith('COPYING') or path.name in ('LICENSE.md', 'COPYRIGHT')):
            output = target / 'ffmpeg' / pathlib.Path(*path.parts[1:])
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(z.read(item))
sources += [f'FFmpeg 7.1.2: {ffmpeg_url}', 'Qt FFmpeg build scripts: https://code.qt.io/cgit/qt/qt5.git/tree/coin/provisioning/common?h=v6.10.2']
(target / 'SOURCES.txt').write_text('\n'.join(sources) + '\n', encoding='utf-8')
