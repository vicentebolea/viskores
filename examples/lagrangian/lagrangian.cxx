//============================================================================
//  The contents of this file are covered by the Viskores license. See
//  LICENSE.txt for details.
//
//  By contributing to this file, all contributors agree to the Developer
//  Certificate of Origin Version 1.1 (DCO 1.1) as stated in DCO.txt.
//============================================================================

//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================

#include "ABCfield.h"
#include <iostream>
#include <vector>
#include <viskores/Types.h>
#include <viskores/cont/ArrayHandle.h>
#include <viskores/cont/DataSet.h>
#include <viskores/cont/DataSetBuilderUniform.h>
#include <viskores/cont/Initialize.h>
#include <viskores/filter/flow/Lagrangian.h>

using namespace std;

viskores::cont::DataSet make3DUniformDataSet(double time)
{
  ABCfield field;

  double xmin, xmax, ymin, ymax, zmin, zmax;
  xmin = 0.0;
  ymin = 0.0;
  zmin = 0.0;

  xmax = 6.28;
  ymax = 6.28;
  zmax = 6.28;

  int dims[3] = { 16, 16, 16 };

  viskores::cont::DataSetBuilderUniform dsb;

  double xdiff = (xmax - xmin) / (dims[0] - 1);
  double ydiff = (ymax - ymin) / (dims[1] - 1);
  double zdiff = (zmax - zmin) / (dims[2] - 1);

  viskores::Id3 DIMS(dims[0], dims[1], dims[2]);
  viskores::Vec3f_64 ORIGIN(0, 0, 0);
  viskores::Vec3f_64 SPACING(xdiff, ydiff, zdiff);

  viskores::cont::DataSet dataset = dsb.Create(DIMS, ORIGIN, SPACING);

  int numPoints = dims[0] * dims[1] * dims[2];

  viskores::cont::ArrayHandle<viskores::Vec3f_64> velocityField;
  velocityField.Allocate(numPoints);

  int count = 0;
  for (int i = 0; i < dims[0]; i++)
  {
    for (int j = 0; j < dims[1]; j++)
    {
      for (int k = 0; k < dims[2]; k++)
      {
        double vec[3];
        double loc[3] = { i * xdiff + xmin, j * ydiff + ymax, k * zdiff + zmin };
        field.calculateVelocity(loc, time, vec);
        velocityField.WritePortal().Set(count, viskores::Vec3f_64(vec[0], vec[1], vec[2]));
        count++;
      }
    }
  }
  dataset.AddPointField("velocity", velocityField);
  return dataset;
}


int main(int argc, char** argv)
{
  auto opts =
    viskores::cont::InitializeOptions::DefaultAnyDevice | viskores::cont::InitializeOptions::Strict;
  viskores::cont::Initialize(argc, argv, opts);

  viskores::filter::flow::Lagrangian lagrangianFilter;
  lagrangianFilter.SetResetParticles(true);
  viskores::Float32 stepSize = 0.01f;
  lagrangianFilter.SetStepSize(stepSize);
  lagrangianFilter.SetWriteFrequency(10);
  for (int i = 0; i < 100; i++)
  {
    viskores::cont::DataSet inputData = make3DUniformDataSet((double)i * stepSize);
    lagrangianFilter.SetActiveField("velocity");
    viskores::cont::DataSet extractedBasisFlows = lagrangianFilter.Execute(inputData);
  }
  return 0;
}
