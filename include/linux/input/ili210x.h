#ifndef _ILI210X_H
#define _ILI210X_H

struct ili210x_platform_data {
	unsigned long irq_flags;
	unsigned int poll_period;
	bool (*get_pendown_state)(void);
#ifdef CONFIG_LCD_KIT_B01
	bool polling;
	u16 r8c_addr;
#endif
};

#endif
