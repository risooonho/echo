#include "MainWindow.h"
#include "Studio.h"
#include "LogPanel.h"
#include "ResPanel.h"
#include "NodeTreePanel.h"
#include <QFileDialog>
#include <QDesktopservices>
#include <QShortcut>
#include <QMdiArea>
#include "TimelinePanel.h"
#include "DebuggerPanel.h"
#include "EchoEngine.h"
#include "PlayGameToolBar.h"
#include "QResSelect.h"
#include "ResChooseDialog.h"

namespace Studio
{
	// 构造函数
	MainWindow::MainWindow(QMainWindow* parent/*=0*/)
		: QMainWindow( parent)
		, m_resPanel(nullptr)
		, m_timelinePanel(nullptr)
		, m_debuggerPanel(nullptr)
		, m_gameProcess(nullptr)
		, m_playGameToolBar(nullptr)
	{
		setupUi( this);

		// 隐藏标题
		setWindowFlags(windowFlags() | Qt::FramelessWindowHint);

		// 设置菜单左上控件
		menubar->setTopLeftCornerIcon(":/icon/Icon/icon.png");

		// connect signal slot
		QObject::connect(m_actionSave, SIGNAL(triggered(bool)), this, SLOT(onSaveProject()));
		QObject::connect(m_actionPlayGame, SIGNAL(triggered(bool)), this, SLOT(onPlayGame()));
		QObject::connect(m_actionStopGame, SIGNAL(triggered(bool)), &m_gameProcess, SLOT(terminate()));
		QObject::connect(m_actionExitEditor, SIGNAL(triggered(bool)), this, SLOT(close()));

		// connect
		QT_UI::QResSelect::setOpenFileDialogFunction(ResChooseDialog::getExistingFile);
	}

	// 析构函数
	MainWindow::~MainWindow()
	{
	}

	// 打开项目时调用
	void MainWindow::onOpenProject()
	{
		m_resPanel = EchoNew(ResPanel(this));
		m_scenePanel = EchoNew(NodeTreePanel(this));
		m_timelinePanel = EchoNew(TimelinePanel(this));
		m_debuggerPanel = EchoNew(DebuggerPanel(this));

		//QMdiArea* midArea = new QMdiArea(this);

		QWidget* renderWindow = AStudio::Instance()->getRenderWindow();

		setCentralWidget(renderWindow);
		//midArea->addSubWindow(renderWindow);
		//m_playGameToolBar = EchoNew(PlayGameToolBar(centralWidget()));
		//centralWidget()->layout()->addWidget(m_playGameToolBar);
		//centralWidget()->layout()->addWidget(renderWindow);

		this->setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
		this->setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

		this->addDockWidget(Qt::LeftDockWidgetArea, m_resPanel);
		this->addDockWidget(Qt::RightDockWidgetArea, m_scenePanel);
		
		this->addDockWidget(Qt::BottomDockWidgetArea, AStudio::Instance()->getLogPanel());
		this->addDockWidget(Qt::BottomDockWidgetArea, m_debuggerPanel);
		this->addDockWidget(Qt::BottomDockWidgetArea, m_timelinePanel);

		this->tabifyDockWidget(AStudio::Instance()->getLogPanel(), m_debuggerPanel);
		this->tabifyDockWidget(m_debuggerPanel, m_timelinePanel);

		m_resPanel->onOpenProject();
	}

	// 保存文件
	void MainWindow::onSaveProject()
	{
		Studio::EchoEngine::Instance()->saveCurrentEditNodeTree();
	}

	// play game
	void MainWindow::onPlayGame()
	{
		onSaveProject();

		Echo::String app = QCoreApplication::applicationFilePath().toStdString().c_str();
		Echo::String project = Echo::Root::instance()->getProjectFile()->getPathName();
		Echo::String cmd = Echo::StringUtil::Format("%s play %s", app.c_str(), project.c_str());

		m_gameProcess.terminate();
		m_gameProcess.waitForFinished();

		m_gameProcess.start(cmd.c_str());

		QObject::connect(&m_gameProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(onGameProcessFinished(int, QProcess::ExitStatus)));
		QObject::connect(&m_gameProcess, SIGNAL(readyRead()), this, SLOT(onReadMsgFromGame()));

		EchoLogWarning("**start game debug [%s]**", cmd.c_str());
	}

	// 打开文件
	void MainWindow::OpenProject(const char* projectName)
	{
		AStudio::Instance()->OpenProject(projectName);

		// 初始化渲染窗口
		AStudio::Instance()->getRenderWindow();
	}

	void MainWindow::closeEvent(QCloseEvent *event)
	{
		AStudio::Instance()->getLogPanel()->close();
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
			Echo::StringArray msgArray = Echo::StringUtil::Split(msg, "@:");
			
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
}