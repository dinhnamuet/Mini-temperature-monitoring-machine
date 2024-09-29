#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/kref.h>
#include <linux/errno.h>
#include <linux/wait.h>

/* Private Macro */
// #define DEBUG
#define STM32_MAX_WRQ       0x0008
/* USB Device Info */
#define STM32_VENDOR_ID     0x1979
#define STM32_PRODUCT_ID    0x2002
#define STM32_INTF_CLASS    0x000A
#define STM32_MINOR_BASE    0x0000
#define STM32_TIMEOUT       0x03E8L
/* Matching Table */
static const struct usb_device_id stm32_usb_id[] = {
    { USB_DEVICE_INTERFACE_CLASS(STM32_VENDOR_ID, STM32_PRODUCT_ID, STM32_INTF_CLASS), },
    {},
};
MODULE_DEVICE_TABLE(usb, stm32_usb_id);

/* Device Structure */
struct stm32_usb_dev {
    /* USB Device Structure */
    struct usb_device *udev;
    struct usb_interface *interface;
    struct usb_anchor urb_manager;
    struct urb *bulk_in_urb;
    /* Somethings */
    struct device *dev;
    struct mutex stm32_lock;
    struct semaphore limit_sem;
    /* EndPoint */
    __u8 endpoint_addr_in;
    __u8 endpoint_addr_out;
    /* Buffer */
    u8 *bulk_in_buf;
    size_t bulk_in_size;
    size_t bulk_out_size;
    size_t bulk_in_filled;
    size_t bulk_in_copied;
    /* Private variable */
    int errors;
    bool ongoing_read;
    spinlock_t err_lock;
    struct kref kref;
    unsigned long disconnected:1;
    /* waitqueue */
    wait_queue_head_t bulk_in_wait;
} __attribute__((packed));

/* Function Prototype */
static void usb_stop_urb(struct stm32_usb_dev *stm32);
static int stm32_probe(struct usb_interface *interface, const struct usb_device_id *id);
static void stm32_disconnect(struct usb_interface *interface);
static int stm32_suspend(struct usb_interface *interface, pm_message_t message);
static int stm32_pre_reset(struct usb_interface *interface);
static int stm32_post_reset(struct usb_interface *interface);
static int stm32_resume(struct usb_interface *interface);
static void stm32_delete(struct kref *kref);
static int stm32_open(struct inode *inodep, struct file *filep);
static int stm32_close(struct inode *inodep, struct file *filep);
static int stm32_flush(struct file *filep, fl_owner_t id);
void urb_rx_callback(struct urb *rx);
static int usb_read_io(struct stm32_usb_dev *stm32, size_t len);
static ssize_t stm32_read(struct file *filep, char __user *usr_buf, size_t size, loff_t *offset);
void urb_tx_callback(struct urb *tx);
static ssize_t stm32_write(struct file *filep, const char __user *usr_buf, size_t size, loff_t *offset);

/* Entry Point */
static const struct file_operations stm32_fops = {
    .owner      = THIS_MODULE,
    .open       = stm32_open,
    .release    = stm32_close,
    .read       = stm32_read,
    .write      = stm32_write,
    .flush      = stm32_flush,
};
/* USB Class */
static struct usb_class_driver stm32_class = {
    .name       = "stm32-%d",
    .fops       = &stm32_fops,
    .minor_base = STM32_MINOR_BASE,
};
/* USB Driver Structure */
static struct usb_driver stm32_driver = {
    .name       = "stm32_usb_dev",
    .probe      = stm32_probe,
    .disconnect = stm32_disconnect,
    .suspend    = stm32_suspend,
    .resume     = stm32_resume,
    .pre_reset  = stm32_pre_reset,
    .post_reset = stm32_post_reset,
    .id_table   = stm32_usb_id,
    .supports_autosuspend   = 1,
};
/* Implementation */
static void usb_stop_urb(struct stm32_usb_dev *stm32) {
    int time = usb_wait_anchor_empty_timeout(&stm32->urb_manager, STM32_TIMEOUT);
    if (!time) {
        usb_kill_anchored_urbs(&stm32->urb_manager);
    }
    usb_kill_urb(stm32->bulk_in_urb);
#ifdef DEBUG
    dev_info(stm32->dev, "%s called!\n", __func__);
#endif
}
static int stm32_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    int ret;
    struct stm32_usb_dev *stm32;
    struct usb_endpoint_descriptor *bulk_in, *bulk_out;
    /* Memory allocation */
    stm32 = devm_kzalloc(&interface->dev, sizeof(*stm32), GFP_KERNEL);
    if (IS_ERR(stm32)) {
        dev_err(&interface->dev, "Kzalloc failed %s, in line %d!\n", __func__, __LINE__);
        return -ENOMEM;
    }
    /* Kernel component initialization */
    kref_init(&stm32->kref);
    sema_init(&stm32->limit_sem, STM32_MAX_WRQ);
    mutex_init(&stm32->stm32_lock);
    spin_lock_init(&stm32->err_lock);
    init_usb_anchor(&stm32->urb_manager);
    init_waitqueue_head(&stm32->bulk_in_wait);
    stm32->disconnected = 0;
    /* get config */
    stm32->interface = usb_get_intf(interface);
    stm32->udev = usb_get_dev(interface_to_usbdev(interface));
    stm32->dev = &interface->dev;
    /* Get Bulk Endpoint */
    ret = usb_find_common_endpoints(interface->cur_altsetting, &bulk_in, &bulk_out, NULL, NULL);
    if (ret) {
        dev_err(stm32->dev, "%s - can't find bulk endpoint!\n", __func__);
        goto error;
    }
    /* Fill */
    stm32->bulk_out_size        = usb_endpoint_maxp(bulk_out);
    stm32->bulk_in_size         = usb_endpoint_maxp(bulk_in);
    stm32->endpoint_addr_in     = bulk_in->bEndpointAddress;
    stm32->endpoint_addr_out    = bulk_out->bEndpointAddress;
    stm32->bulk_in_buf          = devm_kzalloc(stm32->dev, stm32->bulk_in_size, GFP_KERNEL);
    if (IS_ERR(stm32->bulk_in_buf)) {
        ret = -ENOMEM;
        goto error;
    }
    /* USB Request Block Init */
    stm32->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL);
    if (IS_ERR(stm32->bulk_in_urb)) {
        dev_err(stm32->dev, "%s - Can't alloc urb!\n", __func__);
        ret = -ENOMEM;
        goto error;
    }
    /* register USB Class to kernel (Device file will be created) */
    ret = usb_register_dev(stm32->interface, &stm32_class);
    if (ret < 0) {
        dev_err(stm32->dev, "%s - Cannot create device file!\n", __func__);
        ret = -EFAULT;
        goto error;
    }
    /* Init completed */
    usb_set_intfdata(interface, stm32);
    dev_info(stm32->dev, "STM32 create device /dev/stm32-%d\n", interface->minor);
    return 0;
error:
    if (stm32) {
        kref_put(&stm32->kref, stm32_delete);
    }
    return ret;
}
static void stm32_disconnect(struct usb_interface *interface) {
    struct stm32_usb_dev *stm32 = usb_get_intfdata(interface);
    int minor = interface->minor;
    usb_deregister_dev(interface, &stm32_class);
    mutex_lock(&stm32->stm32_lock);
    stm32->disconnected = 1;
    mutex_unlock(&stm32->stm32_lock);
    usb_kill_urb(stm32->bulk_in_urb);
    usb_kill_anchored_urbs(&stm32->urb_manager);
    kref_put(&stm32->kref, stm32_delete);
    dev_info(stm32->dev, "STM32 stop device /dev/stm32-%d!\n", minor);
}
static int stm32_suspend(struct usb_interface *interface, pm_message_t message) {
    struct stm32_usb_dev *stm32 = usb_get_intfdata(interface);
    if (IS_ERR(stm32)) {
        pr_err("%s - cannot find device!\n", __func__);
        return -ENODEV;
    }
    usb_stop_urb(stm32);
#ifdef DEBUG
    dev_info(stm32->dev, "%s called!\n", __func__);
#endif
    return 0;
}
static int stm32_pre_reset(struct usb_interface *interface) {
    struct stm32_usb_dev *stm32 = usb_get_intfdata(interface);
    if (IS_ERR(stm32)) {
        pr_err("%s - cannot find device!\n", __func__);
        return -ENODEV;
    }
    mutex_lock(&stm32->stm32_lock);
    usb_stop_urb(stm32);
#ifdef DEBUG
    dev_info(stm32->dev, "%s called!\n", __func__);
#endif
    return 0;
}
static int stm32_post_reset(struct usb_interface *interface) {
    struct stm32_usb_dev *stm32 = usb_get_intfdata(interface);
    if (IS_ERR(stm32)) {
        pr_err("%s - cannot find device!\n", __func__);
        return -ENODEV;
    }
    stm32->errors = -EPIPE;
    mutex_unlock(&stm32->stm32_lock);
#ifdef DEBUG
    dev_info(stm32->dev, "%s called!\n", __func__);
#endif
    return 0;
}
static int stm32_resume(struct usb_interface *interface) {
    struct stm32_usb_dev *stm32 = usb_get_intfdata(interface);
    if (IS_ERR(stm32)) {
        pr_err("%s - cannot find device!\n", __func__);
        return -ENODEV;
    }
#ifdef DEBUG
    dev_info(stm32->dev, "%s called!\n", __func__);
#endif
    return 0;
}
static void stm32_delete(struct kref *kref) {
    struct stm32_usb_dev *stm32 = (struct stm32_usb_dev *)container_of(kref, struct stm32_usb_dev, kref);
    if (IS_ERR(stm32)) {
        pr_err("%s - cannot find device!\n", __func__);
        return;
    } else {
        usb_free_urb(stm32->bulk_in_urb);
        usb_put_intf(stm32->interface);
        usb_put_dev(stm32->udev);
    }
#ifdef DEBUG
    dev_info(stm32->dev, "%s called!\n", __func__);
#endif
}
static int stm32_open(struct inode *inodep, struct file *filep) {
    int ret;
    struct stm32_usb_dev *stm32;
    struct usb_interface *interface = usb_find_interface(&stm32_driver, MINOR(inodep->i_rdev));
    if (IS_ERR(interface)) {
        pr_err("%s - error, can't find device for minor %d\n",
            __func__, MINOR(inodep->i_rdev));
        ret = -ENODEV;
        goto err_handle;
    }
    /* Get Driver's Private Data */
    stm32 = usb_get_intfdata(interface);
    if (IS_ERR(stm32)) {
        pr_err("%s - error, can't find device!\n", __func__);
        ret = -ENODEV;
        goto err_handle;
    }
    ret = usb_autopm_get_interface(interface);
    if (ret)
        goto err_handle;
    /* increament usage count */
    kref_get(&stm32->kref);
#ifdef DEBUG
    dev_info(stm32->dev, "%s called!\n", __func__);
#endif
    /* Attach Device Structure to File's Private Data */
    filep->private_data = stm32;

err_handle:
    return ret;
}
static int stm32_close(struct inode *inodep, struct file *filep) {
    struct stm32_usb_dev *stm32 = (struct stm32_usb_dev *)filep->private_data;
    if (IS_ERR(stm32)) {
        pr_err("%s - can't find device!\n", __func__);
        return -ENODEV;
    }
    if (stm32->interface) {
        usb_autopm_put_interface(stm32->interface);
    }
    /* Decrement usage count */
    kref_put(&stm32->kref, stm32_delete);
#ifdef DEBUG
    dev_info(stm32->dev, "%s called!\n", __func__);
#endif
    filep->private_data = NULL;
    return 0;
}
static int stm32_flush(struct file *filep, fl_owner_t id) {
    int ret;
    struct stm32_usb_dev *stm32 = (struct stm32_usb_dev *)filep->private_data;
    if (IS_ERR(stm32)) {
        pr_err("%s - can't find device!\n", __func__);
        return -ENODEV;
    }
    mutex_lock(&stm32->stm32_lock);
    /* Wait for io to stop */
    usb_stop_urb(stm32);
    /* read out errors */
    
    spin_lock_irq(&stm32->err_lock);
    ret = (stm32->errors) ? ((stm32->errors == -EPIPE ) ? -EPIPE : -EIO) : 0;
    stm32->errors = 0;
    spin_unlock_irq(&stm32->err_lock);

    mutex_unlock(&stm32->stm32_lock);
#ifdef DEBUG
    dev_info(stm32->dev, "%s called!\n", __func__);
#endif
    return ret;
}
void urb_rx_callback(struct urb *rx) {
    unsigned long flags;
    struct stm32_usb_dev *stm32 = (struct stm32_usb_dev *)rx->context;
    if (IS_ERR(stm32)) {
        pr_err("%s - can't find device!\n", __func__);
        return;
    }
    /* Store Value and status */
    spin_lock_irqsave(&stm32->err_lock, flags);
    /* urb status return 0 if success or return negative error code */
    if (rx->status) {
        if (!(rx->status == -ENOENT    ||
                rx->status == -ECONNRESET  ||
                rx->status == -ESHUTDOWN))
            dev_err(stm32->dev,
                "%s - nonzero write bulk status received: %d\n",
                __func__, rx->status);
        stm32->errors = rx->status;
    } else {
        stm32->bulk_in_filled = rx->actual_length;
    }
    stm32->ongoing_read = 0;
    spin_unlock_irqrestore(&stm32->err_lock, flags);
    /* Waitqueue wake up */
    wake_up_interruptible(&stm32->bulk_in_wait);
#ifdef DEBUG
    dev_info(stm32->dev, "%s called!\n", __func__);
#endif
}
static int usb_read_io(struct stm32_usb_dev *stm32, size_t len) {
    int ret;
    usb_fill_bulk_urb(stm32->bulk_in_urb, stm32->udev, 
        usb_rcvbulkpipe(stm32->udev, stm32->endpoint_addr_in), stm32->bulk_in_buf,
        min_t(size_t, stm32->bulk_in_size, len), urb_rx_callback, stm32);
    spin_lock_irq(&stm32->err_lock);
    stm32->ongoing_read = 1;
    spin_unlock_irq(&stm32->err_lock);
    stm32->bulk_in_filled = 0;
    stm32->bulk_in_copied = 0;
    ret = usb_submit_urb(stm32->bulk_in_urb, GFP_KERNEL);
    if (ret < 0) {
        dev_err(stm32->dev,
            "%s - failed submitting read urb, error %d\n",
            __func__, ret);
        ret = (ret == -ENOMEM) ? ret : -EIO;
        spin_lock_irq(&stm32->err_lock);
        stm32->ongoing_read = 0;
        spin_unlock_irq(&stm32->err_lock);
    }
#ifdef DEBUG
    dev_info(stm32->dev, "%s called!\n", __func__);
#endif
    return ret;
}
static ssize_t stm32_read(struct file *filep, char __user *usr_buf, size_t size, loff_t *offset) {
    int ret, busy;
    struct stm32_usb_dev *stm32 = (struct stm32_usb_dev *)filep->private_data;
    if (IS_ERR(stm32)) {
        pr_err("%s - can't find device!\n", __func__);
        return -ENODEV;
    }
    if (IS_ERR(stm32->bulk_in_urb) || (!size)) {
        dev_err(stm32->dev, "%s - can't read!\n", __func__);
        return 0;
    }
    ret = mutex_lock_interruptible(&stm32->stm32_lock);
    if (ret < 0) {
        return ret;
    }
    if (IS_ERR(stm32->interface)) {
        ret = -ENODEV;
        goto exit;
    }
    if (stm32->disconnected) {
        ret = -ENODEV;
        goto exit;
    }
retry:
    spin_lock_irq(&stm32->err_lock);
    busy = stm32->ongoing_read;
    spin_unlock_irq(&stm32->err_lock);
    if (busy) {
        if (filep->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto exit;
        }
        ret = wait_event_interruptible_timeout(stm32->bulk_in_wait, (!stm32->ongoing_read), (STM32_TIMEOUT * HZ/MSEC_PER_SEC));
        if (ret <= 0)
            goto exit;
    }
    ret = stm32->errors;
    if (ret < 0) {
        stm32->errors = 0;
        ret = (ret == -EPIPE) ? ret : -EIO;
        goto exit;
    }
    if (stm32->bulk_in_filled) {
        size_t available = stm32->bulk_in_filled - stm32->bulk_in_copied;
        size_t chunk = min_t(size_t, available, size);
#if defined(DEBUG)
        dev_info(stm32->dev, "%s - Size: %ld, available: %ld, chunk: %ld\n", __func__, size, available, chunk);
#endif
        if (!available) {
            ret = usb_read_io(stm32, size);
            if (ret < 0)
                goto exit;
            else
                goto retry;
        }
        if (copy_to_user(usr_buf, stm32->bulk_in_buf + stm32->bulk_in_copied, chunk)) {
            dev_err(stm32->dev, "%s - Cannot copy to user buf!\n", __func__);
            ret = -EFAULT;
        } else {
            ret = chunk;
        }

        stm32->bulk_in_copied += chunk;
        if (available < size)
            usb_read_io(stm32, size - chunk);
    } else {
        ret = usb_read_io(stm32, size);
        if (ret < 0)
            goto exit;
        else
            goto retry;
    }
exit:
    mutex_unlock(&stm32->stm32_lock);
#ifdef DEBUG
    dev_info(stm32->dev, "%s called!\n", __func__);
#endif
    return ret;
}
void urb_tx_callback(struct urb *tx) {
    unsigned long flags;
    struct stm32_usb_dev *stm32 = (struct stm32_usb_dev *)tx->context;
    if (IS_ERR(stm32)) {
        pr_err("%s - can't find device!\n", __func__);
        return;
    }
    /* urb status return 0 if success or return negative error code */
    if (tx->status) {
        if (!(tx->status == -ENOENT    ||
                tx->status == -ECONNRESET  ||
                tx->status == -ESHUTDOWN))
            dev_err(stm32->dev,
                "%s - nonzero write bulk status received: %d\n",
                __func__, tx->status);
        spin_lock_irqsave(&stm32->err_lock, flags);
        stm32->errors = tx->status;
        spin_unlock_irqrestore(&stm32->err_lock, flags);
    }
    usb_free_coherent(tx->dev, tx->transfer_buffer_length, 
                        tx->transfer_buffer, tx->transfer_dma);
    up(&stm32->limit_sem);
#ifdef DEBUG
    dev_info(stm32->dev, "%s called!\n", __func__);
#endif
}
static ssize_t stm32_write(struct file *filep, const char __user *usr_buf, size_t size, loff_t *offset) {
    int ret = 0;
    struct urb *urb = NULL;
    u8 *buf = NULL;
    size_t writesize;
    struct stm32_usb_dev *stm32 = (struct stm32_usb_dev *)filep->private_data;
    if (IS_ERR(stm32)) {
        pr_err("%s - can't find device!\n", __func__);
        return -ENODEV;
    }
    if (!size)
        goto exit;
    writesize = min_t(size_t, size, stm32->bulk_out_size);
    if (!(filep->f_flags & O_NONBLOCK)) {
        if (down_interruptible(&stm32->limit_sem)) {
            ret = -ERESTARTSYS;
            goto exit;
        }
    } else {
        if (down_trylock(&stm32->limit_sem)) {
            ret = -EAGAIN;
            goto exit;
        }
    }
    spin_lock_irq(&stm32->err_lock);
    ret = stm32->errors;
    if (ret < 0) {
        stm32->errors = 0;
        ret = (ret == -EPIPE) ? ret : -EIO;
    }
    spin_unlock_irq(&stm32->err_lock);
    if (ret < 0) {
        goto error;
    }
    urb = usb_alloc_urb(0, GFP_KERNEL);
    if (IS_ERR(urb)) {
        ret = -ENOMEM;
        goto error;
    }
    buf = usb_alloc_coherent(stm32->udev, writesize, GFP_KERNEL, &urb->transfer_dma);
    if (IS_ERR(buf)) {
        ret = -ENOMEM;
        goto error;
    }
    if (copy_from_user(buf, usr_buf, writesize)) {
        ret = -EFAULT;
        goto error;
    }
    mutex_lock(&stm32->stm32_lock);
    if (IS_ERR(stm32->interface)) {
        mutex_unlock(&stm32->stm32_lock);
        ret = -ENODEV;
        goto error;
    }
    if (stm32->disconnected) {
        mutex_unlock(&stm32->stm32_lock);
        ret = -ENODEV;
        goto error;
    }
    usb_fill_bulk_urb(urb, stm32->udev, 
        usb_sndbulkpipe(stm32->udev, stm32->endpoint_addr_out), buf, 
        writesize, urb_tx_callback, stm32);
    urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
    usb_anchor_urb(urb, &stm32->urb_manager);
    ret = usb_submit_urb(urb, GFP_KERNEL);
    mutex_unlock(&stm32->stm32_lock);
    if (ret) {
        dev_err(stm32->dev,
            "%s - failed submitting write urb, error %d\n",
            __func__, ret);
        goto error_unanchor;
    }
    usb_free_urb(urb);
#ifdef DEBUG
    dev_info(stm32->dev, "%s called!\n", __func__);
#endif
    return writesize;
error_unanchor:
    usb_unanchor_urb(urb);
error:
    if (urb) {
        usb_free_coherent(stm32->udev, writesize, buf, urb->transfer_dma);
        usb_free_urb(urb);
    }
    up(&stm32->limit_sem);
exit:
    return ret;
}

static int __init stm32_init(void) {
    return usb_register(&stm32_driver);
}
module_init(stm32_init);

static void __exit stm32_exit(void) {
    usb_deregister(&stm32_driver);
}
module_exit(stm32_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("USB Driver to communicate STM32");
MODULE_AUTHOR("DinhNam <dinhnamuet@gmail.com>");
MODULE_VERSION("1.0.1");
