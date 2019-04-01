Driver for PSTN and others
#############################################		
Li Zhijie 2005.02.04


* slic: v2 PSTN, SLIC�豸��������������IXP42X��PC��
* pcm: v1 PSTN; PCM����������ֻ����IXP42X
* zarlink: ����ȡ��оƬ��������ֻ����IXP42X
* misc: �������Ϳ��Ź���������ֻ����IXP42X;


PSTN of PCM driver
===========================

fxs module/layer
-------------------

* Initial PCI device and FXS hardware;
   * request memory region;
   * Allocate memory zone for read and write; Also dma_addr_t for the buffer;
   * request IRQ;
   * hardware detect and init;
   * start DMA;
   * enable interrupt;
* Interrupt from hardware to 
   * Read data from DMA buffer and put into buffer of every channel;
   * Write data into DMA buffer from the buffer of every channel;
   
span module/layer
-------------------

Work queue
++++++++++++++++++

* 3 wait_queue_head_t: read, write and selet;
* wake up work queue: wake_up_interruptible(&wait_queue_head_t);
* put thread into work queue: 
   * DECLARE_WAITQUEUE(wait, current): push current into new wait_queue_head_t;
   * add_wait_queue(&wait_queue_head_t, &wait_queue_head_t); remove_wait_queue(&queue, &wait)
   * current->state = TASK_INTERRUPTIBLE  | TASK_RUNNING; 
   * schedule(): system reschedule again;

Only spin_lock is used;

* ISR and thread mutex with spin_lock_irqsave()/spin_unlock_irqrestore();
* ISR wake thread with wake_up_interruptible();
* Thread block itself into wait_queue_head_t;
* Data access is mutexed;


* No signal and control operations are needed for PCM only driver;
   
   