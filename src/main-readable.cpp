#include "iconnconfigmain.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#include <QFile>

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	QCoreApplication::setOrganizationName("punkt-k");
	QCoreApplication::setOrganizationDomain("www.punkt-k.de");
	QCoreApplication::setApplicationName("iConnConfig");
	QCoreApplication::setApplicationVersion("0.5-beta");

	// Load readable stylesheet
	QFile styleFile("readable-style.qss");
	if (styleFile.open(QFile::ReadOnly)) {
		QString style = QLatin1String(styleFile.readAll());
		app.setStyleSheet(style);
	} else {
		// Fallback inline style for readability
		QString styleSheet = R"(
			QDockWidget {
				font-size: 12pt;
				font-weight: bold;
				min-width: 280px;
			}
			
			QDockWidget::title {
				padding: 10px;
				font-weight: bold;
				font-size: 12pt;
				text-align: center;
			}
			
			QListWidget {
				font-size: 13pt;
				font-weight: bold;
			}
			
			QListWidget::item {
				padding: 15px;
				margin: 3px;
				border-radius: 8px;
				font-size: 13pt;
				font-weight: bold;
				min-height: 35px;
			}
			
			QListWidget::item:selected {
				background: palette(highlight);
				color: palette(highlighted-text);
			}
			
			QComboBox {
				padding: 10px 15px;
				font-size: 12pt;
				font-weight: bold;
				min-height: 30px;
				border-radius: 6px;
			}
			
			QTabBar::tab {
				padding: 15px 25px;
				margin: 3px;
				font-size: 13pt;
				font-weight: bold;
				min-width: 140px;
				border-radius: 8px;
			}
			
			QPushButton {
				padding: 12px 20px;
				font-size: 12pt;
				font-weight: bold;
				min-height: 30px;
				border-radius: 8px;
			}
			
			QGroupBox {
				font-weight: bold;
				font-size: 14pt;
				margin: 20px 0px;
				padding-top: 25px;
			}
			
			QGroupBox::title {
				font-size: 14pt;
				font-weight: bold;
				padding: 0 10px;
			}
			
			QLabel {
				font-size: 12pt;
			}
			
			QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
				padding: 10px;
				font-size: 12pt;
				min-height: 30px;
				border-radius: 6px;
			}
		)";
		app.setStyleSheet(styleSheet);
	}

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