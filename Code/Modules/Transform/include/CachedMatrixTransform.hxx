// --------------------------------------------------------------------------------------
// File:          CachedMatrixTransform.hxx
// Date:          Nov 25, 2014
// Author:        code@oscaresteban.es (Oscar Esteban)
// Version:       1.0 beta
// License:       GPLv3 - 29 June 2007
// Short Summary:
// --------------------------------------------------------------------------------------
//
// Copyright (c) 2013, code@oscaresteban.es (Oscar Esteban)
// with Signal Processing Lab 5, EPFL (LTS5-EPFL)
// and Biomedical Image Technology, UPM (BIT-UPM)
// All rights reserved.
//
// This file is part of ACWE-Reg
//
// ACWE-Reg is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// ACWE-Reg is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ACWE-Reg.  If not, see <http://www.gnu.org/licenses/>.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#ifndef CACHEDMATRIXTRANSFORM_HXX_
#define CACHEDMATRIXTRANSFORM_HXX_

#include "CachedMatrixTransform.h"
#include <itkGaussianKernelFunction.h>
#include <itkBSplineKernelFunction.h>
#include <itkBSplineDerivativeKernelFunction.h>
#include <itkProgressReporter.h>

#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkImageAlgorithm.h>

#include <vnl/algo/vnl_sparse_lu.h>
#include <vnl/vnl_copy.h>
#include <vnl/vnl_matrix.h>
#include <vcl_vector.h>

namespace rstk {

template< class TScalar, unsigned int NDimensions >
CachedMatrixTransform<TScalar,NDimensions>
::CachedMatrixTransform():
Superclass(),
m_NumberOfPoints(0),
m_UseImageOutput(false),
m_InterpolationMode(UNKNOWN) {
	for( size_t i = 0; i<Dimension; i++ ) {
		this->m_PointValues[i] = DimensionVector();
	}
}

template< class TScalar, unsigned int NDimensions >
inline typename CachedMatrixTransform<TScalar,NDimensions>::VectorType
CachedMatrixTransform<TScalar,NDimensions>
::GetPointValue( const size_t id ) const {
	VectorType ci;
	ci.Fill( 0.0 );

	for( size_t d = 0; d < Dimension; d++) {
		ci[d] = this->m_PointValues[d][id];
	}

	return ci;
}


template< class TScalar, unsigned int NDimensions >
void
CachedMatrixTransform<TScalar,NDimensions>
::SetPhysicalDomainInformation( const DomainBase* image ) {
	ContinuousIndexType o_idx;
	o_idx.Fill( -0.5 );

	this->m_ReferenceSize = image->GetLargestPossibleRegion().GetSize();
	this->m_ReferenceSpacing = image->GetSpacing();
	this->m_ReferenceOrigin = image->GetOrigin();

	ContinuousIndexType e_idx;
	for ( size_t dim=0; dim< Dimension; dim++ ) {
		e_idx[dim] = this->m_ReferenceSize[dim] - 0.5;
	}

	this->m_DomainDirection = image->GetDirection();

	image->TransformContinuousIndexToPhysicalPoint( o_idx, m_DomainExtent[0] );
	image->TransformContinuousIndexToPhysicalPoint( e_idx, m_DomainExtent[1] );

	for (size_t i = 0; i<Dimension; i++) {
		if( m_DomainExtent[1][i] < m_DomainExtent[0][i] ) {
			double tmp = m_DomainExtent[0][i];
			m_DomainExtent[0][i] = m_DomainExtent[1][i];
			m_DomainExtent[1][i] = tmp;
		}
	}
}


template< class TScalar, unsigned int NDimensions >
void
CachedMatrixTransform<TScalar,NDimensions>
::SetOutputReference( const DomainBase* image ) {
	if( this->m_InterpolationMode == POINTS_MODE ) {
		itkExceptionMacro(<< "trying to change iterpolation mode from scattered to regular.");
	} else {
		this->m_InterpolationMode = GRID_MODE;
	}

	this->m_UseImageOutput = true;
	VectorType zerov; zerov.Fill( 0.0 );

	FieldPointer newfield = FieldType::New();
	newfield->SetRegions( image->GetLargestPossibleRegion().GetSize() );
	newfield->SetOrigin( image->GetOrigin() );
	newfield->SetSpacing( image->GetSpacing() );
	newfield->SetDirection( image->GetDirection() );
	newfield->Allocate();
	newfield->FillBuffer( zerov );
	this->SetDisplacementField( newfield );

	this->m_NumberOfPoints = image->GetLargestPossibleRegion().GetNumberOfPixels();
	// Initialize off-grid positions
	PointType p;
	for( size_t i = 0; i < this->m_NumberOfPoints; i++ ) {
		image->TransformIndexToPhysicalPoint( image->ComputeIndex( i ), p );
		this->m_PointLocations.push_back( p );
	}
}

template< class TScalar, unsigned int NDimensions >
void CachedMatrixTransform<TScalar,NDimensions>
::SetOutputPoints( const PointsList points ){
	if( this->m_InterpolationMode ==  GRID_MODE) {
		itkExceptionMacro(<< "trying to change iterpolation mode from regular to scattered.");
	} else {
		this->m_InterpolationMode = POINTS_MODE;
	}

	this->m_PointLocations = points;
	this->m_NumberOfPoints = points.size();
	this->Modified();
}


template< class TScalar, unsigned int NDimensions >
typename CachedMatrixTransform<TScalar,NDimensions>::DimensionVector
CachedMatrixTransform<TScalar,NDimensions>
::Vectorize( const CoefficientsImageType* image ) {
	DimensionVector v( image->GetLargestPossibleRegion().GetNumberOfPixels() );
	v.fill(0.0);

	const ScalarType *cbuffer = image->GetBufferPointer();

	for( size_t row = 0; row<this->m_NumberOfParameters; row++ ) {
		v[row] = *( cbuffer + row );
	}
	return v;
}


template< class TScalar, unsigned int NDimensions >
typename CachedMatrixTransform<TScalar,NDimensions>::DimensionParametersContainer
CachedMatrixTransform<TScalar,NDimensions>
::VectorizeField( const FieldType* image ) {
	DimensionParametersContainer vectorized;

	for( size_t col = 0; col<Dimension; col++) {
		vectorized[col] = DimensionVector( image->GetLargestPossibleRegion().GetNumberOfPixels() );
		vectorized[col].fill(0.0);
	}

	VectorType v;
	const VectorType *cbuffer = image->GetBufferPointer();

	for( size_t row = 0; row<this->m_NumberOfParameters; row++ ) {
		v = *( cbuffer + row );
		for( size_t col = 0; col<Dimension; col++) {
			vectorized[col][row] = v[col];
		}
	}
	return vectorized;
}

}


#endif /* CACHEDMATRIXTRANSFORM_HXX_ */