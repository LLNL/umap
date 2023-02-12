#############################################################################
# Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################
add_subdirectory(stream)
add_subdirectory(umapsort)
add_subdirectory(bfs)
if (UMAP_ENABLE_ZFP)
   add_subdirectory(zfp)
endif(UMAP_ENABLE_ZFP)
