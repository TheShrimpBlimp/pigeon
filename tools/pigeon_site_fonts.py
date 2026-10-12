#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Rerender captured firmware text using original Little Character TTF outlines.
The layout, colours and non-text pixels come from the real host UI renderer.
"""
from pathlib import Path
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.transformPen import TransformPen
import html

root=Path(__file__).resolve().parents[1]
font=TTFont(root/'web/pigeon/fonts/LittleCharacterRegular.ttf')
glyphs=font.getGlyphSet();cmap=font.getBestCmap()
def colour(c):
    return '#%02x%02x%02x'%((c>>11)*255//31,((c>>5)&63)*255//63,(c&31)*255//31)
for name in ('waveform','sequencer','tempo-sync'):
    folder=root/'web/site/ui';raw=(folder/(name+'.ppm')).read_bytes();header=b'P6\n240 240\n255\n';assert raw.startswith(header)
    pixels=raw[len(header):];parts=[]
    for y in range(240):
        x=0
        while x<240:
            rgb=pixels[(y*240+x)*3:(y*240+x)*3+3];end=x+1
            while end<240 and pixels[(y*240+end)*3:(y*240+end)*3+3]==rgb:end+=1
            parts.append(f'<path fill="#{rgb.hex()}" d="M{x} {y}h{end-x}v1H{x}z"/>');x=end
    overlays=[]
    for line in (folder/(name+'-text.txt')).read_text().splitlines():
        fields=line.split();x0,y0,x1,y1,fg,bg=map(int,fields[:6]);text=bytes.fromhex(fields[6]).decode('cp1252')
        if not (0<=x0<x1<=240 and 0<=y0<y1<=240):continue
        pen=SVGPathPen(glyphs);bounds=BoundsPen(glyphs);advance=0
        for c in text:
            glyph=glyphs[cmap.get(ord(c),'.notdef')]
            transform=(1,0,0,1,advance,0)
            glyph.draw(TransformPen(pen,transform));glyph.draw(TransformPen(bounds,transform));advance+=glyph.width
        if not bounds.bounds:continue
        a,b,c,d=bounds.bounds;sx=(x1-x0)/(c-a);sy=(y1-y0)/(d-b)
        overlays.append(f'<rect x="{x0}" y="{y0}" width="{x1-x0}" height="{y1-y0}" fill="{colour(bg)}"/><path fill="{colour(fg)}" transform="matrix({sx} 0 0 {-sy} {x0-a*sx} {y0+d*sy})" d="{pen.getCommands()}"/>')
    svg='<svg xmlns="http://www.w3.org/2000/svg" width="960" height="960" viewBox="0 0 240 240"><title>'+html.escape('Pigeon '+name+' — original Little Character outlines, firmware layout')+'</title><g shape-rendering="crispEdges">'+''.join(parts)+'</g><g>'+''.join(overlays)+'</g></svg>'
    (folder/(name+'.svg')).write_text(svg)
    print(name, len(overlays),'text runs rerendered from original TTF outlines')
