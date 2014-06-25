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

#define ADC_RESOLUTION			10
#define ADC_SINGLE_SAMPLE_MASK		((1 << ADC_RESOLUTION) - 1)

#define SH_ADC_ADDRA	0x00
#define SH_ADC_ADDRB	0x02
#define SH_ADC_ADDRC	0x04
#define SH_ADC_ADDRD	0x06
#define SH_ADC_ADDRE	0x08
#define SH_ADC_ADDRF	0x0a
#define SH_ADC_ADDRG	0x0c
#define SH_ADC_ADDRH	0x0e
#define SH_ADC_ADCSR	0x60

#define SH_ADC_ADDR_MASK	0xffc0
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
#define silica_ts_get_reg_addr(ch)	(SH_ADC_ADDRA + ch * 2)
   
enum {
	X = 0,
	Y,
	NUM_AXIS
};

struct silica_ts {
	struct input_dev *idev;
	struct clk *clk;
	spinlock_t irq_lock;
	wait_queue_head_t irq_wait;
	unsigned irq_disabled;
	struct task_struct *rtask;
	void __iomem *reg;
	int irq;
};

static void silica_ts_write(struct silica_ts *adc, unsigned short data,
			 unsigned long offset)
{
	iowrite16(data, adc->reg + offset);
}

static unsigned short silica_ts_read(struct silica_ts *adc, unsigned long offset)
{
	return ioread16(adc->reg + offset);
}

static void silica_ts_set_bit(struct silica_ts *adc, unsigned short val,
			   unsigned long offset)
{
	unsigned short tmp;

	tmp = silica_ts_read(adc, offset);
	tmp |= val;
	silica_ts_write(adc, tmp, offset);
}

static void silica_ts_clear_bit(struct silica_ts *adc, unsigned short val,
			     unsigned long offset)
{
	unsigned short tmp;

	tmp = silica_ts_read(adc, offset);
	tmp &= ~val;
	silica_ts_write(adc, tmp, offset);
}

static int sh_adc_start_adc(struct silica_ts *adc, int channel)
{
	silica_ts_clear_bit(adc, SH_ADC_ADCSR_ADST, SH_ADC_ADCSR);
	silica_ts_clear_bit(adc, SH_ADC_ADCSR_TRGS_MASK, SH_ADC_ADCSR);

	silica_ts_clear_bit(adc, SH_ADC_ADCSR_MDS_MASK, SH_ADC_ADCSR);
	silica_ts_clear_bit(adc, SH_ADC_ADCSR_CH_MASK, SH_ADC_ADCSR);
	silica_ts_set_bit(adc, channel, SH_ADC_ADCSR);

	mdelay(1); ////////

	silica_ts_set_bit(adc, SH_ADC_ADCSR_ADST,
		       SH_ADC_ADCSR);

	return 0;
}

static void CC_Touch_Set_Meas_Pins(unsigned char axis)
{
	if (axis == Y) {
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

static void CC_Touch_Set_Stby_Pins(void)
{
	rza1_pfc_pin_assign(P5_8,  PMODE, DIR_IN);	
	rza1_pfc_pin_assign(P8_15, PMODE, PORT_OUT_HIGH);	
	rza1_pfc_pin_assign(P7_0,  PMODE, DIR_IN);	
	rza1_pfc_pin_assign(P8_14, PMODE, DIR_IN);	
}

static inline unsigned int silica_ts_read_pos(struct silica_ts *ts, int channel)
{
	unsigned int val;

	for (;;) {
		val = silica_ts_read(ts, SH_ADC_ADCSR);
		if (val & SH_ADC_ADCSR_ADF)
			break;
		set_current_state(TASK_INTERRUPTIBLE);
	//	schedule_timeout(1);
	}

	val = silica_ts_read(ts, silica_ts_get_reg_addr(channel));
	val >>= 6;
	val &= 0x03FFuL;

	return val;
}

static int silica_tsc_thread(void *_ts)
{
	struct silica_ts *ts = _ts;
	DECLARE_WAITQUEUE(wait, current);
	bool frozen, ignore = false;
	int valid = 0;
	signed long residue;

	printk(KERN_EMERG "--->>> silica_tsc_thread <<<---\n");

	set_freezable();
	add_wait_queue(&ts->irq_wait, &wait);
	while (!kthread_freezable_should_stop(&frozen)) {
		unsigned int x, y;
		signed long timeout;

		if (frozen)
			ignore = true;

		set_current_state(TASK_INTERRUPTIBLE);
		timeout = HZ;

		if (residue != 0) {
			CC_Touch_Set_Meas_Pins(X);
			sh_adc_start_adc(ts, 3);
			x = silica_ts_read_pos(ts, 3);
			silica_ts_clear_bit(ts, SH_ADC_ADCSR_ADF, SH_ADC_ADCSR);

			CC_Touch_Set_Meas_Pins(Y);
			sh_adc_start_adc(ts, 1);
			y = silica_ts_read_pos(ts, 1);
			silica_ts_clear_bit(ts, SH_ADC_ADCSR_ADF, SH_ADC_ADCSR);
			
			printk(KERN_EMERG "--- x: %d\n", x);
			printk(KERN_EMERG "--- y: %d\n", y);
		}

		printk(KERN_EMERG "--- before schedule_timeout\n");

		if (ts->irq_disabled) {
			ts->irq_disabled = 0;
			CC_Touch_Set_Stby_Pins();
			enable_irq(ts->irq);
		}

		residue = schedule_timeout(timeout);
		printk(KERN_EMERG "--- residue: %ld\n", residue);
	}

	remove_wait_queue(&ts->irq_wait, &wait);
	ts->rtask = NULL;
	return 0;
}

static irqreturn_t silica_tsc_irqh(int irq, void *dev)
{
	struct silica_ts *ts_dev = dev;

	printk(KERN_EMERG "--->>> silica_tsc_irqh <<<---\n");

	spin_lock(&ts_dev->irq_lock);
	ts_dev->irq_disabled = 1;
	disable_irq_nosync(ts_dev->irq);
	spin_unlock(&ts_dev->irq_lock);
	wake_up(&ts_dev->irq_wait);
	 
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

static int silica_tsc_probe(struct platform_device *pdev)
{
	struct silica_ts *ts_dev;
	struct input_dev *input_dev;
	struct resource *res;
	int err = -EINVAL;

	printk(KERN_EMERG "------------------- silica_tsc_probe -------------\n");

	ts_dev = kzalloc(sizeof(struct silica_ts), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!ts_dev || !input_dev) {
		dev_err(&pdev->dev, "failed to allocate memory.\n");
		err = -ENOMEM;
		goto err_free_mem;
	}
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ts_dev->reg = devm_request_and_ioremap(&pdev->dev, res);
	if (!ts_dev->reg) {
		dev_err(&pdev->dev, "cannot ioremap.\n");
		goto err_free_mem;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	ts_dev->irq = res->start;
		
	err = tsc_adc_get_clk(pdev, &ts_dev->clk, "adc0");
	if (err < 0) {
		dev_err(&pdev->dev, "failed to get clock\n");
		goto err_free_mem;
	}

	ts_dev->idev = input_dev;
	input_dev->name = "Touchscreen panel";
	input_dev->dev.parent = &pdev->dev;

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	
	input_set_abs_params(input_dev, ABS_X, 0, ADC_SINGLE_SAMPLE_MASK, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, ADC_SINGLE_SAMPLE_MASK, 0, 0);

	input_set_drvdata(input_dev, ts_dev);

	err = input_register_device(input_dev);
	if (err)
		goto err_free_irq;

	platform_set_drvdata(pdev, ts_dev);

// ----------------------------------------------------------
	ts_dev->irq_disabled = 0;
	init_waitqueue_head(&ts_dev->irq_wait);
	ts_dev->rtask = kthread_run(silica_tsc_thread, ts_dev, "ktsd");
	if (IS_ERR(ts_dev->rtask))
		goto err_free_irq;
	
	err = request_irq(ts_dev->irq, silica_tsc_irqh, 0,
			  pdev->dev.driver->name, ts_dev);
	if (err) {
		dev_err(&pdev->dev, "failed to allocate irq.\n");
		goto err_free_mem;
	}

// ----------------------------------------------------------
	CC_Touch_Set_Stby_Pins();

//	silica_ts_set_bit(ts_dev, SH_ADC_ADCSR_ADIE, SH_ADC_ADCSR);
	silica_ts_set_bit(ts_dev, SH_ADC_ADCSR_CKS_256, SH_ADC_ADCSR);
// ----------------------------------------------------------

	printk(KERN_EMERG "------------------- silica_tsc_probe done -------------\n");

	return 0;

err_free_irq:
	free_irq(ts_dev->irq, ts_dev);
err_free_mem:
	input_free_device(input_dev);
	kfree(ts_dev);

	return err;
}

static int silica_tsc_remove(struct platform_device *pdev)
{
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
