#define FUSE_USE_VERSION 26


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "global.h"
#include "dm.h"
//依据final_project.pdf编写
/**
 *  This function should look up the input path to
 *  determine if it is a directory or a file. If it is a
 *  directory, return the appropriate permissions. If it
 *  is a file, return the appropriate permissions as well
 *  as the actual size. This size must be accurate since
 *  it is used to determine EOF and thus read may not
 *  be called.
 * 
 *  return 0 on success, with a correctly set structure
 *  or -ENOENT if the file is not found
 */
//根据路径读取文件大小和权限信息到stbuf里
static int u_fs_getattr(const char *path, struct stat *stbuf)
{ 
	u_fs_file_directory *attr=malloc(sizeof(u_fs_file_directory));
    if(dm_open(path,attr)==-1){
    	free(attr);
		return -ENOENT; 
    }
    memset(stbuf, 0, sizeof(struct stat));
	// All files will be full access (i.e., chmod 0666), with permissions to be mainly ignored 
    if (attr->flag==2) {
    	stbuf->st_mode=S_IFDIR | 0666;
    } else if (attr->flag==1) {
        stbuf->st_mode=S_IFREG | 0666;
        stbuf->st_size=attr->fsize;
	}
	free(attr);
	return 0;

}

/**
 * This function should look up the input path, ensuring
 * that it is a directory, and then list the contents.
 * 
 * return 0 on success
 * or -ENOENT if the directory is not valid or found
 */
 //根据路径找到目录，将目录信息读入buf中
static int u_fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
	return dm_readdir(path,buf,filler,offset);
}

/**
 * This function should not be modified, 
 * as you get the full path every
 * time any of the other functions are called.
 * 
 */
static int u_fs_open(const char *path, struct fuse_file_info *fi)
{   
    return 0;  
}


/**
 * This function should add the new directory to the
 * root level, and should update the .directories file
 * 
 * return -ENAMETOOLONG if the name is beyond 8 chars
 * return -EPERM if the directory is not under the root dir only
 * return -EEXIST if the directory already exists
 */
static int u_fs_mkdir(const char *path, mode_t mode)
{
	return dm_create(path, 2);
}


/**
 * Deletes an empty directory
 * 
 * return 0 read on success
 * -ENOTEMPTY if the directory is not empty
 * -ENOENT if the directory is not found
 * -ENOTDIR if the path is not a directory
 */
static int u_fs_rmdir(const char *path)
{
	return dm_rm(path,2);
}

/**
 * This function should add a new file to a
 * subdirectory, and should update the .directories file
 * appropriately with the modified directory entry
 * structure.
 * 
 * return 0 on success
 * -ENAMETOOLONG if the name is beyond 8.3 chars
 * -EPERM if the file is trying to be created in the root dir
 * -EEXIST if the file already exists
 */
static int u_fs_mknod(const char *path, mode_t mode, dev_t rdev)
{	
	return dm_create(path, 1);
}

/**
 * This function should read the data in the file
 * denoted by path into buf, starting at offset.
 * 
 * reuturn size read on success
 * -EISDIR if the path is a directory
 * -ENOENT if the file not exist
 * -1 if error
 */ 
 //按path打开文件，从offset指定位置开始读出数据到buf中，返回文件大小
static int u_fs_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi)
{
	return dm_read(path,buf,size,offset);
}

/**
 * This function should write the data in buf into the
 * file denoted by path, starting at offset.
 * 
 * return size on success
 * -EFBIG if the offset is beyond the file size (but handle appends)
 * -errno if else
 */ 
 //path待存储文件路径
 //buf待存储的内容
 //size待存储内容的大小
 //offset偏移量
static int u_fs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
	return dm_write(path,buf,size,offset);
}

/**
 * Delete a file
 * return 0 read on success
 *-EISDIR if the path is a directory
 *-ENOENT if the file is not found
 */ 
static int u_fs_unlink(const char *path)
{
	return dm_rm(path,1);
}

/** 
 * This function should not be modified. 
 */
static int u_fs_truncate(const char *path, off_t size)
{
	return 0;
}
          
/** 
 * This function should not be modified. 
 */
static int u_fs_flush (const char * path, struct fuse_file_info * fi)
{
	return 0;
}

/**
 * This function initialize the filesystem when run,
 * read the supper block and 
 * set the global variables of the program.
 */
void * u_fs_init (struct fuse_conn_info *conn){
	
	FILE * fp=NULL;

	fp=fopen(disk_path, "r+");
	if(fp==NULL) {
		fprintf(stderr,"unsuccessful!\n");
		return 0;
	}
	//读超级块
    sb *super_block_record=malloc(sizeof(sb));
    fread(super_block_record, sizeof(sb), 1, fp);
    //用超级块中的fs_size初始化全局变量
	total_block_num=super_block_record->fs_size;
	fclose(fp);
	fslog("INIT","init success\n");
	return (long*)total_block_num;
}

static struct fuse_operations u_fs_oper={
    .getattr	= u_fs_getattr,
    .readdir	= u_fs_readdir,
    .mkdir     =u_fs_mkdir,
    .rmdir     =u_fs_rmdir,
    .mknod     =u_fs_mknod,
    .read      =u_fs_read,
    .write     =u_fs_write,
	.unlink    =u_fs_unlink,    
    .open	   =u_fs_open,
    .flush	   =u_fs_flush,
	.truncate	= u_fs_truncate,
	.init      =u_fs_init,
};


int main(int argc, char *argv[])
{
	//./ufs mountpoint diskimgpath l
	if(argc>2){
		 if(argv[2][0]=='/'){
			//printf("%s\n",argv[2]);
			int i=0;
			disk_path=strdup(argv[2]);
			
			for(i=3;i < argc;i++)
				strcpy(argv[i-1],argv[i]);
			//for(int i=0;i<argc;i++)
				//printf("%s\n",argv[i]);
			argc--;
			if(argc>2){
				if(argv[2][0]=='l'&&argv[2][1]=='\0'){
					logFlag=1;
					int i=0;
					for(i=3;i < argc;i++)
						strcpy(argv[i-1],argv[i]);
					argc--;
				}else {
					free(disk_path);
					printf("Format:./ufs mountpoint diskimgpath l\n");
					printf("\tExample:\n\t./ufs dir /home/cwh/diskimg l\n");
					printf("\t./ufs dir /home/cwh/diskimg\n");
					return 0;
				}
			}
			else {
				logFlag=0;
			}
			strcpy(argv[argc],disk_path);
			free(disk_path);
			disk_path=argv[argc];			
		}
		else {
			printf("Format:./ufs mountpoint diskimgpath l\n\tExample:\n\t./ufs dir /home/cwh/diskimg l\n\t./ufs dir /home/cwh/diskimg\n");
			return 0;
		}
	}
	else {
		printf("Format:./ufs mountpoint diskimgpath l\n\tExample:\n\t./ufs dir /home/cwh/diskimg l\n\t./ufs dir /home/cwh/diskimg\n");
		return 0;
	}
	FILE * fp=NULL;
	fp=fopen(disk_path, "r+");
	if(fp==NULL) {
		printf("open diskimg fail!\n");
		return 0;
	}
    return fuse_main(argc, argv, &u_fs_oper, NULL);
}
