#ifndef _CORE_VT_DEVIRT_H_
#define _CORE_VT_DEVIRT_H_

#include "types.h"

#define _DIRECT_DEVIRT_

//void vt_devirt_handover (long *shared_area_vaddr);

/* Called from vt_mainloop in vt_main.c */
void vt_devirtualize ();

/* Called from vt_mainloop in vt_main.c */
bool vt_devirt_requested ();

#endif
