void str_ip_to_str(unsigned char *data, char *str);
void str_mask_to_str(unsigned netmask_number, char *str);
char *quoted_name(void *pzt_name);
int str_pasc_to_zeroterm(unsigned char *pasc_str, unsigned char *zeroterm_str, int target_size);

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t strlcpy(char *dst, const char *src, size_t siz);


/*
 * by LBS!
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied, excluding limit_char. Always NUL terminates (unless dst_size == 0).
 * Returns copied char number, except term 0.
 */
size_t strlccpy(char *dst, const char *src, char limit_char, size_t dst_size);

/*
Returns 0 if ok, -1 otherwise. Broadcast ip is ok.
*/
int str_str_to_ip(char *str, unsigned char *ip);

int str_owaddr_to_str(unsigned char *owa, char *s);

// terminates dst with zero, returns dst length except zero
// last param is _SRC_ max length, not dst buffer length!
unsigned str_transliterate(char *dst, char *src, unsigned src_max_len);

// always terminates dst
// returns length of dst excluding term.zero
unsigned str_escape_for_js_string(char *dst, char *src, unsigned dst_size);

