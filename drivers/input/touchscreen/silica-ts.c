#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/platform_data/silica-ts.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <mach/rza1.h>

#define SH_ADC_ADDRA	0x00
#define SH_ADC_ADDRB	0x02
#define SH_ADC_ADDRC	0x04
#define SH_ADC_ADDRD	0x06
#define SH_ADC_ADDRE	0x08
#define SH_ADC_ADDRF	0x0a
#define SH_ADC_ADDRG	0x0c
#define SH_ADC_ADDRH	0x0e
#define SH_ADC_ADCSR	0x60

#define SH_ADC_ADDR_MASK	0x03FF
#define SH_ADC_ADDR_SHIFT	6

#define SH_ADC_ADCSR_ADF	0x8000
#define SH_ADC_ADCSR_ADIE	0x4000
#define SH_ADC_ADCSR_ADST	0x2000
#define SH_ADC_ADCSR_TRGS_MASK	0x1e00
#define SH_ADC_ADCSR_TRGS_NON	0x0000	/* external trigger input is disable */
#define SH_ADC_ADCSR_TRGS_TRGAN	0x0200	/* MTU2, TRGAN */
#define SH_ADC_ADCSR_TRGS_TRG0N	0x0400	/* MTU2, TRG0N */
#define SH_ADC_ADCSR_TRGS_TRG4AN	0x0600	/* MTU2, TRG4AN */
#define SH_ADC_ADCSR_TRGS_TRG4BN	0x0800	/* MTU2, TRG4BN */
#define SH_ADC_ADCSR_TRGS_EXT	0x1200	/* external pin trigger */
#define SH_ADC_ADCSR_CKS_MASK	0x01c0
#define SH_ADC_ADCSR_CKS_256	0x0000
#define SH_ADC_ADCSR_CKS_298	0x0040
#define SH_ADC_ADCSR_CKS_340	0x0080
#define SH_ADC_ADCSR_CKS_382	0x00c0
#define SH_ADC_ADCSR_CKS_1054	0x0100
#define SH_ADC_ADCSR_CKS_1096	0x0140
#define SH_ADC_ADCSR_CKS_1390	0x0180
#define SH_ADC_ADCSR_CKS_1432	0x01c0
#define SH_ADC_ADCSR_MDS_MASK	0x0038
#define SH_ADC_ADCSR_MDS_SINGLE	0x0000
#define SH_ADC_ADCSR_MDS_M_1_4	0x0020	/* multi mode, channel 1 to 4 */
#define SH_ADC_ADCSR_MDS_M_1_8	0x0028	/* multi mode, channel 1 to 8 */
#define SH_ADC_ADCSR_MDS_S_1_4	0x0030	/* scan mode, channel 1 to 4 */
#define SH_ADC_ADCSR_MDS_S_1_8	0x0038	/* scan mode, channel 1 to 8 */
#define SH_ADC_ADCSR_CH_MASK	0x0003
#define SH_ADC_ADCSR_CH_AN0	0x0000
#define SH_ADC_ADCSR_CH_AN1	0x0001
#define SH_ADC_ADCSR_CH_AN2	0x0002
#define SH_ADC_ADCSR_CH_AN3	0x0003
#define SH_ADC_ADCSR_CH_AN4	0x0004
#define SH_ADC_ADCSR_CH_AN5	0x0005
#define SH_ADC_ADCSR_CH_AN6	0x0006
#define SH_ADC_ADCSR_CH_AN7	0x0007

#define SH_ADC_NUM_CHANNEL	8
#define silica_tsc_get_reg_addr(ch)	(SH_ADC_ADDRA + ch * 2)
   
struct silica_tsc {
	struct input_dev *idev;
	struct clk *clk;
	struct silica_tsc_pdata *pdata;
	spinlock_t irq_lock;
	wait_queue_head_t irq_wait;
	unsigned irq_disabled;
	struct task_struct *rtask;
	void __iomem *reg;
	int irq;
};

static void silica_tsc_write(struct silica_tsc *tsc, unsigned short data,
			     unsigned long offset)
{
	iowrite16(data, tsc->reg + offset);
}

static unsigned short silica_tsc_read(struct silica_tsc *tsc, unsigned long offset)
{
	return ioread16(tsc->reg + offset);
}

static void silica_tsc_set_bit(struct silica_tsc *tsc, unsigned short val,
			       unsigned long offset)
{
	unsigned short tmp;

	tmp = silica_tsc_read(tsc, offset);
	tmp |= val;
	silica_tsc_write(tsc, tmp, offset);
}

static void silica_tsc_clear_bit(struct silica_tsc *tsc, unsigned short val,
				 unsigned long offset)
{
	unsigned short tmp;

	tmp = silica_tsc_read(tsc, offset);
	tmp &= ~val;
	silica_tsc_write(tsc, tmp, offset);
}

static int silica_tsc_start_adc(struct silica_tsc *tsc, int channel)
{
	silica_tsc_clear_bit(tsc, SH_ADC_ADCSR_ADST, SH_ADC_ADCSR);
	silica_tsc_clear_bit(tsc, SH_ADC_ADCSR_TRGS_MASK, SH_ADC_ADCSR);

	silica_tsc_clear_bit(tsc, SH_ADC_ADCSR_MDS_MASK, SH_ADC_ADCSR);
	silica_tsc_clear_bit(tsc, SH_ADC_ADCSR_CH_MASK, SH_ADC_ADCSR);
	silica_tsc_set_bit(tsc, channel, SH_ADC_ADCSR);

	mdelay(1);

	silica_tsc_set_bit(tsc, SH_ADC_ADCSR_ADST, SH_ADC_ADCSR);

	return 0;
}

static void silica_tsc_pinmux_meas(unsigned char axis)
{
	if (axis == ABS_Y) {
		rza1_pfc_pin_assign(P5_8,  PMODE, PORT_OUT_HIGH);	
		rza1_pfc_pin_assign(P8_14, PMODE, PORT_OUT_LOW);	
		rza1_pfc_pin_assign(P8_15, PMODE, DIR_IN);	
		rza1_pfc_pin_assign(P7_0,  PMODE, DIR_IN);	

		rza1_pfc_pin_assign(P1_9, ALT1, DIIO_PBDC_DIS);

	} else {
		rza1_pfc_pin_assign(P8_15, PMODE, PORT_OUT_HIGH);	
		rza1_pfc_pin_assign(P7_0,  PMODE, PORT_OUT_LOW);	
		rza1_pfc_pin_assign(P5_8,  PMODE, DIR_IN);	
		rza1_pfc_pin_assign(P8_14, PMODE, DIR_IN);	

		rza1_pfc_pin_assign(P1_11, ALT1, DIIO_PBDC_DIS);
	}
}

static void silica_tsc_pinmux_int(void)
{
	rza1_pfc_pin_assign(P5_8,  PMODE, DIR_IN);	
	rza1_pfc_pin_assign(P8_15, PMODE, PORT_OUT_HIGH);	
	rza1_pfc_pin_assign(P7_0,  PMODE, DIR_IN);	
	rza1_pfc_pin_assign(P8_14, PMODE, DIR_IN);	
}

static inline unsigned int silica_tsc_read_pos(struct silica_tsc *tsc, int channel)
{
	unsigned int val;

	for (;;) {
		val = silica_tsc_read(tsc, SH_ADC_ADCSR);
		if (val & SH_ADC_ADCSR_ADF)
			break;
	}

	val = silica_tsc_read(tsc, silica_tsc_get_reg_addr(channel));
	val >>= SH_ADC_ADDR_SHIFT;
	val &= SH_ADC_ADDR_MASK;

	return val;
}

static inline unsigned int silica_tsc_read_xres(struct silica_tsc *tsc, unsigned char axis, unsigned char ain)
{
	unsigned int val;

	silica_tsc_pinmux_meas(axis);
	silica_tsc_start_adc(tsc, ain);
	val = silica_tsc_read_pos(tsc, ain);
	silica_tsc_clear_bit(tsc, SH_ADC_ADCSR_ADF, SH_ADC_ADCSR);

	return val;
}

static inline void silica_tsc_evt_add(struct silica_tsc *tsc, unsigned int x, unsigned int y,
				      int valid)
{
	struct input_dev *idev = tsc->idev;
	static unsigned int x_old = 0;
	static unsigned int y_old = 0;

	if (!valid) {
		x_old = x;
		y_old = y;
	}

	if (abs(x - x_old) > 20 || abs(y - y_old) > 20) {
		x = x_old;
		y = y_old;
	} else {
		x_old = x;
		y_old = y;
	}


	//printk(KERN_EMERG "--- x: %d\n", x);
	//printk(KERN_EMERG "--- y: %d\n", y);

	input_report_abs(idev, ABS_X, x);
	input_report_abs(idev, ABS_Y, y);
	input_report_key(idev, BTN_TOUCH, 1);
	input_sync(idev);
}

static inline void silica_tsc_evt_release(struct silica_tsc *tsc)
{
	struct input_dev *idev = tsc->idev;

	//printk(KERN_EMERG "--- release\n");

	input_report_key(idev, BTN_TOUCH, 0);
	input_sync(idev);
}

static int silica_tsc_thread(void *_tsc)
{
	struct silica_tsc *tsc = _tsc;
	DECLARE_WAITQUEUE(wait, current);
	bool frozen, ignore = false;
	int valid = 0;
	signed long residue = 0;

	set_freezable();
	add_wait_queue(&tsc->irq_wait, &wait);
	while (!kthread_freezable_should_stop(&frozen)) {
		unsigned int x, y;
		signed long timeout;

		if (frozen)
			ignore = true;

		set_current_state(TASK_INTERRUPTIBLE);
		timeout = HZ / 10;

		if (residue != 0) {
			x = silica_tsc_read_xres(tsc, ABS_X, tsc->pdata->ain_x);
			y = silica_tsc_read_xres(tsc, ABS_Y, tsc->pdata->ain_y);

			if (!ignore) {
				silica_tsc_evt_add(tsc, x, y, valid);
				valid = 1;
			}
		} else if (!residue && valid) {
			silica_tsc_evt_release(tsc);
			valid = 0;
		}

		if (tsc->irq_disabled) {
			tsc->irq_disabled = 0;
			silica_tsc_pinmux_int();
			enable_irq(tsc->irq);
		}

		residue = schedule_timeout(timeout);
	}

	remove_wait_queue(&tsc->irq_wait, &wait);
	tsc->rtask = NULL;
	return 0;
}

static irqreturn_t silica_tsc_irqh(int irq, void *dev)
{
	struct silica_tsc *tsc = dev;

	spin_lock(&tsc->irq_lock);
	tsc->irq_disabled = 1;
	disable_irq_nosync(tsc->irq);
	spin_unlock(&tsc->irq_lock);
	wake_up(&tsc->irq_wait);
	 
	return IRQ_HANDLED;
}

static int tsc_adc_get_clk(struct platform_device *pdev, struct clk **clk,
			   char *name)
{
	int ret;

	*clk = devm_clk_get(&pdev->dev, name);
	if (IS_ERR(*clk)) {
		dev_err(&pdev->dev, "Failed to get the clock.\n");
		return -1;
	}
	ret = clk_prepare_enable(*clk);
	if (ret) {
		dev_err(&pdev->dev,
			"Could not prepare or enable the clock.\n");
		return -1;
	}
	/* enable clock */
	ret = clk_enable(*clk);
	if (ret) {
		dev_err(&pdev->dev, "cannot enable clock\n");
		return -1;
	}
	return 0;
}

static int silica_tsc_open(struct input_dev *idev)
{
	struct silica_tsc *tsc = input_get_drvdata(idev);
	int ret = 0;

	init_waitqueue_head(&tsc->irq_wait);

	ret = request_irq(tsc->irq, silica_tsc_irqh, 0,
			  "silica-tsc", tsc);
	if (ret < 0)
		goto out;

	silica_tsc_set_bit(tsc, SH_ADC_ADCSR_CKS_256, SH_ADC_ADCSR);
	silica_tsc_pinmux_int();

	tsc->rtask = kthread_run(silica_tsc_thread, tsc, "ktsd");
	if (!IS_ERR(tsc->rtask)) {
		ret = 0;
	} else {
		free_irq(tsc->irq, tsc);
		tsc->rtask = NULL;
		ret = -EFAULT;
	}

out:
	return ret;
}

static void silica_tsc_close(struct input_dev *idev)
{
	struct silica_tsc *tsc = input_get_drvdata(idev);

	if (tsc->rtask)
		kthread_stop(tsc->rtask);

	free_irq(tsc->irq, tsc);
	silica_tsc_write(tsc, 0, SH_ADC_ADCSR);
}

static int silica_tsc_probe(struct platform_device *pdev)
{
	struct silica_tsc *tsc;
	struct input_dev *input_dev;
	struct silica_tsc_pdata *pdata;
	struct resource *res;
	int err = -EINVAL;

	tsc = kzalloc(sizeof(struct silica_tsc), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!tsc || !input_dev) {
		dev_err(&pdev->dev, "failed to allocate memory.\n");
		err = -ENOMEM;
		goto err_free_mem;
	}

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		err = -EINVAL;
		dev_err(&pdev->dev, "cannot get platform data\n");
		goto err_free_mem;
	}
	tsc->pdata = pdata;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tsc->reg = devm_request_and_ioremap(&pdev->dev, res);
	if (!tsc->reg) {
		dev_err(&pdev->dev, "cannot ioremap.\n");
		goto err_free_mem;
	}

	err = tsc_adc_get_clk(pdev, &tsc->clk, "adc0");
	if (err < 0) {
		dev_err(&pdev->dev, "failed to get clock\n");
		goto err_free_mem;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	tsc->irq = res->start;
	tsc->irq_disabled = 0;

	platform_set_drvdata(pdev, tsc);

	tsc->idev = input_dev;
	input_dev->name = "Touchscreen panel";
	input_dev->open = silica_tsc_open;
	input_dev->close = silica_tsc_close;
	input_dev->dev.parent = &pdev->dev;

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	
	input_set_abs_params(input_dev, ABS_X, pdata->x_min, pdata->x_max, 2, 0);
	input_set_abs_params(input_dev, ABS_Y, pdata->y_min, pdata->y_max, 2, 0);

	input_set_drvdata(input_dev, tsc);

	err = input_register_device(input_dev);
	if (err)
		goto err_free_mem;

	dev_info(&pdev->dev, "Silica touchscreen probed\n");

	return 0;

err_free_mem:
	input_free_device(input_dev);
	kfree(tsc);

	return err;
}

static int silica_tsc_remove(struct platform_device *pdev)
{
	struct silica_tsc *tsc = platform_get_drvdata(pdev);

	input_unregister_device(tsc->idev);
	kfree(tsc);

	return 0;
}

static struct platform_driver silica_tsc_driver = {
	.probe	= silica_tsc_probe,
	.remove	= silica_tsc_remove,
	.driver	= {
		.name   = "silica_tsc",
		.owner	= THIS_MODULE,
	},
};
module_platform_driver(silica_tsc_driver);

MODULE_DESCRIPTION("SILICA touchscreen controller driver");
MODULE_AUTHOR("Carlo Caione <carlo@caione.org>");
MODULE_LICENSE("GPL");
