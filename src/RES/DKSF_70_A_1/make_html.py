#!python
# -*- coding: cp1251 -*-

from html_5_1 import *

residents = [
    "index.html",
    "index.css",
    "menu.js",
	"settings.html", ### it was no comma, updated 28.08.2012, between releases
    "update.html",
    "log.html"
    ]
## residents - menu.js is included in update.html, used if not available via http

res_dir          = "./html_ru"
output_file      = "../../html_data_dksf70_ru.c"

pack_html_pages(res_dir, output_file, residents)

res_dir          = "./html_en"
output_file      = "../../html_data_dksf70_en.c"

pack_html_pages(res_dir, output_file, residents)

