"""Generate the app's vector mark and Windows icon (Pillow required)."""
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parent.parent / 'resources'
root.mkdir(exist_ok=True)
scale = 4
im = Image.new('RGBA', (256*scale, 256*scale))
d = ImageDraw.Draw(im)
def line(points, color, width):
    points = [(x*scale,y*scale) for x,y in points]
    d.line(points, fill=color, width=width*scale, joint='curve')
    r=width*scale/2
    for x,y in points:
        d.ellipse((x-r,y-r,x+r,y+r), fill=color)
d.rounded_rectangle((8*scale,8*scale,248*scale,248*scale), radius=52*scale, fill='#14182f', outline='#43436b', width=2*scale)
for a,b,y in [(40,81,69),(29,65,93),(40,71,186)]:
    line([(a,y),(b,y)], '#a78bfa', 9)
wave=[(39,139),(76,139),(94,96),(117,178),(145,65),(168,139),(213,139)]
line(wave,'#67e8f9',13)
line([(193,117),(215,139),(193,161)],'#67e8f9',13)
im=im.resize((256,256),Image.Resampling.LANCZOS)
im.save(root/'musicsorter.png')
im.save(root/'musicsorter.ico',sizes=[(16,16),(20,20),(24,24),(32,32),(40,40),(48,48),(64,64),(128,128),(256,256)])
(root/'musicsorter.svg').write_text('''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">
<rect x="8" y="8" width="240" height="240" rx="52" fill="#14182f" stroke="#43436b" stroke-width="2"/>
<path d="M40 69H81 M29 93H65 M40 186H71" fill="none" stroke="#a78bfa" stroke-width="9" stroke-linecap="round"/>
<path d="M39 139H76L94 96 117 178 145 65 168 139H213 M193 117L215 139 193 161" fill="none" stroke="#67e8f9" stroke-width="13" stroke-linecap="round" stroke-linejoin="round"/>
</svg>''',encoding='utf-8')
