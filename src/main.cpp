#include "app/Args.h"
#include "app/CliExport.h"
#include "app/UiTheme.h"
#include "ui/DiagramView.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QFileInfo>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(QStringLiteral("drawio-qt"));
    QCoreApplication::setApplicationVersion(QStringLiteral(DRAWIO_QT_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("drawio-qt"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("drawio-qt.local"));

    // QCoreApplication::arguments() is not available before the application
    // object exists, so build the list from argv for the early CLI handling.
    QStringList arguments;

    for (int index = 0; index < argc; ++index)
    {
        arguments.append(QString::fromLocal8Bit(argv[index]));
    }

    const dq::ParseResult parsed = dq::Args::parse(arguments);

    // Headless machines (CI, servers) can still export files by selecting the
    // offscreen platform before QApplication exists.
    const bool hasDisplay = !qEnvironmentVariableIsEmpty("DISPLAY")
        || !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");

    if (parsed.options.exportMode && !hasDisplay
        && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QTextStream out(stdout);
    QTextStream err(stderr);

    // Help and version do not need a QGuiApplication (and avoid loading the
    // platform plugin entirely).
    if (parsed.help)
    {
        out << dq::Args::helpText(QStringLiteral(DRAWIO_QT_VERSION)) << '\n';
        return 0;
    }

    if (parsed.version)
    {
        out << QStringLiteral(DRAWIO_QT_VERSION) << '\n';
        return 0;
    }

    QApplication app(argc, argv);
    QApplication::setApplicationDisplayName(QStringLiteral("drawio-qt"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));

    // A single, platform independent look: Fusion everywhere.
    dq::UiTheme::applyFusionStyle();

    if (parsed.options.exportMode)
    {
        if (!parsed.options.layout.isEmpty())
        {
            err << QStringLiteral("警告：当前版本尚未实现 --layout，已忽略。\n");
        }

        if (parsed.options.normalize)
        {
            err << QStringLiteral("警告：当前版本尚未实现 --normalize，已忽略。\n");
        }

        return dq::CliExport::run(parsed.options);
    }

    QApplication::setQuitOnLastWindowClosed(true);

    auto *window = new dq::MainWindow();
    window->show();

    if (!parsed.options.inputs.isEmpty())
    {
        bool first = true;

        for (const QString &input : parsed.options.inputs)
        {
            if (first)
            {
                window->openFile(input);
                first = false;
            }
            else
            {
                auto *other = new dq::MainWindow();
                other->resize(window->size());
                other->show();
                other->openFile(input);
            }
        }
    }
    else if (parsed.options.create)
    {
        window->newDocument();
    }

    if (parsed.options.zoom > 0 && window->view() != nullptr)
    {
        window->view()->setZoom(parsed.options.zoom);
    }

    return app.exec();
}
