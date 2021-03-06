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

#include "vtkCudaReconstructionFilter.h"

#include "vtkCellDataToPointData.h"
#include "vtkContourFilter.h"
#include "vtkImageData.h"
#include "vtkMath.h"
#include "vtkMatrix3x3.h"
#include "vtkMatrix4x4.h"
#include "vtkNew.h"
#include "vtkPiecewiseFunction.h"
#include "vtkPolyData.h"
#include "vtkPointData.h"
#include "vtkStructuredGrid.h"
#include "vtkTransform.h"
#include "vtkTransformFilter.h"
#include "vtkUnstructuredGrid.h"
#include "vtkXMLImageDataReader.h"
#include "vtkXMLImageDataWriter.h"
#include "vtkXMLStructuredGridReader.h"
#include "vtkXMLStructuredGridWriter.h"
#include "vtkXMLPolyDataWriter.h"
#include "vtkMetaImageWriter.h"

#include <vtksys/CommandLineArguments.hxx>
#include <vtksys/SystemTools.hxx>

#include <algorithm>
#include <string>

//-----------------------------------------------------------------------------
// READ ARGUMENTS
//-----------------------------------------------------------------------------
std::vector<int> g_gridDims;
std::vector<double> g_gridSpacing;
std::vector<double> g_gridOrigin;
std::vector<double> g_gridEnd;
std::vector<double> g_gridVecX;
std::vector<double> g_gridVecY;
std::vector<double> g_gridVecZ;
std::string g_outputGridFilename;
std::string g_outputMeshFilename;
std::string g_pathFolder; // Path to the folder which contains all data
std::string g_depthMapContainer = "vtiList.txt"; // File which contains all path of depth map
std::string g_KRTContainer = "kList.txt"; // File which contains all path of KRT matrix ofr each depth map
double rayPotentialThick = 2; // Define parameter 'thick' on ray potential function when cuda is using
double rayPotentialRho = 0.8; // Define parameter 'rho' on ray potential function when cuda is using
double rayPotentialEta = 0.03;
double rayPotentialDelta = 0.3;
double thresholdBestCost = 0.14;
double contourValue = 1.0;
bool verbose = false; // Display debug information during execution
bool writeSummaryFile = false; // Define if a file with all parameters will be write at the end of execution
bool forceCubicVoxel = false; // Force to set the voxel to have the same size on X, Y and Z

//-----------------------------------------------------------------------------
// FILLED ATTRIBUTES
//-----------------------------------------------------------------------------
double g_reconstructionExecutionTime;
double g_totalExecutionTime;

//-----------------------------------------------------------------------------
// FUNCTIONS
//-----------------------------------------------------------------------------
bool ReadArguments(int argc, char ** argv);
bool AreVectorsOrthogonal();
void CreateGridMatrixFromInput(vtkMatrix4x4* gridMatrix);
void ShowInformation(std::string message);
void ShowFilledParameters();
void WriteSummaryFile(std::string path, int argc, char** argv);


//cudareconstruction.exe --rayThick 0.08 --rayRho 0.8 --rayEta 0.03 --rayDelta 0.3 --threshBestCost 0.3 --gridDims 100 100 100 --gridSpacing 0.0348 0.0391 0.0342 --gridOrigin -2.29 -2.24 -2.2 --gridVecX 1 0 0 --gridVecY 0 1 0 --gridVecZ 0 0 1 --dataFolder C:\Dev\nda\TRG\DataSonia2 --outputGridFilename C:\Dev\nda\TRG\DataSonia2\output.vts
//--verbose --rayThick 0.027 --rayRho 0.8 --rayEta 0.03 --rayDelta 0.063 --threshBestCost 0.14 --gridDims 400 400 400 --gridSpacing 0.007 0.009 0.008 --gridOrigin - 1.48 - 1.94 - 2.19 --gridVecX 1 0 0 --gridVecY 0 1 0 --gridVecZ 0 0 1 --dataFolder C : \Dev\nda\TRG\DATA\160428 - DM_ZNCC_ref_view_step_5_low_res --outputGridFilename C : \Dev\nda\TRG\DATA\160428 - DM_ZNCC_ref_view_step_5_low_res\output.vts
//-----------------------------------------------------------------------------
/* Main function */
int main(int argc, char ** argv)
{
  clock_t start = clock();
  if (!ReadArguments(argc, argv))
    {
    return EXIT_FAILURE;
    }

  ShowInformation("---START---");

  ShowFilledParameters();

  // Create grid matrix from VecXYZ
  vtkNew<vtkMatrix4x4> g_gridMatrix;
  CreateGridMatrixFromInput(g_gridMatrix.Get());

  // Generate grid from arguments
  vtkNew<vtkImageData> grid;
  grid->SetDimensions(&g_gridDims[0]);
  grid->SetSpacing(&g_gridSpacing[0]);
  grid->SetOrigin(&g_gridOrigin[0]);

  ShowInformation("** Launch reconstruction...");
  std::string dmapGlobalFile = g_pathFolder + "/" + g_depthMapContainer;
  std::string krtGlobalFile = g_pathFolder + "/" + g_KRTContainer;

  // Launch reconstruction process
  vtkNew<vtkCudaReconstructionFilter> cudaReconstructionFilter;
  cudaReconstructionFilter->SetFilePathKRTD(krtGlobalFile.c_str());
  cudaReconstructionFilter->SetFilePathVTI(dmapGlobalFile.c_str());
  cudaReconstructionFilter->SetRayPotentialRho(rayPotentialRho);
  cudaReconstructionFilter->SetRayPotentialThickness(rayPotentialThick);
  cudaReconstructionFilter->SetRayPotentialEta(rayPotentialEta);
  cudaReconstructionFilter->SetRayPotentialDelta(rayPotentialDelta);
  cudaReconstructionFilter->SetThresholdBestCost(thresholdBestCost);
  cudaReconstructionFilter->SetInputData(grid.Get());
  cudaReconstructionFilter->SetGridMatrix(g_gridMatrix.Get());
  cudaReconstructionFilter->Update();

  g_reconstructionExecutionTime = cudaReconstructionFilter->GetExecutionTime();
  std::string message = "Reconstruction execution time : " + std::to_string(g_reconstructionExecutionTime) + " s";
  ShowInformation(message);


  ShowInformation("** Transform cell data to point data...");
  vtkNew<vtkCellDataToPointData> transformCellDataToPointData;
  transformCellDataToPointData->SetInputData(cudaReconstructionFilter->GetOutput());
  transformCellDataToPointData->PassCellDataOn();
  transformCellDataToPointData->Update();
  transformCellDataToPointData->GetOutput()->GetPointData()->SetActiveScalars("reconstruction_scalar");

  vtkNew<vtkMetaImageWriter> mIWriter;
  mIWriter->SetFileName("meta_image_volume.mha");
  mIWriter->SetInputData(transformCellDataToPointData->GetOutput());
  mIWriter->SetCompression(true);
  mIWriter->Write();


  vtkImageData* out = transformCellDataToPointData->GetImageDataOutput();
  out->GetPointData()->SetActiveScalars("reconstruction_scalar");


  ShowInformation("** Compute contour...");
  vtkNew<vtkContourFilter> contourFilter;
  contourFilter->SetInputData(out);
  contourFilter->SetNumberOfContours(1);
  contourFilter->SetValue(0, contourValue);
  contourFilter->Update();


  ShowInformation("** Apply grid matrix to the reconstruction output...");
  vtkNew<vtkTransform> transform;
  transform->SetMatrix(g_gridMatrix.Get());
  vtkNew<vtkTransformFilter> transformFilter;
  transformFilter->SetInputConnection(contourFilter->GetOutputPort());
  transformFilter->SetTransform(transform.Get());
  transformFilter->Update();


  ShowInformation("** Save mesh...");
  vtkNew<vtkXMLPolyDataWriter> writer;
  writer->SetFileName(g_outputMeshFilename.c_str());
  writer->SetInputData(transformFilter->GetOutput());
  writer->Write();

  ShowInformation("** Save volume...");
  transformFilter->SetInputConnection(cudaReconstructionFilter->GetOutputPort());
  transformFilter->Update();
  vtkStructuredGrid* outputGrid = vtkStructuredGrid::SafeDownCast(transformFilter->GetOutput());
  vtkNew<vtkXMLStructuredGridWriter> gridWriter;
  gridWriter->SetFileName(g_outputGridFilename.c_str());
  gridWriter->SetInputData(outputGrid);
  gridWriter->Write();


  g_totalExecutionTime = (double)(clock() - start) / CLOCKS_PER_SEC;
  if (writeSummaryFile)
    {
    ShowInformation("** Save summary file...");
    std::string filePath = g_pathFolder + "\\summary.txt";
    WriteSummaryFile(filePath, argc, argv);
    }

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
  arg.AddArgument("--gridVecX", argT::MULTI_ARGUMENT, &g_gridVecX, "Input grid direction X (default 1 0 0)");
  arg.AddArgument("--gridVecY", argT::MULTI_ARGUMENT, &g_gridVecY, "Input grid direction Y (default 0 1 0)");
  arg.AddArgument("--gridVecZ", argT::MULTI_ARGUMENT, &g_gridVecZ, "Input grid direction Z (default 0 0 1)");
  arg.AddArgument("--outputGridFilename", argT::SPACE_ARGUMENT, &g_outputGridFilename, "Output grid filename (.vts) (required)");
  arg.AddArgument("--dataFolder", argT::SPACE_ARGUMENT, &g_pathFolder, "Folder which contains all data (required)");
  arg.AddArgument("--depthMapFile", argT::SPACE_ARGUMENT, &g_depthMapContainer, "File which contains all the depth map path(default vtiList.txt)");
  arg.AddArgument("--KRTFile", argT::SPACE_ARGUMENT, &g_KRTContainer, "File which contains all the KRTD path (default kList.txt)");
  arg.AddArgument("--rayThick", argT::SPACE_ARGUMENT, &rayPotentialThick, "Define the ray potential thickness threshold when cuda is using (default 2)");
  arg.AddArgument("--rayRho", argT::SPACE_ARGUMENT, &rayPotentialRho, "Define the ray potential rho when cuda is using (default 0.8)");
  arg.AddArgument("--rayEta", argT::SPACE_ARGUMENT, &rayPotentialEta, "0 < Eta < 1 : will be applied as a percentage of rho (default 0.03)");
  arg.AddArgument("--rayDelta", argT::SPACE_ARGUMENT, &rayPotentialDelta, "It has to be superior to Thick (default 0.3)");
  arg.AddArgument("--threshBestCost", argT::SPACE_ARGUMENT, &thresholdBestCost, "Define threshold that will be applied on depth map (default 0.14)");
  arg.AddArgument("--gridEnd", argT::MULTI_ARGUMENT, &g_gridEnd, "Define the end of the grid");
  arg.AddArgument("--contour", argT::SPACE_ARGUMENT, &contourValue, "Define the isocontour value when contour is extracted from reconstructionFilter (default 1.0)");
  arg.AddArgument("--outputMeshFilename", argT::SPACE_ARGUMENT, &g_outputMeshFilename, "Output mesh filename (.vtp) (required)");
  arg.AddBooleanArgument("--verbose", &verbose, "Use to display debug information on console");
  arg.AddBooleanArgument("--summary", &writeSummaryFile, "Use to write a summary file which contains command line and all used parameters (will be write on dataFolder)");
  arg.AddBooleanArgument("--forceCubicVoxel", &forceCubicVoxel, "Define if voxel have the same spacing on X, Y and Z (min of three spacing)");
  arg.AddBooleanArgument("--help", &help, "Print this help message");

  int result = arg.Parse();
  if (!result || help)
    {
    std::cerr << arg.GetHelp();
    return false;
    }

  // User can't set both spacing and dimensions
  if (g_gridSpacing.size() != 0 && g_gridDims.size() != 0)
    {
    std::cerr << "Error : Spacing and dimensions can't be both set" << std::endl;
    std::cerr << arg.GetHelp();
    return false;
    }

  // If only one value is set for dimensions, we set the same dimension for Y and Z
  if (g_gridDims.size() == 1)
    {
    g_gridDims.push_back(g_gridDims[0]);
    g_gridDims.push_back(g_gridDims[0]);
    }

  // Test if arguments are missing
  if (g_outputGridFilename == "" || g_outputMeshFilename == "" || g_depthMapContainer == "" || g_KRTContainer == "" ||
      rayPotentialDelta < rayPotentialThick || rayPotentialEta < 0 || rayPotentialEta > 1)
    {
    std::cerr << "Error arguments." << std::endl;
    std::cerr << arg.GetHelp();
    return false;
    }

  if (g_gridVecX.size() == 0)
    {
    g_gridVecX.push_back(1);    g_gridVecX.push_back(0);    g_gridVecX.push_back(0);
    }

  if (g_gridVecY.size() == 0)
    {
    g_gridVecY.push_back(0);    g_gridVecY.push_back(1);    g_gridVecY.push_back(0);
    }

  if (g_gridVecZ.size() == 0)
    {
    g_gridVecZ.push_back(0);    g_gridVecZ.push_back(0);    g_gridVecZ.push_back(1);
    }

  std::string extensionMesh(".vtp");
  std::string extensionVolume(".vts");
  if (g_outputGridFilename.find(extensionVolume) == std::string::npos ||
    g_outputMeshFilename.find(extensionMesh) == std::string::npos)
    {
    std::cerr << "Error : Bad output extension." << std::endl;
    return false;
    }

  if (!AreVectorsOrthogonal())
    {
    std::cerr << "Given vectors are not orthogonals." << std::endl;
    return false;
    }


  // Get the requested grid size on each axis
  double sizeX = g_gridEnd[0] - g_gridOrigin[0];
  double sizeY = g_gridEnd[1] - g_gridOrigin[1];
  double sizeZ = g_gridEnd[2] - g_gridOrigin[2];

  // Compute the spacing according to the dimensions
  if (g_gridSpacing.size() == 0)
    {
    g_gridSpacing.resize(3);
    g_gridSpacing[0] = sizeX / (double)g_gridDims[0];
    g_gridSpacing[1] = sizeY / (double)g_gridDims[1];
    g_gridSpacing[2] = sizeZ / (double)g_gridDims[2];
    }

  // Compute the dimensions according to the spacing
  if (g_gridDims.size() == 0)
    {
    g_gridDims.resize(3);
    // Compute the dimension on each axis
    g_gridDims[0] = (int)(sizeX / g_gridSpacing[0]);
    g_gridDims[1] = (int)(sizeY / g_gridSpacing[1]);
    g_gridDims[2] = (int)(sizeZ / g_gridSpacing[2]);
    }

  if (forceCubicVoxel)
    {
    // Get the minimum spacing
    std::vector<double>::iterator iter = std::min_element(std::begin(g_gridSpacing), std::end(g_gridSpacing));
    double min = *iter;
    for (int i = 0; i < 3; i++)
      g_gridSpacing[i] = min;
    }

  return true;
}

void CreateGridMatrixFromInput(vtkMatrix4x4* gridMatrix)
{
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
/* Show information on console if we are on verbose mode */
void ShowInformation(std::string information)
{
  if (verbose)
    {
    std::cout << information << "\n" << std::endl;
    }
}

//-----------------------------------------------------------------------------
/* Display all parameters on the console */
void ShowFilledParameters()
{
  if (!verbose)
    return;

  double avgVoxelDim = 1.0/3.0 * (g_gridSpacing[0]+g_gridSpacing[1]+g_gridSpacing[2]);

  std::cout << std::endl;
  std::cout << "----------------------" << std::endl;
  std::cout << "** OUTPUT GRID :" << std::endl;
  std::cout << "----------------------" << std::endl;
  std::cout << "--- Dimensions : ( " << g_gridDims[0] << ", " << g_gridDims[1] << ", " << g_gridDims[2] << " )" << std::endl;
  std::cout << "--- Spacing    : ( " << g_gridSpacing[0] << ", " << g_gridSpacing[1] << ", " << g_gridSpacing[2] << " )" << std::endl;
  std::cout << "--- Origin     : ( " << g_gridOrigin[0] << ", " << g_gridOrigin[1] << ", " << g_gridOrigin[2] << " )" << std::endl;
  std::cout << "--- End        : ( " << g_gridEnd[0] << ", " << g_gridEnd[1] << ", " << g_gridEnd[2] << " )" << std::endl;
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
  std::cout << std::endl;
  std::cout << "----------------------" << std::endl;
  std::cout << "** CUDA :" << std::endl;
  std::cout << "----------------------" << std::endl;
  std::cout << "                                                            _________      " << std::endl;
  std::cout << "     rho|                                                  /         |     " << std::endl;
  std::cout << "        |                                                 /          |     " << std::endl;
  std::cout << "        |                                                /           |     " << std::endl;
  std::cout << "       0| _   _   _   _   _   _   _   _   _   _   _   _ /_   _   _  _|_____" << std::endl;
  std::cout << "        |___________________________________           /                   " << std::endl;
  std::cout << " eta*rho|                                  |          /     |              " << std::endl;
  std::cout << "        |                                  |         /                     " << std::endl;
  std::cout << "        |                                  |________/       |              " << std::endl;
  std::cout << "        |                                               |                  " << std::endl;
  std::cout << "                                           |        |   d   |              " << std::endl;
  std::cout << "                                        Delta   d-thick  d+thick           " << std::endl;
  std::cout << "----------------------" << std::endl;
  std::cout << "----------------------" << std::endl;
  std::cout << "----------------------" << std::endl;
  std::cout << "--- Thickness ray potential : " << rayPotentialThick << " ( ~ " << 1*(rayPotentialThick/avgVoxelDim) << " voxels) "  << std::endl;
  std::cout << "--- Rho ray potential :       " << rayPotentialRho << std::endl;
  std::cout << "--- Eta ray potential :       " << rayPotentialEta << std::endl;
  std::cout << "--- Delta ray potential :     " << rayPotentialDelta << " ( ~ " << 1*(rayPotentialDelta/avgVoxelDim) << " voxels) " << std::endl;
  std::cout << std::endl;
  std::cout << "----------------------" << std::endl;
  std::cout << "** OTHER :" << std::endl;
  std::cout << "----------------------" << std::endl;
  std::cout << "--- Contour : " << contourValue << std::endl;
  std::cout << std::endl;
  std::cout << std::endl;
}

//-----------------------------------------------------------------------------
/* Write a file with all parameters and command line */
void WriteSummaryFile(std::string path, int argc, char** argv)
{
  std::ofstream output(path.c_str());
  std::string file = "";

  output << "----------------------" << std::endl;
  output << "** COMMAND LINE :" << std::endl;
  output << "----------------------" << std::endl;
  for (int i = 0; i < argc; i++)
  {
    output << argv[i] << " ";
  }

  output << std::endl << std::endl;
  output << "----------------------" << std::endl;
  output << "** OUTPUT GRID :" << std::endl;
  output << "----------------------" << std::endl;
  output << "--- Output file path : " << g_outputGridFilename << std::endl;
  output << "--- Dimensions : ( " << g_gridDims[0] << ", " << g_gridDims[1] << ", " << g_gridDims[2] << " )" << std::endl;
  output << "--- Spacing    : ( " << g_gridSpacing[0] << ", " << g_gridSpacing[1] << ", " << g_gridSpacing[2] << " )" << std::endl;
  output << "--- Origin     : ( " << g_gridOrigin[0] << ", " << g_gridOrigin[1] << ", " << g_gridOrigin[2] << " )" << std::endl;
  output << "--- End        : ( " << g_gridEnd[0] << ", " << g_gridEnd[1] << ", " << g_gridEnd[2] << " )" << std::endl;
  output << "--- Nb voxels  : " << g_gridDims[0] * g_gridDims[1] * g_gridDims[2] << std::endl;
  output << "--- Real volume size : ( " << g_gridOrigin[0] - g_gridEnd[0] << ", " << g_gridOrigin[1] - g_gridEnd[1] << ", " << g_gridOrigin[2] - g_gridEnd[2] << ")" << std::endl;
  output << "--- Matrix :" << std::endl;
  std::string l1 = "  " + std::to_string(g_gridVecX[0]) + "  " + std::to_string(g_gridVecY[0]) + "  " + std::to_string(g_gridVecZ[0]) + "\n";
  std::string l2 = "  " + std::to_string(g_gridVecX[1]) + "  " + std::to_string(g_gridVecY[1]) + "  " + std::to_string(g_gridVecZ[1]) + "\n";
  std::string l3 = "  " + std::to_string(g_gridVecX[2]) + "  " + std::to_string(g_gridVecY[2]) + "  " + std::to_string(g_gridVecZ[2]) + "\n";
  output << l1 << std::endl;
  output << l2 << std::endl;
  output << l3 << std::endl;
  output << "----------------------" << std::endl;
  output << "** DEPTH MAP :" << std::endl;
  output << "----------------------" << std::endl;
  output << "--- Threshold for BestCost  : " << std::to_string(thresholdBestCost) << std::endl;
  output << std::endl;
  output << "----------------------" << std::endl;
  output << "** CUDA :" << std::endl;
  output << "----------------------" << std::endl;
  output << "--- Thickness ray potential : " << rayPotentialThick << std::endl;
  output << "--- Rho ray potential :       " << rayPotentialRho << std::endl;
  output << "--- Eta ray potential :       " << rayPotentialEta << std::endl;
  output << "--- Delta ray potential :     " << rayPotentialDelta << std::endl;
  output << std::endl;
  output << "----------------------" << std::endl;
  output << "** OTHER :" << std::endl;
  output << "----------------------" << std::endl;
  output << "--- Contour : " << contourValue << std::endl;
  output << std::endl;
  output << "----------------------" << std::endl;
  output << "** TIME :" << std::endl;
  output << "----------------------" << std::endl;
  output << "--- Reconstruction : " << g_reconstructionExecutionTime << " s" << std::endl;
  output << "--- Total :          " << g_totalExecutionTime << " s" << std::endl;
  output << std::endl;

  output << file;
  output.close();
}
