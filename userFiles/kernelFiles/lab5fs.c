#include <linux/module.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include "lab5fs.h"
#include <linux/namei.h>

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/writeback.h>
#include <linux/blkdev.h>
#include <linux/backing-dev.h>

#define ROOT_INO 0
static struct super_operations lab5fs_ops;
static struct file_operations lab5fs_file_operations;
static struct file_operations lab5fs_dir_operations;
static struct inode_operations lab5fs_inode_operations;
static struct address_space_operations lab5fs_aspace_operations;
static struct block_device *g_bdev;
static lab5fs_sb* l5sb;
static struct super_block *g_sb;

/* Define error conditions */
typedef enum _filesystem_errors
{
  FILE_SYSTEM_OUT_OF_RANGE_ERROR = -EIO,
  FILE_SYSTEM_BLOCKSIZE_ERROR = -EIO,
  FILE_SYSTEM_OUT_OF_SPACE = -ENOSPC,
  FILE_SYSTEM_NO_ENTRY_ERROR = -ENOENT
} FILESYSTEM_ERROR;


int get_ori_inode_number(int ino)
{
  unsigned long block_num = ino / 8 + 33;
  int offset = (ino % 8) * 64;
  struct buffer_head *bh = NULL;
  bh = __bread(g_bdev, block_num, l5sb->blocksize);
  struct lab5fs_ino *tmp_ino = (lab5fs_ino*) (bh->b_data + offset);
  brelse(bh);
  if (tmp_ino->is_hard_link == 1){
    return get_ori_inode_number(tmp_ino->block_to_link_to);
  }
  return ino;
}

int find_index_given_name(char * name)
{

  struct buffer_head *bh2 = NULL;
  bh2 = __bread(g_bdev, 1, 2*l5sb->blocksize);
  char *map = NULL;
  map = (char *) bh2->b_data;
  unsigned long block_num = NULL;
  int offset = NULL;
  struct buffer_head *bh = NULL;
  struct lab5fs_ino *tmp_ino = NULL;
  int i = 0;

  for (i = 1; i < 2*l5sb->blocksize; i++) {
    char is_inode_used = map[i];

    if (is_inode_used) {

      block_num = i / 8 + 33;
      offset = (i % 8) * 64;
      bh = __bread(g_bdev, block_num, l5sb->blocksize);
      tmp_ino = (lab5fs_ino*) (bh->b_data + offset);
      if (strcmp(name, tmp_ino->name) == 0){
          return i;
      }
      brelse(bh);
   }
  }
}


int check_other_link_to_this_file(int ino)
{

  struct buffer_head *bh2 = NULL;
  bh2 = __bread(g_bdev, 1, 2*l5sb->blocksize);
  char *map = NULL;
  map = (char *) bh2->b_data;
  unsigned long block_num = NULL;
  int offset = NULL;
  struct buffer_head *bh = NULL;
  struct lab5fs_ino *tmp_ino = NULL;
  int i = 0;

  for (i = 1; i < 2*l5sb->blocksize; i++) {
    char is_inode_used = map[i];

    if (is_inode_used) {

      block_num = i / 8 + 33;
      offset = (i % 8) * 64;
      bh = __bread(g_bdev, block_num, l5sb->blocksize);
      tmp_ino = (lab5fs_ino*) (bh->b_data + offset);
      if (tmp_ino->block_to_link_to == ino){
          return i;
      }
      brelse(bh);
   }
  }

  return -1;
}

/* Superblock operations */
void lab5fs_read_inode(struct inode *inode)
{
  printk(KERN_INFO "********Inside lab5fs_read_inode********\n");
  ino_t ino = inode->i_ino;
  lab5fs_ino *inode_temp = NULL;
  struct buffer_head *bh = NULL;

  unsigned long block_addr = ino / 8 + 33;
  int offset = (ino % 8) * 64;

  if(!inode)
     printk(KERN_INFO "inode is null\n");
  printk(KERN_INFO "Read inode num -> blk = %lu\n", block_addr);
  printk(KERN_INFO "blocksize = %lu\n", l5sb->blocksize);

  /* Read at given block address */
  if (!g_bdev) { printk(KERN_INFO "BLOCK DEV IS NULL!\n"); }
  bh = __bread(g_bdev, block_addr, l5sb->blocksize);
  // bh = sb_bread(g_sb, block_addr);

  if (!bh) { printk(KERN_INFO "BUFFER HEAD IS NULL!\n"); }

  inode_temp = (lab5fs_ino *) (bh->b_data + offset);

  
  if (!inode_temp) { printk(KERN_INFO "INODE TEMP IS NULL!\n"); }
  printk(KERN_INFO "Filling out inode info...\n");
  inode->i_mode = inode_temp->i_mode;
  inode->i_uid = inode_temp->i_uid;
  inode->i_gid = inode_temp->i_gid;
  inode->i_atime = inode_temp->i_atime; 
  inode->i_mtime = inode_temp->i_mtime;
  inode->i_ctime = inode_temp->i_ctime;
  printk(KERN_INFO "Inode temp has %d blocks\n", inode_temp->blocks);
  inode->i_blocks = inode_temp->blocks;
  inode->i_size = inode_temp->size;
  inode->i_blksize = inode->i_sb->s_blocksize;
  inode->i_nlink = 1;  // temp
  inode->i_fop = &lab5fs_file_operations;
  inode->i_op = &lab5fs_inode_operations;
  inode->i_mapping->a_ops = &lab5fs_aspace_operations;

  

  if (ino == ROOT_INO) {
    printk(KERN_INFO "lab5fs reading root\n");
    inode->i_mode |= S_IFDIR;
    inode->i_nlink = 2;
    inode->i_fop = &lab5fs_dir_operations;
    inode->i_size = l5sb->blocksize;
    inode->i_blocks = 1;
    inode->i_bytes = l5sb->blocksize;
  }
  brelse(bh);

  long i_sblock =  158 + (ino * 8000) / l5sb->blocksize;  
  long i_eblock =  158 + ((ino +1) * 8000) / l5sb->blocksize;  

  struct buffer_head *bh2 = __bread(g_bdev, i_sblock, l5sb->blocksize);
  // struct buffer_head *bh2 = sb_bread(g_sb, i_sblock);
  mark_buffer_dirty(bh2);
  brelse(bh2);

  struct buffer_head *bh3 = __bread(g_bdev, i_sblock + 1, l5sb->blocksize);
  // struct buffer_head *bh3 = sb_bread(g_sb, i_sblock + 1);
  mark_buffer_dirty(bh3);
  brelse(bh3);

} /* read_inode */

int lab5fs_write_inode(struct inode *inode, int unused)
{
  printk(KERN_INFO "********Inside lab5fs_write_inode********\n");
  // Read inode from disk and update
  printk(KERN_INFO "PRE: Writing inode\n");
  int ino = inode->i_ino;
  struct buffer_head *bh;

  int block_addr = ino / 8 + 33;
  int offset = (ino % 8) * 64;

  // bh = __bread(g_bdev, block_addr, l5sb->blocksize);
  bh = sb_bread(g_sb, block_addr);
  lab5fs_ino *ondiskino = (lab5fs_ino *) (bh->b_data + offset);


  ondiskino->i_uid = inode->i_uid;
  ondiskino->i_gid = inode->i_gid;
  ondiskino->i_mode = inode->i_mode;
  ondiskino->blocks = inode->i_blocks;
  ondiskino->size = inode->i_size;
  ondiskino->i_atime = inode->i_atime;
  ondiskino->i_ctime = inode->i_ctime;
  ondiskino->i_mtime = inode->i_mtime;

  
  mark_buffer_dirty_inode(bh, inode);
  // printk(KERN_INFO "Done marking as dirty\n");

  brelse(bh);


  long i_sblock =  158 + (ino * 8000) / l5sb->blocksize;  
  long i_eblock =  158 + ((ino +1) * 8000) / l5sb->blocksize;  

  // struct buffer_head *bh2 = __bread(g_bdev, i_sblock, l5sb->blocksize);
  struct buffer_head *bh2 = sb_bread(g_sb, i_sblock);
  mark_buffer_dirty(bh2);
  brelse(bh2);

  // struct buffer_head *bh3 = __bread(g_bdev, i_sblock + 1, l5sb->blocksize);
  struct buffer_head *bh3 = sb_bread(g_sb, i_sblock + 1);
  mark_buffer_dirty(bh3);
  brelse(bh3);

  return 0;
} /* write_inode */

/* Superblock functions */
static void lab5fs_put_super(struct super_block *sb)
{
  printk(KERN_INFO "********Inside put_super*********\n");
  //kfree(l5sb);
}

int lab5fs_fill_sb(struct super_block *sb,
                   void *data,
                   int silent)
{
  printk(KERN_INFO "********Inside lab5fs_fill_sb********\n");
  struct inode *root;

  /* Read in superblock from device */
  struct buffer_head *bh = sb_bread(sb, 0);
  l5sb = (lab5fs_sb*) bh->b_data;
  //memcpy(l5sb, bh->b_data, sizeof(lab5fs_sb));

  /* Set global block device */
  g_bdev = sb->s_bdev;

  if (!sb_set_blocksize(sb, l5sb->blocksize)){
    printk(KERN_INFO "Cannot set device to size\n");
    return FILE_SYSTEM_BLOCKSIZE_ERROR; 
  }

  if (l5sb) {
    printk(KERN_INFO "Read lab5fs superblock...\n");
    printk(KERN_INFO "Magic number of device: 0x%lX\n", l5sb->magic_num);
    printk(KERN_INFO "Blocksize of device: %lu\n", l5sb->blocksize);
  }

  /* Set sizes, etc. */
  sb->s_op = &lab5fs_ops;
  sb->s_blocksize = l5sb->blocksize;
  sb->s_blocksize_bits = l5sb->blocksize_bits;
  sb->s_maxbytes = l5sb->max_bytes;
  sb->s_magic = l5sb->magic_num;

  /* Get root / */
  root = iget(sb, ROOT_INO);
  sb->s_root = d_alloc_root(root);
  if (!sb->s_root) {
    printk(KERN_INFO "lab5fs could not allocate root\n");
    return -EINVAL;
  }

  brelse(bh);
  return 0;
}

struct super_block *lab5fs_get_sb(struct file_system_type *fs_type,
                                  int flags,
                                  const char *dev_name,
                                  void *data)
{
  printk(KERN_INFO "********Inside lab5fs_get_sb********\n");
  struct super_block *sb = NULL;
  sb = get_sb_bdev(fs_type, flags, dev_name, data, lab5fs_fill_sb);
  g_sb = sb;
  return sb;
}

/* Superblock operations struct */
static struct super_operations lab5fs_ops = {
  .read_inode = lab5fs_read_inode,
  .write_inode = lab5fs_write_inode,
  .put_super = lab5fs_put_super
};

/* Directory operations */
int lab5fs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
  printk(KERN_INFO "********Inside lab5fs_readdir********\n");
  // printk(KERN_INFO "READING FROM %d\n", filp->f_pos);
  int retval = 0, i, inode_index = 0;
  char *map = NULL;
  struct inode *in;
  struct dentry *de = filp->f_dentry;
  struct inode *dir = filp->f_dentry->d_inode;
  struct buffer_head *bh = NULL;
  lab5fs_ino *l5inode = NULL;

  /* Check bounds of directory */
  if (filp->f_pos > (l5sb->inode_blocks_total - l5sb->inode_blocks_free)) {
    // printk(KERN_INFO "At end of directory\n");
    return 0;
  }

  /* Read . or .. from directory entry */
  if (filp->f_pos == 0) {
    /* . dir */
    // printk(KERN_INFO "Getting .\n");
    retval = filldir(dirent, ".", 1, filp->f_pos, 19, DT_DIR);//dir->i_ino, DT_DIR);
    // printk(KERN_INFO "Inode num %d...\n", dir->i_ino);
    //if (retval < 0) goto done;
    filp->f_pos++;
    goto done;
  } 
  if (filp->f_pos == 1) {
    /* .. dir */
    // printk(KERN_INFO "Getting ..\n");
    retval = filldir(dirent, "..", 2, filp->f_pos,
                     20, DT_DIR);//de->d_parent->d_inode->i_ino, DT_DIR);
    // printk(KERN_INFO "Inode num %d...\n", de->d_parent->d_inode->i_ino);
    //if (retval < 0) goto done;
    filp->f_pos++;
    goto done;
  }

  /* Loop through other files */
  bh = __bread(g_bdev, 1, 2*l5sb->blocksize);
  map = (char *) bh->b_data;

  for (i = 1; i < 2*l5sb->blocksize; i++) {
    char is_inode_used = map[i];
    // printk(KERN_INFO "i = %d, is_inode_used = %c\n", (i,is_inode_used));

    if (is_inode_used) {
      inode_index++;
      printk(KERN_INFO "found i = %d\n", i);
      printk(KERN_INFO "inode_index (used) = %d\n", inode_index);

      if (inode_index == filp->f_pos - 1) {
        // printk(KERN_INFO "found i = %d\n", i);
        break;
      }
    } else {
      // printk(KERN_INFO "inode_index not used @ %d\n", inode_index);
    }
  }

  brelse(bh);

  unsigned long block_num = i / 8 + 33;
  int offset = (i % 8) * 64;
  // printk(KERN_INFO "Reading from block %d\n", block_num);
  // printk(KERN_INFO "Reading from block offset %d\n", offset);

  bh = __bread(g_bdev, block_num, l5sb->blocksize);
  l5inode = (lab5fs_ino *) (bh->b_data + offset);

  in = (struct inode *) kmalloc(sizeof(struct inode), GFP_KERNEL);
  in->i_ino = filp->f_pos + 2; 
  
  in->i_mode = l5inode->i_mode;
  in->i_uid = l5inode->i_uid;
  in->i_gid = l5inode->i_gid;
  in->i_atime = l5inode->i_atime; 
  in->i_mtime = l5inode->i_mtime;
  in->i_ctime = l5inode->i_ctime;
  // printk(KERN_INFO "CTIME: %lu\n", l5inode->i_ctime);
  in->i_blocks = l5inode->blocks;
  in->i_size = l5inode->size;
  in->i_blksize = l5sb->blocksize;
  in->i_nlink = 1;  // temp
  in->i_op = &lab5fs_inode_operations;
  in->i_fop = &lab5fs_dir_operations;
  //in->i_mapping->a_ops = &lab5fs_aspace_operations;
  brelse(bh);

  // printk(KERN_INFO "About to do filldir\n");
  retval = filldir(dirent, l5inode->name, 21, filp->f_pos, in->i_ino, DT_REG);
  // printk(KERN_INFO "Updating filepos\n");
  filp->f_pos++;

 done:
  /* Update access time */
  filp->f_version = dir->i_version;
  update_atime(dir);
  // printk(KERN_INFO "Going to return %d...\n", retval);
  return 0;
}

int lab5fs_fsync(struct file *file, struct dentry *de, int datasync)
{
  printk(KERN_INFO "************* fsync **************\n");
  struct inode * inode = de->d_inode;
  struct super_block * sb;
  int ret;

  simple_sync_file(file, de, datasync);

  struct writeback_control wbc = {
    .nr_to_write = LONG_MAX,
    .sync_mode = WB_SYNC_ALL,
  };

  sync_inode(inode, &wbc);

  /* sync the inode to buffers */
  write_inode_now(inode, 1);

  mark_inode_dirty(inode);

  /* sync the superblock to buffers */
  sb = inode->i_sb;
  lock_super(sb);
  if (sb->s_op->write_super)
    sb->s_op->write_super(sb);
  unlock_super(sb);

  /* sync the buffers to disk */
  ret = sync_blockdev(sb->s_bdev);
  return ret;

  
}

static int lab5fs_link(struct dentry *old, struct inode *dir, struct dentry *new)
{
  printk(KERN_INFO "********* lab5fs_link ***********\n");

  /* Get old inode */
  struct inode *inode = old->d_inode;
  int ino = inode->i_ino;
  lab5fs_sb *sb = NULL;
  char *name = new->d_name.name;
  int name_len = new->d_name.len;
  struct inode *new_ino = NULL;

  /* Find free bit for new inode */
  // printk(KERN_INFO "Finding bit to assign inum...\n");
  struct buffer_head *bh = __bread(g_bdev, 1, 2*l5sb->blocksize);
  char *map = (char*) bh->b_data;
  int ino_num = find_first_free_index(map);

  if (ino_num <= 0) {
    // printk(KERN_INFO "Couldn't get a free bit\n");
    return -ENOSPC;
  }


  map[ino_num] = 1;
  mark_buffer_dirty(bh);
  brelse(bh);


  unsigned long block_num = ino_num / 8 + 33;


  /* Create a new inode */
  printk(KERN_INFO "Creating new inode, ino_num = %d link to %d ...\n", ino_num, ino);
  // bh = __bread(g_bdev, block_num, l5sb->blocksize);
  bh = sb_bread(g_sb, block_num);
  struct lab5fs_ino *new_ino_lab = (lab5fs_ino*) (bh->b_data + offset);
  new_ino_lab->i_uid = inode->i_uid;
  new_ino_lab->i_gid = inode->i_gid;
  new_ino_lab->i_mode = inode->i_mode;
  new_ino_lab->blocks = inode->i_blocks;
  new_ino_lab->size = inode->i_size;
  new_ino_lab->i_atime = CURRENT_TIME;
  new_ino_lab->i_mtime = CURRENT_TIME;
  new_ino_lab->i_ctime = CURRENT_TIME;
  strcpy(new_ino_lab->name, name);
  new_ino_lab->is_hard_link = 1;
  new_ino_lab->block_to_link_to = ino;
  // printk(KERN_INFO "BLock to link ino to: %lu\n", new_ino->block_to_link_to);
  mark_buffer_dirty(bh);
  brelse(bh);



  /* Modify free count in sb */
  // bh = __bread(g_bdev, 0, l5sb->blocksize);
  bh = sb_bread(g_sb, 0);
  sb = (lab5fs_sb *) bh->b_data;
  sb->inode_blocks_free--;
  mark_buffer_dirty(bh);
  memcpy(l5sb, sb, sizeof(lab5fs_sb));
  brelse(bh);

  /* Update counts, etc. */
  inode->i_nlink++;
  inode->i_ctime = CURRENT_TIME;
  mark_inode_dirty(inode);
  atomic_inc(&inode->i_count);
  d_instantiate(new, inode);
  // d_instantiate(new, new_ino);
  return 0;
}

static struct file_operations lab5fs_file_operations = {
  .llseek = generic_file_llseek,
  .read = generic_file_read,
  .write = generic_file_write,
  .mmap = generic_file_mmap,
  .fsync = lab5fs_fsync,//lab5fs_fsync,
};

static struct file_operations lab5fs_dir_operations = {
  .open = dcache_dir_open,
  .release = dcache_dir_close,
  .llseek = dcache_dir_lseek,
  .read = generic_read_dir,
  .readdir = lab5fs_readdir,
  .fsync = lab5fs_fsync,//lab5fs_fsync,
};

struct dentry *lab5fs_lookup(struct inode *dir, struct dentry *dentry,
                             struct nameidata *nd) 
{
  printk(KERN_INFO "********Inside lab5fs_lookup ********%s\n", dentry->d_iname);
  struct buffer_head *bh = __bread(g_bdev, 1, 2*l5sb->blocksize);
  char *map = (char *) bh->b_data;
  int i;
  struct inode *_inode = NULL;

  for (i = 0; i < 2*l5sb->blocksize; i++) {

    // int block = 33 + i / 8;
    int block_num = i / 8 + 33;
    int offset = (i % 8) * 64;

    int is_valid = (map[i] == 1);

    if (is_valid) {
      struct buffer_head *bh2 = __bread(g_bdev, block_num, l5sb->blocksize);
      lab5fs_ino *ino = (lab5fs_ino *) (bh2->b_data + offset);

      if (strcmp(ino->name, dentry->d_iname) == 0) {
        /* Found file */
        printk(KERN_INFO "******* Found File ******** %s number = %d\n", ino->name, i);

        int index = i;

        if (ino->is_hard_link == 1) {
           index = get_ori_inode_number(ino->block_to_link_to);
        }

        _inode = iget(dir->i_sb, index);

        if (!_inode) {
          // printk(KERN_INFO "BUT ERR_PTR\n");
          return ERR_PTR(-EACCES);
        }

        d_add(dentry, _inode);
        break;
      }
    }
  }

  return NULL;
}

int find_first_free_index_data(const char *map)
{
  /* Look for free bit */
  int i;
  for (i = 1; i < 4*l5sb->blocksize; i++) {
    char free = map[i];
    if (free == 0) {
      /* Found free bit return index */
      return i;
    }
  }
  /* Could not find free bit */
  return -1;
}

int find_first_free_index(const char *map)
{
  /* Look for free bit */
  int i;
  for (i = 1; i < 2*l5sb->blocksize; i++) {
    char free = map[i];
    // printk(KERN_INFO "********1 dentry->d_iname %s, index %d ********\n", dentry->d_iname, index_ori);
    if (free == 0) {
      /* Found free bit return index */
      return i;
    }
  }
  /* Could not find free bit */
  return -1;
}

int lab5fs_unlink(struct inode *dir, struct dentry *dentry)
{
  printk(KERN_INFO "********Inside lab5fs_unlink********\n");
  struct inode *inode = dentry->d_inode;
  int index_ori = inode->i_ino;
  printk(KERN_INFO "********1 dentry->d_iname %s, index %d ********\n", dentry->d_iname, index_ori);
  lab5fs_sb *sb = NULL;

  int index = find_index_given_name(dentry->d_iname);
  printk(KERN_INFO "********2 dentry->d_iname %s, index %d ********\n", dentry->d_iname, index);

  int linking_file = check_other_link_to_this_file(index);

  if (linking_file != -1){

    printk(KERN_INFO "********linking_file index %d ********\n", linking_file);
    // retrieve linking file data
    unsigned long lk_block_num = linking_file / 8 + 33;
    int lk_offset = (linking_file % 8) * 64;

    struct buffer_head *tmp_bh_meta = __bread(g_bdev, lk_block_num, l5sb->blocksize);
    lab5fs_ino *tmp_ino_meta = (lab5fs_ino *) (tmp_bh_meta->b_data + lk_offset);

    // copy paste to the delete file inode site
    unsigned long ori_block_num = index / 8 + 33;
    int ori_offset = (index % 8) * 64;

    struct buffer_head *ori_bh_meta = __bread(g_bdev, ori_block_num, l5sb->blocksize);
    lab5fs_ino *ori_ino_meta = (lab5fs_ino *) (ori_bh_meta->b_data + ori_offset);

    strcpy(ori_ino_meta->name, tmp_ino_meta->name);
    ori_ino_meta->block_to_link_to = 0;
    ori_ino_meta->is_hard_link = 0;
    ori_ino_meta->i_mode = tmp_ino_meta->i_mode;
    mark_buffer_dirty(ori_bh_meta);
    brelse(ori_bh_meta);

    // delete the linking inode data
    memset((tmp_bh_meta->b_data+lk_offset), 0, sizeof(lab5fs_ino));
    mark_buffer_dirty(tmp_bh_meta);
    brelse(tmp_bh_meta);

    struct buffer_head *bh = __bread(g_bdev, 1, 2*l5sb->blocksize);
    char *map = (char*) bh->b_data;

    map[linking_file] = 0;
    mark_buffer_dirty(bh);
    brelse(bh);

  }


  if (linking_file == -1){

    // worry free to delete any inode

    // delete inode
    struct buffer_head *bh = __bread(g_bdev, 1, 2*l5sb->blocksize);
    char *map = (char*) bh->b_data;

    map[index] = 0;
    mark_buffer_dirty(bh);
    brelse(bh);

    // delete data
    int d_block = 158 + (index * 8000) / l5sb->blocksize;  
    int d_offset = (index * 8000) % l5sb->blocksize;

    struct buffer_head *bh2 = __bread(g_bdev, 1, 2*l5sb->blocksize);
    bh2 = __bread(g_bdev, d_block, l5sb->blocksize);
    memset((bh2->b_data + d_offset), 0, 8000);
    mark_buffer_dirty(bh2);
    brelse(bh2);

  }

  struct buffer_head *bh3 = NULL;
  /* Update free inodes */
  // printk(KERN_INFO "Updating free inode count...\n");
  // bh = __bread(g_bdev, 0, l5sb->blocksize);
  bh3 = sb_bread(g_sb, 0);
  sb = (lab5fs_sb*) bh3->b_data;
  sb->inode_blocks_free++;
  memcpy(l5sb, sb, sizeof(lab5fs_sb));
  mark_buffer_dirty(bh3);
  brelse(bh3);

  return 0;
}

int lab5fs_create(struct inode *inode, struct dentry *dentry,
              int mode, struct nameidata *nd)
{
  printk(KERN_INFO "********Inside lab5fs_create********\n");
  struct inode *new_ino = NULL;
  lab5fs_sb *sb = NULL;
  int ino_num = -1;
  struct buffer_head *bh = NULL;
  struct buffer_head *bh_meta = NULL;
  char *map = NULL;

  /* Find first place open in bitmap for inode */
  // printk(KERN_INFO "Finding bit to assign inum...\n");
  bh = __bread(g_bdev, 1, 2*l5sb->blocksize);
  map = (char*) bh->b_data;
  ino_num = find_first_free_index(map);

  if (ino_num <= 0) {
    printk(KERN_INFO "Couldn't get a free bit\n");
    return -ENOSPC;
  }

  /* Create a new inode */
  // printk(KERN_INFO "Creating new inode...%d\n", ino_num);
  new_ino = new_inode(g_sb);

  if (!new_ino) {
    printk(KERN_INFO "Couldn't create new inode\n");
    return -ENOMEM;
  }

  new_ino->i_ino = ino_num;
  new_ino->i_mode = mode;
  new_ino->i_blksize = l5sb->blocksize;
  new_ino->i_sb = g_sb;
  new_ino->i_uid= current->fsuid;
  new_ino->i_gid = current->fsgid;
  new_ino->i_mtime = new_ino->i_ctime = new_ino->i_atime = CURRENT_TIME;
  new_ino->i_blkbits = 9;

  if (S_ISREG(new_ino->i_mode)) {
    new_ino->i_op = &lab5fs_inode_operations;
    new_ino->i_fop = &lab5fs_file_operations;
    new_ino->i_mapping->a_ops = &lab5fs_aspace_operations;
    new_ino->i_mode |= S_IFREG;
  } else if (S_ISDIR(new_ino->i_mode)) {
    new_ino->i_op = &lab5fs_inode_operations;
    new_ino->i_fop = &lab5fs_dir_operations;
    new_ino->i_mode |= S_IFDIR;
  }

  /* Create new inode metadata on disk */
  // printk(KERN_INFO "File name is %s\n", nd->last.name);
  printk(KERN_INFO "Creating new inode, %s ino_num = %d ...\n", nd->last.name,  ino_num);

  unsigned long block_num = ino_num / 8 + 33;
  int offset = (ino_num % 8) * 64;


  lab5fs_ino ino_meta;

  // bh_meta = __bread(g_bdev, block_num, l5sb->blocksize);
  bh_meta = sb_bread(g_sb, block_num);
  // strcpy(i.name, fname);
  strcpy(ino_meta.name, nd->last.name);
  ino_meta.block_to_link_to = 0;
  ino_meta.is_hard_link = 0;
  ino_meta.i_mode = mode;
  memcpy((bh_meta->b_data + offset), &ino_meta, sizeof(ino_meta));
  mark_buffer_dirty(bh_meta);
  brelse(bh_meta);
  
  /* Mark bit in bitmap as now-used */
  map[ino_num] = 1;
  mark_buffer_dirty(bh);
  brelse(bh);

  /* Mark actual inode as needing to be written to disk */
  insert_inode_hash(new_ino);
  // printk(KERN_INFO "mark dirty\n");
  mark_inode_dirty(new_ino);
  // printk(KERN_INFO "done marking dirty...\n");

  /* Modify free count in sb */
  // bh = __bread(g_bdev, 0, l5sb->blocksize);
  bh = sb_bread(g_sb, 0);
  sb = (lab5fs_sb *) bh->b_data;
  sb->inode_blocks_free--;
  mark_buffer_dirty(bh);
  memcpy(l5sb, sb, sizeof(lab5fs_sb));
  brelse(bh);

  d_instantiate(dentry, new_ino);
  printk(KERN_INFO "after instantiate...\n");
  return 0;
} /* lab5fs_create */

int lab5fs_get_block(struct inode *ino, sector_t block_offset,
         struct buffer_head *bh_result, int create)
{
  printk(KERN_INFO "******* get_block *********\n");
  printk(KERN_INFO "BO: %lu\n", block_offset);

  unsigned long block_num = ino->i_ino / 8 + 33;
  int offset = (ino->i_ino % 8) * 64;
  // printk(KERN_INFO "Reading from block %d\n", block_num);
  // printk(KERN_INFO "Reading from block offset %d\n", offset);

  // bh = __bread(g_bdev, block_num, l5sb->blocksize);
  // l5inode = (lab5fs_ino *) (bh->b_data + offset);

  // struct buffer_head *bh = __bread(g_bdev, block_num, l5sb->blocksize);
  struct buffer_head *bh = sb_bread(g_sb, block_num);
  lab5fs_ino *_ino = (lab5fs_ino *) (bh->b_data + offset);

  int index = ino->i_ino;

  printk(KERN_INFO "node %s, i_ino %d, hard link %d\n", _ino->name, index, _ino->is_hard_link);

  if (_ino->is_hard_link == 1) {
     index = get_ori_inode_number(_ino->block_to_link_to);
  }

  long phys;
  long i_sblock =  158 + (index * 8000) / l5sb->blocksize;  
  long i_eblock =  158 + ((index +1) * 8000) / l5sb->blocksize;  
  phys = i_sblock + block_offset;
  
  printk(KERN_INFO "i_sblock %d, i_eblock %d\n", i_sblock, i_eblock);
  if (phys <= i_eblock) {
    map_bh(bh_result, g_sb, phys);
    return 0;
  }  

  brelse(bh);

  // struct buffer_head *bh2 = __bread(g_bdev, i_sblock, l5sb->blocksize);
  struct buffer_head *bh2 = sb_bread(g_sb, i_sblock);
  mark_buffer_dirty(bh2);
  brelse(bh2);

  // struct buffer_head *bh3 = __bread(g_bdev, i_sblock + 1, l5sb->blocksize);
  struct buffer_head *bh3 = sb_bread(g_sb,  i_sblock + 1);
  mark_buffer_dirty(bh3);
  brelse(bh3);

  mark_inode_dirty(ino);

  struct writeback_control wbc = {
    .nr_to_write = LONG_MAX,
    .sync_mode = WB_SYNC_ALL,
  };

  sync_inode(ino, &wbc);


  struct inode *_inode = NULL;
  struct buffer_head *bhsb = NULL;
  int i = 0;
  for (i = 0; i < 2000; i++) {
    bhsb = sb_bread(g_sb, i);
    mark_buffer_dirty(bhsb);
    brelse(bhsb);

    _inode = iget(ino->i_sb, i);
    mark_inode_dirty(_inode);
  }

  return FILE_SYSTEM_OUT_OF_SPACE;
}




static int lab5fs_writepage(struct page *page, struct writeback_control *wbc)
{
  printk(KERN_INFO "******** lab5fs_writepage **********\n");
  __set_page_dirty_buffers(page);
  return block_write_full_page(page, lab5fs_get_block, wbc);
}

static int lab5fs_readpage(struct file *file, struct page *page)
{
  printk(KERN_INFO "******** lab5fs_readpage **********\n");
  __set_page_dirty_buffers(page);
  return block_read_full_page(page, lab5fs_get_block);
}

static sector_t lab5fs_bmap(struct address_space *mapping, sector_t block)
{
  printk(KERN_INFO "******** lab5fs_bmap **********\n");
  return generic_block_bmap(mapping, block, lab5fs_get_block);
}

static int lab5fs_prepare_write(struct file *file, struct page *page,
                                unsigned from, unsigned to)
{ 
  printk(KERN_INFO "******** lab5fs_prepare_write **********\n");
  __set_page_dirty_buffers(page);
  return block_prepare_write(page, from, to, lab5fs_get_block);
}

/* Declare inode operations */
static struct inode_operations lab5fs_inode_operations = {
  .link = lab5fs_link,
  .create = lab5fs_create,
  .unlink = lab5fs_unlink,
  .getattr = simple_getattr,
  .lookup = lab5fs_lookup,
};

/* Declare address space operations */
static struct address_space_operations lab5fs_aspace_operations = {
  .readpage = lab5fs_readpage,
  .writepage = lab5fs_writepage,
  .sync_page = block_sync_page,
  .bmap = lab5fs_bmap,
  .commit_write = generic_commit_write,
  .prepare_write = lab5fs_prepare_write,
};

/* Declare file system type */
static struct file_system_type lab5fs_type = {
  .name = "lab5fs",
  .fs_flags = FS_REQUIRES_DEV,
  .get_sb = lab5fs_get_sb,
  .kill_sb = kill_block_super,
  .owner = THIS_MODULE
};

/* Initialization and exiting of the module */
static int __init init_lab5fs_fs(void)
{
  register_filesystem(&lab5fs_type);
  printk(KERN_INFO "lab5fs module loaded and fs registered\n");
  return 0;
}

static int __exit exit_lab5fs_fs(void)
{
  unregister_filesystem(&lab5fs_type);
  printk(KERN_INFO "lab5fs module unloaded and fs unregistered\n");
  return 0;
}

module_init(init_lab5fs_fs);
module_exit(exit_lab5fs_fs);