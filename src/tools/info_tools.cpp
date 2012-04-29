//	created by Sebastian Reiter
//	s.b.reiter@googlemail.com
//	y10 m05 d12

#include <vector>
#include "app.h"
#include "standard_tools.h"
#include "tetgen.h"

using namespace std;
using namespace ug;

class ToolPrintSelectionCenter : public ITool
{
	public:
		void execute(LGObject* obj, QWidget*){

			ug::Grid& grid = obj->get_grid();
			ug::Selector& sel = obj->get_selector();
			Grid::VertexAttachmentAccessor<APosition> aaPos(grid, aPosition);

			vector3 center;

		//	calculate and print the center
			if(CalculateCenter(center, sel, aaPos)){
				UG_LOG("selection center: " << center << endl);
			}
			else{
				UG_LOG("selection center: (-,-,-)\n");
			}
		}

		const char* get_name()		{return "Print Selection Center";}
		const char* get_tooltip()	{return "Calculates and prints the position of the center of the current selection.";}
		const char* get_group()		{return "Info";}
};

class ToolPrintGeometryInfo : public ITool
{
	public:
		void execute(LGObject* obj, QWidget*){

			ug::Grid& grid = obj->get_grid();
			vector3 vMin, vMax;
			vector3 vDim;
			obj->get_bounding_box(vMin, vMax);
			VecSubtract(vDim, vMax, vMin);

			UG_LOG("Geometry Info:\n");
			UG_LOG("  pivot:\t\t" << obj->pivot() << endl);
			UG_LOG("  bounding box:\t" << vMin << ", " << vMax << endl);
			UG_LOG("  dimensions:\t" << vDim << endl);

			UG_LOG("  vertices:\t" << grid.num<VertexBase>() << endl);
			UG_LOG("  edges:\t" << grid.num<EdgeBase>() << endl);
			UG_LOG("  faces:\t" << grid.num<Face>() << endl);
			UG_LOG("  volumes:\t " << grid.num<Volume>() << endl);

			UG_LOG(endl);
		}

		const char* get_name()		{return "Print Geometry Info";}
		const char* get_tooltip()	{return "Prints info about the current geometry";}
		const char* get_group()		{return "Info";}
};

class ToolPrintFaceQuality : public ITool
{
	public:
		void execute(LGObject* obj, QWidget*){
			using namespace ug;
			ug::Grid& grid = obj->get_grid();
			ug::Selector& sel = obj->get_selector();
			Grid::VertexAttachmentAccessor<APosition> aaPos(grid, aPosition);

			UG_LOG("face qualities:\n");
			for(FaceIterator iter = sel.begin<Face>(); iter != sel.end<Face>(); ++iter){
				UG_LOG("  " << FaceQuality(*iter, aaPos) << endl);
			}
			UG_LOG(endl);
		}

		const char* get_name()		{return "Print Face Quality";}
		const char* get_tooltip()	{return "Prints the quality of selected faces";}
		const char* get_group()		{return "Info";}
};

class ToolPrintSelectionInfo : public ITool
{
	public:
		void execute(LGObject* obj, QWidget*){
			using namespace ug;
			ug::Grid& grid = obj->get_grid();
			ug::Selector& sel = obj->get_selector();
			UG_LOG("Selection Info:\n");
			PrintElementNumbers(sel.get_geometric_objects());

		//	count the number of selected boundary faces
			if(grid.num_volumes() > 0 && sel.num<Face>() > 0){
				int numBndFaces = 0;
				for(FaceIterator iter = sel.faces_begin(); iter != sel.faces_end(); ++iter){
					if(IsVolumeBoundaryFace(grid, *iter))
						++numBndFaces;
				}
				UG_LOG("  selected boundary faces: " << numBndFaces << endl);
			}
			UG_LOG(endl);
		}

		const char* get_name()		{return "Print Selection Info";}
		const char* get_tooltip()	{return "Prints the quantities of selected elements";}
		const char* get_group()		{return "Info";}
};


template <class TGeomObj>
static bool SubsetContainsSelected(SubsetHandler& sh, Selector& sel, int si)
{
	typedef typename geometry_traits<TGeomObj>::iterator GeomObjIter;
	for(GeomObjIter iter = sh.begin<TGeomObj>(si);
	iter != sh.end<TGeomObj>(si); ++iter)
	{
		if(sel.is_selected(*iter)){
			return true;
		}
	}

	return false;
}

class ToolPrintSelectionContainingSubsets : public ITool
{
	public:
		void execute(LGObject* obj, QWidget*){
			using namespace ug;
			ug::Selector& sel = obj->get_selector();
			ug::SubsetHandler& sh = obj->get_subset_handler();

			UG_LOG("Selection containing subsets:");

		//	check for each subset whether it contains selected elements
			for(int i = 0; i < sh.num_subsets(); ++i){
			//	check vertices
				bool gotOne = SubsetContainsSelected<VertexBase>(sh, sel, i);
			//	check edges
				if(!gotOne)
					gotOne = SubsetContainsSelected<EdgeBase>(sh, sel, i);
			//	check faces
				if(!gotOne)
					gotOne = SubsetContainsSelected<Face>(sh, sel, i);
			//	check volumes
				if(!gotOne)
					gotOne = SubsetContainsSelected<Volume>(sh, sel, i);

				if(gotOne){
					UG_LOG(" " << i);
				}
			}

			UG_LOG(endl);
		}

		const char* get_name()		{return "Print Selection Containing Subsets";}
		const char* get_tooltip()	{return "Prints subset indices of all subsets, which contain a selected element.";}
		const char* get_group()		{return "Info";}
};


class ToolPrintVertexDistance : public ITool
{
	public:
		void execute(LGObject* obj, QWidget*){
			using namespace ug;
			ug::Grid& grid = obj->get_grid();
			ug::Selector& sel = obj->get_selector();
			Grid::VertexAttachmentAccessor<APosition> aaPos(grid, aPosition);
			UG_LOG("Vertex Distance:");

			number max = 0;
			number min = obj->get_bounding_sphere().get_radius() * 3.;

			vector<VertexBase*> vrts;
			CollectVerticesTouchingSelection(vrts, sel);

		//	if there are less than 2 vertices, both distances are set to 0
			if(vrts.size() < 2)
				min = max = 0;
			else{
			//	iterate over all selected vertices
				typedef vector<VertexBase*>::iterator VrtIter;
				for(VrtIter baseIter = vrts.begin();
					baseIter != vrts.end(); ++baseIter)
				{
					vector3 basePos = aaPos[*baseIter];

				//	iteate over all vertices between baseVrt and sel.end
					VrtIter iter = baseIter;
					for(iter++; iter != vrts.end(); ++iter){
						number dist = VecDistance(basePos, aaPos[*iter]);
						if(dist > max)
							max = dist;
						if(dist < min)
							min = dist;
					}
				}
			}

			UG_LOG("    min = " << min << ",    max = " << max << "\n");
		}

		const char* get_name()		{return "Print Vertex Distance";}
		const char* get_tooltip()	{return "Prints the min and max distance of vertices of selected elements.";}
		const char* get_group()		{return "Info";}
};

void RegisterInfoTools(ToolManager* toolMgr)
{
	toolMgr->set_group_icon("Info", ":images/tool_info.png");

	toolMgr->register_tool(new ToolPrintSelectionCenter, Qt::Key_I, SMK_ALT);
	toolMgr->register_tool(new ToolPrintGeometryInfo, Qt::Key_I);
	toolMgr->register_tool(new ToolPrintFaceQuality);
	toolMgr->register_tool(new ToolPrintSelectionInfo);
	toolMgr->register_tool(new ToolPrintSelectionContainingSubsets);
	toolMgr->register_tool(new ToolPrintVertexDistance);
}
