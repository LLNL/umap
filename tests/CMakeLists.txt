#############################################################################
# Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################
add_subdirectory(churn)
add_subdirectory(flush_buffer)
add_subdirectory(pfbenchmark)
add_subdirectory(multi_thread)
add_subdirectory(umap-sparsestore)
if (caliper_DIR)
   add_subdirectory(caliper_trace)
endif()
