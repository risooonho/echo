#include "MainWindow.h"
#include "Studio.h"
#include "LogPanel.h"
#include "ResPanel.h"
#include "NodeTreePanel.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopservices>
#include <QShortcut>
#include <QMdiArea>
#include <QComboBox>
#include "TimelinePanel.h"
#include "DebuggerPanel.h"
#include "EchoEngine.h"
#include "PlayGameToolBar.h"
#include "QResSelect.h"
#include "ResChooseDialog.h"
#include "LuaEditor.h"
#include "BottomPanel.h"
#include "ProjectWnd.h"
#include "PathChooseDialog.h"
#include <engine/core/util/PathUtil.h>
#include <engine/core/io/IO.h>
#include <engine/core/scene/render_node.h>

namespace Studio
{
	static MainWindow* g_inst = nullptr;

	// 构造函数
	MainWindow::MainWindow(QMainWindow* parent/*=0*/)
		: QMainWindow( parent)
		, m_resPanel(nullptr)
		, m_gameProcess(nullptr)
		, m_playGameToolBar(nullptr)
	{
		setupUi( this);

		// 隐藏标题
		setWindowFlags(windowFlags() | Qt::FramelessWindowHint);

		// 设置菜单左上控件
		menubar->setTopLeftCornerIcon(":/icon/Icon/icon.png");

		// project operate
		QObject::connect(m_actionNewProject, SIGNAL(triggered(bool)), this, SLOT(onNewAnotherProject()));
		QObject::connect(m_actionOpenProject, SIGNAL(triggered(bool)), this, SLOT(onOpenAnotherProject()));
		QObject::connect(m_actionSaveProject, SIGNAL(triggered(bool)), this, SLOT(onSaveProject()));
		QObject::connect(m_actionSaveAsProject, SIGNAL(triggered(bool)), this, SLOT(onSaveasProject()));

		// connect scene operate signal slot
		QObject::connect(m_actionNewScene, SIGNAL(triggered(bool)), this, SLOT(onNewScene()));
		QObject::connect(m_actionSaveScene, SIGNAL(triggered(bool)), this, SLOT(onSaveScene()));
		QObject::connect(m_actionSaveAsScene, SIGNAL(triggered(bool)), this, SLOT(onSaveAsScene()));

		// connect signal slot
		QObject::connect(m_actionPlayGame, SIGNAL(triggered(bool)), this, SLOT(onPlayGame()));
		QObject::connect(m_actionStopGame, SIGNAL(triggered(bool)), &m_gameProcess, SLOT(terminate()));
		QObject::connect(m_actionExitEditor, SIGNAL(triggered(bool)), this, SLOT(close()));

		// add combox, switch 2D,3D,Script etc.
		m_subEditComboBox = new QComboBox(m_toolBar);
		m_subEditComboBox->addItem("2D");
		m_subEditComboBox->addItem("3D");
		m_subEditComboBox->addItem("Script");
		m_toolBar->addWidget(m_subEditComboBox);
		QObject::connect(m_subEditComboBox, SIGNAL(currentIndexChanged(const QString &)), this, SLOT(onSubEditChanged(const QString&)));

		EchoAssert(!g_inst);
		g_inst = this;
	}

	// 析构函数
	MainWindow::~MainWindow()
	{
	}

	MainWindow* MainWindow::instance()
	{
		return g_inst;
	}

	// 打开项目时调用
	void MainWindow::onOpenProject()
	{
		m_resPanel = EchoNew(ResPanel(this));
		m_scenePanel = EchoNew(NodeTreePanel(this));
		m_bottomPanel = EchoNew(BottomPanel(this));

		//QMdiArea* midArea = new QMdiArea(this);

		QWidget* renderWindow = AStudio::instance()->getRenderWindow();

		setCentralWidget(renderWindow);
		//m_tabWidget->addTab(renderWindow, "NodeTree");

		//midArea->addSubWindow(renderWindow);
		//m_playGameToolBar = EchoNew(PlayGameToolBar(centralWidget()));
		//centralWidget()->layout()->addWidget(m_playGameToolBar);
		//centralWidget()->layout()->addWidget(renderWindow);

		this->setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
		this->setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

		this->addDockWidget(Qt::LeftDockWidgetArea, m_resPanel);
		this->addDockWidget(Qt::RightDockWidgetArea, m_scenePanel);
		this->addDockWidget(Qt::BottomDockWidgetArea, m_bottomPanel);

		m_resPanel->onOpenProject();

		// update rencents project display
		updateRencentProjectsDisplay();
	}

	// new scene
	void MainWindow::onNewScene()
	{
		onSaveProject();
		m_scenePanel->clear();
		EchoEngine::instance()->newEditNodeTree();
	}

	// on save scene
	void MainWindow::onSaveScene()
	{
		onSaveProject();
	}

	// on save as scene
	void MainWindow::onSaveAsScene()
	{
		Echo::String savePath = PathChooseDialog::getExistingPathName(this, ".scene", "Save").toStdString().c_str();
		if (!savePath.empty() && !Echo::PathUtil::IsDir(savePath))
		{
			Echo::String resPath;
			if (Echo::IO::instance()->covertFullPathToResPath(savePath, resPath))
				EchoEngine::instance()->saveCurrentEditNodeTreeAs(resPath.c_str());

			// refresh respanel display
			m_resPanel->reslectCurrentDir();
		}
	}

	// new
	void MainWindow::onNewAnotherProject()
	{
		Echo::String newProjectPathName = AStudio::instance()->getProjectWindow()->newProject();
		if (!newProjectPathName.empty())
		{
			openAnotherProject(newProjectPathName);
		}
	}

	void MainWindow::onOpenAnotherProject()
	{
		QString projectName = QFileDialog::getOpenFileName(this, tr("Open Project"), "", tr("(*.echo)"));
		if (!projectName.isEmpty())
		{
			openAnotherProject(projectName.toStdString().c_str());
		}
	}

	// open another project
	void MainWindow::openAnotherProject(const Echo::String& fullPathName)
	{
		Echo::String app = QCoreApplication::applicationFilePath().toStdString().c_str();
		Echo::String cmd = Echo::StringUtil::Format("%s editopen %s", app.c_str(), fullPathName.c_str());

		QProcess process;
		process.startDetached(cmd.c_str());

		// exit
		close();
	}

	void MainWindow::onSaveasProject()
	{
		Echo::String projectName = QFileDialog::getSaveFileName(this, tr("Save as Project"), "", tr("(*.echo)")).toStdString().c_str();
		if (!projectName.empty())
		{
			// 0.confirm path and file name
			Echo::String fullPath = projectName;
			Echo::String filePath = Echo::PathUtil::GetFileDirPath(fullPath);
			Echo::String fileName = Echo::PathUtil::GetPureFilename(fullPath, false);
			Echo::String saveasPath = filePath + fileName + "/";

			if (!Echo::PathUtil::IsDirExist(saveasPath))
			{
				Echo::PathUtil::CreateDir(saveasPath);

				Echo::String currentProject = Echo::IO::instance()->getFullPath(Echo::Engine::instance()->getProjectFile()->getPath());
				Echo::String currentPath = Echo::PathUtil::GetFileDirPath(currentProject);
				Echo::String currentName = Echo::PathUtil::GetPureFilename(currentProject, true);

				// copy resource
				Echo::PathUtil::CopyDir(currentPath, saveasPath);

				// rename
				Echo::String currentPathname = saveasPath + currentName;
				Echo::String destPathName = saveasPath + fileName + ".echo";
				Echo::PathUtil::RenameFile(currentPathname, destPathName);

				// open dest folder
				QString qSaveasPath = saveasPath.c_str();
				QDesktopServices::openUrl(qSaveasPath);
			}
			else
			{
				EchoLogError("Save as directory [%s] isn't null", saveasPath.c_str());
			}
		}
	}

	// Open rencent project
	void MainWindow::onOpenRencentProject()
	{
		QAction* action = qobject_cast<QAction*>(sender());
		if (action)
		{
			Echo::String text = action->text().toStdString().c_str();
			if (Echo::PathUtil::IsFileExist(text))
				openAnotherProject(text);
			else
				EchoLogError("Project file [%s] not exist.", text.c_str());
		}
	}

	// sub editor operate
	void MainWindow::onSubEditChanged(const QString& subeditName)
	{
		Echo::String renderType = subeditName.toStdString().c_str();
		if (renderType == "2D")
		{
			Echo::Render::setRenderTypes(Echo::Render::Type_2D);
		}
		else if (renderType == "3D")
		{ 
			Echo::Render::setRenderTypes(Echo::Render::Type_3D);
		}
	}

	// 保存文件
	void MainWindow::onSaveProject()
	{
		// if path isn't exist. choose a save directory
		if (EchoEngine::instance()->getCurrentEditNode() && EchoEngine::instance()->getCurrentEditNodeSavePath().empty())
		{
			Echo::String savePath = PathChooseDialog::getExistingPathName(this, ".scene", "Save").toStdString().c_str();
			if (!savePath.empty() && !Echo::PathUtil::IsDir(savePath))
			{
				Echo::String resPath;
				if (Echo::IO::instance()->covertFullPathToResPath(savePath, resPath))
					EchoEngine::instance()->setCurrentEditNodeSavePath(resPath.c_str());
			}
		}

		// save current edit node
		EchoEngine::instance()->saveCurrentEditNodeTree();

		// save current edit res
		m_scenePanel->saveCurrentEditRes();
		
		// refresh respanel display
		m_resPanel->reslectCurrentDir();
	}

	// play game
	void MainWindow::onPlayGame()
	{
		// if launch scene not exist, set it
		Echo::ProjectSettings* projSettings = Echo::Engine::instance()->getProjectFile();
		if (projSettings)
		{
			const Echo::String& launchScene = projSettings->getLaunchScene().getPath();
			if (launchScene.empty())
			{
				if (QMessageBox::Yes == QMessageBox(QMessageBox::Information, "Warning", "Launch Scene is empty, Would you set it now?", QMessageBox::Yes | QMessageBox::No).exec())
				{
					Echo::String  scene = ResChooseDialog::getExistingFile(this, ".scene");
					if (!scene.empty())
					{
						projSettings->setLaunchScene(Echo::ResourcePath(scene));
					}
					else
					{
						return;
					}
				}
				else
				{
					return;
				}
			}
		}

		// save project
		onSaveProject();

		// start game
		if (!projSettings->getLaunchScene().getPath().empty())
		{
			Echo::String app = QCoreApplication::applicationFilePath().toStdString().c_str();
			Echo::String project = Echo::Engine::instance()->getConfig().m_projectFile;
			Echo::String cmd = Echo::StringUtil::Format("%s play %s", app.c_str(), project.c_str());

			m_gameProcess.terminate();
			m_gameProcess.waitForFinished();

			m_gameProcess.start(cmd.c_str());

			QObject::connect(&m_gameProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(onGameProcessFinished(int, QProcess::ExitStatus)));
			QObject::connect(&m_gameProcess, SIGNAL(readyRead()), this, SLOT(onReadMsgFromGame()));

			EchoLogWarning("**start game debug [%s]**", cmd.c_str());
		}
	}

	// 打开文件
	void MainWindow::OpenProject(const char* projectName)
	{
		AStudio::instance()->OpenProject(projectName);

		// 初始化渲染窗口
		AStudio::instance()->getRenderWindow();
	}

	// open lua file for edit
	void MainWindow::openLuaScript(const Echo::String& fileName)
	{
		Echo::String fullPath = Echo::IO::instance()->getFullPath(fileName);

		LuaEditor* editor = new LuaEditor(this);
		editor->open(fullPath);

		setCentralWidget(editor);

		//m_tabWidget->addTab( editor, fileName.c_str());
		//m_tabWidget->setCurrentIndex(m_tabWidget->count() - 1);

		QObject::connect(m_actionSaveProject, SIGNAL(triggered(bool)), editor, SLOT(save()));
	}

	void MainWindow::closeEvent(QCloseEvent *event)
	{
		AStudio::instance()->getLogPanel()->close();
	}

	// game process exit
	void MainWindow::onGameProcessFinished(int id, QProcess::ExitStatus status)
	{
		EchoLogWarning("stop game debug");
	}

	// receive msg from game
	void MainWindow::onReadMsgFromGame()
	{
		Echo::String msg = m_gameProcess.readAllStandardOutput().toStdString().c_str();
		if (!msg.empty())
		{
			Echo::StringArray msgArray = Echo::StringUtil::Split(msg, "@@");
			
			int i = 0;
			int argc = msgArray.size();
			while (i < argc)
			{
				Echo::String command = msgArray[i++];
				if (command == "-log")
				{
					int    logLevel     = Echo::StringUtil::ParseInt(msgArray[i++]);
					Echo::String logMsg = msgArray[i++];

					Echo::LogManager::instance()->logMessage(Echo::Log::LogLevel(logLevel), logMsg.c_str());
				}
			}
		}
	}

	// update rencent project display
	void MainWindow::updateRencentProjectsDisplay()
	{
		// clear
		m_menuRecents->clear();

		ConfigMgr* configMgr = AStudio::instance()->getConfigMgr();

		Echo::list<Echo::String>::type recentProjects;
		configMgr->getAllRecentProject(recentProjects);

		Echo::list<Echo::String>::iterator iter = recentProjects.begin();
		for (; iter != recentProjects.end(); ++iter)
		{
			Echo::String projectFullPath = (*iter).c_str();
			if (!projectFullPath.empty() && Echo::PathUtil::IsFileExist(projectFullPath))
			{
				Echo::String icon = Echo::PathUtil::GetFileDirPath(projectFullPath) + "icon.png";
				if (Echo::PathUtil::IsFileExist(icon))
				{
					QAction* action = new QAction(this);
					action->setText(projectFullPath.c_str());
					action->setIcon(QIcon(icon.c_str()));

					m_menuRecents->addAction(action);

					QObject::connect(action, SIGNAL(triggered()), this, SLOT(onOpenRencentProject()));
				}
			}
		}
	}
}