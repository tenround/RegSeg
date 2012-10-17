// --------------------------------------------------------------------------
// File:             GradientDescentEnergyOptimizer.cxx
// Date:             17/10/2012
// Author:           code@oscaresteban.es (Oscar Esteban, OE)
// Version:          0.1
// License:          BSD
// --------------------------------------------------------------------------
//
// Copyright (c) 2012, code@oscaresteban.es (Oscar Esteban)
// with Signal Processing Lab 5, EPFL (LTS5-EPFL)
// and Biomedical Image Technology, UPM (BIT-UPM)
// All rights reserved.
// 
// This file is part of ACWERegistration-Debug@Debug
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// * Neither the names of the LTS5-EFPL and the BIT-UPM, nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY Oscar Esteban ''AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL OSCAR ESTEBAN BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include "GradientDescentEnergyOptimizer.h"

namespace rstk {

/**
 * Default constructor
 */
GradientDescentEnergyOptimizer::GradientDescentEnergyOptimizer() {
	this->m_LearningRate = itk::NumericTraits<InternalComputationValueType>::One;
	this->m_MaximumStepSizeInPhysicalUnits = itk::NumericTraits<InternalComputationValueType>::Zero;
	this->m_MinimumConvergenceValue = 1e-8;
	this->m_ConvergenceWindowSize = 50;
}

GradientDescentEnergyOptimizer::~GradientDescentEnergyOptimizer() {}

void GradientDescentEnergyOptimizer
::PrintSelf(std::ostream &os, itk::Indent indent) const {
	Superclass::PrintSelf(os,indent);
	os << indent << "Learning rate:" << this->m_LearningRate << std::endl;
	os << indent << "MaximumStepSizeInPhysicalUnits: "
             << this->m_MaximumStepSizeInPhysicalUnits << std::endl;
}

void GradientDescentEnergyOptimizer::Start() {
	itkDebugMacro("GradientDescentEnergyOptimizer::Start()");

	/* Validate some settings */

	/* Estimate parameter scales, if needed */

	/* Initialize convergence checker */
	this->m_ConvergenceMonitoring = ConvergenceMonitoringType::New();
	this->m_ConvergenceMonitoring->SetWindowSize( this->m_ConvergenceWindowSize );

	/* Must call the superclass version for basic validation and setup */
	Superclass::Start();

//	if( this->m_ReturnBestParametersAndValue )	{
//		this->m_BestParameters = this->GetCurrentPosition( );
//		this->m_CurrentBestValue = NumericTraits< MeasureType >::max();
//	}

	this->m_CurrentIteration = 0;
	this->Resume();
}

void GradientDescentEnergyOptimizer::Stop() {

//	if( this->m_ReturnBestParametersAndValue )	{
//		this->GetMetric()->SetParameters( this->m_BestParameters );
//		this->m_CurrentEnergyValue = this->m_CurrentBestValue;
//	}
	Superclass::Stop();
}

void GradientDescentEnergyOptimizer::Resume() {
	this->m_StopConditionDescription.str("");
	this->m_StopConditionDescription << this->GetNameOfClass() << ": ";
	this->InvokeEvent( itk::StartEvent() );

	this->m_Stop = false;
	while( ! this->m_Stop )	{
		/* Compute metric value/derivative. */
		try	{
			/* m_Gradient will be sized as needed by metric. If it's already
			 * proper size, no new allocation is done. */
			this->m_Energy->GetValueAndDerivative( this->m_CurrentEnergyValue, this->m_Gradient );
		}
		catch ( itk::ExceptionObject & err ) {
			this->m_StopCondition = COSTFUNCTION_ERROR;
			this->m_StopConditionDescription << "Energy error during optimization";
			this->Stop();
			// Pass exception to caller
			throw err;
		}

		/* Check if optimization has been stopped externally.
		 * (Presumably this could happen from a multi-threaded client app?) */
		if ( this->m_Stop ) {
			this->m_StopConditionDescription << "Stop() called externally";
			break;
		}

		/* Check the convergence by WindowConvergenceMonitoringFunction.
		 */
		this->m_ConvergenceMonitoring->AddEnergyValue( this->m_CurrentEnergyValue );
		try
		{
			this->m_ConvergenceValue = this->m_ConvergenceMonitoring->GetConvergenceValue();
			if (this->m_ConvergenceValue <= this->m_MinimumConvergenceValue)
			{
				this->m_StopConditionDescription << "Convergence checker passed at iteration " << m_CurrentIteration << ".";
				this->m_StopCondition = CONVERGENCE_CHECKER_PASSED;
				this->Stop();
				break;
			}
		}
		catch(std::exception & e) {
			std::cerr << "GetConvergenceValue() failed with exception: " << e.what() << std::endl;
		}

		/* Advance one step along the gradient.
		 * This will modify the gradient and update the transform. */
		this->Iterate();

		/* Store best value and position */
		//if ( this->m_ReturnBestParametersAndValue && this->m_CurrentEnergyValue < this->m_CurrentBestValue )
		//{
		//	this->m_CurrentBestValue = this->m_CurrentEnergyValue;
		//	this->m_BestParameters = this->GetCurrentPosition( );
		//}

		/* Update and check iteration count */
		this->m_CurrentIteration++;

		if ( this->m_CurrentIteration >= this->m_NumberOfIterations ) {
			this->m_StopConditionDescription << "Maximum number of iterations (" << this->m_NumberOfIterations << ") exceeded.";
			this->m_StopCondition = MAXIMUM_NUMBER_OF_ITERATIONS;
			this->Stop();
			break;
		}
	} //while (!m_Stop)
}

void GradientDescentEnergyOptimizer::Iterate() {
	itkDebugMacro("Optimizer Iteration");

	/* Begin threaded gradient modification.
	 * Scale by gradient scales, then estimate the learning
	 * rate if options are set to (using the scaled gradient),
	 * then modify by learning rate. The m_Gradient variable
	 * is modified in-place. */
	this->ModifyGradientByScales();
	this->EstimateLearningRate();
	this->ModifyGradientByLearningRate();

	try {
		/* Pass gradient to transform and let it do its own updating */
		this->m_Energy->UpdateTransformParameters( this->m_Gradient );
	}
	catch ( itk::ExceptionObject & err ) {
		this->m_StopCondition = UPDATE_PARAMETERS_ERROR;
		this->m_StopConditionDescription << "UpdateTransformParameters error";
		this->Stop();
		// Pass exception to caller
		throw err;
	}

	this->InvokeEvent( itk::IterationEvent() );
}

}