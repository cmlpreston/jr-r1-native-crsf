#!/usr/bin/env python3
"""Build the printable operating guide from docs/user-manual.md.

Requires reportlab. Optional --font-dir supplies LiberationSans TTF files.
Run: python3 tools/build_user_manual.py [--font-dir /path/to/fonts]
"""
import argparse
import html
import re
from pathlib import Path
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph,
    Spacer, PageBreak, Table, TableStyle, Image, KeepTogether)
from reportlab.platypus.tableofcontents import TableOfContents

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--font-dir', type=Path)
args = parser.parse_args()
font, bold, italic = 'Helvetica', 'Helvetica-Bold', 'Helvetica-Oblique'
if args.font_dir:
    for name, filename in [('Manual','LiberationSans-Regular.ttf'),
                           ('Manual-Bold','LiberationSans-Bold.ttf'),
                           ('Manual-Italic','LiberationSans-Italic.ttf')]:
        pdfmetrics.registerFont(TTFont(name, str(args.font_dir / filename)))
    pdfmetrics.registerFontFamily('Manual',normal='Manual',bold='Manual-Bold',italic='Manual-Italic',boldItalic='Manual-Bold')
    font,bold,italic='Manual','Manual-Bold','Manual-Italic'
TEAL=colors.HexColor('#006f7d'); INK=colors.HexColor('#172a34')
styles=getSampleStyleSheet()
styles.add(ParagraphStyle(name='BodyManual',fontName=font,fontSize=10,leading=14,spaceAfter=8,textColor=INK))
styles.add(ParagraphStyle(name='ChapterManual',fontName=bold,fontSize=20,leading=24,spaceBefore=18,spaceAfter=15,textColor=TEAL,keepWithNext=True))
styles.add(ParagraphStyle(name='SubManual',fontName=bold,fontSize=12,leading=16,spaceBefore=10,spaceAfter=7,textColor=INK,keepWithNext=True))
styles.add(ParagraphStyle(name='CellManual',fontName=font,fontSize=9,leading=12,textColor=INK))
styles.add(ParagraphStyle(name='CellHeadManual',parent=styles['CellManual'],fontName=bold,textColor=colors.white))
styles.add(ParagraphStyle(name='CaptionManual',fontName=italic,fontSize=8.5,leading=12,spaceBefore=5,spaceAfter=12,textColor=colors.HexColor('#52636b')))
styles.add(ParagraphStyle(name='CoverTitleManual',fontName=bold,fontSize=36,leading=42,textColor=INK,spaceAfter=18))
styles.add(ParagraphStyle(name='CoverSubManual',fontName=font,fontSize=20,leading=26,textColor=TEAL,spaceAfter=18))
styles.add(ParagraphStyle(name='TocManual',fontName=font,fontSize=11,leading=16,leftIndent=0,firstLineIndent=0,spaceBefore=7,textColor=INK))

def inline(s):
    s=s.replace('→',' > ').replace('–','-').replace('—','-').replace('\u2011','-').replace('’',"'").replace('“','"').replace('”','"')
    s=html.escape(s)
    def link(m):
        label,url=m.group(1),html.unescape(m.group(2))
        if url.startswith('#'): return label
        if not url.startswith(('http://','https://')):
            url='https://github.com/cmlpreston/jr-r1-native-crsf/blob/main/'+str(Path('docs',url)).replace('docs/../','')
        return '<link href="'+html.escape(url,quote=True)+'" color="#006f7d">'+label+'</link>'
    s=re.sub(r'\[([^\]]+)\]\(([^)]+)\)',link,s)
    s=re.sub(r'\*\*([^*]+)\*\*',r'<b>\1</b>',s)
    s=re.sub(r'`([^`]+)`',r'<font face="Courier">\1</font>',s)
    return s

class ManualDoc(BaseDocTemplate):
    def afterFlowable(self,f):
        if isinstance(f,Paragraph) and f.style.name=='ChapterManual':
            title=f.getPlainText();key='chapter-'+re.sub(r'[^a-z0-9]+','-',title.lower())
            self.canv.bookmarkPage(key)
            self.canv.addOutlineEntry(title,key,level=0,closed=False)
            self.notify('TOCEntry',(0,title,self.page,key))

def decorate(c,doc):
    if doc.page==1:return
    c.saveState();c.setStrokeColor(TEAL);c.setLineWidth(.6)
    c.line(20*mm,282*mm,190*mm,282*mm)
    c.setFont(font,8);c.setFillColor(INK)
    c.drawString(20*mm,285*mm,'JR R-1 EVO | USER MANUAL')
    c.drawString(20*mm,13*mm,'Prototype edition 1.3 | Firmware 06b6928 | 1 November 2026')
    c.drawRightString(190*mm,13*mm,str(doc.page));c.restoreState()

out=ROOT/'output/pdf/r1-evo-user-manual.pdf';out.parent.mkdir(parents=True,exist_ok=True)
doc=ManualDoc(str(out),pagesize=A4,leftMargin=20*mm,rightMargin=20*mm,topMargin=23*mm,bottomMargin=22*mm,
    title='JR R-1 EVO User Manual',author='R-1 EVO project')
doc.addPageTemplates(PageTemplate(id='manual',frames=Frame(20*mm,22*mm,170*mm,252*mm,id='body',leftPadding=0,rightPadding=0,topPadding=0,bottomPadding=0),onPage=decorate))
story=[Spacer(1,15*mm),Paragraph('JR R-1 EVO',styles['CoverTitleManual']),Paragraph('User manual',styles['CoverSubManual']),
    Paragraph('Prototype edition 1.3<br/>1 November 2026<br/>Based on verified firmware 06b6928',styles['BodyManual']),Spacer(1,7*mm)]
hero=ROOT/'assets/handset-captures/drive.png'
story.extend([Image(str(hero),width=160*mm,height=160*mm*320/480),Spacer(1,10*mm),
    Paragraph('Original JR controls. A new touchscreen interface.<br/>ExpressLRS radio and telemetry.',styles['CoverSubManual']),
    Paragraph('Operating guide for the custom XIAO ESP32-S3 / Waveshare LCD (F) transmitter. Includes channel setup, mixing, timers, maps, web configuration and safety states.',styles['BodyManual']),PageBreak(),
    Paragraph('Contents',styles['CoverSubManual'])])
toc=TableOfContents();toc.levelStyles=[styles['TocManual']];story += [toc,PageBreak()]
lines=(ROOT/'docs/user-manual.md').read_text().splitlines();i=next(i for i,s in enumerate(lines) if s.startswith('## 1.'))
first=True
while i<len(lines):
    line=lines[i]
    if not line.strip():i+=1;continue
    if line.startswith('## '):
        first=False;story.append(Paragraph(inline(line[3:]),styles['ChapterManual']));i+=1
    elif line.startswith('### '):story.append(Paragraph(inline(line[4:]),styles['SubManual']));i+=1
    elif line.startswith('|'):
        rows=[]
        while i<len(lines) and lines[i].startswith('|'):
            cells=[s.strip() for s in lines[i].strip().strip('|').split('|')]
            if not all(re.fullmatch(r':?-+:?',s) for s in cells):rows.append(cells)
            i+=1
        n=len(rows[0]);widths=[170*mm*.28,170*mm*.72] if n==2 else [170*mm/n]*n
        data=[[Paragraph(inline(s),styles['CellHeadManual' if r==0 else 'CellManual']) for s in row] for r,row in enumerate(rows)]
        table=Table(data,colWidths=widths,repeatRows=1,hAlign='LEFT')
        table.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),TEAL),('ROWBACKGROUNDS',(0,1),(-1,-1),[colors.HexColor('#f0f5f6'),colors.white]),('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),7),('RIGHTPADDING',(0,0),(-1,-1),7),('TOPPADDING',(0,0),(-1,-1),6),('BOTTOMPADDING',(0,0),(-1,-1),6),('LINEBELOW',(0,0),(-1,0),.5,TEAL),('LINEBELOW',(0,1),(-1,-1),.3,colors.HexColor('#d6e1e4'))]))
        story.extend([table,Spacer(1,10)])
    elif line.startswith('!['):
        match=re.match(r'!\[([^]]*)\]\(([^)]+)\)',line)
        if match:
            p=ROOT/'docs'/match[2]
            figure=Image(str(p))
            scale=min(120*mm/figure.imageWidth,110*mm/figure.imageHeight)
            figure.drawWidth=figure.imageWidth*scale
            figure.drawHeight=figure.imageHeight*scale
            story.append(figure)
            story.append(Paragraph(inline(match[1]),styles['CaptionManual']))
        i+=1
    else:
        block=[line];i+=1
        while i<len(lines) and lines[i].strip() and not re.match(r'^(#{2,3} |\||!\[|\d+\. )',lines[i]):block.append(lines[i]);i+=1
        text=' '.join(block)
        if text.startswith('*') and text.endswith('*') and not text.startswith('**'):text=text[1:-1];style='CaptionManual'
        else:style='BodyManual'
        paragraph=Paragraph(inline(text),styles[style])
        if i<len(lines):
            following=i
            while following<len(lines) and not lines[following].strip():following+=1
            if following<len(lines) and lines[following].startswith('|'):paragraph.keepWithNext=True
        story.append(paragraph)
doc.multiBuild(story)
print(out)
