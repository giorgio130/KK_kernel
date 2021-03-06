/*
 * Copyright 2009 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */


#undef DI_DEBUG        /* enable debug messages */
#undef DI_DEBUG_REGIO  /* show register read/write */
#undef DI_TESTING      /* include test code */

#ifdef DI_DEBUG
#define di_debug(fmt, arg...) os_printk(KERN_INFO fmt, ##arg)
#else
#define di_debug(fmt, arg...) do {} while (0)
#endif

#define di_info(fmt, arg...) os_printk(KERN_INFO fmt, ##arg)
#define di_warn(fmt, arg...) os_printk(KERN_WARNING fmt, ##arg)

#include "sahara2/include/portable_os.h"
#include "dryice.h"
#include "dryice-regs.h"

/* mask of the lock-related function flags */
#define DI_FUNC_LOCK_FLAGS  (DI_FUNC_FLAG_READ_LOCK  | \
			     DI_FUNC_FLAG_WRITE_LOCK | \
			     DI_FUNC_FLAG_HARD_LOCK)

/*
 * dryice hardware states
 */
enum di_states {
	DI_STATE_VALID = 0,
	DI_STATE_NON_VALID,
	DI_STATE_FAILURE,
};

/*
 * todo list actions
 */
enum todo_actions {
	TODO_ACT_WRITE_VAL,
	TODO_ACT_WRITE_PTR,
	TODO_ACT_WRITE_PTR32,
	TODO_ACT_ASSIGN,
	TODO_ACT_WAIT_RKG,
};

/*
 * todo list status
 */
enum todo_status {
	TODO_ST_LOADING,
	TODO_ST_READY,
	TODO_ST_PEND_WCF,
	TODO_ST_PEND_RKG,
	TODO_ST_DONE,
};

OS_DEV_INIT_DCL(dryice_init)
OS_DEV_SHUTDOWN_DCL(dryice_exit)
OS_DEV_ISR_DCL(dryice_norm_irq)
OS_WAIT_OBJECT(done_queue);
OS_WAIT_OBJECT(exit_queue);

struct dryice_data {
	int busy;               /* enforce exclusive access */
	os_lock_t busy_lock;
	int exit_flag;          /* don't start new operations */

	uint32_t baseaddr;      /* physical base address */
	void *ioaddr;           /* virtual base address */

	/* interrupt handling */
	struct irq_struct {
		os_interrupt_id_t irq;
		int set;
	} irq_norm, irq_sec;

	struct clk *clk;        /* clock control */

	int key_programmed;     /* key has been programmed */
	int key_selected;       /* key has been selected */

	/* callback function and cookie */
	void (*cb_func)(di_return_t rc, unsigned long cookie);
	unsigned long cb_cookie;
} *di = NULL;

#define TODO_LIST_LEN	12
static struct {
	struct td {
		enum todo_actions action;
		uint32_t src;
		uint32_t dst;
		int num;
	} list[TODO_LIST_LEN];
	int cur;                /* current todo pointer */
	int num;		/* number of todo's on the list */
	int async;              /* non-zero if list is async */
	int status;             /* current status of the list */
	di_return_t rc;         /* return code generated by the list */
} todo;

/*
 * dryice register read/write functions
 */
#ifdef DI_DEBUG_REGIO
static uint32_t di_read(int reg)
{
	uint32_t val = os_read32(di->ioaddr + (reg));
	di_info("di_read(0x%02x) = 0x%08x\n", reg, val);

	return val;
}

static void di_write(uint32_t val, int reg)
{
	di_info("dryice_write_reg(0x%08x, 0x%02x)\n", val, reg);
	os_write32(di->ioaddr + (reg), val);
}
#else
#define di_read(reg)        os_read32(di->ioaddr + (reg))
#define di_write(val, reg)  os_write32(di->ioaddr + (reg), val);
#endif

/*
 * set the dryice busy flag atomically, allowing
 * for case where the driver is trying to exit.
 */
static int di_busy_set(void)
{
	os_lock_context_t context;
	int rc = 0;

	os_lock_save_context(di->busy_lock, context);
	if (di->exit_flag || di->busy)
		rc = 1;
	else
		di->busy = 1;
	os_unlock_restore_context(di->busy_lock, context);

	return rc;
}

/*
 * clear the dryice busy flag
 */
static inline void di_busy_clear(void)
{
	/* don't acquire the lock because the race is benign */
	di->busy = 0;

	if (di->exit_flag)
		os_wake_sleepers(exit_queue);
}

/*
 * return the current state of dryice
 * (valid, non-valid, or failure)
 */
static enum di_states di_state(void)
{
	enum di_states state = DI_STATE_VALID;
	uint32_t dsr = di_read(DSR);

	if (dsr & DSR_NVF)
		state = DI_STATE_NON_VALID;
	else if (dsr & DSR_SVF)
		state = DI_STATE_FAILURE;

	return state;
}

#define DI_WRITE_LOOP_CNT 0x1000
/*
 * the write-error flag is something that shouldn't get set
 * during normal operation.  if it's set something is terribly
 * wrong.  the best we can do is try to clear the bit and hope
 * that dryice will recover.  this situation is similar to an
 * unexpected bus fault in terms of severity.
 */
static void try_to_clear_wef(void)
{
	int cnt;

	while (1) {
		di_write(DSR_WEF, DSR);
		for (cnt = 0; cnt < DI_WRITE_LOOP_CNT; cnt++) {
			if ((di_read(DSR) & DSR_WEF) == 0)
				break;
		}
		di_warn("WARNING: DryIce cannot clear DSR_WEF "
			"(Write Error Flag)!\n");
	}
}

/*
 * write a dryice register and loop, waiting for it
 * to complete. use only during driver initialization.
 * returns 0 on success or 1 on write failure.
 */
static int di_write_loop(uint32_t val, int reg)
{
	int rc = 0;
	int cnt;

	di_debug("FUNC: %s\n", __func__);
	di_write(val, reg);

	for (cnt = 0; cnt < DI_WRITE_LOOP_CNT; cnt++) {
		uint32_t dsr = di_read(DSR);
		if (dsr & DSR_WEF) {
			try_to_clear_wef();
			rc = 1;
		}
		if (dsr & DSR_WCF)
			break;
	}
	di_debug("wait_write_loop looped %d times\n", cnt);
	if (cnt == DI_WRITE_LOOP_CNT)
		rc = 1;

	if (rc)
		di_warn("DryIce wait_write_done: WRITE ERROR!\n");
	return rc;
}

/*
 * initialize the todo list. must be called
 * before adding items to the list.
 */
static void todo_init(int async_flag)
{
	di_debug("FUNC: %s\n", __func__);
	todo.cur = 0;
	todo.num = 0;
	todo.async = async_flag;
	todo.rc = 0;
	todo.status = TODO_ST_LOADING;
}

/*
 * perform the current action on the todo list
 */
#define TC  todo.list[todo.cur]
void todo_cur(void)
{
	di_debug("FUNC: %s[%d]\n", __func__, todo.cur);
	switch (TC.action) {
	case TODO_ACT_WRITE_VAL:
		di_debug("  TODO_ACT_WRITE_VAL\n");
		/* enable the write-completion interrupt */
		todo.status = TODO_ST_PEND_WCF;
		di_write(di_read(DIER) | DIER_WCIE, DIER);

		di_write(TC.src, TC.dst);
		break;

	case TODO_ACT_WRITE_PTR32:
		di_debug("  TODO_ACT_WRITE_PTR32\n");
		/* enable the write-completion interrupt */
		todo.status = TODO_ST_PEND_WCF;
		di_write(di_read(DIER) | DIER_WCIE, DIER);

		di_write(*(uint32_t *)TC.src, TC.dst);
		break;

	case TODO_ACT_WRITE_PTR:
		{
			uint8_t *p = (uint8_t *)TC.src;
			uint32_t val = 0;
			int num = TC.num;

			di_debug("  TODO_ACT_WRITE_PTR\n");
			while (num--)
				val = (val << 8) | *p++;

			/* enable the write-completion interrupt */
			todo.status = TODO_ST_PEND_WCF;
			di_write(di_read(DIER) | DIER_WCIE, DIER);

			di_write(val, TC.dst);
		}
		break;

	case TODO_ACT_ASSIGN:
		di_debug("  TODO_ACT_ASSIGN\n");
		switch (TC.num) {
		case 1:
			*(uint8_t *)TC.dst = TC.src;
			break;
		case 2:
			*(uint16_t *)TC.dst = TC.src;
			break;
		case 4:
			*(uint32_t *)TC.dst = TC.src;
			break;
		default:
			di_warn("Unexpected size in TODO_ACT_ASSIGN\n");
			break;
		}
		break;

	case TODO_ACT_WAIT_RKG:
		di_debug("  TODO_ACT_WAIT_RKG\n");
		/* enable the random-key interrupt */
		todo.status = TODO_ST_PEND_RKG;
		di_write(di_read(DIER) | DIER_RKIE, DIER);
		break;

	default:
		di_debug("  TODO_ACT_NOOP\n");
		break;
	}
}

/*
 * called when done with the todo list.
 * if async, it does the callback.
 * if blocking, it wakes up the caller.
 */
static void todo_done(di_return_t rc)
{
	todo.rc = rc;
	todo.status = TODO_ST_DONE;
	if (todo.async) {
		di_busy_clear();
		if (di->cb_func)
			di->cb_func(rc, di->cb_cookie);
	} else
		os_wake_sleepers(done_queue);
}

/*
 * performs the actions sequentially from the todo list
 * until it encounters an item that isn't ready.
 */
static void todo_run(void)
{
	di_debug("FUNC: %s\n", __func__);
	while (todo.status == TODO_ST_READY) {
		if (todo.cur == todo.num) {
			todo_done(0);
			break;
		}
		todo_cur();
		if (todo.status != TODO_ST_READY)
			break;
		todo.cur++;
	}
}

/*
 * kick off the todo list by making it ready
 */
static void todo_start(void)
{
	di_debug("FUNC: %s\n", __func__);
	todo.status = TODO_ST_READY;
	todo_run();
}

/*
 * blocking callers sleep here until the todo list is done
 */
static int todo_wait_done(void)
{
	di_debug("FUNC: %s\n", __func__);
	os_sleep(done_queue, todo.status == TODO_ST_DONE, 0);

	return todo.rc;
}

/*
 * add a dryice register write to the todo list.
 * the value to be written is supplied.
 */
#define todo_write_val(val, reg) \
		todo_add(TODO_ACT_WRITE_VAL, val, reg, 0)

/*
 * add a dryice register write to the todo list.
 * "size" bytes pointed to by addr will be written.
 */
#define todo_write_ptr(addr, reg, size) \
		todo_add(TODO_ACT_WRITE_PTR, (uint32_t)addr, reg, size)

/*
 * add a dryice register write to the todo list.
 * the word pointed to by addr will be written.
 */
#define todo_write_ptr32(addr, reg) \
		todo_add(TODO_ACT_WRITE_PTR32, (uint32_t)addr, reg, 0)

/*
 * add a dryice memory write to the todo list.
 * object can only have a size of 1, 2, or 4 bytes.
 */
#define todo_assign(var, val) \
		todo_add(TODO_ACT_ASSIGN, val, (uint32_t)&(var), sizeof(var))

#define todo_wait_rkg() \
		todo_add(TODO_ACT_WAIT_RKG, 0, 0, 0)

static void todo_add(int action, uint32_t src, uint32_t dst, int num)
{
	struct td *p = &todo.list[todo.num];

	di_debug("FUNC: %s\n", __func__);
	if (todo.num == TODO_LIST_LEN) {
		di_warn("WARNING: DryIce todo-list overflow!\n");
		return;
	}
	p->action = action;
	p->src = src;
	p->dst = dst;
	p->num = num;
	todo.num++;
}

#if defined(DI_DEBUG) || defined(DI_TESTING)
/*
 * print out the contents of the dryice status register
 * with all the bits decoded
 */
static void show_dsr(const char *heading)
{
	uint32_t dsr = di_read(DSR);

	di_info("%s\n", heading);
	if (dsr & DSR_TAMPER_BITS) {
		if (dsr & DSR_WTD)
			di_info("Wire-mesh Tampering Detected\n");
		if (dsr & DSR_ETBD)
			di_info("External Tampering B Detected\n");
		if (dsr & DSR_ETAD)
			di_info("External Tampering A Detected\n");
		if (dsr & DSR_EBD)
			di_info("External Boot Detected\n");
		if (dsr & DSR_SAD)
			di_info("Security Alarm Detected\n");
		if (dsr & DSR_TTD)
			di_info("Temperature Tampering Detected\n");
		if (dsr & DSR_CTD)
			di_info("Clock Tampering Detected\n");
		if (dsr & DSR_VTD)
			di_info("Voltage Tampering Detected\n");
		if (dsr & DSR_MCO)
			di_info("Monotonic Counter Overflow\n");
		if (dsr & DSR_TCO)
			di_info("Time Counter Overflow\n");
	} else
		di_info("No Tamper Events Detected\n");

	di_info("%d Key Busy Flag\n",           !!(dsr & DSR_KBF));
	di_info("%d Write Busy Flag\n",         !!(dsr & DSR_WBF));
	di_info("%d Write Next Flag\n",         !!(dsr & DSR_WNF));
	di_info("%d Write Complete Flag\n",     !!(dsr & DSR_WCF));
	di_info("%d Write Error Flag\n",        !!(dsr & DSR_WEF));
	di_info("%d Random Key Error\n",        !!(dsr & DSR_RKE));
	di_info("%d Random Key Valid\n",        !!(dsr & DSR_RKV));
	di_info("%d Clock Alarm Flag\n",        !!(dsr & DSR_CAF));
	di_info("%d Non-Valid Flag\n",          !!(dsr & DSR_NVF));
	di_info("%d Security Violation Flag\n", !!(dsr & DSR_SVF));
}

/*
 * print out a key in hex
 */
static void print_key(const char *tag, uint8_t *key, int bits)
{
	int bytes = (bits + 7) / 8;

	di_info("%s", tag);
	while (bytes--)
		os_printk("%02x", *key++);
	os_printk("\n");
}
#endif  /* defined(DI_DEBUG) || defined(DI_TESTING) */

/*
 * dryice normal interrupt service routine
 */
OS_DEV_ISR(dryice_norm_irq)
{
	/* save dryice status register */
	uint32_t dsr = di_read(DSR);

	if (dsr & DSR_WCF) {
		/* disable the write-completion interrupt */
		di_write(di_read(DIER) & ~DIER_WCIE, DIER);

		if (todo.status == TODO_ST_PEND_WCF) {
			if (dsr & DSR_WEF) {
				try_to_clear_wef();
				todo_done(DI_ERR_WRITE);
			} else {
				todo.cur++;
				todo.status = TODO_ST_READY;
				todo_run();
			}
		}
	} else if (dsr & (DSR_RKV | DSR_RKE)) {
		/* disable the random-key-gen interrupt */
		di_write(di_read(DIER) & ~DIER_RKIE, DIER);

		if (todo.status == TODO_ST_PEND_RKG) {
			if (dsr & DSR_RKE)
				todo_done(DI_ERR_FAIL);
			else {
				todo.cur++;
				todo.status = TODO_ST_READY;
				todo_run();
			}
		}
	}

	os_dev_isr_return(1);
}

/* write loop with error handling -- for init only */
#define di_write_loop_goto(val, reg, rc, label) \
		do {if (di_write_loop(val, reg)) \
		{rc = OS_ERROR_FAIL_S; goto label; } } while (0)

/*
 * dryice driver initialization
 */
OS_DEV_INIT(dryice_init)
{
	di_return_t rc = 0;

	di_info("MXC DryIce driver\n");

	/* allocate memory */
	di = os_alloc_memory(sizeof(*di), GFP_KERNEL);
	if (di == NULL) {
		rc = OS_ERROR_NO_MEMORY_S;
		goto err_alloc;
	}
	memset(di, 0, sizeof(*di));
	di->baseaddr = DRYICE_BASE_ADDR;
	di->irq_norm.irq = MXC_INT_DRYICE_NORM;
	di->irq_sec.irq = MXC_INT_DRYICE_SEC;

	/* map i/o registers */
	di->ioaddr = os_map_device(di->baseaddr, DI_ADDRESS_RANGE);
	if (di->ioaddr == NULL) {
		rc = OS_ERROR_FAIL_S;
		goto err_iomap;
	}

	/* allocate locks */
	di->busy_lock = os_lock_alloc_init();
	if (di->busy_lock == NULL) {
		rc = OS_ERROR_NO_MEMORY_S;
		goto err_locks;
	}

	/* enable clocks (is there a portable way to do this?) */
	di->clk = clk_get(NULL, "dryice_clk");
	clk_enable(di->clk);

	/* register for interrupts */
	/* os_register_interrupt() dosen't support an option to make the
	    interrupt as shared. Replaced it with request_irq().*/
	rc = request_irq(di->irq_norm.irq, dryice_norm_irq, IRQF_SHARED,
				"dry_ice", di);
	if (rc)
		goto err_irqs;
	else
		di->irq_norm.set = 1;

	/*
	 * DRYICE HARDWARE INIT
	 */

#ifdef DI_DEBUG
	show_dsr("DSR Pre-Initialization State");
#endif

	if (di_state() == DI_STATE_NON_VALID) {
		uint32_t dsr = di_read(DSR);

		di_debug("initializing from non-valid state\n");

		/* clear security violation flag */
		if (dsr & DSR_SVF)
			di_write_loop_goto(DSR_SVF, DSR, rc, err_write);

		/* clear tamper detect flags */
		if (dsr & DSR_TAMPER_BITS)
			di_write_loop_goto(DSR_TAMPER_BITS, DSR, rc, err_write);

		/* initialize timers */
		di_write_loop_goto(0, DTCLR, rc, err_write);
		di_write_loop_goto(0, DTCMR, rc, err_write);
		di_write_loop_goto(0, DMCR, rc, err_write);

		/* clear non-valid flag */
		di_write_loop_goto(DSR_NVF, DSR, rc, err_write);
	}

	/* set tamper events we are interested in watching */
	di_write_loop_goto(DTCR_WTE | DTCR_ETBE | DTCR_ETAE, DTCR, rc,
			   err_write);
#ifdef DI_DEBUG
	show_dsr("DSR Post-Initialization State");
#endif
	os_dev_init_return(OS_ERROR_OK_S);

err_write:
	/* unregister interrupts */
	if (di->irq_norm.set)
		os_deregister_interrupt(di->irq_norm.irq);
	if (di->irq_sec.set)
		os_deregister_interrupt(di->irq_sec.irq);

	/* turn off clocks (is there a portable way to do this?) */
	clk_disable(di->clk);
	clk_put(di->clk);

err_irqs:
	/* unallocate locks */
	os_lock_deallocate(di->busy_lock);

err_locks:
	/* unmap i/o registers */
	os_unmap_device(di->ioaddr, DI_ADDRESS_RANGE);

err_iomap:
	/* free the dryice struct */
	os_free_memory(di);

err_alloc:
	os_dev_init_return(rc);
}

/*
 * dryice driver exit routine
 */
OS_DEV_SHUTDOWN(dryice_exit)
{
	/* don't allow new operations */
	di->exit_flag = 1;

	/* wait for the current operation to complete */
	os_sleep(exit_queue, di->busy == 0, 0);

	/* unregister interrupts */
	if (di->irq_norm.set)
		os_deregister_interrupt(di->irq_norm.irq);
	if (di->irq_sec.set)
		os_deregister_interrupt(di->irq_sec.irq);

	/* turn off clocks (is there a portable way to do this?) */
	clk_disable(di->clk);
	clk_put(di->clk);

	/* unallocate locks */
	os_lock_deallocate(di->busy_lock);

	/* unmap i/o registers */
	os_unmap_device(di->ioaddr, DI_ADDRESS_RANGE);

	/* free the dryice struct */
	os_free_memory(di);

	os_dev_shutdown_return(OS_ERROR_OK_S);
}

di_return_t dryice_set_programmed_key(const void *key_data, int key_bits,
				      int flags)
{
	uint32_t dcr;
	int key_bytes, reg;
	di_return_t rc = 0;

	if (di_busy_set())
		return DI_ERR_BUSY;

	if (key_data == NULL) {
		rc = DI_ERR_INVAL;
		goto err;
	}
	if (key_bits < 0 || key_bits > MAX_KEY_LEN || key_bits % 8) {
		rc = DI_ERR_INVAL;
		goto err;
	}
	if (flags & DI_FUNC_FLAG_WORD_KEY) {
		if (key_bits % 32 || (uint32_t)key_data & 0x3) {
			rc = DI_ERR_INVAL;
			goto err;
		}
	}
	if (di->key_programmed) {
		rc = DI_ERR_INUSE;
		goto err;
	}
	if (di_state() == DI_STATE_FAILURE) {
		rc = DI_ERR_STATE;
		goto err;
	}
	dcr = di_read(DCR);
	if (dcr & DCR_PKWHL) {
		rc = DI_ERR_HLOCK;
		goto err;
	}
	if (dcr & DCR_PKWSL) {
		rc = DI_ERR_SLOCK;
		goto err;
	}
	key_bytes = key_bits / 8;

	todo_init((flags & DI_FUNC_FLAG_ASYNC) != 0);

	/* accomodate busses that can only do 32-bit transfers */
	if (flags & DI_FUNC_FLAG_WORD_KEY) {
		uint32_t *keyp = (void *)key_data;

		for (reg = 0; reg < MAX_KEY_WORDS; reg++) {
			if (reg < MAX_KEY_WORDS - key_bytes / 4)
				todo_write_val(0, DPKR7 - reg * 4);
			else {
				todo_write_ptr32(keyp, DPKR7 - reg * 4);
				keyp++;
			}
		}
	} else {
		uint8_t *keyp = (void *)key_data;

		for (reg = 0; reg < MAX_KEY_WORDS; reg++) {
			int size = key_bytes - (MAX_KEY_WORDS - reg - 1) * 4;
			if (size <= 0)
				todo_write_val(0, DPKR7 - reg * 4);
			else {
				if (size > 4)
					size = 4;
				todo_write_ptr(keyp, DPKR7 - reg * 4, size);
				keyp += size;
			}
		}
	}
	todo_assign(di->key_programmed, 1);

	if (flags & DI_FUNC_LOCK_FLAGS) {
		dcr = di_read(DCR);
		if (flags & DI_FUNC_FLAG_READ_LOCK) {
			if (flags & DI_FUNC_FLAG_HARD_LOCK)
				dcr |= DCR_PKRHL;
			else
				dcr |= DCR_PKRSL;
		}
		if (flags & DI_FUNC_FLAG_WRITE_LOCK) {
			if (flags & DI_FUNC_FLAG_HARD_LOCK)
				dcr |= DCR_PKWHL;
			else
				dcr |= DCR_PKWSL;
		}
		todo_write_val(dcr, DCR);
	}
	todo_start();

	if (flags & DI_FUNC_FLAG_ASYNC)
		return 0;

	rc = todo_wait_done();
err:
	di_busy_clear();
	return rc;
}
EXTERN_SYMBOL(dryice_set_programmed_key);

di_return_t dryice_get_programmed_key(uint8_t *key_data, int key_bits)
{
	int reg, byte, key_bytes;
	uint32_t dcr, dpkr;
	di_return_t rc = 0;

	if (di_busy_set())
		return DI_ERR_BUSY;

	if (key_data == NULL) {
		rc = DI_ERR_INVAL;
		goto err;
	}
	if (key_bits < 0 || key_bits > MAX_KEY_LEN || key_bits % 8) {
		rc = DI_ERR_INVAL;
		goto err;
	}
	#if 0
	if (!di->key_programmed) {
		rc = DI_ERR_UNSET;
		goto err;
	}
	#endif
	if (di_state() == DI_STATE_FAILURE) {
		rc = DI_ERR_STATE;
		goto err;
	}
	dcr = di_read(DCR);
	if (dcr & DCR_PKRHL) {
		rc = DI_ERR_HLOCK;
		goto err;
	}
	if (dcr & DCR_PKRSL) {
		rc = DI_ERR_SLOCK;
		goto err;
	}
	key_bytes = key_bits / 8;

	/* read key */
	for (reg = 0; reg < MAX_KEY_WORDS; reg++) {
		if (reg < (MAX_KEY_BYTES - key_bytes) / 4)
			continue;
		dpkr = di_read(DPKR7 - reg * 4);

		for (byte = 0; byte < 4; byte++) {
			if (reg * 4 + byte >= MAX_KEY_BYTES - key_bytes) {
				int shift = 24 - byte * 8;
				*key_data++ = (dpkr >> shift) & 0xff;
			}
		}
		dpkr = 0;	/* cleared for security */
	}
err:
	di_busy_clear();
	return rc;
}
EXTERN_SYMBOL(dryice_get_programmed_key);

di_return_t dryice_release_programmed_key(void)
{
	uint32_t dcr;
	di_return_t rc = 0;

	if (di_busy_set())
		return DI_ERR_BUSY;

	if (!di->key_programmed) {
		rc = DI_ERR_UNSET;
		goto err;
	}
	dcr = di_read(DCR);
	if (dcr & DCR_PKWHL) {
		rc = DI_ERR_HLOCK;
		goto err;
	}
	if (dcr & DCR_PKWSL) {
		rc = DI_ERR_SLOCK;
		goto err;
	}
	di->key_programmed = 0;

err:
	di_busy_clear();
	return rc;
}
EXTERN_SYMBOL(dryice_release_programmed_key);

di_return_t dryice_set_random_key(int flags)
{
	uint32_t dcr;
	di_return_t rc = 0;

	if (di_busy_set())
		return DI_ERR_BUSY;

	if (di_state() == DI_STATE_FAILURE) {
		rc = DI_ERR_STATE;
		goto err;
	}
	dcr = di_read(DCR);
	if (dcr & DCR_RKHL) {
		rc = DI_ERR_HLOCK;
		goto err;
	}
	if (dcr & DCR_RKSL) {
		rc = DI_ERR_SLOCK;
		goto err;
	}
	todo_init((flags & DI_FUNC_FLAG_ASYNC) != 0);

	/* clear Random Key Error bit, if set */
	if (di_read(DSR) & DSR_RKE)
		todo_write_val(DSR_RKE, DCR);

	/* load random key */
	todo_write_val(DKCR_LRK, DKCR);

	/* wait for RKV (valid) or RKE (error) */
	todo_wait_rkg();

	if (flags & DI_FUNC_LOCK_FLAGS) {
		dcr = di_read(DCR);
		if (flags & DI_FUNC_FLAG_WRITE_LOCK) {
			if (flags & DI_FUNC_FLAG_HARD_LOCK)
				dcr |= DCR_RKHL;
			else
				dcr |= DCR_RKSL;
		}
		todo_write_val(dcr, DCR);
	}
	todo_start();

	if (flags & DI_FUNC_FLAG_ASYNC)
		return 0;

	rc = todo_wait_done();
err:
	di_busy_clear();
	return rc;
}
EXTERN_SYMBOL(dryice_set_random_key);

di_return_t dryice_select_key(di_key_t key, int flags)
{
	uint32_t dcr, dksr;
	di_return_t rc = 0;

	if (di_busy_set())
		return DI_ERR_BUSY;

	switch (key) {
	case DI_KEY_FK:
		dksr = DKSR_IIM_KEY;
		break;
	case DI_KEY_PK:
		dksr = DKSR_PROG_KEY;
		break;
	case DI_KEY_RK:
		dksr = DKSR_RAND_KEY;
		break;
	case DI_KEY_FPK:
		dksr = DKSR_PROG_XOR_IIM_KEY;
		break;
	case DI_KEY_FRK:
		dksr = DKSR_RAND_XOR_IIM_KEY;
		break;
	default:
		rc = DI_ERR_INVAL;
		goto err;
	}
	if (di->key_selected) {
		rc = DI_ERR_INUSE;
		goto err;
	}
	if (di_state() != DI_STATE_VALID) {
		rc = DI_ERR_STATE;
		goto err;
	}
	dcr = di_read(DCR);
	if (dcr & DCR_KSHL) {
		rc = DI_ERR_HLOCK;
		goto err;
	}
	if (dcr & DCR_KSSL) {
		rc = DI_ERR_SLOCK;
		goto err;
	}
	todo_init((flags & DI_FUNC_FLAG_ASYNC) != 0);

	/* select key */
	todo_write_val(dksr, DKSR);

	todo_assign(di->key_selected, 1);

	if (flags & DI_FUNC_LOCK_FLAGS) {
		dcr = di_read(DCR);
		if (flags & DI_FUNC_FLAG_WRITE_LOCK) {
			if (flags & DI_FUNC_FLAG_HARD_LOCK)
				dcr |= DCR_KSHL;
			else
				dcr |= DCR_KSSL;
		}
		todo_write_val(dcr, DCR);
	}
	todo_start();

	if (flags & DI_FUNC_FLAG_ASYNC)
		return 0;

	rc = todo_wait_done();
err:
	di_busy_clear();
	return rc;
}
EXTERN_SYMBOL(dryice_select_key);

di_return_t dryice_check_key(di_key_t *key)
{
	uint32_t dksr;
	di_return_t rc = 0;

	if (di_busy_set())
		return DI_ERR_BUSY;

	if (key == NULL) {
		rc = DI_ERR_INVAL;
		goto err;
	}

	dksr = di_read(DKSR);

	if (di_state() != DI_STATE_VALID) {
		dksr = DKSR_IIM_KEY;
		rc = DI_ERR_STATE;
	} else if (dksr == DI_KEY_RK || dksr == DI_KEY_FRK) {
		if (!(di_read(DSR) & DSR_RKV)) {
			dksr = DKSR_IIM_KEY;
			rc = DI_ERR_UNSET;
		}
	}
	switch (dksr) {
	case DKSR_IIM_KEY:
		*key = DI_KEY_FK;
		break;
	case DKSR_PROG_KEY:
		*key = DI_KEY_PK;
		break;
	case DKSR_RAND_KEY:
		*key = DI_KEY_RK;
		break;
	case DKSR_PROG_XOR_IIM_KEY:
		*key = DI_KEY_FPK;
		break;
	case DKSR_RAND_XOR_IIM_KEY:
		*key = DI_KEY_FRK;
		break;
	}
err:
	di_busy_clear();
	return rc;
}
EXTERN_SYMBOL(dryice_check_key);

di_return_t dryice_release_key_selection(void)
{
	uint32_t dcr;
	di_return_t rc = 0;

	if (di_busy_set())
		return DI_ERR_BUSY;

	if (!di->key_selected) {
		rc = DI_ERR_UNSET;
		goto err;
	}
	dcr = di_read(DCR);
	if (dcr & DCR_KSHL) {
		rc = DI_ERR_HLOCK;
		goto err;
	}
	if (dcr & DCR_KSSL) {
		rc = DI_ERR_SLOCK;
		goto err;
	}
	di->key_selected = 0;

err:
	di_busy_clear();
	return rc;
}
EXTERN_SYMBOL(dryice_release_key_selection);

di_return_t dryice_get_tamper_event(uint32_t *events, uint32_t *timestamp,
				    int flags)
{
	di_return_t rc = 0;

	if (di_busy_set())
		return DI_ERR_BUSY;

	if (di_state() == DI_STATE_VALID) {
		rc = DI_ERR_STATE;
		goto err;
	}
	if (events == NULL) {
		rc = DI_ERR_INVAL;
		goto err;
	}
		*events = di_read(DSR) & DSR_TAMPER_BITS;
	if (timestamp) {
		if (di_state() == DI_STATE_NON_VALID)
			*timestamp = di_read(DTCMR);
		else
			*timestamp = 0;
	}
err:
	di_busy_clear();
	return rc;
}
EXTERN_SYMBOL(dryice_get_tamper_event);

di_return_t dryice_register_callback(void (*func)(di_return_t,
						  unsigned long cookie),
				     unsigned long cookie)
{
	di_return_t rc = 0;

	if (di_busy_set())
		return DI_ERR_BUSY;

	di->cb_func = func;
	di->cb_cookie = cookie;

	di_busy_clear();
	return rc;
}
EXTERN_SYMBOL(dryice_register_callback);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("DryIce");
MODULE_LICENSE("GPL");
