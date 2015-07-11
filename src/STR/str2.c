/*
v2.1
13.03.2013
  reduced and simplified version, using libc
v2.2
20.12.2013
 strlcpy() added
 str_str_to_ip() added
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t strlcpy(char *dst, const char *src, size_t siz)
{
  char *d = dst;
  const char *s = src;
  size_t n = siz;
  /* Copy as many bytes as will fit */
  if (n != 0) {
    while (--n != 0) {
      if ((*d++ = *s++) == '\0')
        break;
    }
  }
  /* Not enough room in dst, add NUL and traverse rest of src */
  if (n == 0) {
    if (siz != 0)
      *d = '\0';		/* NUL-terminate dst */
    while (*s++)
      ;
  }
  return(s - src - 1);	/* count does not include NUL */
}

/*
 * by LBS!
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied, excluding limit_char. Always NUL terminates (unless dst_size == 0).
 * Returns copied char number, except term 0.
 */
size_t strlccpy(char *dst, char *src, char limit_char, size_t dst_size)
{
  if(dst_size == 0) return 0;
  char *d = dst;
  const char *s = src;
  size_t n = dst_size;
  char c;
  /* Copy as many bytes as will fit */
  while (--n != 0)
  {
    c = *s++;
    if(c == 0 || c == limit_char) break;
    *d++ = c;
  }
  *d = 0;
  return dst_size - n - 1;
}

void str_ip_to_str(unsigned char *data, char *str)
{
  sprintf((char*)str, "%u.%u.%u.%u", data[0], data[1], data[2], data[3] );
}

int str_str_to_ip(char *str, unsigned char *ip)
{
  int n;
  char *s = str;
  for(int i=0;i<4; ++i)
  {
    n = atoi(s);
    if(n < 0 || n > 255) goto error;
    s += 1;
    if(n > 9)  s += 1;
    if(n > 99) s += 1;
    if(i < 3 && *s != '.') goto error;
    if(i == 3 && *s != 0) goto error;
    s += 1; // skip dot
    ip[i] = n;
  }
  return 0;
error:
  memset(ip, 0, 4);
  return -1;
}

void str_mask_to_str(unsigned netmask_number, char *str)
{
  union {
    unsigned char mask[4];
    unsigned mask32;
  };
  //mask32 = ~((1<<(32-netmask_number))-1);
  mask32 = ~0U << (32 - netmask_number);
  sprintf((char*)str, "%u.%u.%u.%u", mask[3], mask[2], mask[1], mask[0] );
}

//name is pasc+zeroterm string
char *quoted_name(unsigned char *pzt_name)
{
  static char str_quoted_nm[36];
  if(pzt_name[0] == 0)
    str_quoted_nm[0] = 0;
  else
  {
    memset(str_quoted_nm, 0, sizeof str_quoted_nm);
    str_quoted_nm[0] = ' ';
    str_quoted_nm[1] = '"';
    int n = pzt_name[0];
    if(n > sizeof str_quoted_nm - 4)
      n = sizeof str_quoted_nm - 4;
    strncpy(str_quoted_nm + 2, (char*)pzt_name + 1, n);
    strcat(str_quoted_nm, "\"");
  }
  return str_quoted_nm;
}

int str_pasc_to_zeroterm(unsigned char *pasc_str, unsigned char *zeroterm_str, int target_size)
{
  int n = pasc_str[0];
  if(n >= target_size) n = target_size - 1;
  memcpy(zeroterm_str, pasc_str+1, n);
  zeroterm_str[n] = 0;
  return n;
}

int str_owaddr_to_str(unsigned char *owa, char *s)
{
  return sprintf(s, "%02x%02x %02x%02x %02x%02x %02x%02x",
     owa[0], owa[1], owa[2], owa[3], owa[4], owa[5], owa[6], owa[7]);
}

/*
"JO", //168 \xA8 (0xA8) \250 (0250) 10101000       ¨
"jo", //184 \xB8 (0xB8) \270 (0270) 10111000       ¸
*/
char  str_translit_table[][4] = {

"A",  //192 \xC0 (0xC0) \300 (0300) 11000000       À
"B",  //193 \xC1 (0xC1) \301 (0301) 11000001       Á
"V",  //194 \xC2 (0xC2) \302 (0302) 11000010       Â
"G",  //195 \xC3 (0xC3) \303 (0303) 11000011       Ã
"D",  //196 \xC4 (0xC4) \304 (0304) 11000100       Ä
"E",  //197 \xC5 (0xC5) \305 (0305) 11000101       Å
"ZH", //198 \xC6 (0xC6) \306 (0306) 11000110       Æ
"Z",  //199 \xC7 (0xC7) \307 (0307) 11000111       Ç
"I",  //200 \xC8 (0xC8) \310 (0310) 11001000       È
"J",  //201 \xC9 (0xC9) \311 (0311) 11001001       É
"K",  //202 \xCA (0xCA) \312 (0312) 11001010       Ê
"L",  //203 \xCB (0xCB) \313 (0313) 11001011       Ë
"M",  //204 \xCC (0xCC) \314 (0314) 11001100       Ì
"N",  //205 \xCD (0xCD) \315 (0315) 11001101       Í
"O",  //206 \xCE (0xCE) \316 (0316) 11001110       Î
"P",  //207 \xCF (0xCF) \317 (0317) 11001111       Ï
"R",  //208 \xD0 (0xD0) \320 (0320) 11010000       Ð
"S",  //209 \xD1 (0xD1) \321 (0321) 11010001       Ñ
"T",  //210 \xD2 (0xD2) \322 (0322) 11010010       Ò
"U",  //211 \xD3 (0xD3) \323 (0323) 11010011       Ó
"F",  //212 \xD4 (0xD4) \324 (0324) 11010100       Ô
"KH", //213 \xD5 (0xD5) \325 (0325) 11010101       Õ
"C",  //214 \xD6 (0xD6) \326 (0326) 11010110       Ö
"CH", //215 \xD7 (0xD7) \327 (0327) 11010111       ×
"SH", //216 \xD8 (0xD8) \330 (0330) 11011000       Ø
"SHH", //217 \xD9 (0xD9) \331 (0331) 11011001      Ù
"'",  //218 \xDA (0xDA) \332 (0332) 11011010       Ú
"Y",  //219 \xDB (0xDB) \333 (0333) 11011011       Û
"",   //220 \xDC (0xDC) \334 (0334) 11011100       Ü
"E",  //221 \xDD (0xDD) \335 (0335) 11011101       Ý
"YU", //222 \xDE (0xDE) \336 (0336) 11011110       Þ
"YA", //223 \xDF (0xDF) \337 (0337) 11011111       ß
"a",  //224 \xE0 (0xE0) \340 (0340) 11100000       à
"b",  //225 \xE1 (0xE1) \341 (0341) 11100001       á
"v",  //226 \xE2 (0xE2) \342 (0342) 11100010       â
"g",  //227 \xE3 (0xE3) \343 (0343) 11100011       ã
"d",  //228 \xE4 (0xE4) \344 (0344) 11100100       ä
"e",  //229 \xE5 (0xE5) \345 (0345) 11100101       å
"zh", //230 \xE6 (0xE6) \346 (0346) 11100110       æ
"z",  //231 \xE7 (0xE7) \347 (0347) 11100111       ç
"i",  //232 \xE8 (0xE8) \350 (0350) 11101000       è
"j",  //233 \xE9 (0xE9) \351 (0351) 11101001       é
"k",  //234 \xEA (0xEA) \352 (0352) 11101010       ê
"l",  //235 \xEB (0xEB) \353 (0353) 11101011       ë
"m",  //236 \xEC (0xEC) \354 (0354) 11101100       ì
"n",  //237 \xED (0xED) \355 (0355) 11101101       í
"o",  //238 \xEE (0xEE) \356 (0356) 11101110       î
"p",  //239 \xEF (0xEF) \357 (0357) 11101111       ï
"r",  //240 \xF0 (0xF0) \360 (0360) 11110000       ð
"s",  //241 \xF1 (0xF1) \361 (0361) 11110001       ñ
"t",  //242 \xF2 (0xF2) \362 (0362) 11110010       ò
"u",  //243 \xF3 (0xF3) \363 (0363) 11110011       ó
"f",  //244 \xF4 (0xF4) \364 (0364) 11110100       ô
"kh", //245 \xF5 (0xF5) \365 (0365) 11110101       õ
"c",  //246 \xF6 (0xF6) \366 (0366) 11110110       ö
"ch", //247 \xF7 (0xF7) \367 (0367) 11110111       ÷
"sh", //248 \xF8 (0xF8) \370 (0370) 11111000       ø
"shh", //249 \xF9 (0xF9) \371 (0371) 11111001      ù
"'",  //250 \xFA (0xFA) \372 (0372) 11111010       ú
"y",  //251 \xFB (0xFB) \373 (0373) 11111011       û
"",   //252 \xFC (0xFC) \374 (0374) 11111100       ü
"e",  //253 \xFD (0xFD) \375 (0375) 11111101       ý
"yu", //254 \xFE (0xFE) \376 (0376) 11111110       þ
"ya"  //255 \xFF (0xFF) \377 (0377) 11111111       ÿ

};


// terminates dst with zero, returns dst length except zero
// last param is _SRC_ max length, not dst buffer length!
unsigned str_transliterate(char *dst, char *src, unsigned src_max_len)
{
  char *p;
  char *q = dst;
  char c;

  for(int i=0; i<src_max_len; ++i)
  {
    c = *src++;
    if(c < 192)
    {
      if(c == 0) break;
      if(c == 168) // ¨
      {
        *q++ = 'J';
        *q++ = 'O';
      }
      else if(c == 184) // ¸
      {
        *q++ = 'j';
        *q++ = 'o';
      }
      else
        *q++ = c; // not cyrillic char
    }
    else
    {
      p = str_translit_table[c - 192];
      while(c = *p++) *q++ = c; // assignment in condition, not comparison
    }
  }
  *q = 0; // w/o increment!
  return q - dst; // length w/o term.zero
}

// always terminates dst
// returns length of dst excluding term.zero
unsigned str_escape_for_js_string(char *dst, char *src, unsigned dst_size)
{
  char *q = dst, c;
  unsigned left = dst_size - 1; // place for zterm
  for(;;)
  {
    c = *src++;
    if(c == 0) break;
    switch(c)
    {
    case '"':
    case '\'':
    case '\\':
      break;
    case '\r': c = 'r';
      break;
    case '\n': c = 'n';
      break;
    default:
      if(left == 0)
      {
        *q = 0;
        return q - dst;
      }
      --left;
      *q++ = c;
      continue;
    }
    if(left < 2)
      break;
    left -= 2;
    *q++ = '\\';
    *q++ = c;
  }
  *q = 0;
  return q - dst;
}
