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

	// Modern iConnectivity-style interface
	QString styleSheet = R"(
		/* Main window */
		QMainWindow {
			background-color: #2b2b2b;
			color: #ffffff;
		}
		
		/* Left navigation panel */
		QDockWidget {
			background-color: #1e1e1e;
			border: none;
			font-size: 11pt;
			min-width: 200px;
		}
		
		QDockWidget::title {
			background-color: #333333;
			color: #ffffff;
			padding: 8px;
			font-weight: bold;
			text-align: center;
		}
		
		/* Navigation list */
		QListWidget {
			background-color: #1e1e1e;
			border: none;
			font-size: 11pt;
			color: #ffffff;
		}
		
		QListWidget::item {
			padding: 12px 16px;
			margin: 1px 4px;
			border-radius: 4px;
			font-weight: bold;
		}
		
		QListWidget::item:selected {
			background-color: #0078d4;
			color: #ffffff;
		}
		
		QListWidget::item:hover {
			background-color: #404040;
		}
		
		/* Main content area */
		QWidget {
			background-color: #2b2b2b;
			color: #ffffff;
		}
		
		/* Tabs - like the original */
		QTabWidget::pane {
			border: 1px solid #404040;
			background-color: #2b2b2b;
		}
		
		QTabBar::tab {
			background-color: #404040;
			color: #ffffff;
			padding: 8px 16px;
			margin-right: 2px;
			font-weight: bold;
			border-top-left-radius: 4px;
			border-top-right-radius: 4px;
		}
		
		QTabBar::tab:selected {
			background-color: #0078d4;
			color: #ffffff;
		}
		
		QTabBar::tab:hover:!selected {
			background-color: #505050;
		}
		
		/* Form elements */
		QLabel {
			color: #ffffff;
			font-size: 10pt;
		}
		
		QLineEdit {
			background-color: #404040;
			border: 1px solid #606060;
			color: #ffffff;
			padding: 6px;
			border-radius: 3px;
			font-size: 10pt;
		}
		
		QLineEdit:focus {
			border: 2px solid #0078d4;
		}
		
		QComboBox {
			background-color: #404040;
			border: 1px solid #606060;
			color: #ffffff;
			padding: 6px;
			border-radius: 3px;
			font-size: 10pt;
			min-height: 20px;
		}
		
		QComboBox:hover {
			border: 1px solid #0078d4;
		}
		
		QComboBox::drop-down {
			border: none;
			width: 20px;
		}
		
		QComboBox::down-arrow {
			image: none;
			border-left: 4px solid transparent;
			border-right: 4px solid transparent;
			border-top: 4px solid #ffffff;
		}
		
		QComboBox QAbstractItemView {
			background-color: #404040;
			border: 1px solid #606060;
			color: #ffffff;
			selection-background-color: #0078d4;
		}
		
		/* Checkboxes */
		QCheckBox {
			color: #ffffff;
			font-size: 10pt;
			spacing: 8px;
		}
		
		QCheckBox::indicator {
			width: 16px;
			height: 16px;
		}
		
		QCheckBox::indicator:unchecked {
			background-color: #404040;
			border: 1px solid #606060;
			border-radius: 2px;
		}
		
		QCheckBox::indicator:checked {
			background-color: #0078d4;
			border: 1px solid #0078d4;
			border-radius: 2px;
		}
		
		/* Buttons */
		QPushButton {
			background-color: #0078d4;
			color: #ffffff;
			border: none;
			padding: 8px 16px;
			border-radius: 4px;
			font-weight: bold;
			font-size: 10pt;
		}
		
		QPushButton:hover {
			background-color: #106ebe;
		}
		
		QPushButton:pressed {
			background-color: #005a9e;
		}
		
		/* Group boxes */
		QGroupBox {
			color: #ffffff;
			font-weight: bold;
			font-size: 11pt;
			border: 1px solid #606060;
			border-radius: 4px;
			margin-top: 10px;
			padding-top: 10px;
		}
		
		QGroupBox::title {
			subcontrol-origin: margin;
			left: 10px;
			padding: 0 5px 0 5px;
			background-color: #2b2b2b;
		}
		
		/* Toolbar */
		QToolBar {
			background-color: #333333;
			border: none;
			spacing: 4px;
			padding: 4px;
		}
		
		QToolButton {
			background-color: #404040;
			color: #ffffff;
			border: 1px solid #606060;
			padding: 6px 12px;
			border-radius: 4px;
			font-weight: bold;
		}
		
		QToolButton:hover {
			background-color: #0078d4;
			border: 1px solid #0078d4;
		}
		
		/* Menu bar */
		QMenuBar {
			background-color: #333333;
			color: #ffffff;
			border-bottom: 1px solid #606060;
		}
		
		QMenuBar::item {
			padding: 6px 12px;
			background-color: transparent;
		}
		
		QMenuBar::item:selected {
			background-color: #0078d4;
		}
		
		QMenu {
			background-color: #404040;
			color: #ffffff;
			border: 1px solid #606060;
		}
		
		QMenu::item {
			padding: 6px 20px;
		}
		
		QMenu::item:selected {
			background-color: #0078d4;
		}
		
		/* Status bar */
		QStatusBar {
			background-color: #333333;
			color: #ffffff;
			border-top: 1px solid #606060;
		}
		
		/* Scrollbars */
		QScrollBar:vertical {
			background-color: #2b2b2b;
			width: 12px;
			border: none;
		}
		
		QScrollBar::handle:vertical {
			background-color: #606060;
			border-radius: 6px;
			min-height: 20px;
		}
		
		QScrollBar::handle:vertical:hover {
			background-color: #0078d4;
		}
		
		QScrollBar:horizontal {
			background-color: #2b2b2b;
			height: 12px;
			border: none;
		}
		
		QScrollBar::handle:horizontal {
			background-color: #606060;
			border-radius: 6px;
			min-width: 20px;
		}
		
		QScrollBar::handle:horizontal:hover {
			background-color: #0078d4;
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