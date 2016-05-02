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

#include "vtkImageData.h"
#include "vtkMath.h"
#include "vtkMatrix3x3.h"
#include "vtkMatrix4x4.h"
#include "vtkNew.h"
#include "vtkPiecewiseFunction.h"
#include "vtkPolyData.h"
#include "vtkCudaReconstructionFilter.h"
#include "vtkStructuredGrid.h"
#include "vtkTransform.h"
#include "vtkTransformFilter.h"
#include "vtkUnstructuredGrid.h"
#include "vtkXMLImageDataReader.h"
#include "vtkXMLStructuredGridReader.h"
#include "vtkXMLStructuredGridWriter.h"

#include <vtksys/CommandLineArguments.hxx>
#include <vtksys/SystemTools.hxx>

#include "ReconstructionData.h"

#include <string>

//-----------------------------------------------------------------------------
// READ ARGUMENTS
//-----------------------------------------------------------------------------
std::vector<int> g_gridDims;
std::vector<double> g_gridSpacing;
std::vector<double> g_gridOrigin;
std::vector<double> g_gridVecX;
std::vector<double> g_gridVecY;
std::vector<double> g_gridVecZ;
std::string g_outputGridFilename;
std::string g_pathFolder; // Path to the folder which contains all data
std::string g_depthMapContainer = "vtiList.txt"; // File which contains all path of depth map
std::string g_KRTContainer = "kList.txt"; // File which contains all path of KRT matrix ofr each depth map
double rayPotentialThick = 2; // Define parameter 'thick' on ray potential function when cuda is using
double rayPotentialRho = 0.8; // Define parameter 'rho' on ray potential function when cuda is using
double rayPotentialEta = 0.03;
double rayPotentialDelta = 0.3;
double thresholdBestCost = 0;
double thresholdUniqueness = 0;
bool noCuda = false; // Determine if the algorithm reconstruction is launched on GPU (with cuda) or CPU (without cuda)
bool verbose = false; // Display debug information during execution

//-----------------------------------------------------------------------------
// FILLED ATTRIBUTES
//-----------------------------------------------------------------------------
std::vector<std::string> g_depthMapPathList; // Contains all depth map path
std::vector<std::string> g_KRTPathList; // Contains all KRT matrix path
std::vector<ReconstructionData*> g_dataList; // Contains all read depth map and matrix
std::string g_globalKRTDFilePath;
std::string g_globalVTIFilePath;
vtkMatrix4x4* g_gridMatrix;

//-----------------------------------------------------------------------------
// FUNCTIONS
//-----------------------------------------------------------------------------
bool ReadArguments(int argc, char ** argv);
bool AreVectorsOrthogonal();
void CreateGridMatrixFromInput();
std::vector<std::string> &SplitString(const std::string &s, char delim, std::vector<std::string> &elems);
void ShowInformation(std::string message);
void ShowFilledParameters();


//cudareconstruction.exe --rayThick 0.08 --rayRho 0.8 --rayEta 0.03 --rayDelta 0.3 --threshBestCost 0.3 --gridDims 100 100 100 --gridSpacing 0.0348 0.0391 0.0342 --gridOrigin -2.29 -2.24 -2.2 --gridVecX 1 0 0 --gridVecY 0 1 0 --gridVecZ 0 0 1 --dataFolder C:\Dev\nda\TRG\DataSonia2 --outputGridFilename C:\Dev\nda\TRG\DataSonia2\output.vts
//-----------------------------------------------------------------------------
/* Main function */
int main(int argc, char ** argv)
{
  if (!ReadArguments(argc, argv))
    {
    return EXIT_FAILURE;
    }

  ShowInformation("---START---");

  ShowFilledParameters();

  // Create grid matrix from VecXYZ
  CreateGridMatrixFromInput();

  // Generate grid from arguments
  vtkNew<vtkImageData> grid;
  grid->SetDimensions(&g_gridDims[0]);
  grid->SetSpacing(&g_gridSpacing[0]);
  grid->SetOrigin(&g_gridOrigin[0]);

  ShowInformation("** Launch reconstruction...");

  std::string dmapGlobalFile = g_pathFolder + "\\" + g_depthMapContainer;
  std::string krtGlobalFile = g_pathFolder + "\\" + g_KRTContainer;

  // Launch reconstruction process
  vtkNew<vtkCudaReconstructionFilter> cudaReconstructionFilter;
  if (noCuda)
    cudaReconstructionFilter->UseCudaOff();
  else
    cudaReconstructionFilter->UseCudaOn();
  cudaReconstructionFilter->SetFilePathKRTD(krtGlobalFile.c_str());
  cudaReconstructionFilter->SetFilePathVTI(dmapGlobalFile.c_str());
  cudaReconstructionFilter->SetRayPotentialRho(rayPotentialRho);
  cudaReconstructionFilter->SetRayPotentialThickness(rayPotentialThick);
  cudaReconstructionFilter->SetRayPotentialEta(rayPotentialEta);
  cudaReconstructionFilter->SetRayPotentialDelta(rayPotentialDelta);
  cudaReconstructionFilter->SetThresholdBestCost(thresholdBestCost);
  cudaReconstructionFilter->SetInputData(grid.Get());
  cudaReconstructionFilter->SetGridMatrix(g_gridMatrix);
  cudaReconstructionFilter->Update();

  double time = cudaReconstructionFilter->GetExecutionTime();
  std::string message = "Execution time : " + std::to_string(time) + " s";
  ShowInformation(message);

  ShowInformation("** Apply grid matrix to the reconstruction output...");

  vtkNew<vtkTransform> transform;
  transform->SetMatrix(g_gridMatrix);
  vtkNew<vtkTransformFilter> transformFilter;
  transformFilter->SetInputConnection(cudaReconstructionFilter->GetOutputPort());
  transformFilter->SetTransform(transform.Get());
  transformFilter->Update();
  vtkStructuredGrid* outputGrid = vtkStructuredGrid::SafeDownCast(transformFilter->GetOutput());

  ShowInformation("** Save output...");
  ShowInformation("Output path : " + g_outputGridFilename);

  vtkNew<vtkXMLStructuredGridWriter> gridWriter;
  gridWriter->SetFileName(g_outputGridFilename.c_str());
  gridWriter->SetInputData(outputGrid);
  gridWriter->Write();

  // Clean pointers
  g_gridMatrix->Delete();
  g_dataList.clear();

  ShowInformation("---END---");

  return EXIT_SUCCESS;
}

//-----------------------------------------------------------------------------
/* Read input argument and check if they are valid */
bool ReadArguments(int argc, char ** argv)
{
  bool help = false;

  vtksys::CommandLineArguments arg;
  arg.Initialize(argc, argv);
  typedef vtksys::CommandLineArguments argT;

  arg.AddArgument("--gridDims", argT::MULTI_ARGUMENT, &g_gridDims, "Input grid dimensions (required)");
  arg.AddArgument("--gridSpacing", argT::MULTI_ARGUMENT, &g_gridSpacing, "Input grid spacing (required)");
  arg.AddArgument("--gridOrigin", argT::MULTI_ARGUMENT, &g_gridOrigin, "Input grid origin (required)");
  arg.AddArgument("--gridVecX", argT::MULTI_ARGUMENT, &g_gridVecX, "Input grid direction X (required)");
  arg.AddArgument("--gridVecY", argT::MULTI_ARGUMENT, &g_gridVecY, "Input grid direction Y (required)");
  arg.AddArgument("--gridVecZ", argT::MULTI_ARGUMENT, &g_gridVecZ, "Input grid direction Z (required)");
  arg.AddArgument("--outputGridFilename", argT::SPACE_ARGUMENT, &g_outputGridFilename, "Output grid filename (required)");
  arg.AddArgument("--dataFolder", argT::SPACE_ARGUMENT, &g_pathFolder, "Folder which contains all data (required)");
  arg.AddArgument("--depthMapFile", argT::SPACE_ARGUMENT, &g_depthMapContainer, "File which contains all the depth map path(default vtiList.txt)");
  arg.AddArgument("--KRTFile", argT::SPACE_ARGUMENT, &g_KRTContainer, "File which contains all the KRTD path (default kList.txt)");
  arg.AddArgument("--rayThick", argT::SPACE_ARGUMENT, &rayPotentialThick, "Define the ray potential thickness threshold when cuda is using (default 2)");
  arg.AddArgument("--rayRho", argT::SPACE_ARGUMENT, &rayPotentialRho, "Define the ray potential rho when cuda is using (default 3)");
  arg.AddArgument("--rayEta", argT::SPACE_ARGUMENT, &rayPotentialEta, "0 < Eta < 1 : will be applied as a percentage of rho");
  arg.AddArgument("--rayDelta", argT::SPACE_ARGUMENT, &rayPotentialDelta, "It has to be superior to Thick");
  arg.AddArgument("--threshBestCost", argT::SPACE_ARGUMENT, &thresholdBestCost, "Define threshold that will be applied on depth map");
  arg.AddArgument("--threshUniqueness", argT::SPACE_ARGUMENT, &thresholdUniqueness, "Define threshold that will be applied on depth map");
  arg.AddBooleanArgument("--noCuda", &noCuda, "Use CPU");
  arg.AddBooleanArgument("--verbose", &verbose, "Use to display debug information (default false)");
  arg.AddBooleanArgument("--help", &help, "Print this help message");

  int result = arg.Parse();
  if (!result || help)
    {
    std::cout << arg.GetHelp() ;
    return false;
    }

  if (g_outputGridFilename == "" || g_depthMapContainer == "" || g_KRTContainer == "" ||
      rayPotentialDelta < rayPotentialThick || rayPotentialEta < 0 || rayPotentialEta > 1)
    {
    std::cerr << "Error arguments." << std::endl;
    std::cerr << arg.GetHelp();
    return false;
    }

  if (g_gridVecX.size() == 0)
  {
    g_gridVecX.push_back(1);
    g_gridVecX.push_back(0);
    g_gridVecX.push_back(0);
  }

  if (g_gridVecY.size() == 0)
  {
    g_gridVecY.push_back(0);
    g_gridVecY.push_back(1);
    g_gridVecY.push_back(0);
  }

  if (g_gridVecZ.size() == 0)
  {
    g_gridVecZ.push_back(0);
    g_gridVecZ.push_back(0);
    g_gridVecZ.push_back(1);
  }


  if (!AreVectorsOrthogonal())
    {
    std::cerr << "Given vectors are not orthogonals" << std::endl;
    return false;
    }

  return true;
}

//-----------------------------------------------------------------------------
/* Check if input vectors are orthogonals (gridVecX, gridVecY, gridVecZ) */
bool AreVectorsOrthogonal()
{
  double X[3] = { g_gridVecX[0], g_gridVecX[1], g_gridVecX[2] };
  double Y[3] = { g_gridVecY[0], g_gridVecY[1], g_gridVecY[2] };
  double Z[3] = { g_gridVecZ[0], g_gridVecZ[1], g_gridVecZ[2] };

  double XY = vtkMath::Dot(X, Y);
  double YZ = vtkMath::Dot(Y, Z);
  double ZX = vtkMath::Dot(Z, X);

  if (XY == 0 && YZ == 0 && ZX == 0)
    return true;
  return false;
}

//-----------------------------------------------------------------------------
/* Construct a vtkMatrix4x4 from grid vec X, Y and Z */
void CreateGridMatrixFromInput()
{
  vtkMatrix4x4* gridMatrix = vtkMatrix4x4::New();
  gridMatrix->Identity();

  // Fill matrix
  gridMatrix->SetElement(0, 0, g_gridVecX[0]);
  gridMatrix->SetElement(0, 1, g_gridVecX[1]);
  gridMatrix->SetElement(0, 2, g_gridVecX[2]);
  gridMatrix->SetElement(1, 0, g_gridVecY[0]);
  gridMatrix->SetElement(1, 1, g_gridVecY[1]);
  gridMatrix->SetElement(1, 2, g_gridVecY[2]);
  gridMatrix->SetElement(2, 0, g_gridVecZ[0]);
  gridMatrix->SetElement(2, 1, g_gridVecZ[1]);
  gridMatrix->SetElement(2, 2, g_gridVecZ[2]);

  g_gridMatrix = gridMatrix;

}

//-----------------------------------------------------------------------------
/* Split a string from a delimiter char and return a vector of extracted words */
std::vector<std::string> &SplitString(const std::string &s, char delim,
                                      std::vector<std::string> &elems)
{
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim))
    {
    elems.push_back(item);
    }
  return elems;
}

//-----------------------------------------------------------------------------
/* Show information on console if we are on verbose mode */
void ShowInformation(std::string information)
{
  if (verbose)
    {
    std::cout << information << "\n" << std::endl;
    }
}

void ShowFilledParameters()
{
  if (!verbose)
    return;

  std::cout << std::endl;
  std::cout << "----------------------" << std::endl;
  std::cout << "** OUTPUT GRID :" << std::endl;
  std::cout << "----------------------" << std::endl;
  std::cout << "--- Dimensions : ( " << g_gridDims[0] << ", " << g_gridDims[1] << ", " << g_gridDims[2] << " )" << std::endl;
  std::cout << "--- Spacing    : ( " << g_gridSpacing[0] << ", " << g_gridSpacing[1] << ", " << g_gridSpacing[2] << " )" << std::endl;
  std::cout << "--- Origin     : ( " << g_gridOrigin[0] << ", " << g_gridOrigin[1] << ", " << g_gridOrigin[2] << " )" << std::endl;
  std::cout << "--- Nb voxels  : " << g_gridDims[0] * g_gridDims[1] * g_gridDims[2] << std::endl;
  std::cout << "--- Real volume size : ( " << g_gridDims[0] * g_gridSpacing[0] << ", " << g_gridDims[1] * g_gridSpacing[1] << ", " << g_gridDims[2] * g_gridSpacing[2] << ")" << std::endl;
  std::cout << "--- Matrix :" << std::endl;
    std::string l1 = "  " + std::to_string(g_gridVecX[0]) + "  " + std::to_string(g_gridVecY[0]) + "  " + std::to_string(g_gridVecZ[0]) + "\n";
    std::string l2 = "  " + std::to_string(g_gridVecX[1]) + "  " + std::to_string(g_gridVecY[1]) + "  " + std::to_string(g_gridVecZ[1]) + "\n";
    std::string l3 = "  " + std::to_string(g_gridVecX[2]) + "  " + std::to_string(g_gridVecY[2]) + "  " + std::to_string(g_gridVecZ[2]) + "\n";
  std::cout << l1 << std::endl;
  std::cout << l2 << std::endl;
  std::cout << l3 << std::endl;
  std::cout << "----------------------" << std::endl;
  std::cout << "** DEPTH MAP :" << std::endl;
  std::cout << "----------------------" << std::endl;
  std::cout << "--- Threshold for BestCost  : " << std::to_string(thresholdBestCost) << std::endl;
  std::cout << "--- Threshold for Uniqueness : " << std::to_string(thresholdUniqueness) << std::endl;
  std::cout << "----------------------" << std::endl;
  std::cout << "** CUDA :" << std::endl;
  std::cout << "----------------------" << std::endl;
  std::cout << "--- Thickness ray potential : " << rayPotentialThick << std::endl;
  std::cout << "--- Rho ray potential :       " << rayPotentialRho << std::endl;
  std::cout << "--- Eta ray potential :       " << rayPotentialEta << std::endl;
  std::cout << "--- Delta ray potential :     " << rayPotentialDelta << std::endl;
  std::cout << "--- Use cuda :                " << !noCuda << std::endl;
  std::cout << std::endl;
  std::cout << std::endl;
}