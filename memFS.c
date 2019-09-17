#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>

#include "memFS.h" 

struct Index root;
struct DataNode rootNode;
char roorData[DATA_LENGTH];


static struct fuse_operations memFSOper = {
	.init = memFS_init,
	.readdir = memFS_readdir,
	.getattr = memFS_getattr,
	.mkdir = memFS_mkdir,
	.rmdir = memFS_rmdir,
	.create = memFS_create,
	.mknod = memFS_mknod,
	.unlink = memFS_unlink,
	.open = memFS_open,
	.write = memFS_write,
	.read = memFS_read,
	.chmod = memFS_chmod,
	.truncate = memFS_truncate,
	.rename = memFS_rename,
};

int main(int argc, char *argv[]) {
	int ret;
	printf("sizeof(struct Index) is %ld\n", sizeof(struct Index));
	ret = fuse_main(argc, argv, &memFSOper, NULL);
	clean();
	printf("this is main() end\n");
	return ret;
}

static void clean() {
	struct DataNode* dataNode = &rootNode;
	int times = dataNode->m_using / sizeof(struct Index);
	while(times) {	
		struct Index* index = (struct Index*)(dataNode->m_data + (times - 1) * sizeof(struct Index));
		delete_helper(&root, index);
		if(times == 1)
			break;
		times = dataNode->m_using / sizeof(struct Index);
	}
	dataNode = dataNode->m_next;
	while(dataNode) {
		struct DataNode* next = dataNode->m_next;
		times = dataNode->m_using / sizeof(struct Index);
		while(times) {
			struct Index* index = (struct Index*)(dataNode->m_data + (times - 1) * sizeof(struct Index));
			
			delete_helper(&root, index);
			if(times == 1)
				break;
			times = dataNode->m_using / sizeof(struct Index);
		}
		printf("next is %p\n", next);
		dataNode = next;
		rootNode.m_next = dataNode;
	}
	printf("clean end()\n");
}

static void *memFS_init(struct fuse_conn_info *conn, 
			struct fuse_config *cfg) {
	(void) conn;
	
	cfg->kernel_cache = 1;
	root.m_fname[0] = 'r';
	root.m_fname[1] = 'o';
	root.m_fname[2] = 'o';
	root.m_fname[3] = 't';
	root.m_fname[4] = '\n';

	root.m_fsize = -1;
	root.m_first = &rootNode;
	root.m_last = &rootNode;
	root.m_flag = 2;

	rootNode.m_data = (char*)(&roorData);
	rootNode.m_next = NULL;
	rootNode.m_using = 0;
	printf("this is hello_init() end\n");

	return NULL;
}


/** Read directory
	 *
	 * The filesystem may choose between two modes of operation:
	 *
	 * 1) The readdir implementation ignores the offset parameter, and
	 * passes zero to the filler function's offset.  The filler
	 * function will not return '1' (unless an error happens), so the
	 * whole directory is read in a single readdir operation.
	 *
	 * 2) The readdir implementation keeps track of the offsets of the
	 * directory entries.  It uses the offset parameter and always
	 * passes non-zero offset to the filler function.  When the buffer
	 * is full (or an error happens) the filler function will return
	 * '1'.
	 */
	// this using 1)
static int memFS_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
			 off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
	
	struct Index* index = NULL;
	(void) offset;
	(void) fi;
	(void) flags;
	index = get_Index(path, NULL);
	if(index) {
		if(index->m_flag == 2) {
			struct DataNode* dataNode = index->m_first;
			filler(buf, ".", NULL, 0, 0);
			filler(buf, "..", NULL, 0, 0);
			if(dataNode->m_using != 0) {
				get_dir_content(buf, dataNode, filler);
			}
			return 0;
		}
	}
	return -ENOENT;
}


static void get_dir_content(void *buf, struct DataNode* dataNode, fuse_fill_dir_t filler) {
	unsigned int i = 0;
	for(; i < (dataNode->m_using / sizeof(struct Index)); i++) {
		struct Index* index = (struct Index*)(dataNode->m_data + i * sizeof(struct Index));
		filler(buf, index->m_fname, NULL, 0, 0);
	}
	if(dataNode->m_next != NULL) {
		get_dir_content(buf, dataNode->m_next, filler);
	}
}

static struct Index* get_Index(const char* path, struct DataNode* dataNode) {
	struct Index* ret = NULL;
	if(strcmp(path, "/") == 0) {
		ret = &root;
	} else if(strncmp(path, "/", 1) == 0) {
		return get_Index(path + 1, NULL);
	} else {
		unsigned int i = 0;
		size_t strLen = strlen(path);
		if(dataNode == NULL)
			dataNode = &rootNode;
		for(; i < strLen; i++)
		{
			if((*(path + i)) == '/')
				break;
		}
		while(1) {
			unsigned int j = 0;
			for(; j < dataNode->m_using / sizeof(struct Index); j++) {
				if(strncmp(path, ((struct Index*)(dataNode->m_data + j * sizeof(struct Index)))->m_fname, i) == 0) {
					ret = ((struct Index*)(dataNode->m_data + j * sizeof(struct Index)));
					break;
				}
			}
			if(ret != NULL || dataNode->m_next == NULL)
				break;
			
			dataNode = dataNode->m_next;
		}
		
		if(((*(path + i)) == '/') && ret) {
			ret = get_Index(path + i + 1, ret->m_first);
		}
	}
	return ret;
}

/** Get file attributes.
	 *
	 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
	 * ignored. The 'st_ino' field is ignored except if the 'use_ino'
	 * mount option is given. In that case it is passed to userspace,
	 * but libfuse and the kernel will still assign a different
	 * inode for internal use (called the "nodeid").
	 *
	 * `fi` will always be NULL if the file is not currently open, but
	 * may also be NULL if the file is open.
	 */
	// struct stat describe the attribute of file in linux
static int memFS_getattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi)
{
	(void) fi;
	int ret = 0;
	ret = -ENOENT;
	struct Index* index = get_Index(path, NULL);
	printf("path is %s index is %p\n", path, index);
	if(index != NULL)
	{
		if(index->m_flag == 2){
			stbuf->st_mode = S_IFDIR | index->m_mode;
			
		}else if(index->m_flag == 1){
			stbuf->st_mode = S_IFREG | index->m_mode;
		}

		if(index->m_fsize == -1)
		{
			index->m_fsize = get_size(index);
		}
		stbuf->st_size = index->m_fsize;
		ret = 0;
	}
	return ret;	
}

static long get_size(struct Index* index) {
	long ret = 0;
	if(index->m_fsize != -1)
		ret = index->m_fsize;
	else {
		struct DataNode* dataNode = index->m_first;
		if(index->m_flag == 1) {
			while(dataNode) {
				ret += dataNode->m_using;
				dataNode = dataNode->m_next;
			}
		} else if(index->m_flag == 2) {
			while(dataNode) {
				int i = 0;
				for(; i < dataNode->m_using / sizeof(struct Index); i++)
				{
					ret += get_size((struct Index*)(dataNode->m_data + i * sizeof(struct Index)));
				}
				dataNode = dataNode->m_next;
			}
		}
	}
	return ret;
}

/** Create a directory
	 *
	 * Note that the mode argument may not have the type specification
	 * bits set, i.e. S_ISDIR(mode) can be false.  To obtain the
	 * correct directory type bits use  mode|S_IFDIR
	 * */
static int memFS_mkdir (const char *path, mode_t mode) {
	return make_new_node(path, 2, mode);
}

static void path_process(const char* src, char **path, char **name)
{
    int i = 0, j = 0;
	int postion = 0;
	char* pathName = NULL;
    char* dirName = NULL;
	for(; i < strlen(src); i++)
	{
		if(src[i] == '/')
			postion = i;
	}
    dirName = (char*)malloc(MAX_FILENAME);
	for(i = postion + 1; i < strlen(src); i++, j++)
	{
		dirName[j] = src[i];
	}
	dirName[j] = '\0';
	if(postion == 0)
		postion = 1;
		
	pathName = strndup(src, postion);
    *path = pathName;
    *name = dirName;
}

static void delete(const char* path) {
	char* filePath = NULL;
	char* fileName =  NULL;
	struct Index* Pindex = NULL;
	struct Index* Cindex = NULL;
	path_process(path, &filePath, &fileName);
	
	Cindex = get_Index(path, NULL);
	Pindex = get_Index(filePath, NULL);
	delete_helper(Pindex, Cindex);
	free(filePath);
	free(fileName);
	
}

static void delete_helper(struct Index* Pindex, struct Index* Cindex) {
	struct DataNode* dataNode = NULL;
	int flag = 1;
	if(!Cindex || !Pindex){
		return;
	}
	if(Cindex->m_flag == 1) {
		dataNode = Cindex->m_first;
		while(dataNode){
			struct DataNode* next = dataNode->m_next;
			printf("delete file %s\n", Cindex->m_fname);
			if(dataNode->m_data)
			{
				free(dataNode->m_data);
				dataNode->m_data = NULL;
			}
			free(dataNode);
			dataNode = NULL;
			dataNode = next;
		}
	} else if(Cindex->m_flag == 2) {
		printf("delete dir %s\n", Cindex->m_fname);
		dataNode = Cindex->m_first;
		while(dataNode) {
			int times = dataNode->m_using / sizeof(struct Index);
			struct DataNode* next = dataNode->m_next;
			while(times) {
				delete_helper(Cindex, (struct Index*)(dataNode->m_data + sizeof(struct Index) * (times - 1)));
				if(times == 1)
					break;
				times = dataNode->m_using / sizeof(struct Index);
			}
			dataNode = next;
		}
		dataNode = Cindex->m_first;
		if(dataNode) {
			if(dataNode->m_data) {
				free(dataNode->m_data);
				dataNode->m_data = NULL;
			}
			free(dataNode);
			dataNode = NULL;
		}
	}

	dataNode = Pindex->m_first;
	while(flag) {
		int i = 0;
		int times = 0;
		if(dataNode)
			times = dataNode->m_using / sizeof(struct Index);
		for(; i < times; i++) {
			if((dataNode->m_data + (i * sizeof(struct Index))) == ((char*)Cindex)) {
				printf("delete!!! Cindex name is %s\n", Cindex->m_fname);
				if(times == 1) {
					if(Pindex->m_first != dataNode) {
						struct DataNode* target = Pindex->m_first;
						while(target->m_next != dataNode) {
							target = target->m_next;
						}
						target->m_next = dataNode->m_next;
						if(dataNode == Pindex->m_last)
							Pindex->m_last = target;
						free(dataNode->m_data);
						dataNode->m_data = NULL;
						free(dataNode);
						dataNode = NULL;
					} else {
						dataNode->m_using = 0;
					}
				} else if (i != (times - 1)) {
					memcpy(dataNode->m_data + i * sizeof(struct Index), dataNode->m_data + (times - 1) * sizeof(struct Index), sizeof(struct Index));
					dataNode->m_using -= sizeof(struct Index);
				} else {
					dataNode->m_using -= sizeof(struct Index);
				}
				flag = 0;
				break;
			}
		}
		if(dataNode)
			dataNode = dataNode->m_next;
		if(!dataNode)
			flag = 0;
	}
	Pindex->m_fsize = -1;
}

/*remove a dictory*/
static int memFS_rmdir (const char * path) {
	delete(path);
	if(get_Index(path, NULL))
	{
		printf("memFS_rmdir faill!\n");
		return -1;
	}
	else 
	{
		printf("memFS_rmdir susscces!\n");
		return 0;
	}
}

/**
 * Create and open a file
*
* If the file does not exist, first create it with the specified
* mode, and then open it.
*
* If this method is not implemented or under Linux kernel
* versions earlier than 2.6.15, the mknod() and open() methods
* will be called instead.
*/
static int memFS_create (const char* path, mode_t mode, struct fuse_file_info * info) {
	(void)info;

	printf("memFS_create()\n");

	return make_new_node(path, 1, mode);
}

/** Create a file node
	 *
	 * This is called for creation of all non-directory, non-symlink
	 * nodes.  If the filesystem defines a create() method, then for
	 * regular files that will be called instead.
	 */
static int memFS_mknod (const char* path, mode_t mod, dev_t rdev) {
	printf("memFS_mknod()\n");
	(void)rdev;
	return make_new_node(path, 1, mod);
}

int make_new_node(const char* path, int type, mode_t mode) {
	char* fileName = NULL;
	char* pathName = NULL;
	path_process(path, &pathName, &fileName);
	if(strcmp(path, "/") == 0) {
		return -EPERM;
	}

	if(strlen(fileName) > (MAX_FILENAME - 1)) { //name is too long
        return ENAMETOOLONG;
    }

	if(strncmp(path, "/", 1) == 0) {
		struct DataNode* dataNode4New = NULL;
		struct Index* index = get_Index(pathName, NULL);
		if(index == NULL)
			return -1;
		if(index->m_flag != 2)
			return -1;
		struct DataNode* dataNode = index->m_first;
		free(pathName);
		pathName = NULL;
		while(dataNode) {
			if((DATA_LENGTH - dataNode->m_using) > sizeof(struct Index))
			{
				break;
			}
			dataNode = dataNode->m_next;
		}
		if(!dataNode) {
			dataNode = new_DateNode();
			if(!dataNode) {
				free(fileName);
				return -1;
			}
			index->m_last->m_next = dataNode;
			index->m_last = dataNode;
			dataNode->m_next = NULL;
			dataNode->m_using = 0;
		}

		index = (struct Index*)(dataNode->m_data + dataNode->m_using);
		dataNode4New = (struct DataNode*)malloc(sizeof(struct DataNode));
		if(!dataNode4New) {
			free(fileName);
			fileName = NULL;
			return -1;  // memory is not enough
		}
		dataNode4New->m_data = (char*)malloc(DATA_LENGTH);
		if(!dataNode4New->m_data) {
			free(dataNode4New);
			dataNode4New = NULL;
			free(fileName);
			fileName = NULL;
			return -1;  // memory is not enough
		}
		dataNode4New->m_next = NULL;
		dataNode4New->m_using = 0;

		index->m_first = dataNode4New;
		index->m_last = dataNode4New;
		index->m_flag = type;
		index->m_mode = mode;
		strncpy(index->m_fname, fileName, strlen(fileName));
		index->m_fname[strlen(fileName)] = '\0';
		free(fileName);
		fileName = NULL;
		if(index->m_flag == 1)
			index->m_fsize = 0;
		else 
			index->m_fsize = -1;
		dataNode->m_using += sizeof(struct Index);
		printf("into this end index->m_fname is %s \n", index->m_fname);
		return 0;
	}
	return -1;	
}	

/** Remove a file */
static int  memFS_unlink (const char * path) {
	delete(path);
	if(get_Index(path, NULL)) {
		printf("memFS_rmdir faill!\n");
		return -1;
	}
	else {
		printf("memFS_rmdir susscces!\n");
		return 0;
	}
}

/** Open a file
	 *
	 * Open flags are available in fi->flags. The following rules
	 * apply.
	 *
	 *  - Creation (O_CREAT, O_EXCL, O_NOCTTY) flags will be
	 *    filtered out / handled by the kernel.
	 *
	 *  - Access modes (O_RDONLY, O_WRONLY, O_RDWR, O_EXEC, O_SEARCH)
	 *    should be used by the filesystem to check if the operation is
	 *    permitted.  If the ``-o default_permissions`` mount option is
	 *    given, this check is already done by the kernel before calling
	 *    open() and may thus be omitted by the filesystem.
	 *
	 *  - When writeback caching is enabled, the kernel may send
	 *    read requests even for files opened with O_WRONLY. The
	 *    filesystem should be prepared to handle this.
	 *
	 *  - When writeback caching is disabled, the filesystem is
	 *    expected to properly handle the O_APPEND flag and ensure
	 *    that each write is appending to the end of the file.
	 * 
         *  - When writeback caching is enabled, the kernel will
	 *    handle O_APPEND. However, unless all changes to the file
	 *    come through the kernel this will not work reliably. The
	 *    filesystem should thus either ignore the O_APPEND flag
	 *    (and let the kernel handle it), or return an error
	 *    (indicating that reliably O_APPEND is not available).
	 *
	 * Filesystem may store an arbitrary file handle (pointer,
	 * index, etc) in fi->fh, and use this in other all other file
	 * operations (read, write, flush, release, fsync).
	 *
	 * Filesystem may also implement stateless file I/O and not store
	 * anything in fi->fh.
	 *
	 * There are also some flags (direct_io, keep_cache) which the
	 * filesystem may set in fi, to change the way the file is opened.
	 * See fuse_file_info structure in <fuse_common.h> for more details.
	 *
	 * If this request is answered with an error code of ENOSYS
	 * and FUSE_CAP_NO_OPEN_SUPPORT is set in
	 * `fuse_conn_info.capable`, this is treated as success and
	 * future calls to open will also succeed without being send
	 * to the filesystem process.
	 *
	 */
static int memFS_open (const char* path, struct fuse_file_info * fi) {
	if((fi->flags & O_TRUNC) && ((fi->flags & O_WRONLY) || (fi->flags & O_RDWR))) {
		memFS_trunc(path);
	} 
	return 0;
}

static void memFS_trunc(const char* path) {	 
	struct Index* index = get_Index(path, NULL);
	struct DataNode* dataNode = index->m_first; 
	printf("%s O_TRUNC!!!!!!!!!!!!!!!!\n", path);
	dataNode->m_using = 0;
	dataNode = dataNode->m_next;
	index->m_last = index->m_first;
	index->m_last->m_next = NULL;
	while(dataNode) {
		struct DataNode* next = dataNode->m_next;
		if(dataNode->m_data) {
			free(dataNode->m_data);
		}
		free(dataNode);
		dataNode = next;
	}
	index->m_fsize = 0;
}

static size_t resize(struct Index* index, size_t size) {
	if(index->m_fsize < size) {
		struct DataNode* dataNode = index->m_last;
		printf("resize index->m_fsize is %ld size is %ld\n", index->m_fsize, size);
		size -= index->m_fsize;
		printf("resize index->m_fsize is %ld size is %ld\n", index->m_fsize, size);
		while(size > 0) {
			size_t rest = DATA_LENGTH - dataNode->m_using;
			printf("rest is %ld\n", rest);
			if(rest > size) {
				dataNode->m_using += size;
				index->m_fsize += size;
				size = 0; 
			} else {
				dataNode->m_using = DATA_LENGTH;
				size -= rest;
				index->m_fsize += rest;
				dataNode = new_DateNode();
				if(!dataNode) {
					break;
				}
				index->m_last->m_next = dataNode;
				index->m_last = dataNode;
			}
		}	
	}
	return index->m_fsize;
}

/** Write data to an open file
	 *
	 * Write should return exactly the number of bytes requested
	 * except on error.	 An exception to this is when the 'direct_io'
	 * mount option is specified (see read operation).
	 *
	 * Unless FUSE_CAP_HANDLE_KILLPRIV is disabled, this method is
	 * expected to reset the setuid and setgid bits.
	 */
static int memFS_write (const char* path, const char* buf, size_t size, off_t offset,
				struct fuse_file_info * fi) {
	struct Index* index = get_Index(path, NULL);
	if(index == NULL)
		return -1;
	printf("%s write offset is %ld, size is %ld\n", path, offset, size);
	struct DataNode* dataNode = index->m_first;
	int ret = 0;
	size_t writeCount = 0;
	int tmp = 0;
	if(fi->flags & O_APPEND) {
		printf("%s O_APPEND!!!!!!!!!!!!!!!!\n", path);
		offset = index->m_fsize;
	}

	if(offset + size > index->m_fsize) {
		tmp = 0;
		dataNode = index->m_first;
		while(dataNode){
			tmp += 1;
			dataNode = dataNode->m_next;
		}
		printf("datanode num is %d\n",tmp);
		index->m_fsize = resize(index, offset + size);
		if(offset + size > index->m_fsize) {
			printf("write fail!!!!! size is %ld\n", index->m_fsize);
			return -1;
		}
	}
	printf("resize end index->m_fsize is %ld\n", index->m_fsize);
	
	dataNode = index->m_first;
	tmp = 0;
	while(dataNode){
		tmp += 1;
		dataNode = dataNode->m_next;
	}
	printf("datanode num is %d\n",tmp);
	dataNode = index->m_first;
	while(offset >= DATA_LENGTH ) {
		dataNode = dataNode->m_next;
		offset -= DATA_LENGTH;
	}
	printf("start write!!!!! offset is %ld size is %ld \n", offset, size);
	while(size) {
		writeCount = DATA_LENGTH - offset;
		writeCount = writeCount < size ? writeCount : size;
		memcpy(dataNode->m_data + offset, buf + ret, writeCount);
		ret += writeCount;
		offset = 0;
		size -= writeCount;
		dataNode = dataNode->m_next;
	}
	printf("write end size is %ld\n", index->m_fsize);
	return ret;

}

static struct DataNode* new_DateNode() {
	struct DataNode* ret = (struct DataNode*)malloc(sizeof(struct DataNode));
	if(ret) {
		ret->m_data = (char*)malloc(DATA_LENGTH);
		if(ret->m_data) {
			ret->m_using = 0;
			ret->m_next = NULL;
			memset(ret->m_data, 0, DATA_LENGTH);
		} else {
			free(ret);
			ret = NULL;
		}
	}
	return ret;
}

static int memFS_read(const char *path, char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi) {
	(void) fi;
	
	printf("memFS_read...........\n");
	struct Index* index = get_Index(path, NULL);
	if(index == NULL)
		return -1;
	struct DataNode* dataNode = index->m_first;
	int ret = 0;
	if(size > index->m_fsize - offset)
		size = index->m_fsize - offset;
	if(size < 0) {
		return -1;
	}
	dataNode = index->m_first;
	while(offset >= DATA_LENGTH ) {
		dataNode = dataNode->m_next;
		offset -= DATA_LENGTH;
	}
	printf("%s read offset is %ld, size is %ld\n", path ,offset, size);
	while(dataNode && (size > 0)) {
		int readCount = dataNode->m_using - offset;
		readCount = (readCount < size) ? readCount : size;
		memcpy(buf, dataNode->m_data, readCount);
		size -= readCount;
		ret += readCount;
		buf += readCount;
		dataNode = dataNode->m_next;
		offset = 0;
	}
	return ret;
}

/** Change the permission bits of a file
	 *
	 * `fi` will always be NULL if the file is not currenlty open, but
	 * may also be NULL if the file is open.
	 */
static int memFS_chmod (const char * path, mode_t mode, struct fuse_file_info *fi) {
	struct Index* index = get_Index(path, NULL);
	if(index) {
		index->m_mode = mode;
	} else {
		return -1;
	}
	return 0;
}

/** Change the size of a file
	*
	* `fi` will always be NULL if the file is not currenlty open, but
	* may also be NULL if the file is open.
	*
	* Unless FUSE_CAP_HANDLE_KILLPRIV is disabled, this method is
	* expected to reset the setuid and setgid bits.
 */
static int memFS_truncate (const char *path, off_t offset, struct fuse_file_info *fi) {
	
	return 0;
}


/** Rename a file
	 *
	 * *flags* may be `RENAME_EXCHANGE` or `RENAME_NOREPLACE`. If
	 * RENAME_NOREPLACE is specified, the filesystem must not
	 * overwrite *newname* if it exists and return an error
	 * instead. If `RENAME_EXCHANGE` is specified, the filesystem
	 * must atomically exchange the two files, i.e. both must
	 * exist and neither may be deleted.
	 */
static int memFS_rename (const char* path, const char* newPath, unsigned int flags) { 
	struct Index* oldIndex = get_Index(path, NULL);
	struct Index* newIndex = get_Index(newPath, NULL);
	int ret = 0;
	
	printf("path is %s, new path is %s newIndex is %p\n", path, newPath, newIndex);

	if(newIndex == NULL) {
		printf("into this newPath == NULL\n");
		struct DataNode* dataNode = NULL;
		ret = make_new_node(newPath, oldIndex->m_flag, oldIndex->m_mode);
		if(ret == -1) 
			return -1;
		newIndex = get_Index(newPath, NULL);
		if(!newIndex)
			return -1;

		dataNode = oldIndex->m_first;
		oldIndex->m_first = newIndex->m_first;
		oldIndex->m_last = newIndex->m_first;
		oldIndex->m_last->m_next = NULL;
		oldIndex->m_last->m_using = 0;

		newIndex->m_first = dataNode;
		newIndex->m_last = dataNode;
		dataNode = dataNode->m_next;
		while(dataNode)
		{
			newIndex->m_last = dataNode;
			dataNode = dataNode->m_next;
		}

		newIndex->m_fsize = oldIndex->m_fsize;
		delete(path);
		return 0;
	} else {
		printf("into this newPath != NULL\n");
		if(flags & (1 << 1)) { // RENAME_EXCHANGE
			struct DataNode* first = oldIndex->m_first;
			struct DataNode* last = oldIndex->m_last;
			size_t size =  oldIndex->m_fsize;
			int flag =  oldIndex->m_flag;
			mode_t mode = oldIndex->m_mode;

			oldIndex->m_first = newIndex->m_first;
			oldIndex->m_last = newIndex->m_last;
			oldIndex->m_fsize = newIndex->m_fsize;;
			oldIndex->m_flag = newIndex->m_flag;
			oldIndex->m_mode = newIndex->m_mode;

			newIndex->m_first = first;
			newIndex->m_last = last;
			newIndex->m_fsize = size;
			newIndex->m_flag = flag;
			newIndex->m_mode = mode;

		} else {
			return -1;
		}
	}
	return -1;
}