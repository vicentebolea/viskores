#!/usr/bin/env bash

##============================================================================
##  The contents of this file are covered by the Viskores license. See
##  LICENSE.txt for details.
##
##  By contributing to this file, all contributors agree to the Developer
##  Certificate of Origin Version 1.1 (DCO 1.1) as stated in DCO.txt.
##============================================================================

##=============================================================================
##
##  Copyright (c) Kitware, Inc.
##  All rights reserved.
##  See LICENSE.txt for details.
##
##  This software is distributed WITHOUT ANY WARRANTY; without even
##  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
##  PURPOSE.  See the above copyright notice for more information.
##
##=============================================================================

set -x

WORKDIR="$1"
VERSION="$2"

shift 2

if [ ! -d "$WORKDIR" ] || [ -z "$VERSION" ]
then
  echo "[E] missing args: Invoke as .gitlab/ci/config/kokkos.sh <WORKDIR> <VERSION> [extra_args]"
  exit 1
fi

# Build and install Kokkos
curl -L "https://github.com/kokkos/kokkos/archive/refs/tags/$VERSION.tar.gz" \
  | tar -C "$WORKDIR" -xzf -

cmake -S "$WORKDIR/kokkos-$VERSION" -B "$WORKDIR/kokkos_build" \
  "-DCMAKE_BUILD_TYPE:STRING=release" \
  "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache" \
  "-DCMAKE_CXX_STANDARD:STRING=17" \
  "-DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON" \
  "-DKokkos_ENABLE_HIP:BOOL=ON" \
  "-DKokkos_ENABLE_HIP_RELOCATABLE_DEVICE_CODE:BOOL=OFF" \
  "-DKokkos_ENABLE_SERIAL:BOOL=ON" \
  $*

cmake --build "$WORKDIR/kokkos_build"
cmake --install "$WORKDIR/kokkos_build"
