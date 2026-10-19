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

	// LARGE TEXT - Better space usage
	QString styleSheet = R"(
		QMainWindow {
			background-color: #2b2b2b;
			color: #ffffff;
			font-size: 14pt;
		}
		
		QDockWidget {
			background-color: #1e1e1e;
			border: none;
			font-size: 14pt;
			min-width: 250px;
		}
		
		QDockWidget::title {
			background-color: #333333;
			color: #ffffff;
			padding: 12px;
			font-weight: bold;
			font-size: 16pt;
		}
		
		QListWidget {
			background-color: #1e1e1e;
			border: none;
			font-size: 14pt;
			color: #ffffff;
		}
		
		QListWidget::item {
			padding: 16px 20px;
			margin: 2px 6px;
			border-radius: 6px;
			font-weight: bold;
			font-size: 14pt;
		}
		
		QListWidget::item:selected {
			background-color: #0078d4;
			color: #ffffff;
		}
		
		/* MAIN CONTENT - MUCH LARGER TEXT */
		QLabel {
			color: #ffffff;
			font-size: 16pt;
			font-weight: bold;
			padding: 8px;
		}
		
		/* USB Device section - HUGE text */
		QGroupBox {
			color: #ffffff;
			font-weight: bold;
			font-size: 20pt;
			border: 2px solid #0078d4;
			border-radius: 8px;
			margin: 20px 10px;
			padding: 20px 10px;
		}
		
		QGroupBox::title {
			subcontrol-origin: margin;
			left: 20px;
			padding: 0 10px;
			background-color: #2b2b2b;
			font-size: 20pt;
			font-weight: bold;
		}
		
		/* Form fields - MUCH LARGER */
		QLineEdit {
			background-color: #404040;
			border: 2px solid #606060;
			color: #ffffff;
			padding: 12px 16px;
			border-radius: 6px;
			font-size: 16pt;
			font-weight: bold;
			min-height: 40px;
		}
		
		QComboBox {
			background-color: #404040;
			border: 2px solid #606060;
			color: #ffffff;
			padding: 12px 16px;
			border-radius: 6px;
			font-size: 16pt;
			font-weight: bold;
			min-height: 40px;
		}
		
		QComboBox QAbstractItemView {
			background-color: #404040;
			border: 2px solid #606060;
			color: #ffffff;
			font-size: 16pt;
			font-weight: bold;
		}
		
		/* Checkboxes - BIGGER */
		QCheckBox {
			color: #ffffff;
			font-size: 16pt;
			font-weight: bold;
			spacing: 12px;
			padding: 8px;
		}
		
		QCheckBox::indicator {
			width: 24px;
			height: 24px;
		}
		
		/* Tabs - LARGER */
		QTabBar::tab {
			background-color: #404040;
			color: #ffffff;
			padding: 16px 24px;
			margin-right: 4px;
			font-weight: bold;
			font-size: 16pt;
			border-top-left-radius: 8px;
			border-top-right-radius: 8px;
			min-width: 150px;
		}
		
		QTabBar::tab:selected {
			background-color: #0078d4;
			color: #ffffff;
		}
		
		/* Buttons - MUCH LARGER */
		QPushButton {
			background-color: #0078d4;
			color: #ffffff;
			border: none;
			padding: 16px 24px;
			border-radius: 8px;
			font-weight: bold;
			font-size: 16pt;
			min-height: 50px;
		}
		
		QPushButton:hover {
			background-color: #106ebe;
		}
		
		/* Tables and lists - LARGER TEXT */
		QTableWidget, QTreeWidget, QListWidget {
			font-size: 14pt;
			font-weight: bold;
		}
		
		QTableWidget::item, QTreeWidget::item {
			padding: 12px;
			font-size: 14pt;
			font-weight: bold;
		}
		
		QHeaderView::section {
			background-color: #404040;
			color: #ffffff;
			padding: 16px;
			font-weight: bold;
			font-size: 16pt;
			border: 1px solid #606060;
		}
		
		/* Toolbar - LARGER */
		QToolButton {
			background-color: #404040;
			color: #ffffff;
			border: 2px solid #606060;
			padding: 12px 20px;
			border-radius: 8px;
			font-weight: bold;
			font-size: 14pt;
			min-height: 40px;
			min-width: 100px;
		}
		
		/* Menu - LARGER */
		QMenuBar {
			background-color: #333333;
			color: #ffffff;
			font-size: 16pt;
			font-weight: bold;
		}
		
		QMenuBar::item {
			padding: 12px 20px;
			font-size: 16pt;
		}
		
		QMenu {
			background-color: #404040;
			color: #ffffff;
			font-size: 14pt;
			font-weight: bold;
		}
		
		QMenu::item {
			padding: 12px 24px;
			font-size: 14pt;
		}
		
		/* Status bar - LARGER */
		QStatusBar {
			background-color: #333333;
			color: #ffffff;
			font-size: 14pt;
			font-weight: bold;
			padding: 8px;
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