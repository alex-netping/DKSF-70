#!python
# -*- coding: cp1251 -*-

#ver.3.3
#22.03.2010

#ver.3.4
#31.05.2010

#ver.3.5-52
#30.03.2011
#compile_time передвинуто внутрь функции

#ver.3.6-52
#27.07.2011
#добавлено помещение файлов, начинаЮщихся с '_' в сегмент кода

#ver 4.0-253
#30.10.2011
#переделка, каждая страница как отдельный массив char[], ссылки из заголовков по имени

#ver 5.1-52
#11.05.2012
#файлы помещатся в сегмент кода явным указанием в аргументе residents (массив строк-имён файлов без пути)

import os, StringIO, gzip, sys, time
import shortcut ## LBS made
from string import replace

HTML_FLG_GET         = 0x01
HTML_FLG_POST        = 0x02
HTML_FLG_COMPRESSED  = 0x04
HTML_FLG_NOCACHE     = 0x08
HTML_FLG_CODESEG     = 0x10
HTML_FLG_CGI         = 0x40

compressed_types = [ 'html']

def compress(data):
    s = StringIO.StringIO()
    gz = gzip.GzipFile('','wb',9,s)
    gz.write(data)
    gz.close()
    return s.getvalue()

def ext(fn):
    return os.path.splitext(fn)[1][1:]

def pack_html_pages(res_dir, output_file, residents = []):    
    compiled_time = time.ctime()
    flist = os.listdir(res_dir)
    
    fo = open(output_file, 'wb')
    print >>fo, '// NetPing HTML resource compiler v4'
    print >>fo, '// HTML pages data for HTTP2 module\r\n'
    print >>fo, '// Source dir is ' + os.path.abspath(res_dir) + '\r\n'
    print >>fo, '// Compiled ' + compiled_time + '\r\n'
    for fn in flist:
        flags = HTML_FLG_GET | HTML_FLG_NOCACHE
        full_fn = os.path.join(res_dir,fn)
        if(ext(fn) == 'lnk'): ## windows shortcut
          full_fn = shortcut.resolve(full_fn)
          fn = fn[:-4] ## strip .lnk extension
        p = open(full_fn,'rb').read()
        if ext(fn) in compressed_types:
            p = compress(p)
            flags |= HTML_FLG_COMPRESSED
        page_len = len(p)
        p = chr(0x59) + chr(0x95) + p ## add header 0x59 0x95 - page signature
        if fn in residents:
          flags |= HTML_FLG_CODESEG

        data_name = replace(fn, '.', '_')

        segment = ' @ "MT" '
        if flags & HTML_FLG_CODESEG:
            segment = ''            
        print >>fo, '\nconst __root char', '_html_'+data_name, '[]', segment, '='
        print >>fo, '{', ','.join([str(ord(b)) for b in p ]), '};'        

        print >>fo, 'const __root struct page_s %-24s @ "HTML_HEADERS" = { %-24s %-16s %-24s %6d, %#.04x };' % \
              ( '_htmlhdr_'+data_name, \
                '"/'+fn+'",', \
                'mime_'+ext(fn)+',', \
                '_html_'+data_name+'+2,', \
                page_len, \
                flags )
    fo.close()

    print >>sys.stderr, u"\n\n", compiled_time
    print >>sys.stderr,u"Упаковка HTML страниц завершена успешно."
    print >>sys.stderr,u"Выходной файл "+os.path.abspath(output_file)
    ##print >>sys.stderr,u"Размер HTML данных (MT) "+str(len(data))+u" байт."
    ##print >>sys.stderr,"Заголовки страниц "+os.path.abspath(page_header_file)

    print >>sys.stderr,"\n"
pass ## end pack_html_pages()

