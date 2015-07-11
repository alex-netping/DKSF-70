
/*
"JO", //168 \xA8 (0xA8) \250 (0250) 10101000       �
"jo", //184 \xB8 (0xB8) \270 (0270) 10111000       �
*/

char  str_translit_table[][4] = [
"A",  //192 \xC0 (0xC0) \300 (0300) 11000000       �
"B",  //193 \xC1 (0xC1) \301 (0301) 11000001       �
"V",  //194 \xC2 (0xC2) \302 (0302) 11000010       �
"G",  //195 \xC3 (0xC3) \303 (0303) 11000011       �
"D",  //196 \xC4 (0xC4) \304 (0304) 11000100       �
"E",  //197 \xC5 (0xC5) \305 (0305) 11000101       �
"ZH", //198 \xC6 (0xC6) \306 (0306) 11000110       �
"Z",  //199 \xC7 (0xC7) \307 (0307) 11000111       �
"I",  //200 \xC8 (0xC8) \310 (0310) 11001000       �
"J",  //201 \xC9 (0xC9) \311 (0311) 11001001       �
"K",  //202 \xCA (0xCA) \312 (0312) 11001010       �
"L",  //203 \xCB (0xCB) \313 (0313) 11001011       �
"M",  //204 \xCC (0xCC) \314 (0314) 11001100       �
"N",  //205 \xCD (0xCD) \315 (0315) 11001101       �
"O",  //206 \xCE (0xCE) \316 (0316) 11001110       �
"P",  //207 \xCF (0xCF) \317 (0317) 11001111       �
"R",  //208 \xD0 (0xD0) \320 (0320) 11010000       �
"S",  //209 \xD1 (0xD1) \321 (0321) 11010001       �
"T",  //210 \xD2 (0xD2) \322 (0322) 11010010       �
"U",  //211 \xD3 (0xD3) \323 (0323) 11010011       �
"F",  //212 \xD4 (0xD4) \324 (0324) 11010100       �
"KH", //213 \xD5 (0xD5) \325 (0325) 11010101       �
"C",  //214 \xD6 (0xD6) \326 (0326) 11010110       �
"CH", //215 \xD7 (0xD7) \327 (0327) 11010111       �
"SH", //216 \xD8 (0xD8) \330 (0330) 11011000       �
"SHH", //217 \xD9 (0xD9) \331 (0331) 11011001      �
"'",  //218 \xDA (0xDA) \332 (0332) 11011010       �
"Y",  //219 \xDB (0xDB) \333 (0333) 11011011       �
"",   //220 \xDC (0xDC) \334 (0334) 11011100       �
"E",  //221 \xDD (0xDD) \335 (0335) 11011101       �
"YU", //222 \xDE (0xDE) \336 (0336) 11011110       �
"YA", //223 \xDF (0xDF) \337 (0337) 11011111       �
"a",  //224 \xE0 (0xE0) \340 (0340) 11100000       �
"b",  //225 \xE1 (0xE1) \341 (0341) 11100001       �
"v",  //226 \xE2 (0xE2) \342 (0342) 11100010       �
"g",  //227 \xE3 (0xE3) \343 (0343) 11100011       �
"d",  //228 \xE4 (0xE4) \344 (0344) 11100100       �
"e",  //229 \xE5 (0xE5) \345 (0345) 11100101       �
"zh", //230 \xE6 (0xE6) \346 (0346) 11100110       �
"z",  //231 \xE7 (0xE7) \347 (0347) 11100111       �
"i",  //232 \xE8 (0xE8) \350 (0350) 11101000       �
"j",  //233 \xE9 (0xE9) \351 (0351) 11101001       �
"k",  //234 \xEA (0xEA) \352 (0352) 11101010       �
"l",  //235 \xEB (0xEB) \353 (0353) 11101011       �
"m",  //236 \xEC (0xEC) \354 (0354) 11101100       �
"n",  //237 \xED (0xED) \355 (0355) 11101101       �
"o",  //238 \xEE (0xEE) \356 (0356) 11101110       �
"p",  //239 \xEF (0xEF) \357 (0357) 11101111       �
"r",  //240 \xF0 (0xF0) \360 (0360) 11110000       �
"s",  //241 \xF1 (0xF1) \361 (0361) 11110001       �
"t",  //242 \xF2 (0xF2) \362 (0362) 11110010       �
"u",  //243 \xF3 (0xF3) \363 (0363) 11110011       �
"f",  //244 \xF4 (0xF4) \364 (0364) 11110100       �
"kh", //245 \xF5 (0xF5) \365 (0365) 11110101       �
"c",  //246 \xF6 (0xF6) \366 (0366) 11110110       �
"ch", //247 \xF7 (0xF7) \367 (0367) 11110111       �
"sh", //248 \xF8 (0xF8) \370 (0370) 11111000       �
"shh", //249 \xF9 (0xF9) \371 (0371) 11111001      �
"'",  //250 \xFA (0xFA) \372 (0372) 11111010       �
"y",  //251 \xFB (0xFB) \373 (0373) 11111011       �
"",   //252 \xFC (0xFC) \374 (0374) 11111100       �
"e",  //253 \xFD (0xFD) \375 (0375) 11111101       �
"yu", //254 \xFE (0xFE) \376 (0376) 11111110       �
"ya"  //255 \xFF (0xFF) \377 (0377) 11111111       �
];