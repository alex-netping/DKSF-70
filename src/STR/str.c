/*@{
defgroup STR
* Модуль STR предоставляет утилиты для работы со строками
* модуль позволяет:
*\autor
*version 1.0
*\date 17.09.2007
*
*
12.2009
- новая str_data_to_dec() с проверкой ошибок
активируется включением #include <stdio.h> в начале .c файла:
- модификация str_ip_to_str для сборок использующих stdio.h
- модификация str_mask_to_str для сборок использующих stdio.h
v2.2
7.12.2012
  str_pzt_cpy()
*/


#include "platform_setup.h"

#include <stdio.h> // может скомпилироваться без stdio. Вариант с stdio LBS 12.2009
#include <string.h>

#ifdef STR_MODULE

#ifndef STR_DEBUG
	#undef DEBUG_SHOW_TIME_LINE
	#undef DEBUG_MSG			
	#undef DEBUG_PROC_START
	#undef DEBUG_PROC_END
	#undef DEBUG_INPUT_PARAM
  #undef DEBUG_OUTPUT_PARAM

	#define DEBUG_SHOW_TIME_LINE
	#define DEBUG_MSG			
	#define DEBUG_PROC_START(msg)
	#define DEBUG_PROC_END(msg)
	#define DEBUG_INPUT_PARAM(msg,val)	
  #define DEBUG_OUTPUT_PARAM(msg,val)	
#endif

unsigned long swap_bytes(unsigned long data);
void str_byte_bin_to_data(unsigned char *str, unsigned char * data);
void str_data_to_hex_mod(unsigned char * data, unsigned char *str,uword len);

unsigned char *p;
uword str_error;
//ready
void str_data_to_hex(unsigned char * data, unsigned char *str,uword len)
{       DEBUG_PROC_START("str_data_to_hex");
	p=str;
	do
	{
		len--;
		if((data[len]&0xf0) || p!=str)
		{	*p=((data[len]&0xf0)>>4);
			*p +=((data[len]&0xf0)>>4)>9 ? '7' : '0';
			p++;
		}
		if((data[len]&0xf) || p!=str)
		{	*p =(data[len]&0xf);
			*p+=(data[len]&0xf)>9 ? '7' : '0';
			p++;	
		}
	}
	while (len);
	*p=0;
        DEBUG_PROC_END("str_data_to_hex");
}
//ready


void str_data_to_dec(unsigned char * data, unsigned char *str,uword len)
{
	unsigned char buff[24];
	unsigned long temp=0;
	uword i=23;
	uword j=0;
        DEBUG_PROC_START("str_data_to_dec");
	buff[i]=0;
	while (j!=len)
	{
		temp |=data[j]<<(8*j);
		j++;
	}
	if (temp!=0)
	{
		while(temp && i)
		{
			buff[--i]=temp%10+'0';
			temp /=10;		
		}
	}
	else	
		buff[--i]='0';
		
	util_cpy(&buff[i],str,(24-i));
	p=str+23-i;
        DEBUG_PROC_END("str_data_to_dec");
}
//ready
void str_data_to_bin(unsigned char * data, unsigned char *str,uword len)
{
	uword j = 0;
        DEBUG_PROC_START("str_data_to_bin");
	p=str;
	do
	{	
	    len--;
	    j=0;
	    while(j!=8)
	    {	
		if(((data[len]<<j)&0x80) || p!=str)
		{	
        	    if ((data[len]<<j)&0x80)			
			*p='1';
		    else
			*p='0';
		    p++;
		}
		    j++;
	    }
	}
	while (len);
	*p=0;
        DEBUG_PROC_END("str_data_to_bin");
}
//ready
void str_data_to_bool(uword data, unsigned char *str)
{
    unsigned char enMas[] = "ENABLE";
    unsigned char disMas[] = "DISABLE";
    DEBUG_PROC_START("str_data_to_bool");
    if (data&1)
    {
      util_cpy(enMas,str,7);
	  DEBUG_PROC_END("str_data_to_bool");
	  return;
    }
    util_cpy(disMas,str,8);
    DEBUG_PROC_END("str_data_to_bool");
}
//ready
void str_hex_to_data( unsigned char *str,unsigned char * data,uword len)
{
	uword i=0,j=0,k=2;
	uword temp;
        DEBUG_PROC_START("str_hex_to_data");
        temp=len;
        while(temp--)  data[temp]=0;
        j=len-1;
        temp=str_lenght(str);
        if (temp>(2*len))
        {
          DEBUG_PROC_END("str_hex_to_data");
	  return;
        }
        if (temp%2)
        {
           temp++;
           k=1;
        }
        j=temp/2-1;
	while (str[i])
	{
            data[j] = 0;
	    while (k)
	    {	
                k--;
		temp = str[i++];
		if (temp>='0' && temp<='9')			
			temp=temp-'0';			
		else if (temp>='A' && temp<='F')			
			temp=temp-'7';			
		else if (temp>='a' && temp<='f')			
			temp=temp-'W';
		else
		{
		    while(len--)  data[len]=0;
		    return;
		}	
		temp<<=4*k;
		data[j] |=temp;
	    }
	    j--;k=2;
	}
        DEBUG_PROC_END("str_hex_to_data");
}
//ready


// LBS 12.2009
void str_dec_to_data(unsigned char *str, unsigned char *data, uword len)
{
  //unsigned long long dec = 0;
  unsigned dec = 0;
  for(;*str;)
  {
    char c = *str++;
    if(c<'0' || c>'9')
    {
      #ifdef STR_ERR
      STR_ERR(STR_WRONG_CHAR);
      #endif
      dec = 0;
      break;
    }
    dec = ((dec<<3) + (dec<<1)) + (c - '0'); // no 64-bit mul from math library!
  }
  #ifdef STR_ERR
  //unsigned long long range;
  unsigned range;
  range = 1 << (len * 8); --range; // calculate numeric range
  if(dec > range) { dec = 0; STR_ERR(STR_RANGE); }
  #endif
  util_cpy((unsigned char*)&dec, data, len); // little-endian data
}


void str_dec_to_data_old(unsigned char *str,unsigned char * data, uword len)
{
  unsigned int i=0,j=0,k=0;
  unsigned long mult=0;
  union mean_un
  {
      unsigned char byte[4];
      unsigned long all;
  } mean;
  DEBUG_PROC_START("str_dec_to_data");
  mean.all = 0;
  i = str_lenght(str)-1;
  k = i;
  while ((i+1))
  {
      if ((str[i]<'0') || (str[i]>'9'))
      {
            while(len--)  data[len]=0;
			DEBUG_PROC_END("str_dec_to_data(z)");
        	return;
      }
      mult=1;j=k-i;
      while (j)
      {
          mult*=10;j--;
      }
      mean.all+=(str[i]-'0')*mult;
      i--;
    }
    util_cpy(mean.byte,data,len);
    DEBUG_PROC_END("str_dec_to_data");
}
//ready



void str_bin_to_data(unsigned char *str, unsigned char * data, uword len)
{
  uword i=0,j=0,k=0;
  unsigned char buff[]={"00000000"};
  DEBUG_PROC_START("str_bin_to_data");
  i=len;
  while(i--)  data[i]=0;
  i = str_lenght(str); //кол-во бит
  j=i-i%8;
  j /=8; //кол-во полных байтовых строк.
  while (k!=j)
  {
    util_cpy(&str[i%8+8*(j-k-1)],buff,8);
    str_byte_bin_to_data(buff, &data[k]);
    k++;
  }
  if (i%8)
  {
    unsigned char buff[]={"00000000"};

    j=8-i%8;
    util_cpy(&str[0],&buff[j],i%8);
    j=i-i%8;j/=8;
    str_byte_bin_to_data(buff, &data[j]);
  }
  DEBUG_PROC_END("str_bin_to_data");
}
//ready
void str_byte_bin_to_data(unsigned char *str, unsigned char * data)
{
    uword i=7;
    DEBUG_PROC_START("str_byte_bin_to_data");
    *data=0;
    while (i+1)
    {
        if ((str[i]<'0') || (str[i]>'1'))
        {
            *data=0;
	    return;
        }
        *data |=(str[i]-'0')<<(7-i);
        i--;
    }
    DEBUG_PROC_END("str_byte_bin_to_data");
}
//ready
uword str_bool_to_data(unsigned char *str)
{
    unsigned char enMas[] = "ENABLE";
    DEBUG_PROC_START("str_bool_to_data");
    if(util_cmp(str, enMas,7))
    {
        DEBUG_PROC_END("str_bool_to_data");
        return 0;
    }
    DEBUG_PROC_END("str_bool_to_data");
    return 1;

}

//ready

#ifdef _STDIO
void str_mask_to_str(uword data,unsigned char *str)
{
  union {
    unsigned char mask[4];
    unsigned mask32;
  };
  mask32 = ~((1<<(32-data))-1);
  sprintf((char*)str, "%u.%u.%u.%u", mask[3], mask[2], mask[1], mask[0] );
}
#else
void str_mask_to_str(uword data,unsigned char *str)
{
  union mask_un{
    unsigned char byte[4];
    unsigned long all;
  } mask;
  DEBUG_PROC_START("str_mask_to_str");
  mask.all = ~((1<<(32-data))-1);
   mask.all = swap_bytes(mask.all);
  str_ip_to_str(mask.byte,str);
  DEBUG_PROC_END("str_mask_to_str");
}
#endif

//ready
void str_str_to_mask(unsigned char *str,unsigned char *data)
{
    uword i=0;
    union mask_un{
      unsigned char byte[4];
      unsigned long all;
    } mask;
    DEBUG_PROC_START("str_str_to_mask");
	mask.all = 0;
    str_str_to_ip(str,mask.byte);
    mask.all=swap_bytes(mask.all);
    *data=0;
    while (i!=32)
    {
      if (*data && !((mask.all>>i)&1))
      {
         *data=0;
	 return;
      }
      if ((mask.all>>i)&1)
        *data+=1;
      i++;
    }
    DEBUG_PROC_END("str_str_to_mask");
}
//ready


#ifdef _STDIO

void str_ip_to_str(unsigned char *data,unsigned char *str)
{
  sprintf((char*)str, "%u.%u.%u.%u", data[0], data[1], data[2], data[3] );
}

#else
void str_ip_to_str(unsigned char *data,unsigned char *str)
{
  uword i=0;
  DEBUG_PROC_START("str_ip_to_str");
	p=str;
	while (i!=4)
	{
		str_data_to_dec(&data[i++],p,1);
		*p='.';p++;
	}
	p--;*p=0;
  DEBUG_PROC_END("str_ip_to_str");
}
#endif



//ready
void str_str_to_ip(unsigned char *str,unsigned char *data)
{
    unsigned char buff[4];
    uword i=0,j=0,k=0;
    DEBUG_PROC_START("str_str_to_ip");
    while(k!=4)
    {
      uword count;
      count=3;	
      while (str[i]!='.' && str[i]!='\0')
      {
          buff[j]=str[i];i++;j++;
          if (!(count--))
          {
            str_error=1;
            DEBUG_PROC_END("str_str_to_ip");
            return;
          }
      }
      buff[j]=0;
      str_dec_to_data(buff,&data[k], 1);
      j=0;i++;k++;
    }
    DEBUG_PROC_END("str_str_to_ip");
}

void str_mac_to_str(unsigned char *data,unsigned char *str)
{
    uword i=0;
    DEBUG_PROC_START("str_mac_to_str");
    p=str;
    while (i!=6)
    {
	str_data_to_hex_mod(&data[i++],p,1);
	*p=':';p++;
    }
    p--;*p=0;
    DEBUG_PROC_END("str_mac_to_str");
}

void str_str_to_mac(unsigned char *str,unsigned char *data)
{
    unsigned char buff[4];
    uword i=0,j=0,k=0;
    DEBUG_PROC_START("str_str_to_mac");
    while(k!=6)
    {
      uword count;
      count=2;
      while (str[i]!=':' && str[i]!='\0')
      {
          buff[j]=str[i];i++;j++;
          if (!(count--))
          {
            str_error=1;
            DEBUG_PROC_END("str_str_to_mac");
            return;
          }
      }
      buff[j]=0;
      str_hex_to_data(buff,&data[k], 1);
      j=0;i++;k++;
    }
    DEBUG_PROC_END("str_str_to_mac");
}
//ready
uword str_lenght(unsigned char *str)
{
    uword i = 0;
    DEBUG_PROC_START("str_lenght");
    while(*(str++))
       i++;
    DEBUG_PROC_END("str_lenght");
    return i;
}
//ready
unsigned long swap_bytes(unsigned long data)
{
  unsigned long result;
  uword i;
  DEBUG_PROC_START("swap_bytes");
  result=0;
  i=4;
  do{
    result=result<<8;
    result|=data&0xFF;
    data=data>>8;
  }
  while (--i);
  DEBUG_PROC_END("swap_bytes");
  return result;
}

void str_data_to_hex_mod(unsigned char * data, unsigned char *str,uword len)
{
	DEBUG_PROC_START("str_data_to_hex_mod");
	p=str;
	do
	{
		len--;
		*p=((data[len]&0xf0)>>4);
		*p +=((data[len]&0xf0)>>4)>9 ? '7' : '0';
		p++;		
	        *p =(data[len]&0xf);
		*p+=(data[len]&0xf)>9 ? '7' : '0';
		p++;	
	}
	while (len);
	*p=0;
        DEBUG_PROC_END("str_data_to_hex_mod");
}

// LBS 09.2009
int str_pasc_to_zeroterm(unsigned char *pasc_str, unsigned char *zeroterm_str, int target_size)
{
  int n = pasc_str[0];
  if(n >= target_size) n = target_size - 1;
  util_cpy(pasc_str+1, zeroterm_str, n);
  zeroterm_str[n] = 0;
  return n;
}

void str_pzt_cpy(unsigned char *pasc_str, char *src, size_t pasc_str_size)
{
  strncpy((char*)pasc_str + 1, src, pasc_str_size - 2);
  pasc_str[0] = strlen((char*)pasc_str + 1);
  pasc_str[pasc_str_size - 1] = 0;
}

#endif
//}@
