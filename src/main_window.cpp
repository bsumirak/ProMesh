//	created by Sebastian Reiter
//	s.b.reiter@googlemail.com
//	y09 m10 d01

#include <iostream>
#include <QtWidgets>
#include <QDesktopServices>
#include "main_window.h"
#include "view3d/view3d.h"
#include "scene/lg_scene.h"
#include "scene_inspector.h"
#include "scene_item_model.h"
#include "clip_plane_widget.h"
#include "QDebugStream.h"
#include "lib_grid/lib_grid.h"
#include "lib_grid/file_io/file_io_ug.h"
#include "lib_grid/file_io/file_io_ugx.h"
#include "lib_grid/file_io/file_io_lgb.h"
#include "tools/standard_tools.h"
#include "undo.h"
#include "app.h"
#include "tools/tool_coordinates.h"
#include "common/util/file_util.h"
#include "bridge/bridge.h"
#include "common/util/path_provider.h"
#include "common/util/plugin_util.h"
#include "util/file_util.h"
#include "tools/tool_manager.h"
#include "widgets/tool_browser_widget.h"

#ifdef PROMESH_USE_WEBKIT
	#include "widgets/help_browser.h"
#endif

//tmp
#include <QDir>
#include <QFile>
#include <QFileInfo>

using namespace std;
using namespace ug;

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//	implementation of MainWindow

////////////////////////////////////////////////////////////////////////
//	constructor
MainWindow::MainWindow() :
	m_settings("G-CSC", "ProMesh4"),
	m_selectionElement(0),
	m_selectionMode(0),
	m_elementModeListIndex(3),
	m_mouseMoveAction(MMA_DEFAULT),
	m_activeAxis(X_AXIS | Y_AXIS | Z_AXIS),
	m_activeObject(NULL),
	#ifdef PROMESH_USE_WEBKIT
		m_helpBrowser(NULL),
	#endif
	m_dlgAbout(NULL)
{
}

void MainWindow::init()
{ 
	setObjectName(tr("main_window"));
	setAcceptDrops(true);
	setGeometry(0, 0, 1024, 600);

//	create view and scene
	m_pView = new View3D;
	setCentralWidget(m_pView);
	connect(m_pView, SIGNAL(mousePressed(QMouseEvent*)),
			this, SLOT(view3dMousePressed(QMouseEvent*)));
	connect(m_pView, SIGNAL(mouseMoved(QMouseEvent*)),
			this, SLOT(view3dMouseMoved(QMouseEvent*)));
	connect(m_pView, SIGNAL(mouseReleased(QMouseEvent*)),
			this, SLOT(view3dMouseReleased(QMouseEvent*)));
	connect(m_pView, SIGNAL(keyReleased(QKeyEvent*)),
			this, SLOT(view3dKeyReleased(QKeyEvent*)));

	m_scene = new LGScene;

	m_pView->set_renderer(m_scene);
	connect(m_scene, SIGNAL(visuals_updated()),
			m_pView, SLOT(update()));

//	create the log widget
	m_pLog = new QDockWidget(tr("log"), this);
	m_pLog->setFeatures(QDockWidget::NoDockWidgetFeatures);
	m_pLog->setObjectName(tr("log"));

	QFont logFont("unknown");
	logFont.setStyleHint(QFont::Monospace);
	logFont.setPointSize(10);
	QTextEdit* pLogText = new QTextEdit(m_pLog);
	pLogText->setReadOnly(true);
	pLogText->setAcceptRichText(false);
	pLogText->setUndoRedoEnabled(false);
	pLogText->setWordWrapMode(QTextOption::NoWrap);
	pLogText->setCurrentFont(logFont);
	m_pLog->setWidget(pLogText);

	addDockWidget(Qt::BottomDockWidgetArea, m_pLog);

//	redirect cout
	Q_DebugStream* pDebugStream = new Q_DebugStream(GetLogAssistant().logger(), pLogText);
	pDebugStream->enable_file_output(app::UserDataDir().path() + QString("/log.txt"));

	UG_LOG(GetFileContent(":/resources/greetings.txt").toStdString() << endl);


	try{
		ug::bridge::InitBridge();
		if(!ug::LoadPlugins(ug::PathProvider::get_path(PLUGIN_PATH).c_str(), "ug4/", ug::bridge::GetUGRegistry()))
		{
			UG_LOG("ERROR during initialization of plugins: LoadPlugins failed!\n");
		}
	}
	catch(ug::UGError& err){
		UG_LOG("ERROR during initialization of ug::bridge:\n");
		for(size_t i = 0; i < err.num_msg(); ++i){
			UG_LOG("  " << err.get_msg(i) << std::endl);
		}
	}


//	file-menu
	m_actNew = new QAction(tr("&New"), this);
	m_actNew->setIcon(QIcon(":images/filenew.png"));
	m_actNew->setShortcut(tr("Ctrl+N"));
	m_actNew->setStatusTip(tr("Create a new empty geometry-object."));
	connect(m_actNew, SIGNAL(triggered()), this, SLOT(newGeometry()));

	m_actOpen = new QAction(tr("&Open"), this);
	m_actOpen->setIcon(QIcon(":images/fileopen.png"));
	m_actOpen->setShortcut(tr("Ctrl+O"));
	m_actOpen->setStatusTip(tr("Load a geometry from file."));
	connect(m_actOpen, SIGNAL(triggered()), this, SLOT(openFile()));

	m_actReload = new QAction(tr("&Reload"), this);
	//m_actReload->setIcon(QIcon(":images/fileopen.png"));
	m_actReload->setShortcut(tr("F5"));
	m_actReload->setStatusTip(tr("Reloads the active geometry."));
	connect(m_actReload, SIGNAL(triggered()), this, SLOT(reloadActiveGeometry()));

	m_actReloadAll = new QAction(tr("Reload &All"), this);
	//m_actReloadAll->setIcon(QIcon(":images/fileopen.png"));
	m_actReloadAll->setShortcut(tr("Ctrl+F5"));
	m_actReloadAll->setStatusTip(tr("Reloads all geometries."));
	connect(m_actReloadAll, SIGNAL(triggered()), this, SLOT(reloadAllGeometries()));

	m_actSave = new QAction(tr("&Save"), this);
	m_actSave->setIcon(QIcon(":images/filesave.png"));
	m_actSave->setShortcut(tr("Ctrl+S"));
	m_actSave->setStatusTip(tr("Saves a geometry to a file."));
	connect(m_actSave, SIGNAL(triggered()), this, SLOT(saveToFile()));

	m_actErase = new QAction(tr("&Erase"), this);
	m_actErase->setIcon(QIcon(":images/erase.png"));
	m_actErase->setShortcut(tr("Ctrl+E"));
	m_actErase->setStatusTip(tr("erases the selected geometry from the scene."));
	connect(m_actErase, SIGNAL(triggered()), this, SLOT(eraseActiveSceneObject()));

	m_actExportUG3 = new QAction(tr("Export to ug3"), this);
	m_actExportUG3->setStatusTip(tr("Exports the geometry to ug3 lgm / ng format."));
	connect(m_actExportUG3, SIGNAL(triggered()), this, SLOT(exportToUG3()));

	m_actQuit = new QAction(tr("Quit"), this);
	connect(m_actQuit, SIGNAL(triggered()), this, SLOT(quit()));

	QMenu* filemenu = menuBar()->addMenu("&File");
	filemenu->addAction(m_actNew);
	filemenu->addAction(m_actOpen);
	filemenu->addAction(m_actReload);
	filemenu->addAction(m_actReloadAll);
	filemenu->addAction(m_actSave);
	filemenu->addAction(m_actErase);
	filemenu->addSeparator();
	filemenu->addAction(m_actExportUG3);
	filemenu->addSeparator();
	filemenu->addAction(m_actQuit);


//	script menu
	m_actNewScript = new QAction(tr("New Script"), this);
	m_actNewScript->setStatusTip("Creates a new script and opens it for editing");
	connect(m_actNewScript, SIGNAL(triggered()), this, SLOT(newScript()));

	m_actEditScript = new QAction(tr("Edit Script"), this);
	m_actEditScript->setStatusTip("Opens a script for editing");
	connect(m_actEditScript, SIGNAL(triggered()), this, SLOT(editScript()));

	m_actBrowseUserScripts = new QAction(tr("Browse User Scripts"), this);
	m_actBrowseUserScripts->setStatusTip("Opens the path at which user scripts are located.");
	connect(m_actBrowseUserScripts, SIGNAL(triggered()), this, SLOT(browseUserScripts()));
	
	m_actRefreshToolDialogs = new QAction(tr("Refresh Tool Dialogs"), this);
	m_actRefreshToolDialogs->setShortcut(tr("Ctrl+T"));
	m_actRefreshToolDialogs->setStatusTip("Refreshes contents of tht tool-dialogs.");
	connect(m_actRefreshToolDialogs, SIGNAL(triggered()), this, SLOT(refreshToolDialogsClicked()));

	QMenu* scriptmenu = menuBar()->addMenu("&Scripts");
	scriptmenu->addAction(m_actNewScript);
	scriptmenu->addAction(m_actEditScript);
	scriptmenu->addSeparator();
	scriptmenu->addAction(m_actBrowseUserScripts);
	scriptmenu->addSeparator();
	scriptmenu->addAction(m_actRefreshToolDialogs);

//	help menu
	m_actHelp = new QAction(tr("&User Manual"), this);
	m_actHelp->setShortcut(tr("Ctrl+U"));
	connect(m_actHelp, SIGNAL(triggered()), this, SLOT(showHelp()));

	m_actJumpToScriptReference = new QAction(tr("Script and Tools Reference"), this);
	connect(m_actJumpToScriptReference, SIGNAL(triggered()), this, SLOT(showScriptReference()));

	m_actLicense = new QAction(tr("License"), this);
	connect(m_actLicense, SIGNAL(triggered()), this, SLOT(showLicense()));

	m_actShortcuts = new QAction(tr("Shortcuts"), this);
	connect(m_actShortcuts, SIGNAL(triggered()), this, SLOT(showShortcuts()));

	m_actRecentChanges = new QAction(tr("Recent Changes"), this);
	connect(m_actRecentChanges, SIGNAL(triggered()), this, SLOT(showRecentChanges()));

	m_actShowAbout = new QAction(tr("About"), this);
	connect(m_actShowAbout, SIGNAL(triggered()), this, SLOT(showAbout()));


	QMenu* helpmenu = menuBar()->addMenu("&Help");
	helpmenu->addAction(m_actHelp);
	helpmenu->addSeparator();
	helpmenu->addAction(m_actJumpToScriptReference);
	helpmenu->addAction(m_actShortcuts);
	helpmenu->addAction(m_actRecentChanges);
	helpmenu->addAction(m_actLicense);
	helpmenu->addAction(m_actShowAbout);


	m_toolManager = new ToolManager(this);
	try{
		RegisterStandardTools(m_toolManager);
	}
	catch(UGError& err){
		UG_LOG("ERROR: ")
		for(size_t i = 0; i < err.num_msg(); ++i){
			if(i > 0){
				UG_LOG("       ");
			}
			UG_LOG(err.get_msg(i) << endl);
		}
		UG_LOG("------------------------------------------------------------------------------------------\n")
	}

//	create a tool bar for file handling
	QToolBar* fileToolBar = addToolBar(tr("&File"));
	fileToolBar->setObjectName(tr("file_toolbar"));
	fileToolBar->addAction(m_actNew);
	fileToolBar->addAction(m_actOpen);
	fileToolBar->addAction(m_actSave);
	fileToolBar->addAction(m_actErase);

//	undo
	QAction* actUndo = new QAction(tr("Undo"), fileToolBar);
	actUndo->setIcon(QIcon(":images/editundo.png"));
	actUndo->setShortcut(tr("Ctrl+Z"));
	actUndo->setToolTip(tr("undo"));
	fileToolBar->addAction(actUndo);
	connect(actUndo, SIGNAL(triggered()), this, SLOT(undo()));

//	redo
	QAction* actRedo = new QAction(tr("Redo"), fileToolBar);
	actRedo->setIcon(QIcon(":images/editredo.png"));
	actRedo->setShortcut(tr("Ctrl+Z"));
	actRedo->setToolTip(tr("redo"));
	fileToolBar->addAction(actRedo);
	connect(actRedo, SIGNAL(triggered()), this, SLOT(redo()));

//	create a tool bar for visibility
	createVisibilityToolbar();

//	create the file dialog.
	m_dlgGeometryFiles = new QFileDialog(this);


//////// DOCK WIDGETS
	setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);

//	create the scene inspector
	QDockWidget* pSceneInspectorDock = new QDockWidget(tr("Scene Inspector"), this);
	pSceneInspectorDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
	pSceneInspectorDock->setObjectName(tr("scene_inspector_dock"));
	m_sceneInspector = new SceneInspector(pSceneInspectorDock);
	m_sceneInspector->setObjectName(tr("scene_inspector"));
	m_sceneInspector->setScene(m_scene);

	pSceneInspectorDock->setWidget(m_sceneInspector);
	addDockWidget(Qt::RightDockWidgetArea, pSceneInspectorDock);

//	create the clip-plane widget
	QDockWidget* pClipPlaneDock= new QDockWidget(tr("Clip Planes"), this);
	pClipPlaneDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
	pClipPlaneDock->setObjectName(tr("clip_plane_widget_dock"));
	ClipPlaneWidget* clipPlaneWidget = new ClipPlaneWidget(pClipPlaneDock);
	clipPlaneWidget->setObjectName(tr("clip_plane_widget"));
	clipPlaneWidget->setScene(m_scene);
	pClipPlaneDock->setWidget(clipPlaneWidget);
	addDockWidget(Qt::RightDockWidgetArea, pClipPlaneDock);

//	create the rclick menu for the scene-inspector
	m_rclickMenu_SI = new RClickMenu_SceneInspector(m_sceneInspector);
	m_rclickMenu_SI->setVisible(false);
	connect(m_sceneInspector, SIGNAL(mouseClicked(QMouseEvent*)),
			this, SLOT(sceneInspectorClicked(QMouseEvent*)));


//	create the tool browser
	m_toolBrowserDock = new QDockWidget(tr("Tool Browser"), this);
	m_toolBrowserDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
	m_toolBrowserDock->setObjectName(tr("tool_browser_dock"));

	m_toolBrowser = new ToolBrowser(this);
	m_toolBrowser->refresh(m_toolManager);
	m_toolBrowser->setObjectName(tr("tool_browser"));
	m_toolBrowserDock->setWidget(m_toolBrowser);
	addDockWidget(Qt::LeftDockWidgetArea, m_toolBrowserDock);


	restoreGeometry(settings().value("mainWindow/geometry").toByteArray());
	restoreState(settings().value("mainWindow/windowState").toByteArray());

//	init the status bar
	statusBar()->show();

//	init undo
	QDir tmpPath(QDir::tempPath());
	if(!tmpPath.exists("ProMesh"))
		tmpPath.mkdir("ProMesh");
	tmpPath.cd("ProMesh");
	if(!UndoHistoryProvider::inst().init(tmpPath.path().toLocal8Bit().constData())){
		cout << "initialization of undo failed. couldn't create history path at "
			 << tmpPath.path().toLocal8Bit().constData() << "\n";
	}

	show();
}

MainWindow::~MainWindow()
{
}

QToolBar* MainWindow::createVisibilityToolbar()
{
	QToolBar* visToolBar = addToolBar(tr("&Visibility"));
	visToolBar->setObjectName(tr("visibility_toolbar"));

//	layer for front:
	QLabel* lblFront = new QLabel(visToolBar);
	lblFront->setText(tr(" front: "));
	visToolBar->addWidget(lblFront);

//	combo-box for front-render mode.
	QComboBox* visFront = new QComboBox(visToolBar);
	visToolBar->addWidget(visFront);
	visFront->addItem(tr("none"));
	visFront->addItem(tr("wire"));
	visFront->addItem(tr("solid"));
	visFront->addItem(tr("solid + wire"));

//	connect signals and slots
	connect(visFront, SIGNAL(currentIndexChanged(int)),
			this, SLOT(frontDrawModeChanged(int)));

//	init the value
	visFront->setCurrentIndex(3);

//	seperator
	visToolBar->addSeparator();

//	layer for back:
	QLabel* lblBack = new QLabel(visToolBar);
	lblBack->setText(tr(" back: "));
	visToolBar->addWidget(lblBack);

//	combo-box for front-render mode.
	QComboBox* visBack = new QComboBox(visToolBar);
	visToolBar->addWidget(visBack);
	visBack->addItem(tr("none"));
	visBack->addItem(tr("wire"));
	visBack->addItem(tr("solid"));
	visBack->addItem(tr("solid + wire"));

//	connect signals and slots
	connect(visBack, SIGNAL(currentIndexChanged(int)),
			this, SLOT(backDrawModeChanged(int)));

//	init the value
	visBack->setCurrentIndex(3);


//	add a combo-box for the element-mode
/*
	visToolBar->addSeparator();
	QComboBox* elemMode = new QComboBox(visToolBar);
	visToolBar->addWidget(elemMode);
	elemMode->addItem(tr("draw vertices"));
	elemMode->addItem(tr("draw edges"));
	elemMode->addItem(tr("draw faces"));
	elemMode->addItem(tr("draw volumes"));
	elemMode->setCurrentIndex(3);
	m_elementModeListIndex = 3;

//	connect signals and slots
	connect(elemMode, SIGNAL(currentIndexChanged(int)),
			this, SLOT(elementDrawModeChanged(int)));
*/
	visToolBar->addSeparator();
	m_tbRenderVrts = new QToolButton(visToolBar);
	m_tbRenderVrts->setIcon(QIcon(":images/icon_render_vertices.png"));
	m_tbRenderVrts->setCheckable(true);
	m_tbRenderVrts->setChecked(true);
	m_tbRenderVrts->setAutoExclusive(false);
	m_tbRenderVrts->setToolTip(tr("render vertices"));
	visToolBar->addWidget(m_tbRenderVrts);
	connect(m_tbRenderVrts, SIGNAL(toggled(bool)),
			this, SLOT(elementDrawModeChanged()));

	m_tbRenderEdges = new QToolButton(visToolBar);
	m_tbRenderEdges->setIcon(QIcon(":images/icon_render_edges.png"));
	m_tbRenderEdges->setCheckable(true);
	m_tbRenderEdges->setChecked(true);
	m_tbRenderEdges->setAutoExclusive(false);
	m_tbRenderEdges->setToolTip(tr("render edges"));
	visToolBar->addWidget(m_tbRenderEdges);
	connect(m_tbRenderEdges, SIGNAL(toggled(bool)),
			this, SLOT(elementDrawModeChanged()));

	m_tbRenderFaces = new QToolButton(visToolBar);
	m_tbRenderFaces->setIcon(QIcon(":images/icon_render_faces.png"));
	m_tbRenderFaces->setCheckable(true);
	m_tbRenderFaces->setChecked(true);
	m_tbRenderFaces->setAutoExclusive(false);
	m_tbRenderFaces->setToolTip(tr("render faces"));
	visToolBar->addWidget(m_tbRenderFaces);
	connect(m_tbRenderFaces, SIGNAL(toggled(bool)),
			this, SLOT(elementDrawModeChanged()));

	m_tbRenderVols = new QToolButton(visToolBar);
	m_tbRenderVols->setIcon(QIcon(":images/icon_render_volumes.png"));
	m_tbRenderVols->setCheckable(true);
	m_tbRenderVols->setChecked(true);
	m_tbRenderVols->setAutoExclusive(false);
	m_tbRenderVols->setToolTip(tr("render volumes"));
	visToolBar->addWidget(m_tbRenderVols);
	connect(m_tbRenderVols, SIGNAL(toggled(bool)),
			this, SLOT(elementDrawModeChanged()));


//	add a combo-box for the selection elements
/*
	visToolBar->addSeparator();
	QComboBox* selElems = new QComboBox(visToolBar);
	visToolBar->addWidget(selElems);
	selElems->addItem(tr("select vertices"));
	selElems->addItem(tr("select edges"));
	selElems->addItem(tr("select faces"));
	selElems->addItem(tr("select volumes"));

//	connect signals and slots
	connect(selElems, SIGNAL(currentIndexChanged(int)),
			this, SLOT(selectionElementChanged(int)));

//	init the value
	selElems->setCurrentIndex(0);
*/

	visToolBar->addSeparator();
	m_tbSelVrts = new QToolButton(visToolBar);
	m_tbSelVrts->setIcon(QIcon(":images/icon_vertices.png"));
	m_tbSelVrts->setCheckable(true);
	m_tbSelVrts->setAutoExclusive(true);
	m_tbSelVrts->setToolTip(tr("select vertices"));
	visToolBar->addWidget(m_tbSelVrts);
	connect(m_tbSelVrts, SIGNAL(toggled(bool)),
			this, SLOT(selectionElementChanged(bool)));

	m_tbSelEdges = new QToolButton(visToolBar);
	m_tbSelEdges->setIcon(QIcon(":images/icon_edges.png"));
	m_tbSelEdges->setCheckable(true);
	m_tbSelEdges->setAutoExclusive(true);
	m_tbSelEdges->setToolTip(tr("select edges"));
	visToolBar->addWidget(m_tbSelEdges);
	connect(m_tbSelEdges, SIGNAL(toggled(bool)),
			this, SLOT(selectionElementChanged(bool)));

	m_tbSelFaces = new QToolButton(visToolBar);
	m_tbSelFaces->setIcon(QIcon(":images/icon_faces.png"));
	m_tbSelFaces->setCheckable(true);
	m_tbSelFaces->setAutoExclusive(true);
	m_tbSelFaces->setToolTip(tr("select faces"));
	visToolBar->addWidget(m_tbSelFaces);
	connect(m_tbSelFaces, SIGNAL(toggled(bool)),
			this, SLOT(selectionElementChanged(bool)));

	m_tbSelVols = new QToolButton(visToolBar);
	m_tbSelVols->setIcon(QIcon(":images/icon_volumes.png"));
	m_tbSelVols->setCheckable(true);
	m_tbSelVols->setAutoExclusive(true);
	m_tbSelVols->setToolTip(tr("select volumes"));
	visToolBar->addWidget(m_tbSelVols);
	connect(m_tbSelVols, SIGNAL(toggled(bool)),
			this, SLOT(selectionElementChanged(bool)));

	if(!m_tbSelVrts->isChecked())
		m_tbSelVrts->toggle();

//	add a combo-box for the selection modes
	visToolBar->addSeparator();
	QComboBox* selModes = new QComboBox(visToolBar);
	visToolBar->addWidget(selModes);
	selModes->addItem(QIcon(":images/icon_click_select.png"), tr(""));
	selModes->addItem(QIcon(":images/icon_box_select_cut.png"), tr(""));
	selModes->addItem(QIcon(":images/icon_box_select.png"), tr(""));

//	connect signals and slots
	connect(selModes, SIGNAL(currentIndexChanged(int)),
			this, SLOT(selectionModeChanged(int)));

//	init the value
	selModes->setCurrentIndex(0);



//	add a color-picker for the background color
	visToolBar->addSeparator();

//	layer for color:
	QLabel* lblColor = new QLabel(visToolBar);
	lblColor->setText(tr(" bg-color: "));
	visToolBar->addWidget(lblColor);

	m_bgColor = new ColorWidget(visToolBar);
	visToolBar->addWidget(m_bgColor);
	connect(m_bgColor, SIGNAL(colorChanged(QColor)),
			this, SLOT(backgroundColorChanged(QColor)));

	m_bgColor->setFixedWidth(24);
	m_bgColor->setFixedHeight(24);

	QColor defaultColor(100, 100, 100);
	if(settings().contains("bg-color")){
		QVariant value = settings().value("bg-color", defaultColor);
		if(value.canConvert<QColor>())
			m_bgColor->setColor(value.value<QColor>());
		else
			m_bgColor->setColor(defaultColor);
	}
	else
		m_bgColor->setColor(defaultColor);

	return visToolBar;
}

uint MainWindow::getLGElementMode()
{
	switch(m_elementModeListIndex){
		case 0: return LGEM_VERTEX;
		case 1: return LGEM_EDGE;
		case 2: return LGEM_FACE;
		case 3: return LGEM_VOLUME;
		default: return LGEM_NONE;
	}
}

bool MainWindow::load_grid_from_file(const char* filename)
{
	static bool bFirstLoad = true;

	try{
		LGObject* pObj = CreateLGObjectFromFile(filename);

	//	add it to the scene
		if(pObj)
		{
			pObj->set_element_mode(getLGElementMode());
			int index = m_scene->add_object(pObj);
			if(index != -1)
			{
				setActiveObject(index);

			//	if this is the first object loaded, we will focus it.
				if(bFirstLoad)
				{
					bFirstLoad = false;
					ug::Sphere3 s = pObj->get_bounding_sphere();
					m_pView->fly_to(cam::vector3(s.get_center().x(),
													s.get_center().y(),
													s.get_center().z()),
									s.get_radius() * 3.f);
				}

				return true;
			}
		}
	}
	catch(UGError err){
		UG_LOG("ERROR: " << err.get_msg() << endl);
		return false;
	}

	return false;
}

LGObject* MainWindow::create_empty_object(const char* name)
{
//	create a new object
    LGObject* pObj = CreateEmptyLGObject(name);
    pObj->set_element_mode(getLGElementMode());

//	add it to the scene
	int index = m_scene->add_object(pObj);
	if(index != -1)
		setActiveObject(index);

	return pObj;
}

bool MainWindow::save_object_to_file(ISceneObject* obj, const char* filename)
{
	LGObject* lgobj = dynamic_cast<LGObject*>(obj);
	if(lgobj){
		try{
			return SaveLGObjectToFile(lgobj, filename);
		}
		catch(UGError err){
			UG_LOG("ERROR: " << err.get_msg() << endl);
			return false;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////
//	public slots
void MainWindow::newGeometry()
{
	create_empty_object("newObject");
}

int MainWindow::openFile()
{
	int numOpened = 0;

	QString path = settings().value("file-path", ".").toString();

	QStringList fileNames = QFileDialog::getOpenFileNames(
								this,
								tr("Load Geometry"),
								path,
								tr("geometry files (*.ugx *.vtu *.lgb *.obj *.txt *.art *.net *.dat *.lgm *.ng *.ele *.dump *.msh *.stl *.asc *.ASC)"));

	for(QStringList::iterator iter = fileNames.begin();
		iter != fileNames.end(); ++iter)
	{
		settings().setValue("file-path", QFileInfo(*iter).absolutePath());
	//	load the object
		if(load_grid_from_file((*iter).toLocal8Bit().constData()))
			++numOpened;
		else
		{
			QMessageBox msg(this);
			QString str = tr("Load failed: ");
			str.append(*iter);
			msg.setText(str);
			msg.exec();
		}
	}

	return numOpened;
}

bool MainWindow::reloadActiveGeometry()
{
	LGObject* obj = app::getActiveObject();
	if(obj){
		return ReloadLGObject(obj);
	}
	return false;
}

bool MainWindow::reloadAllGeometries()
{
	LGScene* scene = app::getActiveScene();
	if(scene){
		bool success = true;
		for (int i = 0; i < scene->num_objects(); ++i){
			success &= ReloadLGObject(scene->get_object(i));
		}
		return success;
	}
	return false;
}

bool MainWindow::saveToFile()
{
	bool saveFailed = false;
	ISceneObject* obj = m_sceneInspector->getActiveObject();
	if(obj)
	{
		QString path = settings().value("file-path", ".").toString();
		path.append("/").append(obj->name()).append(".ugx");

		QString fileName = QFileDialog::getSaveFileName(
									this,
									tr("Save Geometry"),
									path,
									tr("geometry files (*.ugx *.vtu *.ncdf *.lgb *.obj *.txt *.ele *.smesh *.stl *.tex *.tikz)"));

		if(!fileName.isEmpty())
		{
			settings().setValue("file-path", QFileInfo(fileName).absolutePath());
		//	save the object
			if(!save_object_to_file(obj, fileName.toLocal8Bit().constData()))
				saveFailed = true;
			else{
				obj->set_name(QFileInfo(fileName).baseName().toLocal8Bit().constData());
				obj->visuals_changed();
			}
		}
	}

	if(saveFailed)
	{
		QMessageBox msg(this);
		QString str = tr("Save failed: ");
		if(!obj)
			str.append("no object selected.");
		else
		{
			str.append("make sure that the selected filename has a valid suffix.\n");
			str.append("valid suffixes are: *.ugx *.ncdf *.lgb *.obj *.txt *.ele");
		}
		msg.setText(str);
		msg.exec();
		return false;
	}

	return true;
}

bool MainWindow::exportToUG3()
{
	bool saveFailed = false;

	LGObject* obj = dynamic_cast<LGObject*>(m_sceneInspector->getActiveObject());
	if(obj)
	{

		QString path = settings().value("file-path", ".").toString();

		QString fileName = QFileDialog::getSaveFileName(
									this,
									tr("Save Geometry"),
									path);
		try{
			if(!fileName.isEmpty())
			{
				settings().setValue("file-path", QFileInfo(fileName).absolutePath());

			//	get the filenames
				QFileInfo fileInfo(fileName);
				QString prefix = fileInfo.absolutePath();
				prefix.append("/").append(fileInfo.baseName());
				Grid& g = obj->grid();
				SubsetHandler& sh = obj->subset_handler();

				if(g.num_volumes() > 0){
				//	Create the subset-handlers
					SubsetHandler shFaces(g, SHE_FACE);
					SubsetHandler shVolumes(g, SHE_VOLUME);

					for(int i = 0; i < sh.num_subsets(); ++i){
						shFaces.assign_subset(sh.begin<Face>(i), sh.end<Face>(i), i);
						shVolumes.assign_subset(sh.begin<Volume>(i), sh.end<Volume>(i), i);
					}

					UG_LOG("Exporting to UG3 3D ... ");
				//	export the grid
					saveFailed = !ExportGridToUG(g, shFaces, shVolumes, prefix.toLocal8Bit().constData(),
												"tmpLGMName", "tmpProblemName", 0);
				}
				else{
					UG_LOG("Exporting to UG3 2D ... ");
				//	export the grid
					saveFailed = !ExportGridToUG_2D(g, prefix.toLocal8Bit().constData(),
													"tmpLGMName", "tmpProblemName", 0, &sh);
				}

				if(saveFailed){
					UG_LOG("failed\n");
				}
				else{
					UG_LOG("done\n");
				}
			}
		}
		catch(UGError err){
			UG_LOG("\n");
			UG_LOG("ERROR: " << err.get_msg() << endl);
		}
	}

	if(saveFailed)
	{
		QMessageBox msg(this);
		QString str = tr("Export failed: ");
		str.append(tr("Make sure that all subsets are consecutive. That means:\n"));
		str.append(tr("  * A subset that contains no faces may not be followed by a subset that contains faces.\n"));
		str.append(tr("  * A subset that contains no volumes may not be followed by a subset that contains volumes.\n"));
		str.append(tr("  Consider calling Tools->Subsets->Adjust Subsets For UG3\n"));
		msg.setText(str);
		msg.exec();
		return false;
	}

	return true;
}

void MainWindow::eraseActiveSceneObject()
{
//	get the active object from the scene-inspector
	ISceneObject* obj = m_sceneInspector->getActiveObject();
	if(obj)
	{
	//	get the objects index and erase it from the scene
		int index = m_scene->get_object_index(obj);
		if(index >= 0)
		{
			QString msg = QString("Erase '").append(obj->name()).append("'?\n").
											append("No undo will be possible!");
			QMessageBox::StandardButton reply;
			reply = QMessageBox::question(this, "Erase?", msg,
										  QMessageBox::Yes | QMessageBox::No);
			if (reply == QMessageBox::Yes){
			//	perform erase
				m_scene->erase_object(index);
			//	select the next object
				if(index < m_scene->num_objects())
					setActiveObject(index);
				else if(index > 0)
					setActiveObject(index - 1);
			}
		}
	}
}

LGObject* MainWindow::getActiveObject()
{
	return dynamic_cast<LGObject*>(m_sceneInspector->getActiveObject());
}

void MainWindow::setActiveObject(int index)
{
	m_sceneInspector->setActiveObject(index);
	if(getActiveObject() != m_activeObject){
		m_activeObject = getActiveObject();
		emit activeObjectChanged();
	}
}

void MainWindow::launchHelpBrowser(const QString& pageName)
{
	#ifdef PROMESH_USE_WEBKIT
		if(!m_helpBrowser)
			m_helpBrowser = new QHelpBrowser(this);
		m_helpBrowser->browse(QUrl(QString("qrc:///docs/html/").append(pageName)));
		QRect geom = this->geometry();
		geom.adjust(50, 50, -50, -50);
		m_helpBrowser->setGeometry(geom);
		m_helpBrowser->show();
	#else
		try{
		//todo:	on could compare the version.txt files in the resource and the target
		//		folder and only copy if they do not match.
			QString helpHtmlPath = app::UserHelpDir().path().append("/html");
			static bool firstRun = true;
			if(firstRun){
				firstRun = false;
				if(FileExists(helpHtmlPath))
					EraseDirectory(helpHtmlPath);
				CopyDirectory(":/docs/html", app::UserHelpDir().path());
			}
			QDesktopServices::openUrl(helpHtmlPath + "/" + pageName);
		}
		catch(UGError& err){
			UG_LOG("ERROR: " << err.get_msg() << endl);
		}
	#endif
}

void MainWindow::showHelp()
{
	launchHelpBrowser("index.html");
}

void MainWindow::showRecentChanges()
{
	launchHelpBrowser("pageRecentChanges.html");
}

void MainWindow::showShortcuts()
{
	launchHelpBrowser("pageShortcuts.html");
}

void MainWindow::showLicense()
{
	launchHelpBrowser("pageProMeshLicense.html");
}

void MainWindow::showScriptReference()
{
	launchHelpBrowser("modules.html");
}

void MainWindow::showAbout()
{
	launchHelpBrowser("pageAbout.html");
}

void MainWindow::frontDrawModeChanged(int newMode)
{
	m_scene->set_draw_mode_front(newMode);
}

void MainWindow::backDrawModeChanged(int newMode)
{
	m_scene->set_draw_mode_back(newMode);
}

void MainWindow::backgroundColorChanged(const QColor& color)
{
	m_pView->set_background_color(color);
	settings().setValue("bg-color", color);
}

void MainWindow::selectionElementChanged(int newElement)
{
	m_selectionElement = newElement;
}

void MainWindow::selectionElementChanged(bool)
{
	if(m_tbSelVrts->isChecked())
		m_selectionElement = 0;
	else if(m_tbSelEdges->isChecked())
		m_selectionElement = 1;
	else if(m_tbSelFaces->isChecked())
		m_selectionElement = 2;
	else if(m_tbSelVols->isChecked())
		m_selectionElement = 3;
}

void MainWindow::selectionModeChanged(int newMode)
{
	m_selectionMode = newMode;
}

void MainWindow::elementDrawModeChanged()
{
/*
	m_elementModeListIndex = newMode;
//	update rendering mode in all elements.
//todo: allow render mode on per-object basis.
	uint lgElemMode = getLGElementMode();
	for(int i = 0; i < m_scene->num_objects(); ++i){
		LGObject* pObj = m_scene->get_object(i);
		pObj->set_element_mode(lgElemMode);
		pObj->visuals_changed();
	}
*/

	m_scene->set_element_draw_mode(m_tbRenderVrts->isChecked(), m_tbRenderEdges->isChecked(),
								   m_tbRenderFaces->isChecked(), m_tbRenderVols->isChecked());
	m_scene->update_visuals();
}

////////////////////////////////////////////////////////////////////////
//	events
void MainWindow::closeEvent(QCloseEvent *event)
{
	if(m_scene->num_objects() == 0)
		event->accept();
	else{
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(this, "Quit?", "Quit? Unsaved progress will be lost!",
									  QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::Yes){
			QMainWindow::closeEvent(event);
			event->accept();
		}
		else{
			event->ignore();
		}
	}

	if(event->isAccepted()){
		settings().setValue("mainWindow/geometry", saveGeometry());
		settings().setValue("mainWindow/windowState", saveState());
	}
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
	event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
	QList<QUrl> urls = event->mimeData()->urls();
	if (urls.isEmpty())
	   return;

	for(QList<QUrl>::iterator iter = urls.begin();
		iter != urls.end(); ++iter)
	{
		settings().setValue("file-path", QFileInfo((*iter).toLocalFile()).absolutePath());
		if(!load_grid_from_file((*iter).toLocalFile().toLatin1().constData()))
		{
			QMessageBox msg(this);
			QString str = tr("Load failed: ");
			str.append((*iter).toLocalFile());
			msg.setText(str);
			msg.exec();
		}
	}
}

void MainWindow::sceneInspectorClicked(QMouseEvent* event)
{
	if(event->button() == Qt::RightButton){
		m_rclickMenu_SI->exec(QCursor::pos());
	}

	if(getActiveObject() != m_activeObject){
		m_activeObject = getActiveObject();
		emit activeObjectChanged();
	}
}


void MainWindow::undo()
{
	LGObject* obj = app::getActiveObject();
	if(obj){
		if(!obj->undo()){
			UG_LOG("no more steps to undo.\n");
		}
	}
}

void MainWindow::redo()
{
	LGObject* obj = app::getActiveObject();
	if(obj){
		if(!obj->redo()){
			UG_LOG("no more steps to redo.\n");
		}
	}
}

void MainWindow::refreshToolDialogsClicked()
{
	RefreshScriptTools(m_toolManager);
	m_toolBrowser->refresh(m_toolManager);
}

void MainWindow::browseUserScripts()
{
	QDir scriptDir = app::UserScriptDir();
	QString path = QDir::toNativeSeparators(app::UserScriptDir().path());
	QDesktopServices::openUrl(QUrl("file:///" + path));
}

void MainWindow::newScript()
{
	QString fileName = QFileDialog::getSaveFileName(
									this,
									tr("Script Name"),
									QDir::toNativeSeparators(app::UserScriptDir().path()),
									tr("script files (*.lua)"));
	if(!fileName.endsWith(".lua"))
		fileName.append(".lua");

	if(!fileName.isEmpty()){
		if(QFile::exists(fileName))
			QFile::remove(fileName);
		QFile::copy(":/resources/default-script.lua", fileName);
		QFile::setPermissions(fileName, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
										QFileDevice::ReadUser | QFileDevice::WriteUser |
										QFileDevice::ReadGroup);

		QDesktopServices::openUrl(QUrl("file:///" + fileName));
	}
}

void MainWindow::editScript()
{
	QString fileName = QFileDialog::getOpenFileName(
									this,
									tr("Script Name"),
									QDir::toNativeSeparators(app::UserScriptDir().path()),
									tr("script files (*.lua)"));

	if(!fileName.isEmpty()){
		QDesktopServices::openUrl(QUrl("file:///" + fileName));
	}
}

void MainWindow::quit()
{
	this->close();
}