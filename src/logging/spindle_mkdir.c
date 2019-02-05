//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory
//
// Created by Marty McFadden, 'mcfadden8 at llnl dot gov'
// LLNL-CODE-733797
//
// All rights reserved.
//
// This file is part of UMAP.
//
// For details, see https://github.com/LLNL/umap
// Please also see the COPYRIGHT and LICENSE files for LGPL license.
//////////////////////////////////////////////////////////////////////////////

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

//#include "ldcs_api.h"
#include "spindle_debug.h"
#include "config.h"

#define MAX_PATH_LEN 1024
#if defined(USE_CLEANUP_PROC)
extern void add_cleanup_dir(const char *dir);
#endif

static int checkdir(char *path)
{
   struct stat buf;
   int result = stat(path, &buf);
   if (result == -1) {
      err_printf("spindle_mkdir failed because stat on existing directory %s failed: %s\n",
                   path, strerror(errno));
      return -1;
   }
   if (!S_ISDIR(buf.st_mode) || S_ISLNK(buf.st_mode)) {
      err_printf("spindle_mkdir failed because non-directory %s appeared in path during mkdir\n",
                   path);
      return -1;
   }
   if (buf.st_uid != geteuid()) {
      err_printf("spindle_mkdir failed because component %s was owned by %d rather than expected %d\n",
                   path, buf.st_uid, geteuid());
      return -1;
   }
   if (buf.st_gid != getegid()) {
      err_printf("spindle_mkdir failed because component %s had group %d rather than expected %d\n",
                   path, buf.st_gid, getegid());
      return -1;
   }
   if ((buf.st_mode & 0777) != 0700) {
      err_printf("spindle_mkdir failed because component %s had unexpected permissions %o\n",
                   path, buf.st_mode & 0777);
      return -1;
   }

   return 0;
}

int spindle_mkdir(char *orig_path)
{
   char path[MAX_PATH_LEN+1];
   int i, path_len, result, do_mkdir = 0, error;
   struct stat buf;
   char orig_char;

   debug_printf("spindle_mkdir on %s\n", orig_path);
   

   strncpy(path, orig_path, sizeof(path));
   path[MAX_PATH_LEN] = '\0';
   path_len = strlen(path);
   
   i = 0;
   while (path[i] == '/')
      i++;

   for (; i < path_len+1; i++) {
      if (path[i] != '/' && path[i] != '\0')
         continue;
      orig_char = path[i];
      path[i] = '\0';
      
      if (!do_mkdir) {
         //Run a stat on an existing path component.  As long as a directory
         //component already exists, we won't be too picky about its ownership.
         result = stat(path, &buf);
         if (result == -1) {
            error = errno;
            if (error == ENOENT) {
#if defined(USE_CLEANUP_PROC)
               add_cleanup_dir(path);
#endif
               do_mkdir = 1;
            }
            else {
               err_printf("spindle_mkdir failed to stat path component %s: %s\n",
                            path, strerror(error));
               return -1;
            }
         }
         if (!S_ISDIR(buf.st_mode) && !S_ISLNK(buf.st_mode)) {
            err_printf("spindle_mkdir failed because path component %s is not a directory or symlink\n",
                         path);
            return -1;            
         }
      }
      
      if (do_mkdir) {
         result = mkdir(path, 0700);
         if (result == -1) {
            error = errno;
            if (error != EEXIST) {
               err_printf("spindle_mkdir failed to make path component %s: %s\n", path, strerror(error));
               return -1;
            }
            //Someone created this path component while we were doing the mkdir.  
            //May be a race with other Spindle libraries.  Check that it's owned
            //by us with appropriate permissions.
            if (checkdir(path) == -1)
               return -1;
         }
      }
      path[i] = orig_char;

      if (path[i] == '/')
         while (path[i+1] == '/')
            i++;
   }

   if (!do_mkdir) {
      //We never did any mkdirs.  Ensure that the final directory in the existing path 
      // is exclusively ours.
      if (checkdir(path) == -1)
         return -1;
   }
   
   return 0;
}

