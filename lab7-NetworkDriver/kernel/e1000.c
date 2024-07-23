#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++)
  {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64)tx_ring;
  if (sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;

  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++)
  {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64)rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64)rx_ring;
  if (sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA + 1] = 0x5634 | (1 << 31);
  // multicast table
  for (int i = 0; i < 4096 / 32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |                 // enable
                     E1000_TCTL_PSP |                // pad short packets
                     (0x10 << E1000_TCTL_CT_SHIFT) | // collision stuff
                     (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8 << 10) | (6 << 20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN |      // enable receiver
                     E1000_RCTL_BAM |     // enable broadcast
                     E1000_RCTL_SZ_2048 | // 2048-byte rx buffers
                     E1000_RCTL_SECRC;    // strip CRC

  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0;       // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0;       // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

// xv6依靠network stack（网络栈）实现收发数据，即通过network stack收发packet。

/*
当network stackk需要发送一个packet的时候，会先将这个packet存放到发送环形缓冲区tx_ring，
最后通过网卡将这个packet发送出去。
每次发送packet前都需要检查一下上一次的packet发送完没，如果发送完了，要将其的释放掉
*/
int e1000_transmit(struct mbuf *m)
{
  acquire(&e1000_lock);

  // regs[E1000_TDT] 是发送描述符环的尾指针寄存器，指示当前描述符的位置。
  // tx_index 变量保存了当前描述符的索引。
  //#define E1000_TDT (0x03818/4)  /* TX Descripotr Tail - RW */
  uint32 tx_index = regs[E1000_TDT];
  /*
  E1000_TXD_STAT_DD 宏定义用于表示发送描述符的“描述符完成”状态。
  当以太网控制器将这个状态位设置为 1 时，表示数据包已经成功发送。
  */
  if ((tx_ring[tx_index].status & E1000_TXD_STAT_DD) == 0)
  {
    release(&e1000_lock);
    return -1;
  }
  // 释放前一个mbuf
  if (tx_mbufs[tx_index])
  {
    mbuffree(tx_mbufs[tx_index]);
  }
  // 发送mbuf
  /*
  新的 mbuf 存储在 tx_mbufs 数组的当前索引位置。
  设置发送描述符的地址字段为 mbuf 的数据缓冲区地址。
  设置发送描述符的长度字段为 mbuf 的长度。
  设置发送描述符的命令字段，包含 EOP（End of Packet）和 RS（Report Status）标志，
  指示这是一个数据包的结束，并要求网卡在处理完成后报告状态。

  E1000_TXD_CMD_EOP当设置这个标志时，它指示这个描述符包含一个数据包的结束部分。
  这对于分片数据包（分成多个描述符进行发送）尤为重要。

  E1000_TXD_CMD_RS当设置这个标志时，它指示网卡在完成对这个描述符的处理后，
  更新描述符的状态字段，并设置 "Descriptor Done"（DD）标志。
  */
  tx_mbufs[tx_index] = m;
  tx_ring[tx_index].addr = (uint64)m->head;
  tx_ring[tx_index].length = m->len;
  tx_ring[tx_index].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  // 设置mbuf的下一个索引
  regs[E1000_TDT] = (tx_index + 1) % TX_RING_SIZE;

  release(&e1000_lock);

  return 0;
}

/*
当网卡需要接收packet的时候，网卡会直接访问内存（DMA），
先将接受到的RAM的数据（即packet的内容）写入到接收环形缓冲区rx_ring中。
接着，网卡会向cpu发出一个硬件中断，当cpu接受到硬件中断后，
cpu就可以从接收环形缓冲区rx_ring中读取packet传递到network stack中了（net_rx()）。
网卡会一次性接收全部的packets，即接收到rx_ring溢出为止
*/
static void
e1000_recv(void)
{
  while (1)
  {
    //获取下一个索引
    //E1000_RDT 寄存器指向接收描述符环中的尾部描述符，
    //#define E1000_RDT (0x02818/4)  /* RX Descriptor Tail - RW */
    //它标识了驱动程序已经处理完毕的最后一个描述符。
    uint32 rx_index = (regs[E1000_RDT] + 1) % RX_RING_SIZE;

    if ((rx_ring[rx_index].status & E1000_RXD_STAT_DD) == 0)
      return;

    //传递到网络堆栈
    rx_mbufs[rx_index]->len = rx_ring[rx_index].length;
    net_rx(rx_mbufs[rx_index]);

    //分配一个新的 mbuf 并填充一个新的描述符
    rx_mbufs[rx_index] = mbufalloc(0);
    rx_ring[rx_index].addr = (uint64)rx_mbufs[rx_index]->head;
    rx_ring[rx_index].status = 0;

    // 作为当前索引
    regs[E1000_RDT] = rx_index;
  }
}

void e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
