#############################################################################
# Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################
add_subdirectory(churn)
add_subdirectory(flush_buffer)
add_subdirectory(pfbenchmark)
add_subdirectory(multi_thread)
if (MARGO_ROOT)
   add_subdirectory(remote_lookup)
   add_subdirectory(remote_stream)
endif()
