/* PLINK - macros for DHTML page data linking
* v2.1 LBS
* 01.2010
*  functions defined in http2.c
*
* v2.2 by Ageev
* 30.10.2010
*  PDATA_ARRAY, PDATA_ARRAY_2D for handling array data
*/

#ifndef __plink_h__
#define __plink_h__

#define _PLINK(dest, name, offs, len)   \
  dest += sprintf((char*)dest,"%s:{offs:%d,len:%d},", name, offs, len)


#define PLINK(dest, var, field)    \
  _PLINK(dest, #field, (char*)&var.field-(char*)&var, sizeof var.field)

/*
// more compact code, works with scalar 'var' only
#define PLINK(dest, var, field)  \
  _PLINK(dest, #field, (unsigned)&((struct var##_s *)0)->field, sizeof var.field)
*/

#define PDATA(dest, var, field) dest+=sprintf((char*)dest, "%s:%u," , #field , var.field)

#define PDATA_ARRAY(dest, var, field, sz) do { dest+=sprintf((char*)dest, "%s:[", #field); \
  for (int ii=0; ii<sz; ii++) dest+=sprintf((char*)dest, "%u,", var.field[ii]); \
  *(dest-1)=']'; *dest++ = ','; } while(0)

#define PDATA_ARRAY_2D(dest, var, field, szi, szj) do { dest+=sprintf((char*)dest, "%s:[", #field);\
  for (int di=0; di<szi; di++) \
  { \
    *dest++='['; \
    for (int dj=0; dj< szj; dj++) \
      dest+=sprintf((char*)dest, "%u," , var.field[di][dj]);\
    *(dest-1)=']'; \
    if (di!=szi-1) *dest++=','; \
  }\
  *dest++=']'; *dest++=','; \
  } while(0)

#define PDATA_SIGNED(dest, var, field) dest+=sprintf((char*)dest, "%s:%d," , #field , var.field)  // LBS 01.2010, used in TERMO.c

#define PDATA_PASC_STR(dest, var, field)   \
  dest+=pdata_pstring((char*)dest, #field, var.field)

#define PDATA_IP(dest, var, field) \
  dest+=pdata_ip(dest, #field, var.field)

#define PDATA_MASK(dest, var, field) \
  do{ dest+=sprintf((char*)dest, "%s:'", #field); \
      str_mask_to_str(var.field, (/*unsigned*/ char*)dest); while(*dest) ++dest; \
      *dest++='\''; *dest++=','; }while(0)

#define PDATA_MAC(dest, var, field) \
  dest+=sprintf((char*)dest, "%s:'%02x:%02x:%02x:%02x:%02x:%02x'," , #field, \
    var.field[0],var.field[1],var.field[2],var.field[3],var.field[4],var.field[5])

#define PSIZE(dest, size) dest+=sprintf((char*)dest, "__len:%u", size)


#endif
