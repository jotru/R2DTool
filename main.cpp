// Written by: Stevan Gavrilovic
// Latest revision: 10.13.2020

#include "MainWindowWorkflowApp.h"
#include "AgaveCurl.h"
#include "WorkflowAppRDT.h"
#include "GoogleAnalytics.h"
#include "RDTUserPass.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QObject>
#include <QOpenGLWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextStream>
#include <QThread>
#include <QTime>


#include "ArcGISRuntimeEnvironment.h"

using namespace Esri::ArcGISRuntime;


static QString logFilePath;
static bool logToFile = false;

// customMessgaeOutput code taken from web:
// https://stackoverflow.com/questions/4954140/how-to-redirect-qdebug-qwarning-qcritical-etc-output

void customMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QHash<QtMsgType, QString> msgLevelHash({{QtDebugMsg, "Debug"}, {QtInfoMsg, "Info"}, {QtWarningMsg, "Warning"}, {QtCriticalMsg, "Critical"}, {QtFatalMsg, "Fatal"}});
    QByteArray localMsg = msg.toLocal8Bit();
    QTime time = QTime::currentTime();
    QString formattedTime = time.toString("hh:mm:ss.zzz");
    QByteArray formattedTimeMsg = formattedTime.toLocal8Bit();
    QString logLevelName = msgLevelHash[type];
    QByteArray logLevelMsg = logLevelName.toLocal8Bit();

    if (logToFile) {
        QString txt = QString("%1 %2: %3 (%4)").arg(formattedTime, logLevelName, msg,  context.file);
        QFile outFile(logFilePath);
        outFile.open(QIODevice::WriteOnly | QIODevice::Append);
        QTextStream ts(&outFile);
        ts << txt << Qt::endl;
        outFile.close();
    } else {
        fprintf(stderr, "%s %s: %s (%s:%u, %s)\n", formattedTimeMsg.constData(), logLevelMsg.constData(), localMsg.constData(), context.file, context.line, context.function);
        fflush(stderr);
    }

    if (type == QtFatalMsg)
        abort();
}


int main(int argc, char *argv[])
{

#ifdef Q_OS_WIN
    QApplication::setAttribute(Qt::AA_UseOpenGLES);
#endif

    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    // Setting Core Application Name, Organization, Version and Google Analytics Tracking Id
    QCoreApplication::setApplicationName("RDT");
    QCoreApplication::setOrganizationName("SimCenter");
    QCoreApplication::setApplicationVersion("2.1.0");

    // GoogleAnalytics::SetTrackingId("UA-126303135-1");
    // GoogleAnalytics::StartSession();
    // GoogleAnalytics::ReportStart();

    // set up logging of output messages for user debugging
    logFilePath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + QDir::separator() + QCoreApplication::applicationName();

    // make sure tool dir exists in Documentss folder
    QDir dirWork(logFilePath);
    if (!dirWork.exists())
        if (!dirWork.mkpath(logFilePath))   {
            qDebug() << QString("Could not create Working Dir: ") << logFilePath;
        }

    // Full path to debug.log file
    logFilePath = logFilePath + QDir::separator() + QString("debug.log");

    // Remove old log file
    QFile debugFile(logFilePath);
    debugFile.remove();

    QByteArray envVar = qgetenv("QTDIR");  //  check if the app is run in Qt Creator

    if (envVar.isEmpty())
        logToFile = true;

    qInstallMessageHandler(customMessageOutput);

    // window scaling
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    /******************code to reset openGL version .. keep around in case need again
    QSurfaceFormat glFormat;
    glFormat.setVersion(3, 3);
    glFormat.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(glFormat);
    *********************************************************************************/

    // Regular Qt startup
    QApplication a(argc, argv);

    // Set the key for the ArcGIS interface
    ArcGISRuntimeEnvironment::setLicense(getArcGISKey());

    // create a remote interface
    QString tenant("designsafe");
    QString storage("agave://designsafe.storage.default/");
    QString dirName("RDT");

    AgaveCurl *theRemoteService = new AgaveCurl(tenant, storage, &dirName);


    // Create the main window
    WorkflowAppRDT *theInputApp = new WorkflowAppRDT(theRemoteService);
    MainWindowWorkflowApp w(QString("RDT: Regional Resilience Determination Tool"), theInputApp, theRemoteService);

    // Create the  menu bar and actions to run the examples
    theInputApp->initialize();


    QString aboutTitle = "About the SimCenter EE-UQ Application"; // this is the title displayed in the on About dialog
    QString aboutSource = ":/resources/docs/textAboutRDT.html";  // this is an HTML file stored under resources

    w.setAbout(aboutTitle, aboutSource);

    QString version("Version 1.0.0");
    w.setVersion(version);

    QString citeText("TO DO RDT CITATION");
    w.setCite(citeText);

    QString manualURL("https://nheri-simcenter.github.io/EE-UQ-Documentation/");
    w.setDocumentationURL(manualURL);

    QString messageBoardURL("https://simcenter-messageboard.designsafe-ci.org/smf/index.php?board=6.0");
    w.setFeedbackURL(messageBoardURL);


    // Move remote interface to a thread
    QThread *thread = new QThread();
    theRemoteService->moveToThread(thread);

    QWidget::connect(thread, SIGNAL(finished()), theRemoteService, SLOT(deleteLater()));
    QWidget::connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    thread->start();

    // Show the main window, set styles & start the event loop
    w.show();
    w.statusBar()->showMessage("Ready", 5000);

#ifdef Q_OS_WIN
    QFile commonStyleSheetFile(":/styleCommon/stylesheetWIN.qss");
#endif

#ifdef Q_OS_MACOS
    QFile commonStyleSheetFile(":/styleCommon/stylesheetMAC.qss");
#endif

#ifdef Q_OS_LINUX
    QFile commonStyleSheetFile(":/styleCommon/stylesheetMAC.qss");
#endif

    QFile RDTstyleSheetFile(":/styles/stylesheetRDT.qss");

    if(commonStyleSheetFile.open(QFile::ReadOnly) && RDTstyleSheetFile.open(QFile::ReadOnly))
    {
        auto commonStyleSheet = commonStyleSheetFile.readAll();
        auto RDTStyleSheet = RDTstyleSheetFile.readAll();

        // Append the RDT stylesheet to the common stylesheet
        commonStyleSheet.append(RDTStyleSheet);

        a.setStyleSheet(commonStyleSheet);
        commonStyleSheetFile.close();
        RDTstyleSheetFile.close();
    }
    else
    {
        qDebug() << "could not open stylesheet";
    }


    int res = a.exec();

    // On done with event loop, logout & stop the thread
    theRemoteService->logout();
    thread->quit();

    // GoogleAnalytics::EndSession();

    return res;
}
