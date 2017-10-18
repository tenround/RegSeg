// This file is part of RegSeg
//
// Copyright 2014-2017, Oscar Esteban <code@oscaresteban.es>
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#ifndef BSPLINE_FIELD_H_
#define BSPLINE_FIELD_H_

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <iomanip>

#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkRandomImageSource.h>
#include <itkNearestNeighborInterpolateImageFunction.h>
#include <itkBSplineInterpolateImageFunction.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkThresholdImageFilter.h>
#include <itkRandomImageSource.h>
#include <itkGaussianImageSource.h>
#include <itkImageRandomNonRepeatingIteratorWithIndex.h>
#include <itkSmoothingRecursiveGaussianImageFilter.h>
#include <itkStatisticsImageFilter.h>
#include <itkMinimumMaximumImageCalculator.h>
#include <itkMultiplyImageFilter.h>
#include <itkSubtractImageFilter.h>
#include <itkResampleImageFilter.h>
#include <itkMaskImageFilter.h>
#include <itkWarpImageFilter.h>
#include <itkOrientImageFilter.h>


#include "BSplineSparseMatrixTransform.h"
#include "DisplacementFieldComponentsFileWriter.h"
#include "DisplacementFieldFileWriter.h"

#include <itkMesh.h>
#include <itkVTKPolyDataReader.h>
#include "rstkVTKPolyDataWriter.h"

namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;

static const unsigned int DIMENSION = 3;
static const unsigned int NUM_THREADS = 4;

typedef itk::Image<float, DIMENSION>                         ChannelType;
typedef typename ChannelType::Pointer                        ChannelPointer;
typedef itk::ImageFileReader<ChannelType>                    ReaderType;
typedef typename ReaderType::Pointer                         ReaderPointer;
typedef itk::ImageFileWriter<ChannelType>                    WriterType;
typedef typename WriterType::Pointer                         WriterPointer;

typedef itk::Image< unsigned char, DIMENSION >               MaskType;
typedef typename MaskType::Pointer                           MaskPointer;
typedef itk::ImageFileWriter<MaskType>                       MaskWriter;
typedef itk::BinaryThresholdImageFilter
		                           < ChannelType, MaskType > Binarize;


typedef float                                                ScalarType;
typedef rstk::BSplineSparseMatrixTransform
		                          < ScalarType, DIMENSION>   Transform;
typedef Transform::Pointer                                   TPointer;
typedef typename Transform::FieldType                        FieldType;
typedef typename FieldType::Pointer                          FieldPointer;
typedef typename Transform::VectorType                       VectorType;

typedef itk::RandomImageSource< ChannelType >                RandomCoeffSource;
typedef typename RandomCoeffSource::Pointer                  RandomCoeffSourcePointer;

typedef itk::ImageRandomNonRepeatingIteratorWithIndex
		                             <FieldType>             RandomIterator;
typedef itk::ImageFileWriter< FieldType >                    CoefficientsWriterType;
typedef CoefficientsWriterType::Pointer                      CoefficientsWriterPointer;


typedef itk::SmoothingRecursiveGaussianImageFilter< FieldType >
	 	 	 	 	 	 	 	 	 	 	 	 	 	 SmoothingFilterType;
typedef typename SmoothingFilterType::Pointer			 SmoothingFilterPointer;
typedef typename SmoothingFilterType::SigmaArrayType     SigmaArrayType;

typedef itk::MultiplyImageFilter< FieldType >           MultiplyFilter;
typedef typename MultiplyFilter::Pointer                       MultiplyPointer;

typedef itk::SubtractImageFilter< FieldType >           SubtractFilter;
typedef typename SubtractFilter::Pointer                       SubtractPointer;

typedef itk::ThresholdImageFilter< ChannelType >               ThresholdFilter;
typedef typename ThresholdFilter::Pointer                      ThresholdPointer;

typedef typename itk::DisplacementFieldTransform
		                           < ScalarType, DIMENSION>    DisplacementFieldTransformType;
typedef typename DisplacementFieldTransformType::Pointer       DisplacementFieldTransformPointer;

typedef itk::MaskImageFilter< ChannelType, MaskType, ChannelType > MaskFilter;

typedef itk::ResampleImageFilter< ChannelType, ChannelType, ScalarType >    ResampleFilter;
typedef typename ResampleFilter::Pointer                       ResamplePointer;
typedef itk::BSplineInterpolateImageFunction< ChannelType, ScalarType >    ResampleInterpolator;

typedef itk::ResampleImageFilter
		                 < MaskType, MaskType, ScalarType >    ResampleMaskFilter;
typedef typename ResampleMaskFilter::Pointer                   ResampleMaskPointer;
typedef itk::NearestNeighborInterpolateImageFunction< MaskType, ScalarType >    ResampleMaskInterpolator;

typedef itk::WarpImageFilter
	    < MaskType, MaskType, FieldType >                      WarpMaskFilter;
typedef typename WarpMaskFilter::Pointer                       WarpMaskFilterPointer;
typedef itk::NearestNeighborInterpolateImageFunction< MaskType >    WarpMaskInterpolator;

typedef itk::WarpImageFilter
	    < ChannelType, ChannelType, FieldType >                WarpFilter;
typedef typename WarpFilter::Pointer                           WarpFilterPointer;
typedef itk::BSplineInterpolateImageFunction< ChannelType >    WarpInterpolator;

typedef rstk::DisplacementFieldComponentsFileWriter<FieldType> ComponentsWriter;
typedef rstk::DisplacementFieldFileWriter< FieldType >         FieldWriter;

typedef itk::Matrix< float, DIMENSION, DIMENSION >             MatrixType;
typedef itk::Mesh< float, DIMENSION >                          MeshType;
typedef typename MeshType::Pointer                             MeshPointer;
typedef typename MeshType::PointType                           MeshPointType;
typedef typename MeshType::PointsContainer::Iterator           PointsIterator;
typedef itk::VTKPolyDataReader<MeshType>                       MeshReaderType;
typedef typename MeshReaderType::Pointer                       MeshReaderPointer;
typedef rstk::VTKPolyDataWriter<MeshType>                      MeshWriterType;
typedef typename MeshWriterType::Pointer                       MeshWriterPointer;

typedef itk::OrientImageFilter< ChannelType, ChannelType > OrientFilter;
typedef itk::OrientImageFilter< MaskType, MaskType > OrientMaskFilter;


int main(int argc, char *argv[]);

#endif /* BSPLINE_FIELD_H_ */
