#ifndef __MEMFS_H__
#define __MEMFS_H__

#define MAX_FILENAME 128
#define DATA_LENGTH 512

struct DataNode {
    char* m_data; // size is DATA_LENGTH
    size_t m_using;
    struct DataNode* m_next;
};

struct Index {      //size is 160
    char m_fname[MAX_FILENAME]; //filename
    size_t m_fsize; //file size
    struct DataNode* m_first; // first of dataNode
    struct DataNode* m_last; // last of dataNode
    int m_flag; //indicate type of file 1:for file; 2:for directory
    mode_t m_mode;

};



/*function*/
static void clean();
static void *memFS_init(struct fuse_conn_info *conn, 
			struct fuse_config *cfg);
static int memFS_readdir (const char *path, void *buf, fuse_fill_dir_t filler, 
			 off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
static int memFS_getattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi);
static struct Index* get_Index(const char* path, struct DataNode* dataNode);
static void get_dir_content(void *buf, struct DataNode* dataNode, fuse_fill_dir_t filler);
static int memFS_mkdir (const char *path, mode_t mode);
static long get_size(struct Index* index);
static void delete(const char* path);
static void path_process(const char* src, char **path, char **name);
static void delete_helper(struct Index* Pindex, struct Index* Cindex);
static int memFS_rmdir (const char * path);
static int memFS_create (const char* path, mode_t mode, struct fuse_file_info * info);
static int memFS_mknod (const char* path, mode_t mod, dev_t rdev);
static int make_new_node(const char* path, int type, mode_t mode);
static int  memFS_unlink (const char * path);
static int memFS_open (const char* path, struct fuse_file_info * fi);
static int memFS_write (const char* path, const char* buf, size_t size, off_t offset,
				struct fuse_file_info * fi);
static struct DataNode* new_DateNode();
static int memFS_read(const char *path, char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi);
static int memFS_chmod (const char * path, mode_t mode, struct fuse_file_info *fi);
static int memFS_truncate (const char *path, off_t offset, struct fuse_file_info *fi);
static void memFS_trunc(const char* path);
static size_t resize(struct Index* index, size_t size);
static int memFS_rename (const char* path, const char* newPath, unsigned int flags);
#endif