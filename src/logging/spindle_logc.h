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

#if !defined(UMAP_SPINDLE_LOGC_H_)
#define UMAP_SPINDLE_LOGC_H_

#include <stdio.h>
#include <string.h>

extern int spindle_debug_prints;
extern char *spindle_debug_name;
extern FILE *spindle_debug_output_f;

extern void spindle_dump_on_error();

#define BASE_FILE (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/')+1 : __FILE__)

#define debug_printf(format, ...)                                       \
   do {                                                                 \
      if (spindle_debug_prints && spindle_debug_output_f) {             \
         fprintf(spindle_debug_output_f, "[%s.%d@%s:%u] %s - " format,  \
                 spindle_debug_name, getpid(),                          \
                 BASE_FILE, __LINE__, __func__, ## __VA_ARGS__);        \
         fflush(spindle_debug_output_f);                                \
      }                                                                 \
   } while (0)

#define debug_printf2(format, ...)                                      \
   do {                                                                 \
      if (spindle_debug_prints > 1 && spindle_debug_output_f) {         \
         fprintf(spindle_debug_output_f, "[%s.%d@%s:%u] %s - " format,  \
                 spindle_debug_name, getpid(),                          \
                 BASE_FILE, __LINE__, __func__, ## __VA_ARGS__);        \
         fflush(spindle_debug_output_f);                                \
      }                                                                 \
   } while (0)

#define debug_printf3(format, ...)                                      \
   do {                                                                 \
      if (spindle_debug_prints > 2 && spindle_debug_output_f) {         \
         fprintf(spindle_debug_output_f, "[%s.%d@%s:%u] %s - " format,  \
                 spindle_debug_name, getpid(),                          \
                 BASE_FILE, __LINE__, __func__, ## __VA_ARGS__);        \
         fflush(spindle_debug_output_f);                                \
      }                                                                 \
   } while (0)

#define bare_printf(format, ...)                                        \
   do {                                                                 \
      if (spindle_debug_prints && spindle_debug_output_f) {             \
         fprintf(spindle_debug_output_f, format, ## __VA_ARGS__);       \
         fflush(spindle_debug_output_f);                                \
      }                                                                 \
   } while (0)

#define bare_printf2(format, ...)                                       \
   do {                                                                 \
      if (spindle_debug_prints > 1 && spindle_debug_output_f) {         \
         fprintf(spindle_debug_output_f, format, ## __VA_ARGS__);       \
         fflush(spindle_debug_output_f);                                \
      }                                                                 \
   } while (0)

#define bare_printf3(format, ...)                                       \
   do {                                                                 \
      if (spindle_debug_prints > 2 && spindle_debug_output_f) {         \
         fprintf(spindle_debug_output_f, format, ## __VA_ARGS__);       \
         fflush(spindle_debug_output_f);                                \
      }                                                                 \
   } while (0)

#define err_printf(format, ...)                                         \
   do {                                                                 \
      if (spindle_debug_prints && spindle_debug_output_f) {             \
         fprintf(spindle_debug_output_f, "[%s.%d@%s:%u] - ERROR: "      \
                 format, spindle_debug_name, getpid(),                  \
                 BASE_FILE, __LINE__, ## __VA_ARGS__);                  \
         spindle_dump_on_error();                                       \
         fflush(spindle_debug_output_f);                                \
      }                                                                 \
   } while (0)

void init_spindle_debugging(int survive_exec);
void fini_spindle_debugging();
void reset_spindle_debugging();
int is_debug_fd(int fd);

#endif
