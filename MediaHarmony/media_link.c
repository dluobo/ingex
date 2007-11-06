/*
 * $Id: media_link.c,v 1.1 2007/11/06 10:07:22 stuart_hc Exp $
 *
 * Samba VFS module for exposing media files in an Avid media directory
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
 * Media Link is a Samba VFS module that allows media from a target directory
 * to be linked into an Avid media files directory. 
 *
 * The Media Link module must be added before the Media Harmony module. 
 * The parameters, all of them required, are as follows:
 *
 *  media_link:avid_dir      : the Avid directory into which the media files are
 *                             exposed; typically this will be 
 *                             Avid MediaFiles/MXF/1
 *  media_link:media_dir     : the directory of the media files to be exposed
 *  media_link:media_suffix  : a list of case-insensitive suffixes for the media 
 *                             files; typically this would include mxf
 *
 * Example.
 *
 *   [avid_win]
 *        path = /video
 *        vfs objects = media_link media_harmony
 *        media_link:avid_dir = Avid MediaFiles/MXF/1/
 *        media_link:media_dir = media/
 *        media_link:media_suffix = .mxf
 *        ...
 *
 *
 * CAUTION: it is recommended that the linked media directory (eg. media/ in the
 * above example) and media files are set to read-only using the file system 
 * permission settings. This prevents users from deleting the media files. 
 * 
 * Example, limit the users that can connect to the Samba
 * export and set the directory and contents to read-only for all those users.
 */
 
 

#include "includes.h"

static int vfs_ml_debug_level = DBGC_VFS;

#undef DBGC_CLASS
#define DBGC_CLASS vfs_ml_debug_level

static const char* MH_MODULE_NAME = "media_link";

static const char* MDB_FILENAME = "msmMMOB.mdb";
static const char* PMR_FILENAME = "msmFMID.pmr";
static const char* CREATING_DIRECTORY = "Creating";



typedef struct 
{
    char** mediaSuffixList;
    pstring mediaDir; /* the link target directory (always ends with '/') */
    pstring avidDir; /* the link to directory (always ends with '/') */
} ml_private_data_struct;

typedef struct
{
    BOOL isAvidDir; /* True if directory equals the Avid directory */
    SMB_STRUCT_DIR* mediaDirstream; /* the media directory stream */
    SMB_STRUCT_DIR* avidDirstream; /* the Avid directory stream */
    SMB_STRUCT_DIR* dirstream; /* equals mediaDirstream or avidDirstream */
} ml_dirinfo_struct;



static void get_dir_and_name(const char* path, char* dir, char* name)
{
    char* lastSep = strrchr(path, '/');
    if (lastSep == NULL)
    {
        /* a file or directory in the root directory */
        pstrcpy(dir, "/");
        pstrcpy(name, path);
    }
    else
    {
        /* a file or directory in a sub-directory */
        pstrcpy(dir, path);
        dir[lastSep - path + 1] = '\0';
        pstrcpy(name, lastSep + 1);
    }
}

/* simple check based on filename suffix */
static BOOL is_media_file(vfs_handle_struct* handle, const char* path)
{
    ml_private_data_struct* pd = NULL;

    if (!SMB_VFS_HANDLE_TEST_DATA(handle))
    {
        return False;
    }
    SMB_VFS_HANDLE_GET_DATA(handle, pd, ml_private_data_struct, return False);

    size_t len = strlen(path);
    char** mediaSuffixListPtr = pd->mediaSuffixList;
    while (*mediaSuffixListPtr != NULL)
    {
        size_t suffixLen = strlen(*mediaSuffixListPtr);
        if (len >= suffixLen && StrCaseCmp(&path[len - suffixLen], *mediaSuffixListPtr) == 0)
        {
            return True;
        }
        
        mediaSuffixListPtr++;
    }
    
    return False;
}

/* return True if the path points to the Avid directory */
static BOOL is_avid_dir(vfs_handle_struct* handle, const char *path)
{
    ml_private_data_struct* pd = NULL;

    if (!SMB_VFS_HANDLE_TEST_DATA(handle))
    {
        return False;
    }
    SMB_VFS_HANDLE_GET_DATA(handle, pd, ml_private_data_struct, return False);
    
    
    if (strcmp(path, pd->avidDir) == 0)
    {
        return True;
    }
    /* avidDir always ends in '/', so check if path equals avidDir but without
    the end separator */
    else if (strncmp(path, pd->avidDir, strlen(pd->avidDir) - 1) == 0 &&
        strlen(path) == strlen(pd->avidDir) - 1)
    {
        return True;
    }
    
    return False;
}

/* return true if the directory entry is an avid database file or Creating directory */
static BOOL is_avid_dir_entry(const char* name)
{
    if (strcmp(name, MDB_FILENAME) == 0 || 
        strcmp(name, PMR_FILENAME) == 0 ||
        strcmp(name, CREATING_DIRECTORY) == 0)
    {
        return True;
    }
    
    return False;
}

/* return true if the directory entry is an avid directory */
static BOOL is_avid_dir_dir_entry(const char* name)
{
    if (strcmp(name, CREATING_DIRECTORY) == 0)
    {
        return True;
    }
    
    return False;
}

/* return True if the path points to the Avid directory, is not an Avid file 
and is a media file */
static BOOL is_linked_media(vfs_handle_struct* handle, const char *path)
{
    pstring dir;
    fstring name;

    get_dir_and_name(path, dir, name);
    
    if (is_avid_dir(handle, dir) && !is_avid_dir_entry(name) && is_media_file(handle, name))
    {
        return True;
    }

    return False;    
}

/* returns in realVFSPath the path to the target media file if the path references 
a file in the media directory linked through the Avid directory;  
otherwise the path is returned */
static BOOL get_real_vfs_path(vfs_handle_struct* handle, const char* path, char* realVFSPath)
{
    pstring dir;
    fstring name;
    ml_private_data_struct* pd = NULL;

    if (!is_linked_media(handle, path) && !is_avid_dir(handle, path))
    {
        /* path is a real path */
        pstrcpy(realVFSPath, path);
        return True;
    }

    if (!SMB_VFS_HANDLE_TEST_DATA(handle))
    {
        goto error;
    }
    SMB_VFS_HANDLE_GET_DATA(handle, pd, ml_private_data_struct, goto error);

    get_dir_and_name(path, dir, name);

    /* path = media directory + filename */
    pstrcpy(realVFSPath, pd->mediaDir);    
    pstrcat(realVFSPath, name);   

    return True;
    
error:
    /* this path should cause an error */
    pstrcpy(realVFSPath, "this file does not exist");
    return False;    
}

static void free_private_data(void** p_data)
{
    ml_private_data_struct* pd = *(ml_private_data_struct **)p_data;

    str_list_free(&pd->mediaSuffixList);
    
    SAFE_FREE(pd);
    *p_data = NULL;
}





static int ml_connect(vfs_handle_struct *handle, connection_struct *conn, const char *svc, 
    const char *user)
{
    ml_private_data_struct* pd = NULL;
    const char* mediaDir;
    const char* avidDir;
    const char** mediaSuffixList;
    
    if (!handle) 
    {
        return -1;
    }
        
    /* get parameters */
    
    mediaDir = lp_parm_const_string(SNUM(conn), "media_link", "media_dir", NULL);
    avidDir = lp_parm_const_string(SNUM(conn), "media_link", "avid_dir", NULL);
    mediaSuffixList = lp_parm_string_list(SNUM(conn), "media_link", "media_suffix", NULL);
    
        
    /* check parameters */
    
    if (mediaDir == NULL || avidDir == NULL || mediaSuffixList == NULL)
    {
        if (mediaDir == NULL)
        {
            DEBUG(0, ("Missing media_link:media_dir parameter\n"));
        }
        if (avidDir == NULL)
        {
            DEBUG(0, ("Missing media_link:avid_dir parameter\n"));
        }
        if (mediaSuffixList == NULL)
        {
            DEBUG(0, ("Missing media_link:media_suffix parameter\n"));
        }
        return -1;
    }
    else if (strcmp(mediaDir, avidDir) == 0)
    {
        DEBUG(0, ("media_link:media_dir cannot equal media_link:avid_dir\n"));
        return -1;
    }

    
    /* allocate private data */
    if (!(pd = SMB_MALLOC_P(ml_private_data_struct)))
    {
        return -1;
    }
    ZERO_STRUCTP(pd);
    
    
    /* load parameters into private data */
    
    pstrcpy(pd->mediaDir, mediaDir);
    pstrcpy(pd->avidDir, avidDir);
    /* make sure that directory ends with a path separator '/' */
    if (pd->mediaDir[strlen(pd->mediaDir) - 1] != '/')
    {
        pstrcat(pd->mediaDir, "/");
    }
    if (pd->avidDir[strlen(pd->avidDir) - 1] != '/')
    {
        pstrcat(pd->avidDir, "/");
    }
    if (!str_list_copy(&pd->mediaSuffixList, mediaSuffixList))
    {
        DEBUG(0, ("ml_connect: Failed to copy media suffix list.\n"));
        free_private_data((void**)pd);
        return -1;
    }
    
    
    SMB_VFS_HANDLE_SET_DATA(handle, pd, free_private_data, ml_private_data_struct, return -1);

        
    return SMB_VFS_NEXT_CONNECT(handle, conn, svc, user);
}

static int ml_statvfs(vfs_handle_struct *handle, connection_struct *conn, const char *path,
    vfs_statvfs_struct *statbuf)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_STATVFS(handle, conn, realVFSPath, statbuf);
}

static SMB_STRUCT_DIR *ml_opendir(vfs_handle_struct *handle, connection_struct *conn, const char *path,
    const char *mask, uint32 attr)
{
    ml_dirinfo_struct* dirInfo = SMB_MALLOC_P(ml_dirinfo_struct);
    if (!dirInfo)
    {
        DEBUG(0, ("ml_opendir: Out of memory. Failed to malloc dirinfo.\n"));
        return NULL;
    }
    ZERO_STRUCTP(dirInfo);
    
    if (is_avid_dir(handle, path))
    {
        ml_private_data_struct* pd = NULL;
    
        if (!SMB_VFS_HANDLE_TEST_DATA(handle))
        {
            return NULL;
        }
        SMB_VFS_HANDLE_GET_DATA(handle, pd, ml_private_data_struct, return NULL);
    
        dirInfo->isAvidDir = True;
        dirInfo->mediaDirstream = SMB_VFS_NEXT_OPENDIR(handle, conn, pd->mediaDir, mask, attr);
        dirInfo->avidDirstream = SMB_VFS_NEXT_OPENDIR(handle, conn, path, mask, attr);
        dirInfo->dirstream = dirInfo->avidDirstream;  /* Avid directory is read first */

        return (SMB_STRUCT_DIR*)dirInfo;
    }
    else
    {
        dirInfo->isAvidDir = False;
        dirInfo->mediaDirstream = NULL;
        dirInfo->avidDirstream = SMB_VFS_NEXT_OPENDIR(handle, conn, path, mask, attr);
        dirInfo->dirstream = dirInfo->avidDirstream;
        
        return (SMB_STRUCT_DIR*)dirInfo;
    }
}

static SMB_STRUCT_DIRENT *ml_readdir(vfs_handle_struct *handle, connection_struct *conn, SMB_STRUCT_DIR *dirp)
{
    ml_dirinfo_struct* dirInfo = (ml_dirinfo_struct*)dirp;
    
    if (dirInfo->isAvidDir)
    {
        int skip;
        SMB_STRUCT_DIRENT* dirent;
        do
        {
            skip = False;
            
            dirent = SMB_VFS_NEXT_READDIR(handle, conn, dirInfo->dirstream);
            
            if (dirent == NULL && dirInfo->dirstream != dirInfo->mediaDirstream)
            {
                /* switch to the media directory entries */
                dirInfo->dirstream = dirInfo->mediaDirstream;
                dirent = SMB_VFS_NEXT_READDIR(handle, conn, dirInfo->dirstream);
            }
            
            
            if (dirent != NULL && dirInfo->dirstream == dirInfo->avidDirstream)
            {
                /* skip entries that are not Avid files */
                if (!is_avid_dir_entry(dirent->d_name))
                {
                    skip = True;
                }
            }
            else if (dirent != NULL && dirInfo->dirstream == dirInfo->mediaDirstream)
            {
                /* skip entries that are not media files */
                if (!is_media_file(handle, dirent->d_name))
                {
                    skip = True;
                }
            }
            
        } while (dirent != NULL && skip);
        
        return dirent;
    }
    else
    {
        return SMB_VFS_NEXT_READDIR(handle, conn, dirInfo->dirstream);
    }
}

static void ml_seekdir(vfs_handle_struct *handle, connection_struct *conn, SMB_STRUCT_DIR *dirp, long offset)
{
    DEBUG(0, ("ml_seekdir: not implemented\n"));
    /* TODO: is doing nothing ok behaviour? */
    return;
    
    /* the problem here is that telldir was found to return very large numbers. The initial scheme
    was to add the maximum telldir offset for the Avid directory stream to the offset
    of the media stream, so that which stream was targeted could be determined. */
}

static long ml_telldir(vfs_handle_struct *handle, connection_struct *conn, SMB_STRUCT_DIR *dirp)
{
    /* Note: the returned value will only be valid if dirstream is not swapped */
    return SMB_VFS_NEXT_TELLDIR(handle, conn, ((ml_dirinfo_struct*)dirp)->dirstream);
}

static void ml_rewinddir(vfs_handle_struct *handle, connection_struct *conn, SMB_STRUCT_DIR *dirp)
{
    ml_dirinfo_struct* dirInfo = (ml_dirinfo_struct*)dirp;
    
    if (dirInfo->isAvidDir)
    {
        /* the Avid directory is first in line */
        dirInfo->dirstream = dirInfo->avidDirstream;
        return SMB_VFS_NEXT_REWINDDIR(handle, conn, dirInfo->dirstream);
    }
    else
    {
        return SMB_VFS_NEXT_REWINDDIR(handle, conn, dirInfo->dirstream);
    }
}

static int ml_mkdir(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
    pstring dir;
    fstring name;

    get_dir_and_name(path, dir, name);
    
    if (is_avid_dir(handle, dir) && !is_avid_dir_dir_entry(name))
    {
        /* don't allow for non-Avid sub-directories in the Avid directory */
        errno = EPERM;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_MKDIR(handle, conn, path, mode);
    }
}

static int ml_rmdir(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    if (is_avid_dir(handle, path))
    {
        /* don't allow the avid directory to be removed */
        errno = EPERM;
        return -1;
    }
    else
    {
        pstring realVFSPath;
        get_real_vfs_path(handle, path, realVFSPath);
    
        return SMB_VFS_NEXT_RMDIR(handle, conn, realVFSPath);
    }
}

static int ml_closedir(vfs_handle_struct *handle, connection_struct *conn, SMB_STRUCT_DIR *dirp)
{
    ml_dirinfo_struct* dirInfo = (ml_dirinfo_struct*)dirp;

    if (dirInfo->isAvidDir)
    {
        int result = 1;
        SMB_STRUCT_DIR* mediaDirpy = dirInfo->mediaDirstream;
        SMB_STRUCT_DIR* avidDirpy = dirInfo->avidDirstream;
        SAFE_FREE(dirInfo);
    
        if (mediaDirpy != NULL)
        {
            result = SMB_VFS_NEXT_CLOSEDIR(handle, conn, mediaDirpy);
        }
        return SMB_VFS_NEXT_CLOSEDIR(handle, conn, avidDirpy) && result;
    }
    else
    {
        SMB_STRUCT_DIR* dirpy = dirInfo->dirstream;
        SAFE_FREE(dirInfo);
        
        return SMB_VFS_NEXT_CLOSEDIR(handle, conn, dirpy);
    }
}

static int ml_open(vfs_handle_struct *handle, connection_struct *conn, const char *path, int flags, mode_t mode)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_OPEN(handle, conn, realVFSPath, flags, mode);
}

static int ml_rename(vfs_handle_struct *handle, connection_struct *conn, const char *oldpath, const char *newpath)
{
    pstring olddir, newdir;
    fstring oldname, newname;

    get_dir_and_name(oldpath, olddir, oldname);
    get_dir_and_name(newpath, newdir, newname);
    
    if ((is_avid_dir(handle, olddir) && !is_avid_dir(handle, newdir)) ||
        (!is_avid_dir(handle, olddir) && is_avid_dir(handle, newdir)))
    {
        /* don't renaming to or from the avid directory */
        errno = EPERM;
        return -1;
    }
    else
    {
        pstring oldMediaPath;
        pstring newMediaPath;
        
        get_real_vfs_path(handle, oldpath, oldMediaPath);
        get_real_vfs_path(handle, newpath, newMediaPath);

        return SMB_VFS_NEXT_RENAME(handle, conn, oldMediaPath, newMediaPath);
    }
}

static int ml_stat(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_STRUCT_STAT *sbuf)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_STAT(handle, conn, realVFSPath, sbuf);
}

static int ml_lstat(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_STRUCT_STAT *sbuf)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_LSTAT(handle, conn, realVFSPath, sbuf);
}

static int ml_unlink(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    if (is_linked_media(handle, path))
    {
        /* don't allow the media files to be deleted */
        errno = EPERM;
        return -1;
    }
    else
    {
        return SMB_VFS_NEXT_UNLINK(handle, conn, path);
    }
}

static int ml_chmod(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_CHMOD(handle, conn, realVFSPath, mode);
}

static int ml_chown(vfs_handle_struct *handle, connection_struct *conn, const char *path, uid_t uid, gid_t gid)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_CHOWN(handle, conn, realVFSPath, uid, gid);
}

static int ml_chdir(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_CHDIR(handle, conn, realVFSPath);
}

static char* ml_getwd(vfs_handle_struct *handle, connection_struct *conn, char *buf)
{
	char* path = SMB_VFS_NEXT_GETWD(handle, conn, buf);
    if (path != NULL)
    {
        pstring realVFSPath;
        get_real_vfs_path(handle, path, realVFSPath);
        pstrcpy(path, realVFSPath);
    }
    
    return path;
}

static int ml_utime(vfs_handle_struct *handle, connection_struct *conn, const char *path, struct utimbuf *times)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_UTIME(handle, conn, realVFSPath, times);
}

static BOOL ml_symlink(vfs_handle_struct *handle, connection_struct *conn, const char *oldpath, const char *newpath)
{
    pstring oldMediaPath;
    pstring newMediaPath;
    get_real_vfs_path(handle, oldpath, oldMediaPath);
    get_real_vfs_path(handle, newpath, newMediaPath);
    
    return SMB_VFS_NEXT_SYMLINK(handle, conn, oldMediaPath, newMediaPath);
}

static BOOL ml_readlink(vfs_handle_struct *handle, connection_struct *conn, const char *path, char *buf, size_t bufsiz)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_READLINK(handle, conn, realVFSPath, buf, bufsiz);
}

static int ml_link(vfs_handle_struct *handle, connection_struct *conn, const char *oldpath, const char *newpath)
{
    pstring oldMediaPath;
    pstring newMediaPath;
    get_real_vfs_path(handle, oldpath, oldMediaPath);
    get_real_vfs_path(handle, newpath, newMediaPath);
    
    return SMB_VFS_NEXT_LINK(handle, conn, oldMediaPath, newMediaPath);
}

static int ml_mknod(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode, SMB_DEV_T dev)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_MKNOD(handle, conn, realVFSPath, mode, dev);
}

static char *ml_realpath(vfs_handle_struct *handle, connection_struct *conn, const char *path, char *resolved_path)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_REALPATH(handle, conn, realVFSPath, resolved_path);
}

static size_t ml_get_nt_acl(vfs_handle_struct *handle, files_struct *fsp, const char *path, uint32 security_info, struct security_descriptor_info **ppdesc)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_GET_NT_ACL(handle, fsp, realVFSPath, security_info, ppdesc);
}

static BOOL ml_set_nt_acl(vfs_handle_struct *handle, files_struct *fsp, const char *path, uint32 security_info_sent, struct security_descriptor_info *psd)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_SET_NT_ACL(handle, fsp, realVFSPath, security_info_sent, psd);
}

static int ml_chmod_acl(vfs_handle_struct *handle, connection_struct *conn, const char *path, mode_t mode)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_CHMOD_ACL(handle, conn, realVFSPath, mode);
}

static SMB_ACL_T ml_sys_acl_get_file(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_ACL_TYPE_T type)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_SYS_ACL_GET_FILE(handle, conn, realVFSPath, type);
}

static int ml_sys_acl_set_file(vfs_handle_struct *handle, connection_struct *conn, const char *path, SMB_ACL_TYPE_T acltype, SMB_ACL_T theacl)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_SYS_ACL_SET_FILE(handle, conn, realVFSPath, acltype, theacl);
}

static int ml_sys_acl_delete_def_file(vfs_handle_struct *handle, connection_struct *conn, const char *path)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_SYS_ACL_DELETE_DEF_FILE(handle, conn, realVFSPath);
}

static ssize_t ml_getxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, void *value, size_t size)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_GETXATTR(handle, conn, realVFSPath, name, value, size);
}

static ssize_t ml_lgetxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, void *value, size_t size)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_LGETXATTR(handle, conn, realVFSPath, name, value, size);
}

static ssize_t ml_listxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, char *list, size_t size)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_LISTXATTR(handle, conn, realVFSPath, list, size);
}

static ssize_t ml_llistxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, char *list, size_t size)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_LLISTXATTR(handle, conn, realVFSPath, list, size);
}

static int ml_removexattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_REMOVEXATTR(handle, conn, realVFSPath, name);
}

static int ml_lremovexattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_LREMOVEXATTR(handle, conn, realVFSPath, name);
}

static int ml_setxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, const void *value, size_t size, int flags)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_SETXATTR(handle, conn, realVFSPath, name, value, size, flags);
}

static int ml_lsetxattr(vfs_handle_struct *handle, struct connection_struct *conn,const char *path, const char *name, const void *value, size_t size, int flags)
{
    pstring realVFSPath;
    get_real_vfs_path(handle, path, realVFSPath);
    
    return SMB_VFS_NEXT_LSETXATTR(handle, conn, realVFSPath, name, value, size, flags);
}



/* VFS operations structure */

static vfs_op_tuple ml_op_tuples[] = {

    {SMB_VFS_OP(ml_connect),            SMB_VFS_OP_CONNECT,     SMB_VFS_LAYER_TRANSPARENT},
    
    /* Disk operations */
    {SMB_VFS_OP(ml_statvfs),            SMB_VFS_OP_STATVFS,     SMB_VFS_LAYER_TRANSPARENT},

    
    /* Directory operations */

    {SMB_VFS_OP(ml_opendir),            SMB_VFS_OP_OPENDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_readdir),            SMB_VFS_OP_READDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_seekdir),            SMB_VFS_OP_SEEKDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_telldir),            SMB_VFS_OP_TELLDIR,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_rewinddir),          SMB_VFS_OP_REWINDDIR,   SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_mkdir),              SMB_VFS_OP_MKDIR,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_rmdir),              SMB_VFS_OP_RMDIR,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_closedir),           SMB_VFS_OP_CLOSEDIR,    SMB_VFS_LAYER_TRANSPARENT},

    
    /* File operations */

    {SMB_VFS_OP(ml_open),               SMB_VFS_OP_OPEN,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_rename),             SMB_VFS_OP_RENAME,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_stat),               SMB_VFS_OP_STAT,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_lstat),              SMB_VFS_OP_LSTAT,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_unlink),             SMB_VFS_OP_UNLINK,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_chmod),              SMB_VFS_OP_CHMOD,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_chown),              SMB_VFS_OP_CHOWN,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_chdir),              SMB_VFS_OP_CHDIR,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_getwd),              SMB_VFS_OP_GETWD,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_utime),              SMB_VFS_OP_UTIME,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_symlink),            SMB_VFS_OP_SYMLINK,     SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_readlink),           SMB_VFS_OP_READLINK,    SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_link),               SMB_VFS_OP_LINK,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_mknod),              SMB_VFS_OP_MKNOD,       SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_realpath),           SMB_VFS_OP_REALPATH,    SMB_VFS_LAYER_TRANSPARENT},

    /* NT File ACL operations */

    {SMB_VFS_OP(ml_get_nt_acl),         SMB_VFS_OP_GET_NT_ACL,      SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_set_nt_acl),         SMB_VFS_OP_SET_NT_ACL,      SMB_VFS_LAYER_TRANSPARENT},

    /* POSIX ACL operations */

    {SMB_VFS_OP(ml_chmod_acl),                  SMB_VFS_OP_CHMOD_ACL,               SMB_VFS_LAYER_TRANSPARENT},

    {SMB_VFS_OP(ml_sys_acl_get_file),           SMB_VFS_OP_SYS_ACL_GET_FILE,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_sys_acl_set_file),           SMB_VFS_OP_SYS_ACL_SET_FILE,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_sys_acl_delete_def_file),    SMB_VFS_OP_SYS_ACL_DELETE_DEF_FILE, SMB_VFS_LAYER_TRANSPARENT},
    
    /* EA operations. */
    {SMB_VFS_OP(ml_getxattr),           SMB_VFS_OP_GETXATTR,            SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_lgetxattr),          SMB_VFS_OP_LGETXATTR,           SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_listxattr),          SMB_VFS_OP_LISTXATTR,           SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_llistxattr),         SMB_VFS_OP_LLISTXATTR,          SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_removexattr),        SMB_VFS_OP_REMOVEXATTR,         SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_lremovexattr),       SMB_VFS_OP_LREMOVEXATTR,        SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_setxattr),           SMB_VFS_OP_SETXATTR,            SMB_VFS_LAYER_TRANSPARENT},
    {SMB_VFS_OP(ml_lsetxattr),          SMB_VFS_OP_LSETXATTR,           SMB_VFS_LAYER_TRANSPARENT},

    {NULL,                      SMB_VFS_OP_NOOP,            SMB_VFS_LAYER_NOOP}
};

NTSTATUS init_module(void)
{
    NTSTATUS ret = smb_register_vfs(SMB_VFS_INTERFACE_VERSION, MH_MODULE_NAME, ml_op_tuples);

    if (!NT_STATUS_IS_OK(ret))
    {
        return ret;
    }

    vfs_ml_debug_level = debug_add_class(MH_MODULE_NAME);
    if (vfs_ml_debug_level == -1) 
    {
        vfs_ml_debug_level = DBGC_VFS;
        DEBUG(0, ("%s: Couldn't register custom debugging class!\n",
            "media_link::init_module"));
    } 
    else 
    {
        DEBUG(10, ("%s: Debug class number of '%s': %d\n", 
            "media_link::init_module", MH_MODULE_NAME, vfs_ml_debug_level));
    }

    return ret;
}






