/*
 * $Id: media_many.c,v 1.1 2007/11/06 10:07:22 stuart_hc Exp $
 *
 * Samba VFS module supporting multiple AVID media directories.
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
 
/*
 * MediaMany is a Samba VFS module that allows multiple AVID clients to 
 * share a samba export. Each client sees their own media files directory,
 * "Avid MediaFiles" and "OMFI MediaFiles", and other client's media files
 * directories are hidden.
 *
 * Add the module to the vfs objects option in your Samba share configuration.
 * eg.
 *
 *   [avid_win_mm]
 *        path = /video
 *        vfs objects = media_many
 *        ...
 *
 * MediaMany works well with MediaLink - see media_link.c for details. 
 * For example:
 * 
 *   [avid_win_mm]
 *        path = /video
 *        vfs objects = media_link media_many
 *        media_link:avid_dir = Avid MediaFiles/MXF/2/
 *        media_link:media_dir = media/
 *        media_link:media_suffix = .mxf
 *        ...
 *
 * links in the media into the "Avid MediaFiles/MXF/2/" directory, and the
 * "Avid MediaFiles/MXF/1/" directory can be used for the client's local media.
 *
 *
*/ 

#include "includes.h"

static int vfs_mm_debug_level = DBGC_VFS;

#undef DBGC_CLASS
#define DBGC_CLASS vfs_mm_debug_level



static const char* MM_MODULE_NAME = "media_many";
static const char* AVID_MEDIAFILES_DIRNAME = "Avid MediaFiles";
static const size_t AVID_MEDIAFILES_DIRNAME_LEN = 15;
static const char* OMFI_MEDIAFILES_DIRNAME = "OMFI MediaFiles";
static const size_t OMFI_MEDIAFILES_DIRNAME_LEN = 15;



// supplements the directory list stream
typedef struct mm_dirinfo_struct
{
    fstring clientSuffix;
    BOOL isRootDir;
    SMB_STRUCT_DIR* dirstream;
} mm_dirinfo_struct;


// Add "_<ip address>_<user name>" suffix to path 
static void pcat_client_suffix(vfs_handle_struct *handle, char* path)
{
    pstrcat(path, "_");
    pstrcat(path, handle->conn->client_address);
    pstrcat(path, "_");
    pstrcat(path, handle->conn->user);
}

// Add "_<ip address>_<user name>" suffix to filename 
static void fcat_client_suffix(vfs_handle_struct *handle, char* fname)
{
    fstrcat(fname, "_");
    fstrcat(fname, handle->conn->client_address);
    fstrcat(fname, "_");
    fstrcat(fname, handle->conn->user);
}

// Add client suffix to media file paths, otherwise return the same path 
static void get_actual_path(vfs_handle_struct *handle, const char* path, char* newPath)
{
    const char* pathStart = path;

    // remove any leading './'
    if (pathStart[0] == '.' && pathStart[1] == '/')
    {
        pathStart += 2;
    }

    if (strncmp(AVID_MEDIAFILES_DIRNAME, pathStart, AVID_MEDIAFILES_DIRNAME_LEN) == 0 &&
        (pathStart[AVID_MEDIAFILES_DIRNAME_LEN] == '\0' || path[AVID_MEDIAFILES_DIRNAME_LEN] == '/'))
    {
        pstrcpy(newPath, AVID_MEDIAFILES_DIRNAME);
        pcat_client_suffix(handle, newPath);
        pstrcat(newPath, pathStart + AVID_MEDIAFILES_DIRNAME_LEN);
    }
    else if (strncmp(OMFI_MEDIAFILES_DIRNAME, pathStart, OMFI_MEDIAFILES_DIRNAME_LEN) == 0 &&
        (pathStart[OMFI_MEDIAFILES_DIRNAME_LEN] == '\0' || path[OMFI_MEDIAFILES_DIRNAME_LEN] == '/'))
    {
        pstrcpy(newPath, OMFI_MEDIAFILES_DIRNAME);
        pcat_client_suffix(handle, newPath);
        pstrcat(newPath, pathStart + OMFI_MEDIAFILES_DIRNAME_LEN);
    }
    else
    {
        pstrcpy(newPath, path);
    }
}



static int mm_statvfs(vfs_handle_struct *handle, connection_struct *conn, const char *path,
    vfs_statvfs_struct *statbuf)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_STATVFS(handle, conn, actualPath, statbuf);
}

// returns a mm_dirinfo_struct cast as a SMB_STRUCT_DIR
static SMB_STRUCT_DIR *mm_opendir(vfs_handle_struct *handle, connection_struct *conn, const char *path,
    const char *mask, uint32 attr)
{
    mm_dirinfo_struct* dirInfo = SMB_MALLOC_P(mm_dirinfo_struct);
    if (!dirInfo)
    {
        DEBUG(0, ("mm_opendir: Out of memory. Failed to malloc dirinfo.\n"));
        return NULL;
    }
    dirInfo->clientSuffix[0] = '\0';
    fcat_client_suffix(handle, dirInfo->clientSuffix); 
    
    // TODO: is there a better (fullproof) way to determine whether path is root?
    if (*path == 0 || 
        strcmp(path, "/") == 0 || 
        strcmp(path, ".") == 0 ||
        strcmp(path, "./") == 0)
    {
        dirInfo->isRootDir = True;
    }
    else
    {
        dirInfo->isRootDir = False;
    }
    
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    dirInfo->dirstream = SMB_VFS_NEXT_OPENDIR(handle, conn, actualPath, mask, attr);
        
    return (SMB_STRUCT_DIR*)dirInfo;
}

// skip other client's media directories
static SMB_STRUCT_DIRENT *mm_readdir(vfs_handle_struct *handle, connection_struct *conn, SMB_STRUCT_DIR *dirp)
{
    mm_dirinfo_struct* dirInfo = (mm_dirinfo_struct*)dirp;
    
    if (dirInfo->isRootDir)
    {
        int skip = False;
        SMB_STRUCT_DIRENT* dirent = NULL;
        
        // skip media files directories belonging to other clients
        do
        {
            skip = False;
            dirent = SMB_VFS_NEXT_READDIR(handle, conn, dirInfo->dirstream);
            
            if (dirent != NULL)
            {
                if (strncmp(AVID_MEDIAFILES_DIRNAME, dirent->d_name, AVID_MEDIAFILES_DIRNAME_LEN) == 0)
                {
                    // skip media files directory with no suffix
                    if (dirent->d_name[AVID_MEDIAFILES_DIRNAME_LEN] == '\0')
                    {
                        skip = True;
                    }
                    // skip media files directory with other client's suffix
                    else if (strcmp(dirInfo->clientSuffix, &dirent->d_name[AVID_MEDIAFILES_DIRNAME_LEN]) != 0)
                    {
                        skip = True;
                    }
                    else
                    {
                        // chop off the client suffix
                        dirent->d_name[AVID_MEDIAFILES_DIRNAME_LEN] = '\0';
                    }
                }
                else if (strncmp(OMFI_MEDIAFILES_DIRNAME, dirent->d_name, OMFI_MEDIAFILES_DIRNAME_LEN) == 0)
                {
                    // skip media files directory with no suffix
                    if (dirent->d_name[OMFI_MEDIAFILES_DIRNAME_LEN] == '\0')
                    {
                        skip = True;
                    }
                    // skip media files directory with other client's suffix
                    else if (strcmp(dirInfo->clientSuffix, &dirent->d_name[OMFI_MEDIAFILES_DIRNAME_LEN]) != 0)
                    {
                        skip = True;
                    }
                    else
                    {
                        // chop off the client suffix
                        dirent->d_name[OMFI_MEDIAFILES_DIRNAME_LEN] = '\0';
                    }
                }
            }
        }
        while (dirent != NULL && skip);
        
        return dirent;
    }
    else
    {
        return SMB_VFS_NEXT_READDIR(handle, conn, dirInfo->dirstream);
    }
}

static void mm_seekdir(vfs_handle_struct *handle, connection_struct *conn, SMB_STRUCT_DIR *dirp, long offset)
{
    return SMB_VFS_NEXT_SEEKDIR(handle, conn, ((mm_dirinfo_struct*)dirp)->dirstream, offset);
}

static long mm_telldir(vfs_handle_struct *handle, connection_struct *conn, SMB_STRUCT_DIR *dirp)
{
    return SMB_VFS_NEXT_TELLDIR(handle, conn, ((mm_dirinfo_struct*)dirp)->dirstream);
}

static void mm_rewinddir(vfs_handle_struct *handle, connection_struct *conn, SMB_STRUCT_DIR *dirp)
{
    return SMB_VFS_NEXT_REWINDDIR(handle, conn, ((mm_dirinfo_struct*)dirp)->dirstream);
}

static int mm_mkdir(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_MKDIR(handle, conn, actualPath, mode);
}

static int mm_rmdir(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_RMDIR(handle, conn, actualPath);
}

static int mm_closedir(vfs_handle_struct *handle, connection_struct *conn, SMB_STRUCT_DIR *dirp)
{
    SMB_STRUCT_DIR* realdirp = ((mm_dirinfo_struct*)dirp)->dirstream;
    SAFE_FREE(dirp);
    
    return SMB_VFS_NEXT_CLOSEDIR(handle, conn, realdirp);
}

static int mm_open(vfs_handle_struct *handle, connection_struct *conn, const char *path, int flags, mode_t mode)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_OPEN(handle, conn, actualPath, flags, mode);
}

static int mm_rename(vfs_handle_struct *handle, connection_struct *conn, const char *oldpath, const char *newpath)
{
    pstring oldActualPath;
    pstring newActualPath;
    get_actual_path(handle, oldpath, oldActualPath);
    get_actual_path(handle, newpath, newActualPath);
    
    return SMB_VFS_NEXT_RENAME(handle, conn, oldActualPath, newActualPath);
}

static int mm_stat(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_STRUCT_STAT *sbuf)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_STAT(handle, conn, actualPath, sbuf);
}

static int mm_lstat(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_STRUCT_STAT *sbuf)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_LSTAT(handle, conn, actualPath, sbuf);
}

static int mm_unlink(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_UNLINK(handle, conn, actualPath);
}

static int mm_chmod(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_CHMOD(handle, conn, actualPath, mode);
}

static int mm_chown(vfs_handle_struct *handle, connection_struct *conn, const char *path, uid_t uid, gid_t gid)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_CHOWN(handle, conn, actualPath, uid, gid);
}

static int mm_chdir(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_CHDIR(handle, conn, actualPath);
}

static int mm_utime(vfs_handle_struct *handle, connection_struct *conn, const char *path, struct utimbuf *times)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_UTIME(handle, conn, actualPath, times);
}

static BOOL mm_symlink(vfs_handle_struct *handle, connection_struct *conn, const char *oldpath, const char *newpath)
{
    pstring oldActualPath;
    pstring newActualPath;
    get_actual_path(handle, oldpath, oldActualPath);
    get_actual_path(handle, newpath, newActualPath);
    
    return SMB_VFS_NEXT_SYMLINK(handle, conn, oldActualPath, newActualPath);
}

static BOOL mm_readlink(vfs_handle_struct *handle, connection_struct *conn, const char *path, char *buf, size_t bufsiz)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_READLINK(handle, conn, actualPath, buf, bufsiz);
}

static int mm_link(vfs_handle_struct *handle, connection_struct *conn, const char *oldpath, const char *newpath)
{
    pstring oldActualPath;
    pstring newActualPath;
    get_actual_path(handle, oldpath, oldActualPath);
    get_actual_path(handle, newpath, newActualPath);
    
    return SMB_VFS_NEXT_LINK(handle, conn, oldActualPath, newActualPath);
}

static int mm_mknod(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode, SMB_DEV_T dev)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_MKNOD(handle, conn, actualPath, mode, dev);
}

static char *mm_realpath(vfs_handle_struct *handle, connection_struct *conn, const char *path, char *resolved_path)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_REALPATH(handle, conn, actualPath, resolved_path);
}

static size_t mm_get_nt_acl(vfs_handle_struct *handle, files_struct *fsp, const char *path, uint32 security_info, struct security_descriptor_info **ppdesc)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_GET_NT_ACL(handle, fsp, actualPath, security_info, ppdesc);
}

static BOOL mm_set_nt_acl(vfs_handle_struct *handle, files_struct *fsp, const char *path, uint32 security_info_sent, struct security_descriptor_info *psd)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);

    return SMB_VFS_NEXT_SET_NT_ACL(handle, fsp, actualPath, security_info_sent, psd);
}

static int mm_chmod_acl(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_CHMOD_ACL(handle, conn, actualPath, mode);
}

static SMB_ACL_T mm_sys_acl_get_file(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_ACL_TYPE_T type)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_SYS_ACL_GET_FILE(handle, conn, actualPath, type);
}

static int mm_sys_acl_set_file(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_ACL_TYPE_T acltype, SMB_ACL_T theacl)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
 
    return SMB_VFS_NEXT_SYS_ACL_SET_FILE(handle, conn, actualPath, acltype, theacl);
}

static int mm_sys_acl_delete_def_file(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_SYS_ACL_DELETE_DEF_FILE(handle, conn, actualPath);
}

static ssize_t mm_getxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, void *value, size_t size)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_GETXATTR(handle, conn, actualPath, name, value, size);
}

static ssize_t mm_lgetxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, void *value, size_t
size)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_LGETXATTR(handle, conn, actualPath, name, value, size);
}

static ssize_t mm_listxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, char *list, size_t size)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_LISTXATTR(handle, conn, actualPath, list, size);
}

static ssize_t mm_llistxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, char *list, size_t size)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_LLISTXATTR(handle, conn, actualPath, list, size);
}

static int mm_removexattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_REMOVEXATTR(handle, conn, actualPath, name);
}

static int mm_lremovexattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_LREMOVEXATTR(handle, conn, actualPath, name);
}

static int mm_setxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, const void *value, size_t size, int flags)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_SETXATTR(handle, conn, actualPath, name, value, size, flags);
}

static int mm_lsetxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, const void *value, size_t size, int flags)
{
    pstring actualPath;
    get_actual_path(handle, path, actualPath);
    
    return SMB_VFS_NEXT_LSETXATTR(handle, conn, actualPath, name, value, size, flags);
}



/* VFS operations structure */

static vfs_op_tuple mm_op_tuples[] = {

    /* Disk operations */
    {SMB_VFS_OP(mm_statvfs),            SMB_VFS_OP_STATVFS,     SMB_VFS_LAYER_TRANSPARENT},

    
    /* Directory operations */

    {SMB_VFS_OP(mm_opendir),            SMB_VFS_OP_OPENDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_readdir),            SMB_VFS_OP_READDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_seekdir),            SMB_VFS_OP_SEEKDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_telldir),            SMB_VFS_OP_TELLDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_rewinddir),          SMB_VFS_OP_REWINDDIR,   SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_mkdir),              SMB_VFS_OP_MKDIR,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_rmdir),              SMB_VFS_OP_RMDIR,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_closedir),           SMB_VFS_OP_CLOSEDIR,    SMB_VFS_LAYER_TRANSPARENT},

    
    /* File operations */

    {SMB_VFS_OP(mm_open),               SMB_VFS_OP_OPEN,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_rename),             SMB_VFS_OP_RENAME,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_stat),               SMB_VFS_OP_STAT,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_lstat),              SMB_VFS_OP_LSTAT,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_unlink),             SMB_VFS_OP_UNLINK,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_chmod),              SMB_VFS_OP_CHMOD,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_chown),              SMB_VFS_OP_CHOWN,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_chdir),              SMB_VFS_OP_CHDIR,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_utime),              SMB_VFS_OP_UTIME,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_symlink),            SMB_VFS_OP_SYMLINK,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_readlink),           SMB_VFS_OP_READLINK,    SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_link),               SMB_VFS_OP_LINK,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_mknod),              SMB_VFS_OP_MKNOD,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_realpath),           SMB_VFS_OP_REALPATH,    SMB_VFS_LAYER_TRANSPARENT},

    /* NT File ACL operations */

    {SMB_VFS_OP(mm_get_nt_acl),         SMB_VFS_OP_GET_NT_ACL,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_set_nt_acl),         SMB_VFS_OP_SET_NT_ACL,      SMB_VFS_LAYER_TRANSPARENT},

    /* POSIX ACL operations */

    {SMB_VFS_OP(mm_chmod_acl),                  SMB_VFS_OP_CHMOD_ACL,               SMB_VFS_LAYER_TRANSPARENT},

    {SMB_VFS_OP(mm_sys_acl_get_file),           SMB_VFS_OP_SYS_ACL_GET_FILE,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_sys_acl_set_file),           SMB_VFS_OP_SYS_ACL_SET_FILE,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_sys_acl_delete_def_file),    SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE, SMB_VFS_LAYER_TRANSPARENT},
    
    /* EA operations. */
    {SMB_VFS_OP(mm_getxattr),           SMB_VFS_OP_GETXATTR,            SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_lgetxattr),          SMB_VFS_OP_LGETXATTR,           SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_listxattr),          SMB_VFS_OP_LISTXATTR,           SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_llistxattr),         SMB_VFS_OP_LLISTXATTR,          SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_removexattr),        SMB_VFS_OP_REMOVEXATTR,         SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_lremovexattr),       SMB_VFS_OP_LREMOVEXATTR,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_setxattr),           SMB_VFS_OP_SETXATTR,            SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(mm_lsetxattr),          SMB_VFS_OP_LSETXATTR,           SMB_VFS_LAYER_TRANSPARENT},

    {NULL,                      SMB_VFS_OP_NOOP,            SMB_VFS_LAYER_NOOP}
};

NTSTATUS init_module(void)
{
    NTSTATUS ret =  smb_register_vfs(SMB_VFS_INTERFACE_VERSION, MM_MODULE_NAME, mm_op_tuples);

    if (!NT_STATUS_IS_OK(ret))
    {
        return ret;
    }

    vfs_mm_debug_level = debug_add_class(MM_MODULE_NAME);
    if (vfs_mm_debug_level == -1) 
    {
        vfs_mm_debug_level = DBGC_VFS;
        DEBUG(0, ("%s: Couldn't register custom debugging class!\n",
            "media_many::init_module"));
    } 
    else 
    {
        DEBUG(10, ("%s: Debug class number of '%s': %d\n", 
            "media_many::init_module", MM_MODULE_NAME, vfs_mm_debug_level));
    }

    return ret;
}


