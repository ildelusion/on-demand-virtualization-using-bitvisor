#include <define.h>
#include <smm_types.h>
#include <smiutil.h>
#include <timer.h>

static unsigned int count = 0;
inline void timer_auto_enable (uint8_t time, unsigned int expire)
{
	u64 *timer_unset_value = (u64*)0x49000a90;
	if (count == 0)
	{
		timer_set(time);
	}

	count++;

	if (count >= expire)
	{
		count = 0;
		if ( *timer_unset_value == 1) {
			timer_unset();
		}
		//timer_unset();
		//debug_println("timer is expired ",expire);
	}
}
inline void timer_set (uint8_t time)
{
	outb(PMIO_TIMER2,PMIO_COMMAND);
	outb(time, PMIO_DATA);

	outb(PMIO_MISCCONTROL,PMIO_COMMAND);
	outb(PMIO_TIMER2_ENABLE_BIT, PMIO_DATA);
}
inline void timer_unset(void)
{
	outb(PMIO_MISCCONTROL,PMIO_COMMAND);
	outb(0, PMIO_DATA);
}
