#include "vtkSlicerRegistrationQualityLogic.h"

// RegistrationQuality includes
#include "vtkMRMLRegistrationQualityNode.h"

// SlicerQt Includes
#include "qSlicerApplication.h"
#include "qSlicerCLILoadableModuleFactory.h"
#include "qSlicerCLIModule.h"
#include "qSlicerCLIModuleWidget.h"
#include "qSlicerModuleFactoryManager.h"
#include "qSlicerModuleManager.h"
#include <vtkSlicerCLIModuleLogic.h>
#include "qSlicerIO.h"
#include "qSlicerCoreIOManager.h"
#include "qMRMLLayoutManager.h"
#include "qSlicerLayoutManager.h"


// MRML includes
#include <vtkMRMLVectorVolumeNode.h>
#include <vtkMRMLModelDisplayNode.h>
#include <vtkMRMLScalarVolumeDisplayNode.h>
#include <vtkMRMLAnnotationROINode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLColorTableNode.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLSliceCompositeNode.h>
#include "vtkSlicerVolumesLogic.h"
#include "vtkSlicerAnnotationModuleLogic.h"
#include "vtkMRMLViewNode.h"
#include "vtkSlicerCLIModuleLogic.h"
#include <vtkMRMLVolumeNode.h>
#include <vtkMRMLSliceLogic.h>
#include "vtkMRMLAnnotationSnapshotNode.h"
#include "vtkMRMLAnnotationSnapshotStorageNode.h"
#include "qMRMLUtils.h"
#include "qMRMLScreenShotDialog.h"

// VTK includes
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkVectorNorm.h>
#include <vtkTransform.h>
#include <vtkMatrix4x4.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkGeneralTransform.h>
#include <vtkLookupTable.h>
#include <vtkMath.h>
#include <vtkImageAccumulate.h>

// CTK includes
#include <ctkVTKWidgetsUtils.h>

// STD includes
#include <iostream>
#include <cassert>
#include <math.h>

class vtkSlicerRegistrationQualityLogic::vtkInternal {
public:
	vtkInternal();
	vtkSlicerVolumesLogic* VolumesLogic;
};

//----------------------------------------------------------------------------
vtkSlicerRegistrationQualityLogic::vtkInternal::vtkInternal() {
	this->VolumesLogic = 0;
}
//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerRegistrationQualityLogic);

//----------------------------------------------------------------------------
vtkSlicerRegistrationQualityLogic::vtkSlicerRegistrationQualityLogic() {
	this->RegistrationQualityNode = NULL;
	this->TransformField = vtkSmartPointer<vtkImageData>::New();
	this->Internal = new vtkInternal;
}

//----------------------------------------------------------------------------
vtkSlicerRegistrationQualityLogic::~vtkSlicerRegistrationQualityLogic() {
	vtkSetAndObserveMRMLNodeMacro(this->RegistrationQualityNode, NULL);
	delete this->Internal;
}
//----------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::SetVolumesLogic(vtkSlicerVolumesLogic* logic) {
	this->Internal->VolumesLogic = logic;
}

//----------------------------------------------------------------------------
vtkSlicerVolumesLogic* vtkSlicerRegistrationQualityLogic::GetVolumesLogic() {
	return this->Internal->VolumesLogic;
}
//----------------------------------------------------------------------------

void vtkSlicerRegistrationQualityLogic::PrintSelf(ostream& os, vtkIndent indent) {
	this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic
::SetAndObserveRegistrationQualityNode(vtkMRMLRegistrationQualityNode *node) {
	vtkSetAndObserveMRMLNodeMacro(this->RegistrationQualityNode, node);
}

//---------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::SetMRMLSceneInternal(vtkMRMLScene * newScene) {
	vtkNew<vtkIntArray> events;
	events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
	events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
	events->InsertNextValue(vtkMRMLScene::EndBatchProcessEvent);
	this->SetAndObserveMRMLSceneEvents(newScene, events.GetPointer());
}

//-----------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::RegisterNodes() {
	vtkMRMLScene* scene = this->GetMRMLScene();
	assert(scene != 0);

	scene->RegisterNodeClass(vtkSmartPointer<vtkMRMLRegistrationQualityNode>::New());
}

//---------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::UpdateFromMRMLScene() {
	assert(this->GetMRMLScene() != 0);
	this->Modified();
}

//---------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::OnMRMLSceneNodeAdded(vtkMRMLNode* node) {
	if (!node || !this->GetMRMLScene()) {
		return;
	}

	if( node->IsA("vtkMRMLVectorVolumeNode") ||
		node->IsA("vtkMRMLLinearTransformNode") ||
		node->IsA("vtkMRMLGridTransformNode") ||
		node->IsA("vtkMRMLBSplineTransformNode") ||
		node->IsA("vtkMRMLRegistrationQualityNode")) {
		this->Modified();
	}
}

//---------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::OnMRMLSceneNodeRemoved(vtkMRMLNode* node) {
	if (!node || !this->GetMRMLScene()) {
		return;
	}

	if (node->IsA("vtkMRMLVectorVolumeNode") ||
		node->IsA("vtkMRMLLinearTransformNode") ||
		node->IsA("vtkMRMLGridTransformNode") ||
		node->IsA("vtkMRMLBSplineTransformNode") ||
		node->IsA("vtkMRMLRegistrationQualityNode")) {
		this->Modified();
	}
}

//---------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::OnMRMLSceneEndImport() {
	//Select parameter node if it exists
	vtkSmartPointer<vtkMRMLRegistrationQualityNode> paramNode = NULL;
	vtkSmartPointer<vtkMRMLNode> node = this->GetMRMLScene()->GetNthNodeByClass(
			0, "vtkMRMLRegistrationQualityNode");

	if (node) {
		paramNode = vtkMRMLRegistrationQualityNode::SafeDownCast(node);
		vtkSetAndObserveMRMLNodeMacro(this->RegistrationQualityNode, paramNode);
	}
}

//---------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::OnMRMLSceneEndClose() {
	this->Modified();
}
//---------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::SaveScreenshot(const char* description) {
	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
	    vtkErrorMacro("SaveScreenshot: Invalid scene or parameter set node!");
	    return;
	}
	
	if (!this->RegistrationQualityNode->GetOutputDirectory()) {
		vtkErrorMacro("SaveScreenshot: No output Directory!");
		return;
	}
	
	int screenShotNumber = this->RegistrationQualityNode->GetNumberOfScreenshots();
	std::ostringstream convert;
	convert << screenShotNumber;
	std::string outSS;
	std::string Name("Screenshot_");
	outSS = Name + convert.str();
	
	std::string directory = this->RegistrationQualityNode->GetOutputDirectory();

	
	//Add snapshot node
	vtkMRMLAnnotationSnapshotNode* screenShotNode = NULL;
	vtkNew<vtkMRMLAnnotationSnapshotNode> screenShotNodeNew;
		
	vtkNew<vtkMRMLAnnotationSnapshotStorageNode> screenShotStorageNode;
	this->GetMRMLScene()->AddNode(screenShotStorageNode.GetPointer());
	screenShotNodeNew->SetAndObserveStorageNodeID(screenShotStorageNode->GetID());
	
	this->GetMRMLScene()->AddNode(screenShotNodeNew.GetPointer());
	screenShotNode = screenShotNodeNew.GetPointer();
	
	screenShotNode->SetName(outSS.c_str());
	std::cerr << "Description: "<< description << std::endl;
	if (description) screenShotNode->SetSnapshotDescription(description);
	screenShotNode->SetScreenShotType(4);
	
	//Get Image Data - copied from qMRMLScreenShotDialog
	qSlicerApplication* app = qSlicerApplication::application();
	qSlicerLayoutManager* layoutManager = app->layoutManager();
	

	QWidget* widget = 0;
	widget = layoutManager->viewport();
	QImage screenShot = ctk::grabVTKWidget(layoutManager->viewport());
	
	// Rescale the image which gets saved
// 	QImage rescaledScreenShot = screenShot.scaled(screenShot.size().width()
//       * d->scaleFactorSpinBox->value(), screenShot.size().height()
//       * d->scaleFactorSpinBox->value());
//	 convert the screenshot from QPixmap to vtkImageData and store it with this class
	vtkNew<vtkImageData> newImageData;
	qMRMLUtils::qImageToVtkImageData(screenShot,
                                         newImageData.GetPointer());
					 
	screenShotNode->SetScreenShot(newImageData.GetPointer());
	
	//Save screenshot	
	qSlicerCoreIOManager* coreIOManager = qSlicerCoreApplication::application()->coreIOManager();
	qSlicerIO::IOProperties fileParameters;
	char fileName[512];
	sprintf(fileName, "%s/%s.png", directory.c_str(), outSS.c_str());
	
	fileParameters["nodeID"] = screenShotNode->GetID();
	fileParameters["fileName"] = fileName;
	
	
	
	if (coreIOManager->saveNodes("AnnotationFile", fileParameters)) {
		std::cerr << "Saved Screenshot to: " << fileName << "" << std::endl;	
	} else{
		vtkErrorMacro("SaveScreenshot: Cannot save screenshot!");
	}
	
	//Increase screen shot number
	screenShotNumber += 1;
	this->RegistrationQualityNode->DisableModifiedEventOn();
	this->RegistrationQualityNode->SetNumberOfScreenshots(screenShotNumber);
	this->RegistrationQualityNode->DisableModifiedEventOff();
}
//---------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::SaveOutputFile() {
	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
	    vtkErrorMacro("saveOutputFile: Invalid scene or parameter set node!");
	    return;	    
	}
	
	double statisticValues[4];
	std::string directory = this->RegistrationQualityNode->GetOutputDirectory();
	char fileName[512];
	sprintf(fileName, "%s/OutputFile.html", directory.c_str() );
	
	std::ofstream outfile;
	outfile.open(fileName, std::ios_base::out | std::ios_base::trunc);
	
	if ( !outfile ){
		vtkErrorMacro("SaveOutputFile: Output file '" << fileName << "' cannot be opened!");
		return;
	}
	
	outfile << "<!DOCTYPE html>" << std::endl << "<html>" << std::endl 
	<< "<head>" << std::endl << "<title>Registration Quality</title>" << std::endl 
	<< "<meta charset=\"UTF-8\">" << std::endl << "</head>"	<< "<body>" << std::endl;
	
	outfile << "<h1>Registration Quality Output File</h1>" << std::endl;
	
	// --- Image Checks ---
	outfile << "<h2>Image Checks</h2>" << std::endl;
	//Set table
	outfile << "<table style=\"width:80%\">" << std::endl
	<< "<tr>" << std::endl
	<< "<td> </td> <td> Mean </td> <td> STD </td> <td> Max </td> <td> Min </td>" << std::endl
	<< "</tr>" << std::endl;
	
	//Absolute Difference
	this->RegistrationQualityNode->GetAbsoluteDiffStatistics(statisticValues);
	outfile << "<tr>" << std::endl
	<< "<td>" << "Absolute Difference" << "</td>";
	for(int i=0;i<4;i++) {	
		outfile << "<td>" << statisticValues[i] << "</td>";
	}
	outfile << std::endl<< "</tr>" << std::endl;
	
	//TODO: Inverse Absolute Difference
	
	//TODO: Fiducials
	
	outfile << "</table>" << std::endl;
		
	// --- Vector checks ---	
	outfile << "<h2>Vector Checks</h2>" << std::endl;	
	//Set table
	outfile << "<table style=\"width:80%\">" << std::endl
	<< "<tr>" << std::endl
	<< "<td> </td> <td> Mean </td> <td> STD </td> <td> Max </td> <td> Min </td>" << std::endl
	<< "</tr>" << std::endl;
	
	//Jacobian
	this->RegistrationQualityNode->GetJacobianStatistics(statisticValues);
	outfile << "<tr>" << std::endl
	<< "<td>" << "Jacobian" << "</td>";
	for(int i=0;i<4;i++) {	
		outfile << "<td>" << statisticValues[i] << "</td>";
	}
	outfile << std::endl<< "</tr>" << std::endl;
	
	//TODO: Inverse Jacobian
	
	//Inverse Consistency
	this->RegistrationQualityNode->GetInverseConsistStatistics(statisticValues);
	outfile << "<tr>" << std::endl
	<< "<td>" << "Inverse Consistency" << "</td>";
	for(int i=0;i<4;i++) {	
		outfile << "<td>" << statisticValues[i] << "</td>";
	}
	outfile << std::endl<< "</tr>" << std::endl;

	outfile << "</table>" << std::endl;

	// --- Images ---
	outfile << "<h2>Images</h2>" << std::endl;	
	int screenShotNumber = this->RegistrationQualityNode->GetNumberOfScreenshots();
	if (screenShotNumber > 1){	
		for(int i = 1; i < screenShotNumber; i++) {
			std::ostringstream screenShotName;
			screenShotName << "Screenshot_" << i;
			
				
			//Find the associated node for description
			vtkSmartPointer<vtkCollection> collection = vtkSmartPointer<vtkCollection>::Take(
						this->GetMRMLScene()->GetNodesByName(screenShotName.str().c_str()));
			if (collection->GetNumberOfItems() == 1) {	
				vtkMRMLAnnotationSnapshotNode* snapshot = vtkMRMLAnnotationSnapshotNode::SafeDownCast(
						collection->GetItemAsObject(0));
				outfile << "<h3>" << snapshot->GetSnapshotDescription() << "</h3>" << std::endl;
			}
			
			outfile << "<img src=\"" << directory << "/" << screenShotName.str() << ".png"
				<<"\" alt=\"Screenshot "<< i << "\" width=\"80%\"> " << std::endl;
		}
	}
	outfile  << "</body>" << std::endl << "</html>" << std::endl;
	
	outfile << std::endl;
	outfile.close();
}
//---------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::AbsoluteDifference(int state) {

	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
	    vtkErrorMacro("AbsoluteDifference: Invalid scene or parameter set node!");
	    return;
	}

	vtkMRMLScalarVolumeNode *referenceVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetReferenceVolumeNodeID()));

	vtkMRMLScalarVolumeNode *warpedVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetWarpedVolumeNodeID()));
	vtkMRMLAnnotationROINode *inputROI = vtkMRMLAnnotationROINode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetROINodeID()));


// 	vtkMRMLScalarVolumeNode *outputVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
// 			this->GetMRMLScene()->GetNodeByID(
// 				this->RegistrationQualityNode->GetAbsoluteDiffVolumeNodeID()));
	if (!referenceVolume || !warpedVolume ) {
		vtkErrorMacro("AbsoluteDifference: Invalid reference or warped volume!");
		return;
	}

	if (!state) {
	      	this->SetDefaultDisplay(referenceVolume,warpedVolume);
		return;
	}

	if (!this->RegistrationQualityNode->GetAbsoluteDiffVolumeNodeID()){
	if(!this->Internal->VolumesLogic){
		std::cerr << "AbsoluteDifference: ERROR: failed to get hold of Volumes logic" << std::endl;
		return;
	}

	  vtkMRMLScalarVolumeNode *outputVolume = NULL;
	    vtkMRMLScalarVolumeNode *svnode = vtkMRMLScalarVolumeNode::SafeDownCast(referenceVolume);
	    std::string outSS;
	    std::string Name("-absoluteDifference");

	    outSS = (referenceVolume->GetName() + Name);
	    outSS = this->GetMRMLScene()->GenerateUniqueName(outSS);
	    
	    if(svnode)
	    {
	      outputVolume = this->Internal->VolumesLogic->CloneVolume(this->GetMRMLScene(), referenceVolume, outSS.c_str());
	    }
	    else
	    {
	      std::cerr << "Reference volume not scalar volume!" << std::endl;
	      return;
	    }

	  if ( !outputVolume ) {
		  vtkErrorMacro("AbsoluteDifference: No output volume set!");
		  return;
	  }
// 	  //Check dimensions of both volume, they must be the same.
// 	  vtkSmartPointer<vtkImageData> imageDataRef = referenceVolume->GetImageData();
// 	  vtkSmartPointer<vtkImageData> imageDataWarp = warpedVolume->GetImageData();
// 	    int* dimsRef = imageDataRef->GetDimensions();
// 	    int* dimsWarp = imageDataWarp->GetDimensions();
// 	  // int dims[3]; // can't do this
// 	  if (dimsRef[0] != dimsWarp[0] || dimsRef[1] != dimsWarp[1] || dimsRef[2] != dimsWarp[2] ) {
// 	    vtkErrorMacro("AbsoluteDifference: Dimensions of Reference and Warped image don't match'!");
// 	    return;
// 	  }

	  qSlicerCLIModule* checkerboardfilterCLI = dynamic_cast<qSlicerCLIModule*>(
			  qSlicerCoreApplication::application()->moduleManager()->module("AbsoluteDifference"));
	  QString cliModuleName("AbsoluteDifference");

	  vtkSmartPointer<vtkMRMLCommandLineModuleNode> cmdNode =
			  checkerboardfilterCLI->cliModuleLogic()->CreateNodeInScene();

	  // Set node parameters
	  cmdNode->SetParameterAsString("inputVolume1", referenceVolume->GetID());
	  cmdNode->SetParameterAsString("inputVolume2", warpedVolume->GetID());
	  cmdNode->SetParameterAsString("outputVolume", outputVolume->GetID());
	  if (inputROI)
	  {
	      cmdNode->SetParameterAsString("fixedImageROI", inputROI->GetID());
	  }
	  else
	  {
	    cmdNode->SetParameterAsString("fixedImageROI", "");
	  }

	  // Execute synchronously so that we can check the content of the file after the module execution
	  checkerboardfilterCLI->cliModuleLogic()->ApplyAndWait(cmdNode);

	cout << "cmdNodeStatus: " << cmdNode->GetStatus() << endl;
	if(cmdNode->GetStatus() == vtkMRMLCommandLineModuleNode::CompletedWithErrors) {
		cout << "  Error!" << endl;
		throw std::runtime_error("Error in CLI module, see command line!");
	}

	  this->GetMRMLScene()->RemoveNode(cmdNode);

	  outputVolume->SetAndObserveTransformNodeID(NULL);
	  this->RegistrationQualityNode->SetAbsoluteDiffVolumeNodeID(outputVolume->GetID());
	}


	vtkMRMLScalarVolumeNode *absoluteDiffVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetAbsoluteDiffVolumeNodeID()));

	if ( !absoluteDiffVolume ) {
		vtkErrorMacro("AbsoluteDifference: No output volume set!");
		return;
	}
	absoluteDiffVolume->GetScalarVolumeDisplayNode()->AutoWindowLevelOff();
	int window=300;
	int level=200;

	absoluteDiffVolume->GetScalarVolumeDisplayNode()->SetThreshold(0,3e3);
	absoluteDiffVolume->GetScalarVolumeDisplayNode()->SetLevel(level);
	absoluteDiffVolume->GetScalarVolumeDisplayNode()->SetWindow(window);
	absoluteDiffVolume->GetDisplayNode()->SetAndObserveColorNodeID("vtkMRMLColorTableNodeRainbow");

	this->SetForegroundImage(referenceVolume,absoluteDiffVolume,0.5);

	  // Get mean and std from squared difference volume
	double statisticValues[4];
	this->CalculateStatistics(absoluteDiffVolume,statisticValues);

	this->RegistrationQualityNode->DisableModifiedEventOn();
	this->RegistrationQualityNode->SetAbsoluteDiffStatistics( statisticValues );
	this->RegistrationQualityNode->DisableModifiedEventOff();

	return;
}


//--- Image Checks -----------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::FalseColor(int state) {
	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
		vtkErrorMacro("Invalid scene or parameter set node!");
		throw std::runtime_error("Internal Error, see command line!");
	}

	vtkMRMLScalarVolumeNode *referenceVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetReferenceVolumeNodeID()));

	vtkMRMLScalarVolumeNode *warpedVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetWarpedVolumeNodeID()));

	if (!referenceVolume || !warpedVolume) {
		throw std::runtime_error("Reference or warped volume not set!");
	}

	//TODO: Volumes go back to gray value - perhaps we should rembemer previous color settings?
	if (!state) {
		SetDefaultDisplay(referenceVolume,warpedVolume);
		return;
	}

	vtkMRMLScalarVolumeDisplayNode *referenceVolumeDisplayNode = referenceVolume->GetScalarVolumeDisplayNode();
	vtkMRMLScalarVolumeDisplayNode *warpedVolumeDisplayNode = warpedVolume->GetScalarVolumeDisplayNode();

	referenceVolumeDisplayNode->SetAndObserveColorNodeID("vtkMRMLColorTableNodeWarmTint1");
	warpedVolumeDisplayNode->SetAndObserveColorNodeID("vtkMRMLColorTableNodeCoolTint1");

	// Set window and level the same for warped and reference volume.
	warpedVolumeDisplayNode->AutoWindowLevelOff();
	warpedVolumeDisplayNode->SetWindow( referenceVolumeDisplayNode->GetWindow() );
	warpedVolumeDisplayNode->SetLevel( referenceVolumeDisplayNode->GetLevel() );

	this->SetForegroundImage(referenceVolume,warpedVolume,0.5);

	return;
}

//----------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::Flicker(int opacity) {
	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
		vtkErrorMacro("Flicker: Invalid scene or parameter set node!");
		return;
	}

	vtkMRMLScalarVolumeNode *referenceVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetReferenceVolumeNodeID()));

	vtkMRMLScalarVolumeNode *warpedVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetWarpedVolumeNodeID()));

	if (!referenceVolume || !warpedVolume) {
		vtkErrorMacro("Flicker: Invalid reference or warped volume!");
		return;
	}

	this->SetForegroundImage(referenceVolume,warpedVolume,opacity);

	return;
}

void vtkSlicerRegistrationQualityLogic
::getSliceCompositeNodeRASBounds(vtkMRMLSliceCompositeNode *scn, double* minmax) {

	vtkMRMLScalarVolumeNode* foreground = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(scn->GetBackgroundVolumeID()));
	vtkMRMLScalarVolumeNode* background = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(scn->GetForegroundVolumeID()));

	double rasBounds[6] = {INFINITY,-INFINITY,INFINITY,-INFINITY,INFINITY,-INFINITY};
	double rasBoundsBack[6] = {INFINITY,-INFINITY,INFINITY,-INFINITY,INFINITY,-INFINITY};
	if(foreground) foreground->GetRASBounds(rasBounds);
	if(background) background->GetRASBounds(rasBoundsBack);
	for(int i=0;i<3; i++) {
		minmax[2*i] = std::min(rasBounds[2*i],rasBoundsBack[2*i]);
		minmax[2*i+1] = std::max(rasBounds[2*i+1],rasBoundsBack[2*i+1]);
		if(minmax[2*i]>minmax[2*i+1]) {
			cout << "rasBounds infty" << endl;
			minmax[2*i] = minmax[2*i+1] = 0;
		}
	}
}

//----------------------------------------------------------------------------
/**
 * Movie through slices.
 * TODO:	- Calculate slice spacing
 * 			- Changed slice directions
 * 			- Oblique slices
 */
void vtkSlicerRegistrationQualityLogic::Movie() {
	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
		vtkErrorMacro("Movie: Invalid scene or parameter set node!");
		return;
	}
	vtkMRMLSliceNode* sliceNodeRed = vtkMRMLSliceNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeRed"));
	vtkMRMLSliceNode* sliceNodeYellow = vtkMRMLSliceNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeYellow"));
	vtkMRMLSliceNode* sliceNodeGreen = vtkMRMLSliceNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceNodeGreen"));

	double rasBoundsRed[6], rasBoundsYellow[6], rasBoundsGreen[6];
	int runState = this->RegistrationQualityNode->GetMovieRun();

	if(runState) {
		vtkMRMLSliceCompositeNode *scn = vtkMRMLSliceCompositeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeRed"));
		getSliceCompositeNodeRASBounds(scn,rasBoundsRed);
		scn = vtkMRMLSliceCompositeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeYellow"));
		getSliceCompositeNodeRASBounds(scn,rasBoundsYellow);
		scn = vtkMRMLSliceCompositeNode::SafeDownCast(this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeGreen"));
		getSliceCompositeNodeRASBounds(scn,rasBoundsGreen);

		double redMin = rasBoundsRed[4];
		double redMax = rasBoundsRed[5];
		double redStep = 3;
		double redPos = redMin;
		double yellowMin = rasBoundsYellow[0];
		double yellowMax = rasBoundsYellow[1];
		double yellowStep = 3;
		double yellowPos = yellowMin;
		double greenMin = rasBoundsGreen[2];
		double greenMax = rasBoundsGreen[3];
		double greenStep = 3;
		double greenPos = greenMin;

		cout << "movie:\n"
			<< " red:    " << redMin << " .. " << redMax << "\n"
			<< " yellow: " << yellowMin << " .. " << yellowMax << "\n"
			<< " green:  " << greenMin << " .. " << greenMax << "\n"
			<< endl;

		while(runState) {
			//cout << " runRed=" << (runState&1?"true":"false")
			//		<< " runYellow=" << (runState&2?"true":"false")
			//		<< " runGreen=" << (runState&4?"true":"false")
			//		<< " MovieRun=" << runState << endl;

			if(runState&1) {
				sliceNodeRed->JumpSliceByCentering((yellowMin+yellowMax)/2,(greenMin+greenMax)/2,redPos);
				redPos += redStep;
				if(redPos>redMax)  {
					redPos -= redMax - redMin;
					cout << "red Overflow" << endl;
				}
			}
			if(runState&2) {
				sliceNodeYellow->JumpSliceByCentering(yellowPos,(greenMin+greenMax)/2,(redMin+redMax)/2);
				yellowPos += yellowStep;
				if(yellowPos>yellowMax)  {
					yellowPos -= yellowMax - yellowMin;
					cout << "yellow Overflow" << endl;
				}
			}
			if(runState&4) {
				sliceNodeGreen->JumpSliceByCentering((yellowMin+yellowMax)/2,greenPos,(redMin+redMax)/2);
				greenPos += greenStep;
				if(greenPos>greenMax)  {
					greenPos -= greenMax - greenMin;
					cout << "green Overflow" << endl;
				}
			}
			qSlicerApplication::application()->processEvents();

			runState = RegistrationQualityNode->GetMovieRun();
		}
	}
}

//----------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::Checkerboard(int state) {
	//   Calling checkerboardfilter cli. Logic has been copied and modified from CropVolumeLogic onApply.
	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
		vtkErrorMacro("Invalid scene or parameter set node!");
		throw std::runtime_error("Internal Error, see command line!");
	}
	vtkMRMLScalarVolumeNode *referenceVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetReferenceVolumeNodeID()));
	vtkMRMLScalarVolumeNode *warpedVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetWarpedVolumeNodeID()));

	if (!state) {
	      	this->SetDefaultDisplay(referenceVolume,warpedVolume);
		return;
	}

	if (!referenceVolume || !warpedVolume) {
		throw std::runtime_error("Reference or warped volume not set!");
	}

	if (!this->RegistrationQualityNode->GetCheckerboardVolumeNodeID()) {
		if(!this->Internal->VolumesLogic) {
			vtkErrorMacro("Failed to get hold of Volumes logic!");
			throw std::runtime_error("Internal Error, see command line!");
		}

		int PatternValue = this->RegistrationQualityNode->GetCheckerboardPattern();
		std::string outSS;
		std::string Name("-CheckerboardPattern_");
		std::ostringstream strPattern;
		strPattern << PatternValue;
		outSS = referenceVolume->GetName() + Name + strPattern.str();
		outSS = this->GetMRMLScene()->GenerateUniqueName(outSS);
		
		vtkMRMLScalarVolumeNode *outputVolume = this->Internal->VolumesLogic->CloneVolume(
			this->GetMRMLScene(), referenceVolume, outSS.c_str());
		if (!outputVolume) {
			vtkErrorMacro("Could not create Checkerboard volume!");
			throw std::runtime_error("Internal Error, see command line!");
		}

		qSlicerCLIModule* checkerboardfilterCLI = dynamic_cast<qSlicerCLIModule*>(
			qSlicerCoreApplication::application()->moduleManager()->module("CheckerBoardFilter"));
		if (!checkerboardfilterCLI) {
			vtkErrorMacro("No Checkerboard Filter module!");
			throw std::runtime_error("Internal Error, see command line!");
		}

		vtkSmartPointer<vtkMRMLCommandLineModuleNode> cmdNode =
			checkerboardfilterCLI->cliModuleLogic()->CreateNodeInScene();

		//Convert PatternValue to string
		std::ostringstream outPattern;
		outPattern << PatternValue << "," << PatternValue << "," << PatternValue;
		// Set node parameters
		cmdNode->SetParameterAsString("checkerPattern",outPattern.str().c_str());
		cmdNode->SetParameterAsString("inputVolume1", referenceVolume->GetID());
		cmdNode->SetParameterAsString("inputVolume2", warpedVolume->GetID());
		cmdNode->SetParameterAsString("outputVolume", outputVolume->GetID());

		// Execute synchronously so that we can check the content of the file after the module execution
		checkerboardfilterCLI->cliModuleLogic()->ApplyAndWait(cmdNode);

		this->GetMRMLScene()->RemoveNode(cmdNode);

		outputVolume->SetAndObserveTransformNodeID(NULL);
		this->RegistrationQualityNode->SetCheckerboardVolumeNodeID(outputVolume->GetID());
		return;
	}

	vtkMRMLScalarVolumeNode *checkerboardVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetCheckerboardVolumeNodeID()));
	this->SetForegroundImage(checkerboardVolume,referenceVolume,0);
}

//----------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::Jacobian(int state) {

	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
		vtkErrorMacro("Invalid scene or parameter set node!");
		throw std::runtime_error("Internal Error, see command line!");
	}

	vtkMRMLScalarVolumeNode *referenceVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetReferenceVolumeNodeID()));
	vtkMRMLVectorVolumeNode *vectorVolume = vtkMRMLVectorVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetVectorVolumeNodeID()));
	vtkMRMLAnnotationROINode *inputROI = vtkMRMLAnnotationROINode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetROINodeID()));

	if (!vectorVolume || !referenceVolume ) {
		throw std::runtime_error("Reference volume or vector field not set!");
	}

	if (!state) {
		vtkMRMLScalarVolumeNode *warpedVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetWarpedVolumeNodeID()));
		this->SetDefaultDisplay(referenceVolume,warpedVolume);
		return;
	}
	if (!this->RegistrationQualityNode->GetJacobianVolumeNodeID()){

		if(!this->Internal->VolumesLogic) {
			vtkErrorMacro("Failed to get hold of Volumes logic!");
			throw std::runtime_error("Internal Error, see command line!");
		}

		std::string outSS;
		std::string Name("-jacobian");
		outSS = referenceVolume->GetName() + Name;
		outSS = this->GetMRMLScene()->GenerateUniqueName(outSS);

		vtkMRMLScalarVolumeNode *outputVolume = this->Internal->VolumesLogic->CloneVolume(
			this->GetMRMLScene(), referenceVolume, outSS.c_str());

		qSlicerCLIModule* jacobianfilterCLI = dynamic_cast<qSlicerCLIModule*>(
			qSlicerCoreApplication::application()->moduleManager()->module("JacobianFilter"));
		if (!jacobianfilterCLI) {
			vtkErrorMacro("No Jacobian Filter module!");
			throw std::runtime_error("Internal Error, see command line!");
		}

		vtkSmartPointer<vtkMRMLCommandLineModuleNode> cmdNode =
			jacobianfilterCLI->cliModuleLogic()->CreateNodeInScene();

		// Set node parameters
		cmdNode->SetParameterAsString("inputVolume", vectorVolume->GetID());
		cmdNode->SetParameterAsString("outputVolume", outputVolume->GetID());
		if (inputROI)
		{
		    cmdNode->SetParameterAsString("fixedImageROI", inputROI->GetID());
		}
		else
		{
		  cmdNode->SetParameterAsString("fixedImageROI", "");
		}
		// Execute synchronously so that we can check the content of the file after the module execution
		jacobianfilterCLI->cliModuleLogic()->ApplyAndWait(cmdNode);

		this->GetMRMLScene()->RemoveNode(cmdNode);

		outputVolume->SetAndObserveTransformNodeID(NULL);
		this->RegistrationQualityNode->SetJacobianVolumeNodeID(outputVolume->GetID());

	}

	vtkMRMLScalarVolumeNode *jacobianVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
		this->GetMRMLScene()->GetNodeByID(
			this->RegistrationQualityNode->GetJacobianVolumeNodeID()));

	jacobianVolume->GetDisplayNode()->SetAndObserveColorNodeID("vtkMRMLColorTableNodeRainbow");
	jacobianVolume->GetScalarVolumeDisplayNode()->AutoWindowLevelOff();
	double window=0.8;
	int level=1;

	jacobianVolume->GetScalarVolumeDisplayNode()->SetThreshold(0,3);
	jacobianVolume->GetScalarVolumeDisplayNode()->SetLevel(level);
	jacobianVolume->GetScalarVolumeDisplayNode()->SetWindow(window);

	this->SetForegroundImage(referenceVolume,jacobianVolume,0.5);

	double statisticValues[4]; // 1. Mean 2. STD 3. Max 4. Min
	this->CalculateStatistics(jacobianVolume,statisticValues);

	this->RegistrationQualityNode->DisableModifiedEventOn();
	this->RegistrationQualityNode->SetJacobianStatistics( statisticValues );
	// 	this->RegistrationQualityNode->SetJacobianSTD( statisticValues[1] );
	this->RegistrationQualityNode->DisableModifiedEventOff();

	return;
}
//----------------------------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::InverseConsist(int state) {

	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
		vtkErrorMacro("Inverse Consistency: Invalid scene or parameter set node!");
		return;
	}

	vtkMRMLVectorVolumeNode *vectorVolume1 = vtkMRMLVectorVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetVectorVolumeNodeID()));
	vtkMRMLVectorVolumeNode *vectorVolume2 = vtkMRMLVectorVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetInvVectorVolumeNodeID()));
	vtkMRMLScalarVolumeNode *referenceVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetReferenceVolumeNodeID()));
	vtkMRMLAnnotationROINode *inputROI = vtkMRMLAnnotationROINode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetROINodeID()));

// 	vtkMRMLScalarVolumeNode *warpedVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
// 			this->GetMRMLScene()->GetNodeByID(
// 				this->RegistrationQualityNode->GetWarpedVolumeNodeID()));
// 	vtkMRMLVolumeNode *outputVolume = vtkMRMLVolumeNode::SafeDownCast(
// 			this->GetMRMLScene()->GetNodeByID(
// 				this->RegistrationQualityNode->GetCheckerboardVolumeNodeID()));
//
	if (!vectorVolume1 || !vectorVolume2 || !referenceVolume ) {
	    std::cerr << "Volumes not set!" << std::endl;
	    return;
	}
	if (!state) {

	  vtkMRMLScalarVolumeNode *warpedVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetWarpedVolumeNodeID()));
	      	this->SetDefaultDisplay(referenceVolume,warpedVolume);
		return;
	}

	if (!this->RegistrationQualityNode->GetInverseConsistVolumeNodeID()){
	  if(!this->Internal->VolumesLogic)
	    {
	      std::cerr << "Inverse Consistency: ERROR: failed to get hold of Volumes logic" << std::endl;
	      return;
	    }


	  vtkMRMLScalarVolumeNode *outputVolume = NULL;
	  vtkMRMLScalarVolumeNode *svnode = vtkMRMLScalarVolumeNode::SafeDownCast(referenceVolume);
	  std::string outSS;
	  std::string Name("-invConsist");

	  outSS = referenceVolume->GetName() + Name;
	  outSS = this->GetMRMLScene()->GenerateUniqueName(outSS);
	  if(svnode)
	  {
	    outputVolume = this->Internal->VolumesLogic->CloneVolume(this->GetMRMLScene(), referenceVolume, outSS.c_str());
	  }
	  else
	  {
	    std::cerr << "Reference volume not scalar volume!" << std::endl;
	    return;
	  }


	qSlicerCLIModule* checkerboardfilterCLI = dynamic_cast<qSlicerCLIModule*>(
			qSlicerCoreApplication::application()->moduleManager()->module("InverseConsistency"));
	QString cliModuleName("InverseConsistency");

	vtkSmartPointer<vtkMRMLCommandLineModuleNode> cmdNode =
			checkerboardfilterCLI->cliModuleLogic()->CreateNodeInScene();

	// Set node parameters
	cmdNode->SetParameterAsString("inputVolume1", vectorVolume1->GetID());
	cmdNode->SetParameterAsString("inputVolume2", vectorVolume2->GetID());
	cmdNode->SetParameterAsString("outputVolume", outputVolume->GetID());
	if (inputROI)
	{
	    cmdNode->SetParameterAsString("fixedImageROI", inputROI->GetID());
	}
	else
	{
	    cmdNode->SetParameterAsString("fixedImageROI", "");
	}

	// Execute synchronously so that we can check the content of the file after the module execution
	checkerboardfilterCLI->cliModuleLogic()->ApplyAndWait(cmdNode);

	this->GetMRMLScene()->RemoveNode(cmdNode);

	outputVolume->SetAndObserveTransformNodeID(NULL);
	this->RegistrationQualityNode->SetInverseConsistVolumeNodeID(outputVolume->GetID());
	}

	vtkMRMLScalarVolumeNode *inverseConsistVolume = vtkMRMLScalarVolumeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID(
				this->RegistrationQualityNode->GetInverseConsistVolumeNodeID()));


	inverseConsistVolume->GetDisplayNode()->SetAndObserveColorNodeID("vtkMRMLColorTableNodeGreen");
	inverseConsistVolume->GetScalarVolumeDisplayNode()->AutoWindowLevelOff();
	double window=10;
	int level=5;

	inverseConsistVolume->GetScalarVolumeDisplayNode()->SetThreshold(0,10);
	inverseConsistVolume->GetScalarVolumeDisplayNode()->SetLevel(level);
	inverseConsistVolume->GetScalarVolumeDisplayNode()->SetWindow(window);

	this->SetForegroundImage(referenceVolume,inverseConsistVolume,0.5);

	double statisticValues[4]; // 1. Mean 2. STD 3. Max 4. Min
	this->CalculateStatistics(inverseConsistVolume,statisticValues);

	this->RegistrationQualityNode->DisableModifiedEventOn();
	this->RegistrationQualityNode->SetInverseConsistStatistics( statisticValues );
	this->RegistrationQualityNode->DisableModifiedEventOff();

	return;
}
//--- Default mode when checkbox is unchecked -----------------------------------------------------------
void vtkSlicerRegistrationQualityLogic::SetDefaultDisplay(vtkMRMLScalarVolumeNode *backgroundVolume, vtkMRMLScalarVolumeNode *foregroundVolume) {
	if (!this->GetMRMLScene() || !this->RegistrationQualityNode) {
		vtkErrorMacro("SetDefaultDisplay: Invalid scene or parameter set node!");
		return;
	}


	if (!backgroundVolume || !foregroundVolume) {
		// 		vtkErrorMacro("SetDefaultDisplay: Invalid volumes!");
		this->SetForegroundImage(backgroundVolume,foregroundVolume,0.5);
		return;
	}
	//TODO: Volumes go back to gray value - perhaps we should rembemer previous color settings?
	backgroundVolume->GetDisplayNode()->SetAndObserveColorNodeID("vtkMRMLColorTableNodeGrey");
	foregroundVolume->GetDisplayNode()->SetAndObserveColorNodeID("vtkMRMLColorTableNodeGrey");

	// Set window and level the same for warped and reference volume.
	foregroundVolume->GetScalarVolumeDisplayNode()->AutoWindowLevelOff();
	// 	double window, level;
	// 	window = backgroundVolume->GetScalarVolumeDisplayNode()->GetWindow();
	// 	level = backgroundVolume->GetScalarVolumeDisplayNode()->GetLevel();
	foregroundVolume->GetScalarVolumeDisplayNode()->SetWindow(backgroundVolume->GetScalarVolumeDisplayNode()->GetWindow());
	foregroundVolume->GetScalarVolumeDisplayNode()->SetLevel(backgroundVolume->GetScalarVolumeDisplayNode()->GetLevel());
	this->SetForegroundImage(backgroundVolume,foregroundVolume,0.5);

	return;
}

//---Calculate Statistic of image-----------------------------------
void vtkSlicerRegistrationQualityLogic
::CalculateStatistics(vtkMRMLScalarVolumeNode *inputVolume, double statisticValues[4] ) {
	vtkNew<vtkImageAccumulate> StatImage;
	StatImage->SetInput(inputVolume->GetImageData());
	StatImage->Update();
	statisticValues[0]= StatImage->GetMean()[0];
	statisticValues[1]= StatImage->GetStandardDeviation()[0];
	statisticValues[2]= StatImage->GetMax()[0];
	statisticValues[3]= StatImage->GetMin()[0];
}


//---Change backroung image and set opacity-----------------------------------
void vtkSlicerRegistrationQualityLogic
::SetForegroundImage(vtkMRMLScalarVolumeNode *backgroundVolume,
					 vtkMRMLScalarVolumeNode *foregroundVolume,
					 double opacity) {

	qSlicerApplication * app = qSlicerApplication::application();
	qSlicerLayoutManager * layoutManager = app->layoutManager();

	if (!layoutManager) {
		return;
	}

	vtkMRMLSliceCompositeNode *rcn = vtkMRMLSliceCompositeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeRed"));
	vtkMRMLSliceCompositeNode *ycn = vtkMRMLSliceCompositeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeYellow"));
	vtkMRMLSliceCompositeNode *gcn = vtkMRMLSliceCompositeNode::SafeDownCast(
			this->GetMRMLScene()->GetNodeByID("vtkMRMLSliceCompositeNodeGreen"));

	vtkMRMLSliceLogic* redSliceLogic = rcn != NULL ?
			GetMRMLApplicationLogic()->GetSliceLogicByLayoutName(rcn->GetLayoutName()) : NULL;
	if (redSliceLogic == NULL) {
		vtkErrorMacro("SetForegroundImage: Invalid SliceLogic!");
		return;
	}
	// Link Slice Controls
	rcn->SetLinkedControl(1);
	ycn->SetLinkedControl(1);
	gcn->SetLinkedControl(1);

	// Set volumes and opacity for all three layers.
	if (backgroundVolume) {
		rcn->SetBackgroundVolumeID(backgroundVolume->GetID());
	} else {
		rcn->SetBackgroundVolumeID(NULL);
	}

	if (foregroundVolume) {
		rcn->SetForegroundVolumeID(foregroundVolume->GetID());
	} else {
		rcn->SetForegroundVolumeID(NULL);
	}

	rcn->SetForegroundOpacity(opacity);
	redSliceLogic->StartSliceCompositeNodeInteraction(
		vtkMRMLSliceCompositeNode::ForegroundVolumeFlag
		| vtkMRMLSliceCompositeNode::BackgroundVolumeFlag);
	redSliceLogic->EndSliceCompositeNodeInteraction();
	return;
}
//----------------------------------------------------------------------------
