// Copyright(c) 2016, Kitware SAS
// www.kitware.fr
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met :
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation and
// / or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors
// may be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


// .NAME vtkCudaReconstructionFilter -
// .SECTION Description
//

#ifndef vtkCudaReconstructionFilter_h
#define vtkCudaReconstructionFilter_h

#include "vtkFiltersCoreModule.h" // For export macro
#include "vtkImageAlgorithm.h"

#include <vector>

class vtkDoubleArray;
class vtkImageData;
class vtkMatrix3x3;
class vtkMatrix4x4;

class vtkCudaReconstructionFilter : public vtkImageAlgorithm
{
public:
  static vtkCudaReconstructionFilter *New();
  vtkTypeMacro(vtkCudaReconstructionFilter,vtkImageAlgorithm);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Define the thickness threshold (X axis) of the ray potential function when using cuda
  vtkSetMacro(RayPotentialThickness, double);
  // Description:
  // Define the rho value (Y axis) of the ray potential function when using cuda
  vtkSetMacro(RayPotentialRho, double);
  // Description :
  // Define a percentage of rho value for supposed empty voxel between filled voxel and camera
  vtkSetMacro(RayPotentialEta, double);
  // Description :
  // Define accepted voxel before/after filled voxel
  vtkSetMacro(RayPotentialDelta, double);
  // Description :
  // Threshold that will be applied on vtkImageData depth map according to BestCost
  vtkSetMacro(ThresholdBestCost, double);
  // Description :
  // Entire path to access file that contains all krtd file names
  // krtd files have to be in the same folder as FilePathKRTD
  vtkSetStringMacro(FilePathKRTD);
  // Description :
  // Entire path to access file that contains all vti file names
  // vti files have to be in the same folder as FilePathVTI
  vtkSetStringMacro(FilePathVTI);

  //Description :
  // Get the execution time when update is launch (in seconds)
  vtkGetMacro(ExecutionTime, double);

  // Description
  // Define the matrix transform to orientate the output volume
  // to the right axis
  void SetGridMatrix(vtkMatrix4x4 *gridMatrix);

//BTX
protected:
  vtkCudaReconstructionFilter();
  ~vtkCudaReconstructionFilter();

  virtual int RequestData(vtkInformation *, vtkInformationVector **,
    vtkInformationVector *);
  virtual int RequestInformation(vtkInformation *, vtkInformationVector **,
    vtkInformationVector *);
  virtual int RequestUpdateExtent(vtkInformation *, vtkInformationVector **,
    vtkInformationVector *);


  int Compute(int gridDims[3], double gridOrig[3],
              double gridSpacing[3], vtkDoubleArray* outScalar);


  vtkMatrix4x4 *GridMatrix;
  double RayPotentialRho;
  double RayPotentialThickness;
  double RayPotentialEta;
  double RayPotentialDelta;
  double ThresholdBestCost;
  double ExecutionTime;
  const char* FilePathKRTD;
  const char* FilePathVTI;

private:
  vtkCudaReconstructionFilter(const vtkCudaReconstructionFilter&);  // Not implemented.
  void operator=(const vtkCudaReconstructionFilter&);  // Not implemented.

//ETX
};

#endif
