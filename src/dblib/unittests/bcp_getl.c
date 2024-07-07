
#include "common.h"

int
main(void)
{
	LOGINREC *c = dblogin();
	BCP_SETL(c, TRUE);
	TDS_ASSERT(bcp_getl(c));
	dbloginfree(c);
	return 0;
}
