// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define FSMAGIC 0x10203040

//#define NDIRECT 12
//#define NINDIRECT (BSIZE / sizeof(uint))
//#define MAXFILE (NDIRECT + NINDIRECT)


#define NDIRECT 11                                      // 减少一个直接索引，增加一个二级间接索引
#define NINDIRECT (BSIZE / sizeof(uint))                //计算一个间接索引块能够存储的指针数量。
#define NBI_INDIRECT NINDIRECT * NINDIRECT              // 二级间接索引提供的块
#define MAXFILE (NDIRECT + NINDIRECT + NBI_INDIRECT)    // 计算一个文件可以包含的最大数据块数

// On-disk inode structure
// struct dinode {
//   short type;           // File type
//   short major;          // Major device number (T_DEVICE only)
//   short minor;          // Minor device number (T_DEVICE only)
//   short nlink;          // Number of links to inode in file system
//   uint size;            // Size of file (bytes)
//   uint addrs[NDIRECT+1];   // Data block addresses
// };

struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT + 2];   // Data block addresses 这里修改成了 + 2
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

