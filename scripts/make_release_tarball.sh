#!/bin/bash
#############################################################################
# Copyright 2017-2019 Lawrence Livermore National Security, LLC and other
# UMAP Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: LGPL-2.1-only
#############################################################################

TAR_CMD=tar
VERSION=0.0.4

git archive --prefix=umap-${VERSION}/ -o umap-${VERSION}.tar HEAD 2> /dev/null

gzip umap-${VERSION}.tar
