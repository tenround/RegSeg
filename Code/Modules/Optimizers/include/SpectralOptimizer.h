// --------------------------------------------------------------------------------------
// File:          SpectralOptimizer.h
// Date:          Jul 31, 2013
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

#ifndef SPECTRALOPTIMIZER_H_
#define SPECTRALOPTIMIZER_H_

#include <boost/program_options.hpp>

#include <itkObjectToObjectOptimizerBase.h>

#include <itkWindowConvergenceMonitoringFunction.h>
#include <vector>
#include <itkForwardFFTImageFilter.h>
#include <itkInverseFFTImageFilter.h>
#include <itkRealToHalfHermitianForwardFFTImageFilter.h>
#include <itkHalfHermitianToRealInverseFFTImageFilter.h>

#include <itkImageIteratorWithIndex.h>
#include <itkImageAlgorithm.h>
#include <itkMultiplyImageFilter.h>
#include <itkAddImageFilter.h>

#include "rstkMacro.h"
#include "ConfigurableObject.h"
#include "BSplineSparseMatrixTransform.h"

using namespace itk;
namespace bpo = boost::program_options;


namespace rstk
{
/**
 * \class SpectralOptimizer
 *  \brief Gradient descent optimizer.
 *
 * GradientDescentOptimizer implements a simple gradient descent optimizer.
 * At each iteration the current deformation field is updated according:
 * \f[
 *        u^{t+1} = \mathcal{FT^{-1}}
 * \f]
 */

template< typename TFunctional >
class SpectralOptimizer:
		public ObjectToObjectOptimizerBase,
		public ConfigurableObject
{
public:
	/** Standard class typedefs and macros */
	typedef SpectralOptimizer           Self;
	typedef ObjectToObjectOptimizerBase                Superclass;
	typedef itk::SmartPointer<Self>                    Pointer;
	typedef itk::SmartPointer< const Self >            ConstPointer;
	typedef ConfigurableObject                         SettingsClass;
	typedef typename SettingsClass::SettingsMap        SettingsMap;
	typedef typename SettingsClass::SettingsDesc       SettingsDesc;

	itkTypeMacro( SpectralOptimizer, ObjectToObjectOptimizerBase ); // Run-time type information (and related methods)

	/** Metric type over which this class is templated */
	typedef TFunctional                                             FunctionalType;
	itkStaticConstMacro( Dimension, unsigned int, FunctionalType::Dimension );

	/** Codes of stopping conditions. */
	typedef enum {
		MAXIMUM_NUMBER_OF_ITERATIONS,
		COSTFUNCTION_ERROR,
		UPDATE_PARAMETERS_ERROR,
		STEP_TOO_SMALL,
		QUASI_NEWTON_STEP_ERROR,
		CONVERGENCE_CHECKER_PASSED,
		OTHER_ERROR
	} StopConditionType;

	/** Stop condition return string type */
	typedef std::string                                             StopConditionReturnStringType;

	/** Stop condition internal string type */
	typedef std::ostringstream                                      StopConditionDescriptionType;

	typedef size_t                                                  SizeValueType;

	/** Functional definitions */
	typedef typename FunctionalType::Pointer                        FunctionalPointer;
	typedef typename FunctionalType::MeasureType                    MeasureType;

	typedef typename FunctionalType::PointType                      PointType;
	typedef typename FunctionalType::VectorType                     VectorType;
	typedef typename FunctionalType::PointValueType                 PointValueType;

	typedef typename FunctionalType::TransformType                  TransformType;
	typedef typename TransformType::Pointer                         TransformPointer;
	typedef typename TransformType::CoefficientsImageType           CoefficientsImageType;
	typedef typename TransformType::CoeffImagePointer               CoefficientsImagePointer;
	typedef typename TransformType::CoefficientsImageArray          CoefficientsImageArray;
	typedef typename TransformType::ParametersType                  ParametersType;
	typedef typename TransformType::FieldType                       FieldType;
	typedef typename TransformType::FieldPointer                    FieldPointer;
	typedef typename TransformType::FieldConstPointer               FieldConstPointer;
	typedef typename TransformType::SizeType                        ControlPointsGridSizeType;

	typedef itk::MultiplyImageFilter<CoefficientsImageType, CoefficientsImageType, CoefficientsImageType> MultiplyFilterType;
	typedef itk::AddImageFilter<CoefficientsImageType, CoefficientsImageType, CoefficientsImageType>      AddFilterType;

	typedef BSplineSparseMatrixTransform
			                      < PointValueType, Dimension, 3u > SplineTransformType;
	typedef typename SplineTransformType::Pointer                   SplineTransformPointer;

	typedef itk::RealToHalfHermitianForwardFFTImageFilter
			                          <CoefficientsImageType>       FFTType;
	typedef typename FFTType::Pointer                               FFTPointer;
	typedef typename FFTType::OutputImageType                       FTDomainType;
	typedef typename FTDomainType::Pointer                          FTDomainPointer;
	typedef typename FTDomainType::PixelType                        ComplexType;

	/** Internal computation value type */
	typedef typename ComplexType::value_type                        InternalComputationValueType;
	typedef itk::Vector< InternalComputationValueType, Dimension >  InternalVectorType;
	typedef itk::Image< InternalVectorType, Dimension >             InternalVectorFieldType;
	typedef typename InternalVectorFieldType::Pointer               InternalVectorFieldPointer;
	typedef itk::ContinuousIndex< InternalComputationValueType, Dimension>
																	ContinuousIndexType;

	typedef itk::HalfHermitianToRealInverseFFTImageFilter
			        <FTDomainType, CoefficientsImageType>           IFFTType;
	typedef typename IFFTType::Pointer                              IFFTPointer;


	typedef itk::Image< InternalComputationValueType, Dimension >   RealPartType;
	typedef itk::Vector< ComplexType, Dimension >                   ComplexFieldValue;
	typedef itk::Image< ComplexFieldValue, Dimension >              ComplexFieldType;
	typedef typename ComplexFieldType::Pointer                      ComplexFieldPointer;


	/** Type for the convergence checker */
	typedef itk::Function::WindowConvergenceMonitoringFunction<MeasureType>	         ConvergenceMonitoringType;



	/** Accessors for Functional */
	itkGetObjectMacro( Functional, FunctionalType );
	itkSetObjectMacro( Functional, FunctionalType );

	itkSetMacro(LearningRate, InternalComputationValueType);               // Set the learning rate
	itkGetConstReferenceMacro(LearningRate, InternalComputationValueType); // Get the learning rate

	itkSetMacro(Coefficients, CoefficientsImageArray);
	itkGetConstMacro(Coefficients, CoefficientsImageArray);

	itkSetMacro(DerivativeCoefficients, CoefficientsImageArray);
	itkGetConstMacro(DerivativeCoefficients, CoefficientsImageArray);

	/** Minimum convergence value for convergence checking.
	 *  The convergence checker calculates convergence value by fitting to
	 *  a window of the Functional profile. When the convergence value reaches
	 *  a small value, it would be treated as converged.
	 *
	 *  The default m_MinimumConvergenceValue is set to 1e-8 to pass all
	 *  tests. It is suggested to use 1e-6 for less stringent convergence
	 *  checking.
	 */
	itkSetMacro(MinimumConvergenceValue, InternalComputationValueType);

	/** Window size for the convergence checker.
	 *  The convergence checker calculates convergence value by fitting to
	 *  a window of the Functional (metric value) profile.
	 *
	 *  The default m_ConvergenceWindowSize is set to 50 to pass all
	 *  tests. It is suggested to use 10 for less stringent convergence
	 *  checking.
	 */
	itkSetMacro(ConvergenceWindowSize, SizeValueType);

	/** Get current convergence value */
	itkGetConstReferenceMacro( ConvergenceValue, InternalComputationValueType );

	/** Flag. Set to have the optimizer track and return the best
	 *  best metric value and corresponding best parameters that were
	 *  calculated during the optimization. This captures the best
	 *  solution when the optimizer oversteps or osciallates near the end
	 *  of an optimization.
	 *  Results are stored in m_CurrentMetricValue and in the assigned metric's
	 *  parameters, retrievable via optimizer->GetCurrentPosition().
	 *  This option requires additional memory to store the best
	 *  parameters, which can be large when working with high-dimensional
	 *  transforms such as DisplacementFieldTransform.
	 */
//	itkSetMacro(ReturnBestParametersAndValue, bool);
//	itkGetConstReferenceMacro(ReturnBestParametersAndValue, bool);
//	itkBooleanMacro(ReturnBestParametersAndValue);

	itkSetMacro( Alpha, InternalVectorType );
	itkGetConstMacro( Alpha, InternalVectorType );

	itkSetMacro( Beta, InternalVectorType );
	itkGetConstMacro( Beta, InternalVectorType );

	itkSetMacro( StepSize, InternalComputationValueType );
	itkGetConstMacro( StepSize, InternalComputationValueType );

	void SetAlpha(const InternalComputationValueType v ) {
		this->m_Alpha.Fill(v);
		this->Modified();
	}

	void SetBeta(const InternalComputationValueType v ) {
		this->m_Beta.Fill(v);
		this->Modified();
	}

	/** Get stop condition enum */
	itkGetConstReferenceMacro(StopCondition, StopConditionType);

	/** Set the number of iterations. */
	itkSetMacro(NumberOfIterations, SizeValueType);

	itkSetMacro( GridSize, ControlPointsGridSizeType );

	void SetGridSize( double val ) { this->m_GridSize.Fill(val); }

	/** Get the number of iterations. */
	itkGetConstReferenceMacro(NumberOfIterations, SizeValueType);

	/** Get the current iteration number. */
	itkGetConstMacro(CurrentIteration, SizeValueType);

	/** Start and run the optimization */
	void Start();

	void Stop(void);

	/** Get the reason for termination */
	const StopConditionReturnStringType GetStopConditionDescription() const;

	void Resume();


	MeasureType ComputeIterationChange();
	MeasureType GetCurrentRegularizationEnergy();
	MeasureType GetCurrentEnergy();

	itkGetConstMacro( CurrentValue, MeasureType );


	itkGetConstObjectMacro(CurrentDisplacementField, FieldType);

	itkSetMacro( DescriptorRecomputationFreq, SizeValueType );
	itkGetConstMacro( DescriptorRecomputationFreq, SizeValueType );

	itkSetMacro( UseDescriptorRecomputation, bool );
	itkGetConstMacro( UseDescriptorRecomputation, bool );


	static void AddOptions( SettingsDesc& opts );
protected:
	/** Manual learning rate to apply. It is overridden by
	 * automatic learning rate estimation if enabled. See main documentation.
	 */
	InternalComputationValueType  m_LearningRate;

	/** Minimum convergence value for convergence checking.
	 *  The convergence checker calculates convergence value by fitting to
	 *  a window of the Functional profile. When the convergence value reaches
	 *  a small value, such as 1e-8, it would be treated as converged.
	 */
	InternalComputationValueType m_MinimumConvergenceValue;

	/** Window size for the convergence checker.
	 *  The convergence checker calculates convergence value by fitting to
	 *  a window of the Functional (metric value) profile.
	 */
	SizeValueType m_ConvergenceWindowSize;


	/** Current convergence value. */
	InternalComputationValueType m_ConvergenceValue;

	/** The convergence checker. */
	typename ConvergenceMonitoringType::Pointer m_ConvergenceMonitoring;

	SpectralOptimizer();
	~SpectralOptimizer() {}

	void PrintSelf( std::ostream &os, itk::Indent indent ) const;

	void SpectralUpdate( CoefficientsImageArray parameters, const CoefficientsImageArray lambda, CoefficientsImageArray nextParameters, bool changeDirection = false );

	virtual void InitializeAuxiliarParameters( void ) = 0;
	virtual void SetUpdate() = 0;
	virtual void Iterate(void) = 0;
	virtual void ParseSettings();

	/* Common variables for optimization control and reporting */
	bool                          m_Stop;
	StopConditionType             m_StopCondition;
	StopConditionDescriptionType  m_StopConditionDescription;
	SizeValueType                 m_CurrentIteration;
	SizeValueType                 m_NumberOfIterations;
	ControlPointsGridSizeType     m_GridSize;
	bool                          m_DenominatorCached;
	SizeValueType                 m_DescriptorRecomputationFreq;
	bool                          m_UseDescriptorRecomputation;

//	/** Store the best value and related parameters */
//	MeasureType                  m_CurrentBestValue;
//	ParametersType               m_BestParameters;
//
//	/** Flag to control returning of best value and parameters. */
//	bool m_ReturnBestParametersAndValue;

	/** Particular parameter definitions from our method */
	InternalComputationValueType m_StepSize; // Step-size is tau in the formulations
	InternalVectorType m_Alpha;
	InternalVectorType m_Beta;


	CoefficientsImageArray m_Coefficients;
	CoefficientsImageArray m_NextCoefficients;
	CoefficientsImageArray m_DerivativeCoefficients;
	CoefficientsImageArray m_Denominator;

	MeasureType m_CurrentValue;

	FunctionalPointer m_Functional;
	MeasureType m_RegularizationEnergy;
	MeasureType m_CurrentTotalEnergy;
	bool m_RegularizationEnergyUpdated;

	FieldPointer m_LastField;
	FieldPointer m_CurrentDisplacementField;


	TransformPointer              m_Transform;

private:
	SpectralOptimizer( const Self & ); // purposely not implemented
	void operator=( const Self & ); // purposely not implemented

	void ApplyRegularizationTerm( ComplexFieldType* reference );
	void ApplyRegularizationComponent( size_t d, FTDomainType *reference );
	void InitializeParameters( void );
	void InitializeDenominator( itk::ImageBase<Dimension> *reference );
	void UpdateField();
}; // End of Class

} // End of namespace rstk

#ifndef ITK_MANUAL_INSTANTIATION
#include "SpectralOptimizer.hxx"
#endif


#endif /* SPECTRALOPTIMIZER_H_ */
