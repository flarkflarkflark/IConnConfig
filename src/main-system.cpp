#include "iconnconfigmain.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	QCoreApplication::setOrganizationName("punkt-k");
	QCoreApplication::setOrganizationDomain("www.punkt-k.de");
	QCoreApplication::setApplicationName("iConnConfig");
	QCoreApplication::setApplicationVersion("0.5-beta");

	// Use system theme and colors - no forced palette
	// This will respect Klassy/KDE theme settings
	
	// Only minimal styling for better usability
	QString styleSheet = R"(
		QDockWidget {
			font-size: 11pt;
			font-weight: bold;
			min-width: 250px;
		}
		
		QDockWidget::title {
			padding: 8px;
			font-weight: bold;
			font-size: 11pt;
		}
		
		QTabBar::tab {
			padding: 8px 16px;
			margin: 2px;
			font-size: 11pt;
			font-weight: bold;
			min-width: 100px;
		}
		
		QPushButton {
			padding: 8px 16px;
			font-size: 11pt;
			font-weight: bold;
			min-height: 20px;
		}
		
		QGroupBox {
			font-weight: bold;
			font-size: 12pt;
			margin: 15px 0px;
			padding-top: 15px;
		}
		
		QGroupBox::title {
			subcontrol-origin: margin;
			left: 15px;
			padding: 0 8px 0 8px;
		}
		
		QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
			padding: 6px;
			font-size: 10pt;
			min-height: 20px;
		}
		
		QTableWidget, QTreeWidget, QListWidget {
			font-size: 10pt;
		}
		
		QTableWidget::item, QTreeWidget::item, QListWidget::item {
			padding: 4px;
		}
		
		QHeaderView::section {
			padding: 6px;
			font-weight: bold;
			font-size: 10pt;
		}
		
		QToolButton {
			padding: 6px;
			font-size: 10pt;
			min-width: 60px;
			min-height: 30px;
		}
		
		QMenuBar {
			font-size: 11pt;
		}
		
		QMenuBar::item {
			padding: 6px 12px;
		}
		
		QMenu {
			font-size: 10pt;
		}
		
		QMenu::item {
			padding: 6px 20px;
		}
		
		QStatusBar {
			font-size: 10pt;
		}
	)";
	
	app.setStyleSheet(styleSheet);

	QTranslator mioConfigTranslator;
	QTranslator qtTranslator;
	QLocale::setDefault(QLocale("en_EN"));

	if (mioConfigTranslator.load(QLocale(), QLatin1String("iconnconfig"),
								 QLatin1String("_"),
								 QLatin1String(":/translations/tr"))) {
		app.installTranslator(&mioConfigTranslator);

		qtTranslator.load(
			QLocale(), QLatin1String("qt"), QLatin1String("_"),
			QLibraryInfo::location(QLibraryInfo::TranslationsPath));
		app.installTranslator(&qtTranslator);
	}

	QCommandLineParser *parser = new QCommandLineParser();
	parser->setApplicationDescription("iConnConfig - iConnectivity Device Configuration");
	parser->addHelpOption();
	parser->addVersionOption();

	QCommandLineOption filenameOption(
		QStringList() << "f" << "filename",
		QCoreApplication::translate("main", "filename to read settings from."),
		"filename");
	parser->addOption(filenameOption);

	parser->process(app);

	MioMain w(parser);
	w.show();
	return app.exec();
}