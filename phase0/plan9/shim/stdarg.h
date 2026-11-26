/* va_list for translated code on Plan 9 amd64, as in 9front's /amd64/include/u.h. */
typedef	char*	va_list;
#define va_start(list, start) list =\
	(sizeof(start) < 8?\
		(char*)((long long*)&(start)+1):\
		(char*)(&(start)+1))
#define va_end(list)\
	((void)0)
#define va_copy(dst, src)\
	((dst) = (src))
#define va_arg(list, mode)\
	((sizeof(mode) == 1)?\
		((list += 8), (mode*)list)[-8]:\
	(sizeof(mode) == 2)?\
		((list += 8), (mode*)list)[-4]:\
	(sizeof(mode) == 4)?\
		((list += 8), (mode*)list)[-2]:\
		((list += sizeof(mode)), (mode*)list)[-1])
