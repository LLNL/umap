#!/bin/bash
#############################################################################
# Copyright 2017-2020 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################

TAR_CMD=tar
VERSION=1.0.0

git archive --prefix=umap-${VERSION}/ -o umap-${VERSION}.tar HEAD 2> /dev/null

gzip umap-${VERSION}.tar
